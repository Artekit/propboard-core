/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### SPI.h

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

#ifndef __SPI_H__
#define __SPI_H__

#include <Arduino.h>

#define SPI_CLOCK_DIV2		0x00
#define SPI_CLOCK_DIV4		0x01
#define SPI_CLOCK_DIV8		0x02
#define SPI_CLOCK_DIV16		0x03
#define SPI_CLOCK_DIV32		0x04
#define SPI_CLOCK_DIV64		0x05
#define SPI_CLOCK_DIV128	0x06
#define SPI_CLOCK_DIV256	0x07

#define SPI_MODE0 0x00
#define SPI_MODE1 0x01
#define SPI_MODE2 0x02
#define SPI_MODE3 0x03

typedef enum _spiInitMode
{
	SPI_NORMAL,
	SPI_MOSI_ONLY,
	SPI_MOSI_CLK_ONLY
} spiInitMode;

typedef void(SPICallback)();

class SPISettings
{
public:
	SPISettings(uint32_t clock = 4000000, uint8_t bitOrder = MSBFIRST, uint8_t dataMode = SPI_MODE0, bool tx_only = false)
	{
		uint8_t clock_div = 0;

		// Assume APB2 == SystemCoreClock == 84MHz
		while (SystemCoreClock / (1 << (clock_div+1)) > clock && clock_div <= SPI_CLOCK_DIV256)
			clock_div++;

		// Baud rate
		config = clock_div << 3;

		// Bit order
		if (bitOrder == LSBFIRST)
			config |= SPI_CR1_LSBFIRST;

		// Data mode
		config |= dataMode & 0x03;
		config |= (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI);

		if (tx_only)
		{
			config |= SPI_CR1_BIDIMODE;
			config |= SPI_CR1_BIDIOE;
		}
	}

	uint32_t config;
	friend class SPIClass;
};

class SPIClass
{
public:
	SPIClass();
	~SPIClass();

	void begin();
	void usingInterrupt(uint8_t interruptNumber);
	void notUsingInterrupt(uint8_t interruptNumber);
	void beginTransaction(SPISettings settings);
	uint8_t transfer(uint8_t data);
	uint16_t transfer16(uint16_t data);
	void transfer(void *buf, size_t count);
	void endTransaction(void);
	void end();
	void setBitOrder(uint8_t bitOrder);
	void setDataMode(uint8_t dataMode);
	void setClockDivider(uint8_t clockDiv);
	void setCallback(SPICallback* function);

	// Special functions for the LedStrip class
	void beginTxOnly(bool with_clock);
	inline bool isInitialized() { return initialized; }
private:
	void begin(spiInitMode mode);
	void maskInterrupts();
	void unmaskInterrupts();

	SPICallback* callback;
	bool initialized;
	uint32_t irq_mask;

	spiInitMode init_mode;
};

extern SPIClass SPI;

#endif // __SPI_H__
