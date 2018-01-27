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
 * - Tailored to work over SPI1 on the STM32F401.
 *
 * - Takes over the SPI and configures it at 84MHz/4 = 21000000Hz. At this speed the SPI will
 *	 send a bit every ~47ns.
 *
 * - WS2812/WS2812B/SK6812RGBW
 * 		# For each bit to send, there is a 3-bytes sequence representing the "0 code" and "1 code"
 * 		# For the WS2812/WS2812B, the sequences are:
 * 			- Send 0xFF, 0xFF, 0xE0 for 1. This will raise the MOSI line for about 904ns (900ns +-125ns)
 *			- Send 0xFE, 0x00, 0x00 for 0. This will raise the MOSI line for about 329ns (350ns +-125ns)
 * 		# For the SK6812RGBW, the sequences are:
 * 			- Send 0xFF, 0xF8, 0x00 for 1. This will raise the MOSI line for about 611ns (600ns +-150ns)
 *			- Send 0xFE, 0x00, 0x00 for 0. This will raise the MOSI line for about 329ns (300ns +-150ns)
 * 		# Each 3-bytes sequence is transmitted every 1142.82nS (1.25uS +-150nS).
 * 		# Uses DMA2 Stream5 to send a 72 bytes array representing a single RGB or RGBW LED (24 3-bytes
 *		  sequences for 24-bits color).
 *
 * - There are two 72 bytes buffers: one for sending and one for filling with new data. It uses
 * 	 DMA2 Stream5 Transfer Complete interrupt to switch buffers and send. For the WS2812/WS2812B
 *	 and SK6812RGBW	it will queue an EXTI software interrupt at a lower priority to fill the buffer
 *	 that is not being transmitted.
 * - For APA102C there are no "code" buffers. We use the main buffer to store and send the required bits.
 *   The buffer is split in three parts: the first 4 bytes are the start frame, the it follows the pixel
 *   data, and the last 4 bytes are the end frame. It's probably that the APA102C string is mostly update
 *   entirely, so the buffer can be shot as is through SPI with DMA.
  */

#define LEDSTRIP_UPDATE_LIMITER	20

static uint8_t ws2812_code0[3] = { 0XFE, 0x00, 0x00 };
static uint8_t ws2812_code1[3] = { 0XFF, 0xFF, 0xE0 };
static uint8_t sk6812rgbw_code0[3] = { 0XFE, 0x00, 0x00 };
static uint8_t sk6812rgbw_code1[3] = { 0XFF, 0xF8, 0x00 };

static LedStrip* led_strip_update = NULL;

extern uint32_t getRandom(uint32_t min, uint32_t max);

LedStrip::LedStrip()
{
	initialized = false;
	buffer = NULL;
	bitband_address = NULL;
	led_type = WS2812B;
	use_single_color = false;
	multiplier = 1.0f;
	single_color_sbba = NULL;
	update_limit = led_count = 0;
	on_tx = false;
	leds_to_update = 0;
	updating_buffer = NULL;
	send_apa102_end_frame = false;
	code_buffer1 = NULL;
	code_buffer2 = NULL;
}

LedStrip::~LedStrip()
{
}

bool LedStrip::begin(uint32_t count, LedStripeType type)
{
	uint32_t buffer_size;
	uint32_t buffer_code_size;
		
	if (initialized)
		return true;

	if (buffer)
		free(buffer);
	
	switch (type)
	{
		case WS2812B:
			buffer_size = 3 * count;
			buffer_code_size = 72;
			break;

		case APA102:
			buffer_size = (4 * count) + 8;
			break;

		case SK6812RGBW:
			buffer_size = 4 * count;
			break;
	}

	buffer = (uint8_t*) malloc(buffer_size);
	if (!buffer)
		return false;

	bitband_address = (uint32_t*) (0x22000000 + ((uint32_t) buffer & 0x1FFFFFFF) * 32);

	update_limit = led_count = count;
	led_type = type;

	memset(buffer, 0, buffer_size);
	
	if (type == WS2812B || type == SK6812RGBW)
	{
		code_buffer1 = (uint8_t*) malloc(buffer_code_size * 2);
		if (code_buffer1 == NULL)
		{
			free(buffer);
			return false;
		}

		code_buffer2 = code_buffer1 + buffer_code_size;
		SPI.beginTxOnly(false);
	} else if (type == APA102)
	{
		// Set start and end frames
		memset(buffer, 0, 4);
		memset(buffer + buffer_size - 4 , 0xFF, 4);
		SPI.beginTxOnly(true);
	}

	initialized = true;

	return true;
}

void LedStrip::end()
{
	if (initialized)
	{
		SPI.end();

		if (code_buffer1)
		{
			free(code_buffer1);
			code_buffer1 = NULL;
		}

		if (buffer)
		{
			free(buffer);
			buffer = NULL;
			initialized = false;
		}
	}
}

void LedStrip::packWS2812(uint8_t r, uint8_t g, uint8_t b, uint8_t* ptr, uint32_t* index)
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

