/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### LedStrip.cpp

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

***************************************************************************/

#include "LedStrip.h"
#include <SPI.h>

/*
 * Implementation:
 * -	Tailored to work over SPI1 on the STM32F401.
 * -	Takes over the SPI and configures it at 84MHz/4 = 21000000Hz. At this speed the SPI will
 * 		send a bit every ~47ns.
 * -	For each bit to send, there is a 3-bytes sequence representing the "0 code" and "1 code"
 * 		from the WS2812B datasheet. The sequences are:
 * 			- Send 0XFF, 0xFF, 0xE0 for 1. This will raise the MOSI line for about 904ns (900ns +-125ns)
 *			- Send 0xFE, 0x00, 0x00 for 0. This will raise the MOSI line for about 380ns (350ns +-125ns)
 * -	Each 3-bytes sequence is transmitted every 1142.82nS (1.25uS +-150nS).
 * -	Uses DMA2 Stream5 to send a 72 bytes array representing a single RGB LED (24 3-bytes sequences for
 * 		24-bits color).
 * -	There are two 72 bytes buffers: one for sending and one for filling with new data. It uses
 * 		DMA2 Stream5 Transfer Complete interrupt to switch buffers and send. Then it queue an EXTI
 * 		software interrupt at a lower priority to fill the buffer that is not being transmitted.
  */

#define LEDSTRIP_UPDATE_LIMITER	20


static uint8_t ws2812b_code0[3] = { 0XFE, 0x00, 0x00 };
static uint8_t ws2812b_code1[3] = { 0XFF, 0xFF, 0xE0 };

static volatile uint8_t on_tx; // Indicates when the SPI and DMA are free
static LedStrip* led_strip_tx = NULL;
static uint8_t* updating_buffer;
static uint32_t leds_to_update;

extern uint32_t getRandom(uint32_t min, uint32_t max);

LedStrip::LedStrip()
{
	initialized = false;
	next_tx = buffer = NULL;
	bitband_address = NULL;
	led_type = WS2812B;
	use_single_color = false;
	multiplier = 1.0f;
	single_color_sbba = NULL;
	led_count = 0;
}

LedStrip::~LedStrip()
{
}

bool LedStrip::begin(uint32_t count, LedStripeType type)
{
	if (initialized && count == led_count && type == led_type)
		return true;

	if (buffer)
		free(buffer);

	buffer = (uint8_t*) malloc(count * 3);
	if (!buffer)
		return false;

	bitband_address = (uint32_t*) (0x22000000 + ((uint32_t) buffer & 0x1FFFFFFF) * 32);

	led_count = count;
	led_type = type;

	memset(buffer, 0, led_count * 3);

	SPI.beginTxOnly();
	initialized = true;

	return true;
}

void LedStrip::end()
{
	if (initialized)
	{
		SPI.endTxOnly();

		if (buffer)
		{
			free(buffer);
			buffer = NULL;
			initialized = false;
		}
	}
}

void LedStrip::packWS2812B(uint8_t r, uint8_t g, uint8_t b, uint8_t* ptr, uint32_t* index)
{
	uint32_t idx = 0;

	if (index == NULL)
		index = &idx;

	r = cie_lut[r] * multiplier;
	g = cie_lut[g] * multiplier;
	b = cie_lut[b] * multiplier;

	ptr[(*index)++] = (__RBIT(g) >> 24);
	ptr[(*index)++] = (__RBIT(r) >> 24);
	ptr[(*index)++] = (__RBIT(b) >> 24);
}

void LedStrip::setWS2812B(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t count = 1;

	if (index == 0)
		count = led_count;
	else
		index = (index - 1) * 3;

	while (count)
	{
		packWS2812B(r, g, b, buffer, &index);
		count--;
	}
}

void LedStrip::setAPA102(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
	/* TBD */
}

void LedStrip::set(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
	if (!initialized || index > led_count)
		return;

	if (led_type == WS2812B)
		setWS2812B(index, r, g, b);
	else if (led_type == APA102)
		setAPA102(index, r, g, b);
}

void LedStrip::set(uint32_t index, COLOR color)
{
	set(index, getRed(color), getGreen(color), getBlue(color));
}

bool LedStrip::update(uint32_t index, bool async)
{
	if (withEffect())
		stopEffect();

	use_single_color = false;
	return updateInternal(index, async);
}

bool LedStrip::updateFromEffect()
{
	return updateInternal(0, true);
}

