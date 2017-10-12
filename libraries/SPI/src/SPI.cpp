/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### SPI.cpp

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

#include "SPI.h"

SPIClass SPI;

SPIClass::SPIClass()
{
	callback = NULL;
	initialized_tx_only = initialized = 0;
	irq_mask = 0;
}

SPIClass::~SPIClass()
{
}

void SPIClass::begin(bool tx_only)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	SPI_InitTypeDef SPI_InitStruct;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
	SPI_Cmd(SPI1, DISABLE);

	// MOSI = PB5
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI1);

	if (!tx_only)
	{
		// CLK = PA5
		GPIO_Init(GPIOA, &GPIO_InitStruct);

		// MISO = PA6
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
		GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
	}

	SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;
	SPI_InitStruct.SPI_CPHA = SPI_CPHA_2Edge;
	SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStruct.SPI_Direction = tx_only ? SPI_Direction_1Line_Tx : SPI_Direction_2Lines_FullDuplex;
	SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
	SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
	SPI_Init(SPI1, &SPI_InitStruct);

	SPI_Cmd(SPI1, ENABLE);
}

void SPIClass::begin()
{
	if (!initialized)
		begin(false);

	initialized++;
}

void SPIClass::beginTxOnly()
{
	if (!initialized_tx_only && !initialized)
		begin(true);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
	initialized_tx_only++;
}

void SPIClass::end()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	if (initialized > 1)
	{
		--initialized;
		return;
	}

	// CLK = PA5
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	// MISO = PA6
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	if (initialized_tx_only)
	{
		// Leave SPI activated with MOSI pin only
	} else {
		// Deactivate SPI and MOSI
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
		GPIO_Init(GPIOB, &GPIO_InitStruct);

		SPI_Cmd(SPI1, DISABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
	}

	initialized = 0;
}

void SPIClass::endTxOnly()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	if (initialized_tx_only > 1)
	{
		initialized_tx_only--;
		return;
	}

	if (!initialized)
	{
		// MOSI = PB5
		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_Init(GPIOB, &GPIO_InitStruct);

		SPI_Cmd(SPI1, DISABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
	}

	initialized_tx_only = 0;
}

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
	if (interruptNumber > PINS_COUNT)
		return;

	if (g_APinDescription[interruptNumber].irq_num == NIRQ)
		return;

	irq_mask |= (1 << g_APinDescription[interruptNumber].irq_num);

}

void SPIClass::notUsingInterrupt(uint8_t interruptNumber)
{
	if (interruptNumber > PINS_COUNT)
		return;

	if (g_APinDescription[interruptNumber].irq_num == NIRQ)
		return;

	irq_mask &= ~(1 << g_APinDescription[interruptNumber].irq_num);
}

void SPIClass::maskInterrupts()
{
	for (uint8_t i = 0; i <= NUM_MAX_EXT_IRQ; i++)
	{
		if ((irq_mask >> i) & 0x01)
			maskInterrupt(i);
	}
}

void SPIClass::unmaskInterrupts()
{
	for (uint8_t i = 0; i <= NUM_MAX_EXT_IRQ; i++)
	{
		if ((irq_mask >> i) & 0x01)
			unmaskInterrupt(i);
	}
}

void SPIClass::beginTransaction(SPISettings settings)
{
	if (irq_mask)
		maskInterrupts();

	SPI1->CR1 &= ~SPI_CR1_SPE;
	SPI1->CR1 = settings.config;
	SPI1->CR1 |= SPI_CR1_SPE;
}

uint8_t SPIClass::transfer(uint8_t data)
{
	return transfer16(data);
}

uint16_t SPIClass::transfer16(uint16_t data)
{
	if (!initialized)
		return 0;

	SPI1->DR = data;
	while (!(SPI1->SR & SPI_SR_RXNE));
	return SPI1->DR;
}

void SPIClass::transfer(void *buf, size_t count)
{
	uint8_t* ptr = (uint8_t*) buf;

	while (count)
	{
		transfer(*ptr++);
		count--;
	}
}

void SPIClass::endTransaction(void)
{
	SPI1->CR1 &= ~SPI_CR1_SPE;

	if (irq_mask)
		unmaskInterrupts();
}

void SPIClass::setBitOrder(uint8_t bitOrder)
{
	if (!initialized)
		return;

	bool enabled = (SPI1->CR1 & SPI_CR1_SPE);
	SPI1->CR1 &= SPI_CR1_SPE;

	if (bitOrder == LSBFIRST)
		SPI1->CR1 |= SPI_CR1_LSBFIRST;
	else
		SPI1->CR1 &= ~SPI_CR1_LSBFIRST;

	if (enabled)
		SPI1->CR1 |= SPI_CR1_SPE;
}

void SPIClass::setDataMode(uint8_t dataMode)
{
	if (!initialized)
		return;

	// Data mode
	bool enabled = (SPI1->CR1 & SPI_CR1_SPE);

	SPI1->CR1 &= ~(0x03 | SPI_CR1_SPE);
	SPI1->CR1 |= dataMode & 0x03;

	if (enabled)
		SPI1->CR1 |= SPI_CR1_SPE;
}

void SPIClass::setClockDivider(uint8_t clockDiv)
{
	if (!initialized)
		return;

	bool enabled = (SPI1->CR1 & SPI_CR1_SPE);

	SPI1->CR1 &= ~(0x38 | SPI_CR1_SPE);
	SPI1->CR1 |= clockDiv << 3;

	if (enabled)
		SPI1->CR1 |= SPI_CR1_SPE;
}

void SPIClass::setCallback(SPICallback* function)
{
	if (!initialized)
		return;
}