void LedStrip::packSK6812RGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t* ptr, uint32_t* index)
{
	uint32_t idx = 0;

	if (index == NULL)
		index = &idx;
	
	r = cie_lut[r] * multiplier;
	g = cie_lut[g] * multiplier;
	b = cie_lut[b] * multiplier;
	w = cie_lut[w] * multiplier;
	
	// Note: on the datasheet it says that red comes first,
	// but the in strip I have, red and green seem swapped.
	// TODO: verify
	ptr[(*index)++] = (__RBIT(g) >> 24);
	ptr[(*index)++] = (__RBIT(r) >> 24);
	ptr[(*index)++] = (__RBIT(b) >> 24);
	ptr[(*index)++] = (__RBIT(w) >> 24);
}

void LedStrip::packAPA102(uint8_t r, uint8_t g, uint8_t b, uint8_t* ptr, uint32_t* index)
{
	uint32_t idx = 0;

	if (index == NULL)
		index = &idx;

	r = cie_lut[r] * multiplier;
	g = cie_lut[g] * multiplier;
	b = cie_lut[b] * multiplier;

	ptr[(*index)++] = 0xFF;
	ptr[(*index)++] = b;
	ptr[(*index)++] = g;
	ptr[(*index)++] = r;
}

void LedStrip::setWS2812(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t count = 1;

	if (index == 0)
		count = led_count;
	else
		index = (index - 1) * 3;

	while (count)
	{
		packWS2812(r, g, b, buffer, &index);
		count--;
	}
}

void LedStrip::setAPA102(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t count = 1;
	uint8_t* ptr = buffer + 4;

	if (index == 0)
		count = led_count;
	else
		index = (index - 1) * 4;

	while (count)
	{
		packAPA102(r, g, b, ptr, &index);
		count--;
	}
}

void LedStrip::setSK6812RGBW(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
	uint32_t count = 1;

	if (index == 0)
		count = led_count;
	else
		index = (index - 1) * 4;

	while (count)
	{
		packSK6812RGBW(r, g, b, w, buffer, &index);
		count--;
	}
}

void LedStrip::set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
	if (!initialized || index > led_count)
		return;

	switch (led_type)
	{
		case WS2812B:
			setWS2812(index, r, g, b);
			break;
			
		case APA102:
			setAPA102(index, r, g, b);
			break;
			
		case SK6812RGBW:
			setSK6812RGBW(index, r, g, b, w);
			break;
	}
}

void LedStrip::set(uint32_t index, COLOR color)
{
	set(index, getRed(color), getGreen(color), getBlue(color), getWhite(color));
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

void LedStrip::endUpdate()
{
	// Wait for SPI to finish
	while (!(SPI1->SR & SPI_SR_TXE) || (SPI1->SR & SPI_SR_BSY));
	SPI.endTransaction();
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
	uint8_t bitcnt = 24;
	uint8_t* code0 = ws2812_code0;
	uint8_t* code1 = ws2812_code1;

	if (led_type == SK6812RGBW)
	{
		bitcnt = 32;
		code0 = sk6812rgbw_code0;
		code1 = sk6812rgbw_code1;
	}

	for (uint8_t i = 0; i < bitcnt; i++)
	{
		if (*bbaddr++)
		{
			*dst++ = code1[0];
			*dst++ = code1[1];
			*dst++ = code1[2];
		} else {
			*dst++ = code0[0];
			*dst++ = code0[1];
			*dst++ = code0[2];
		}
	}

	return bbaddr;
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

	use_single_color = true;
	effect.active = true;
	add();
}

void LedStrip::changeShimmerColor(COLOR color)
{
	if (effect.type == LedStripEffectShimmer && effect.active)
	{
		__disable_irq();
		effect.params.shimmer.color = color;
		__enable_irq();
	}
}

void LedStrip::packSingleColor(COLOR color, int16_t multiplier, uint8_t* dest)
{
	uint8_t r, g, b, w;

	r = (getRed(color) * multiplier) / 100;
	g = (getGreen(color) * multiplier) / 100;
	b = (getBlue(color) * multiplier) / 100;

	switch(led_type)
	{
		case WS2812B:
			packWS2812(r, g, b, dest, NULL);
			break;

		case SK6812RGBW:
			w = (getWhite(color) * multiplier) / 100;
			packSK6812RGBW(r, g, b, w, dest, NULL);
			break;

		case APA102:
			packAPA102(r, g, b, dest, NULL);
			break;
	}
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
					effect.tick_count = 0;
					effect.params.shimmer.value = getRandom(effect.params.shimmer.low, 100);
					packSingleColor(effect.params.shimmer.color, effect.params.shimmer.value, single_color);
					updateFromEffect();
				}
			} else {
				effect.params.shimmer.update_count++;
				if (effect.tick_count == effect.ticks)
				{
					effect.tick_count = 0;

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
						packSingleColor(effect.params.shimmer.color, effect.params.shimmer.value, single_color);
						updateFromEffect();
					}
				}
			}
			break;

		case LedStripEffectRamp:
			break;
		case LedStripEffectNone:
			break;
	}
}