bool LedStrip::updateInternal(uint32_t index, bool async)
{
	bool spi_enabled;
	uint32_t count;

	if (!initialized)
		return false;

	if (async && on_tx)
		return false;

	if (index == 0 || index > led_count)
		count = led_count;
	else
		count = index;

	if (!count)
		return false;

	// Wait for any ongoing transmission
	while (on_tx);
	on_tx = 1;

	if (led_type == WS2812B)
	{
		if (use_single_color)
		{
			// Remember the fixed color variable bit-band start address
			single_color_sbba = (uint32_t*) (0x22000000 + ((uint32_t) single_color & 0x1FFFFFFF) * 32);

			// Set bit-band pointer to the beginning of the variable containing the fixed color to send
			bitband_address = single_color_sbba;
		} else {
			// Set bitband pointer to the beginning of the buffer
			bitband_address = (uint32_t*) (0x22000000 + ((uint32_t) buffer & 0x1FFFFFFF) * 32);
			single_color_sbba = 0;
		}

		leds_to_update = count;
		led_strip_tx = this;

		// Pack the first LED into the buffer 1
		bitband_address = packIntoBuffer(update_buffer1, bitband_address);

		// Pack the second LED into the buffer 2
		if (leds_to_update)
		{
			if (single_color_sbba)
				bitband_address = single_color_sbba;

			bitband_address = packIntoBuffer(update_buffer2, bitband_address);
			updating_buffer = update_buffer2;
		}

		// Configure SPI, end any pending transaction
		SPI.endTransaction();
		SPI.beginTransaction(SPISettings(21000000, MSBFIRST, SPI_MODE1, true));

		// Configure EXTI2
		EXTI_ClearFlag(EXTI_Line2);
		NVIC_ClearPendingIRQ(EXTI2_IRQn);

		EXTI_InitTypeDef EXTI_InitStruct;

		EXTI_InitStruct.EXTI_Line = EXTI_Line2;
		EXTI_InitStruct.EXTI_LineCmd = ENABLE;
		EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
		EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
		EXTI_Init(&EXTI_InitStruct);

		NVIC_SetPriority(EXTI2_IRQn, VARIANT_PRIO_LEDSTRIP_EXTI);
		NVIC_EnableIRQ(EXTI2_IRQn);

		// Configure DMA2 Stream5
		NVIC_ClearPendingIRQ(DMA2_Stream5_IRQn);
		NVIC_SetPriority(DMA2_Stream5_IRQn, VARIANT_PRIO_LEDSTRIP_DMA);
		NVIC_EnableIRQ(DMA2_Stream5_IRQn);

		// Disable DMA2 Stream5
		DMA2_Stream5->CR = 0;

		// Configure DMA2 Stream5
		DMA2->HIFCR = 0xF00;
		DMA2_Stream5->NDTR = 72;
		DMA2_Stream5->M0AR = (uint32_t) update_buffer1;
		DMA2_Stream5->PAR = (uint32_t) &SPI1->DR;
		DMA2_Stream5->FCR = 0;
		DMA2_Stream5->CR =	DMA_Channel_3 |
							DMA_DIR_MemoryToPeripheral	|
							DMA_MemoryInc_Enable		|
							DMA_PeripheralDataSize_Byte |
							DMA_MemoryDataSize_Byte		|
							DMA_PeripheralBurst_Single	|
							DMA_MemoryBurst_Single |
							DMA_IT_TC;

		leds_to_update--;

		// Kickstart DMA
		DMA2_Stream5->CR |= DMA_SxCR_EN;

		// Enable SPI TX DMA requests
		SPI1->CR2 |= SPI_CR2_TXDMAEN;

		if (!async)
		{
			// Wait
			while (on_tx);

			// Wait 50uS for reset
			delayMicroseconds(50);

			endUpdate();
		}
	} else if (led_type == APA102)
	{
		/* TBD */
		on_tx = 0;
	}
}

void LedStrip::endUpdate()
{
	// Wait for SPI to finish
	while (!(SPI1->SR & SPI_SR_TXE) || (SPI1->SR & SPI_SR_BSY));
	SPI.endTransaction();
}

bool LedStrip::updating()
{
	if (on_tx && led_strip_tx == this)
		return true;

	return false;
}

void LedStrip::setMultiplier(float value)
{
	if (value > 1)
		value = 1;
	else if (value < 0)
		value = 0;

	multiplier = value;
}

void LedStrip::stopEffect()
{
	if (effect.active)
	{
		remove();
		effect.active = false;
		while (on_tx);
	}
}

uint32_t* LedStrip::packIntoBuffer(uint8_t* dst, uint32_t* bbaddr)
{
	uint32_t idx = 0;

	for (uint8_t i = 0; i < 24; i++)
	{
		if (*bbaddr++)
		{
			*dst++ = ws2812b_code1[0];
			*dst++ = ws2812b_code1[1];
			*dst++ = ws2812b_code1[2];
		} else {
			*dst++ = ws2812b_code0[0];
			*dst++ = ws2812b_code0[1];
			*dst++ = ws2812b_code0[2];
		}
	}

	return bbaddr;
}

