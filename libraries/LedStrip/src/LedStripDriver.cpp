/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2018 Artekit Labs
 * https://www.artekit.eu

### LedStripDriver.cpp

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

#include "LedStripDriver.h"
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

static LedStripDriver* volatile led_strip_update = NULL;

LedStripDriver::LedStripDriver()
{
	initialized = false;
	bitband_address = NULL;
	led_type = WS2812B;
	use_single_color = false;
	brightness = 1.0f;
	single_color_sbba = NULL;
	on_tx = false;
	leds_to_update = 0;
	updating_buffer = NULL;
	send_apa102_end_frame = false;
	code_buffer1 = NULL;
	code_buffer2 = NULL;
	led_data = NULL;
}

bool LedStripDriver::begin(uint32_t count, LedStripeType type)
{
	uint32_t buffer_code_size;

	if (initialized)
		return true;

	switch (type)
	{
		case WS2812B:
			buffer_code_size = 72;
			led_data = new LedStripDataWS2812();
			break;

		case APA102:
			led_data = new LedStripDataAPA102();
			break;

		case SK6812RGBW:
			buffer_code_size = 96;
			led_data = new LedStripDataSK6812RGBW();
			break;

		default:
			return false;
	}

	if (!led_data || !led_data->begin(count))
		return false;

	bitband_address = led_data->getBitBandPointer();
	led_type = type;

	if (type == WS2812B || type == SK6812RGBW)
	{
		code_buffer1 = (uint8_t*) malloc(buffer_code_size * 2);
		if (code_buffer1 == NULL)
		{
			led_data->deallocate();
			delete led_data;
			led_data = NULL;
			return false;
		}

		code_buffer2 = code_buffer1 + buffer_code_size;
		SPI.beginTxOnly(false);
	} else if (type == APA102)
	{
		SPI.beginTxOnly(true);
	}

	initialized = true;
	return true;
}

void LedStripDriver::end()
{
	if (initialized)
	{
		while (busy());

		SPI.end();

		if (code_buffer1)
		{
			free(code_buffer1);
			code_buffer1 = NULL;
		}

		if (led_data)
		{
			led_data->deallocate();
			delete led_data;
			led_data = NULL;
		}

		initialized = false;
	}
}

void LedStripDriver::set(uint32_t index, COLOR color)
{
	set(index, getRed(color), getGreen(color), getBlue(color), getWhite(color));
}

void LedStripDriver::set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
	if (!initialized)
		return;

	r = cie_lut[r] * brightness;
	g = cie_lut[g] * brightness;
	b = cie_lut[b] * brightness;
	w = cie_lut[w] * brightness;

	led_data->set(index, r, g, b, w);
}

void LedStripDriver::setRange(uint32_t start, uint32_t end, COLOR color)
{
	setRange(start, end, getRed(color), getGreen(color), getBlue(color), getWhite(color));
}

void LedStripDriver::setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
	if (!initialized)
		return;

	r = cie_lut[r] * brightness;
	g = cie_lut[g] * brightness;
	b = cie_lut[b] * brightness;
	w = cie_lut[w] * brightness;

	led_data->setRange(start, end, r, g, b, 0);
}

bool LedStripDriver::update(uint32_t index, bool async)
{
	if (!initialized)
		return false;

	use_single_color = false;
	return updateInternal(index, NULL, async);
}

void LedStripDriver::endUpdate()
{
	if (!initialized)
		return;

	// Wait for SPI to finish
	while (!(SPI1->SR & SPI_SR_TXE) || (SPI1->SR & SPI_SR_BSY));
	SPI.endTransaction();
}

void LedStripDriver::setBrightness(float value)
{
	if (value > 1)
		value = 1;
	else if (value < 0)
		value = 0;

	brightness = value;
}

uint32_t* LedStripDriver::packIntoBuffer(uint8_t* dst, uint32_t* bbaddr)
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

void LedStripDriver::packSingleColor(COLOR color, int16_t depth, uint8_t* dest)
{
	uint8_t r, g, b, w;

	r = (getRed(color) * depth) / 100;
	g = (getGreen(color) * depth) / 100;
	b = (getBlue(color) * depth) / 100;
	w = (getWhite(color) * depth) / 100;

	led_data->pack(dest, r, g, b, w);
}

bool LedStripDriver::busy()
{
	if (on_tx)
		return true;

	if (!(SPI1->SR & SPI_SR_TXE))
		return true;

	if (SPI1->SR & SPI_SR_BSY)
		return true;

	if (led_strip_update)
		return true;

	return false;
}

bool LedStripDriver::updateInternal(uint32_t index, LedStripData* data, bool async)
{
	uint32_t count;

	if (!initialized)
		return false;

	if (data == NULL)
		data = led_data;

	if (index == 0 || index > data->getLedCount())
		count = data->getLedCount();
	else
		count = index;

	if (!count)
		return false;

	if (async)
	{
		if (busy())
			return false;
	} else {
		// Wait for any ongoing transmission
		while (busy());
	}

	on_tx = true;
	led_strip_update = this;
	updating_data = data;

	if (led_type == WS2812B || led_type == SK6812RGBW)
	{
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

		if (use_single_color)
		{
			// Remember the fixed color variable bit-band start address
			single_color_sbba = (uint32_t*) (0x22000000 + ((uint32_t) single_color & 0x1FFFFFFF) * 32);

			// Set bit-band pointer to the beginning of the variable containing
			// the fixed color to send
			bitband_address = single_color_sbba;
		} else {
			// Set bitband pointer to the beginning of the buffer
			bitband_address = data->getBitBandPointer();
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
			while (busy());

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

		DMA2_Stream5->M0AR = updating_data->getBufferAddress();

		if (use_single_color)
		{
			// Using single color, so first send the start frame,
			// then send every LED from the DMA interrupt, that
			// will also send the end code
			DMA2_Stream5->NDTR = 4;
			send_apa102_end_frame = true;
			updating_leds = leds_to_update = count;
		} else {

			// If updating the entire strip, send all the contents of the buffer,
			// that includes the start and end frames
			if (count == updating_data->getLedCount())
			{
				DMA2_Stream5->NDTR = updating_data->getBufferSize();
				send_apa102_end_frame = false;
			} else {
				// Otherwise, send the buffer up to the required LED
				// and after that, send the end frame
				send_apa102_end_frame = true;
				DMA2_Stream5->NDTR = count * 4 + 4;
			}

			updating_leds = count;

			// We've updated all the LEDs
			leds_to_update = 0;
		}

		// Kickstart DMA
		DMA2_Stream5->CR |= DMA_SxCR_EN;

		// Enable SPI TX DMA requests
		SPI1->CR2 |= SPI_CR2_TXDMAEN;

		if (!async)
		{
			// Wait for the transmission to finish
			while (busy());
			endUpdate();
		}
	}

	return true;
}

void LedStripDriver::swInterrupt()
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
				uint32_t size = (led_strip_update->updating_leds / 2) / 8;
				if (size == 0) size = 4;
				led_strip_update->send_apa102_end_frame = false;
				DMA2_Stream5->NDTR = size;
				// Retrieve the address of the end frame
				DMA2_Stream5->M0AR = 4 + led_strip_update->updating_data->getBufferAddress() +
									 4 * led_strip_update->updating_data->getLedCount();
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