bool LedStrip::updateInternal(uint32_t index, bool async)
{
	uint32_t count;

	if (!initialized)
		return false;

	if (led_strip_update || (async && on_tx))
		return false;

	if (index == 0 || index > led_count)
		count = led_count;
	else
		count = index;

	// Check if there is an update count limit
	if (update_limit < count)
		count = update_limit;

	if (!count)
		return false;

	// Wait for any ongoing transmission
	while (led_strip_update || on_tx);
	on_tx = true;
	led_strip_update = this;

	if (led_type == WS2812B || led_type == SK6812RGBW)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

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

		// Pack the first LED into the buffer 1
		bitband_address = packIntoBuffer(code_buffer1, bitband_address);

		// Pack the second LED into the buffer 2
		if (leds_to_update)
		{
			if (single_color_sbba)
				bitband_address = single_color_sbba;

			bitband_address = packIntoBuffer(code_buffer2, bitband_address);
			updating_buffer = code_buffer2;
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

		if (led_type == SK6812RGBW)
			DMA2_Stream5->NDTR = 96;
		else
			DMA2_Stream5->NDTR = 72;

		DMA2_Stream5->M0AR = (uint32_t) code_buffer1;
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

			// Wait for reset
			if (led_type == WS2812B)
				delayMicroseconds(50);
			else if (led_type == SK6812RGBW)
				delayMicroseconds(80);

			endUpdate();
		}
	} else if (led_type == APA102)
	{
		// Configure SPI, end any pending transaction
		SPI.endTransaction();
		SPI.beginTransaction(SPISettings(21000000, MSBFIRST, SPI_MODE0, true));

		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

		// Disable DMA2 Stream5
		DMA2_Stream5->CR = 0;

		// Configure DMA2 Stream5
		NVIC_ClearPendingIRQ(DMA2_Stream5_IRQn);
		NVIC_SetPriority(DMA2_Stream5_IRQn, VARIANT_PRIO_LEDSTRIP_APA102);
		NVIC_EnableIRQ(DMA2_Stream5_IRQn);

		DMA2->HIFCR = 0xF00;
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

		DMA2_Stream5->M0AR = (uint32_t) buffer;

		if (use_single_color)
		{
			// Using single color, so first send the start frame,
			// then send every LED from the DMA interrupt, that
			// will also send the end code
			DMA2_Stream5->NDTR = 4;
			send_apa102_end_frame = true;
			leds_to_update = count;
		} else {

			// If updating the entire strip, send all the contents of the buffer,
			// that includes the start and end frames
			if (count == led_count)
			{
				DMA2_Stream5->NDTR = count * 4 + 8;
				send_apa102_end_frame = false;
			} else {
				// Otherwise, send the buffer up to the required LED
				// and after that, send the end frame
				send_apa102_end_frame = true;
				DMA2_Stream5->NDTR = count * 4 + 4;
			}

			// We've updated all the LEDs
			leds_to_update = 0;
		}

		// Kickstart DMA
		DMA2_Stream5->CR |= DMA_SxCR_EN;

		// Enable SPI TX DMA requests
		SPI1->CR2 |= SPI_CR2_TXDMAEN;

		if (async)
		{
			// Wait for the transmission to finish
			while (on_tx);
			SPI.endTransaction();
		}
	}

	return true;
}

void LedStrip::swInterrupt()
{
	if (leds_to_update > 1)
	{
		if (updating_buffer == code_buffer1)
			updating_buffer = code_buffer2;
		else
			updating_buffer = code_buffer1;

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
		on_tx = false;
		led_strip_update = NULL;
	}
}

extern "C" void DMA2_Stream5_IRQHandler(void)
{
	DMA2->HIFCR = 0xF00;
	if (led_strip_update->led_type == APA102)
	{
		if (led_strip_update->leds_to_update)
		{
			// If using single color, send 4 bytes for every pixel.
			if (led_strip_update->use_single_color)
			{
				DMA2_Stream5->M0AR = (uint32_t) led_strip_update->single_color;
				DMA2_Stream5->CR = 0x6000451;
				led_strip_update->leds_to_update--;
			}
		} else {
			if (led_strip_update->send_apa102_end_frame)
			{
				// Send termination sequence
				led_strip_update->send_apa102_end_frame = false;
				DMA2_Stream5->NDTR = 4;
				DMA2_Stream5->M0AR = (uint32_t) led_strip_update->buffer + led_strip_update->led_count * 4 - 4;
				DMA2_Stream5->CR = 0x6000451;
			} else {
				// Disable DMA and irq
				DMA2_Stream5->CR = 0;
				NVIC_DisableIRQ(DMA2_Stream5_IRQn);
				led_strip_update->on_tx = false;
				led_strip_update = NULL;
			}
		}
	} else {

		if (led_strip_update->leds_to_update)
		{
			DMA2_Stream5->M0AR = (uint32_t) led_strip_update->updating_buffer;
			DMA2_Stream5->CR = 0x6000451;
		}

		EXTI_GenerateSWInterrupt(EXTI_Line2);
	}
}

extern "C" void EXTI2_IRQHandler(void)
{
	EXTI_ClearFlag(EXTI_Line2);
	led_strip_update->swInterrupt();
}