void LedStrip::swInterrupt()
{
	uint8_t* pack_buffer;

	if (leds_to_update > 1)
	{
		if (updating_buffer == update_buffer1)
			updating_buffer = update_buffer2;
		else
			updating_buffer = update_buffer1;

		if (single_color_sbba)
			bitband_address = single_color_sbba;

		bitband_address = packIntoBuffer(updating_buffer, bitband_address);
		leds_to_update--;
	} else if (leds_to_update == 1)
	{
		// Last LED being transmitted
		leds_to_update--;
	} else {
		// Last LED sent, end procedure
		DMA2_Stream5->CR = 0;
		NVIC_DisableIRQ(DMA2_Stream5_IRQn);
		NVIC_DisableIRQ(EXTI2_IRQn);
		led_strip_tx = NULL;
		on_tx = 0;
	}
}

void LedStrip::shimmer(COLOR color, uint8_t amplitude, uint32_t hz, bool random)
{
	uint32_t cycle, half_cycle;

	if (!hz || !amplitude)
		return;

	if (amplitude > 100)
		amplitude = 100;

	if (effect.active)
		stopEffect();

	effect.type = LedStripEffectShimmer;
	effect.tick_count = 0;
	effect.ticks = 1;

	cycle = getFrequency() / hz;
	half_cycle = cycle / 2;

	effect.params.shimmer.value = 100;
	effect.params.shimmer.low = 100 - amplitude;
	effect.params.shimmer.random = random;

	if (random)
	{
		effect.ticks = cycle;
	} else {
		effect.params.shimmer.step = amplitude / half_cycle;
		if (effect.params.shimmer.step == 0)
		{
			effect.ticks = half_cycle / amplitude;
			effect.params.shimmer.step = 1;
		}

		effect.params.shimmer.update_count = 0;
		effect.params.shimmer.update_every = 20;
	}

	effect.params.shimmer.color = color;
	effect.params.shimmer.r = getRed(color);
	effect.params.shimmer.g = getGreen(color);
	effect.params.shimmer.b = getBlue(color);


	use_single_color = true;
	effect.active = true;
	add();
}

void LedStrip::poll()
{
	if (!effect.active)
		return;

	effect.tick_count++;

	switch (effect.type)
	{
		case LedStripEffectShimmer:
			if (effect.params.shimmer.random)
			{
				if (effect.tick_count >= effect.ticks && !on_tx)
				{
					uint8_t r = 0, g = 0, b = 0;
					effect.tick_count = 0;
					effect.params.shimmer.value = getRandom(effect.params.shimmer.low, 100);
					r = (getRed(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;
					g = (getGreen(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;
					b = (getBlue(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;

					if (led_type == WS2812B)
						packWS2812B(r, g, b, single_color, NULL);

					updateFromEffect();
				}
			} else {
				effect.params.shimmer.update_count++;
				if (effect.tick_count == effect.ticks)
				{
					effect.tick_count = 0;

					uint8_t r = 0, g = 0, b = 0;

					if (effect.params.shimmer.up)
					{
						effect.params.shimmer.value += effect.params.shimmer.step;
						if (effect.params.shimmer.value >= 100)
						{
							effect.params.shimmer.value = 100;
							effect.params.shimmer.up = false;
							effect.params.shimmer.update_count = effect.params.shimmer.update_every;
						}
					} else {
						effect.params.shimmer.value -= effect.params.shimmer.step;
						if (effect.params.shimmer.value <= effect.params.shimmer.low)
						{
							effect.params.shimmer.value = effect.params.shimmer.low;
							effect.params.shimmer.up = true;
							effect.params.shimmer.update_count = effect.params.shimmer.update_every;
						}
					}

					if (effect.params.shimmer.update_count >= effect.params.shimmer.update_every && !on_tx)
					{
						effect.params.shimmer.update_count = 0;
						r = (getRed(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;
						g = (getGreen(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;
						b = (getBlue(effect.params.shimmer.color) * effect.params.shimmer.value) / 100;

						if (led_type == WS2812B)
							packWS2812B(r, g, b, single_color, NULL);

						updateFromEffect();
					}
				}
			}
			break;
	}
}

extern "C" void DMA2_Stream5_IRQHandler(void)
{
	DMA2->HIFCR = 0xF00;
	if (leds_to_update)
	{
		DMA2_Stream5->M0AR = (uint32_t) updating_buffer;
		DMA2_Stream5->CR = 0x6000451;
	}

	EXTI_GenerateSWInterrupt(EXTI_Line2);
}

extern "C" void EXTI2_IRQHandler(void)
{
	EXTI_ClearFlag(EXTI_Line2);
	led_strip_tx->swInterrupt();
}
