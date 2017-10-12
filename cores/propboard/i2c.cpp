/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### i2c.cpp

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

#include "i2c.h"
#include <stddef.h>

I2C i2c1(I2C1);

I2C::I2C(I2C_TypeDef* i2c_reg)
{
	i2c = i2c_reg;
	freq = 0;
	busy = in_transaction = 0;
	current_packet = NULL;
	timeout_counter = 0;
	initialized = false;
}

I2C::~I2C()
{

}

void I2C::begin(uint32_t speed)
{
	if (initialized)
		return;

	freq = speed;
	initialize();
}

void I2C::reset()
{
	if (!initialized)
		return;

	I2C_SoftwareResetCmd(i2c, DISABLE);
	delay(100);
	I2C_SoftwareResetCmd(i2c, ENABLE);

	initialized = false;
	begin(freq);
}

void I2C::initialize()
{
	GPIO_InitTypeDef GPIO_InitStruct;
	I2C_InitTypeDef I2C_InitStruct;

	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStruct.I2C_ClockSpeed = freq;
	I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStruct.I2C_OwnAddress1 = 0;
	I2C_InitStruct.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;

	switch ((uint32_t) i2c)
	{
		case I2C1_BASE:
			// Enable clock
			RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
			RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

			I2C_DeInit(I2C1);

			// Configure pins
			GPIOB->BSRRL = GPIO_Pin_8;
			GPIOB->BSRRL = GPIO_Pin_9;

			GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			// Start I2C1
			I2C_Init(I2C1, &I2C_InitStruct);

			GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_I2C1);
			GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

			GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
			GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			initialized = true;
			break;

		case I2C2_BASE:
			// Enable clock
			RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
			RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

			I2C_DeInit(I2C2);

			// Configure pins
			GPIOB->BSRRL = GPIO_Pin_3;
			GPIOB->BSRRL = GPIO_Pin_10;

			GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_10;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			// Start I2C2
			I2C_Init(I2C2, &I2C_InitStruct);

			GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_I2C2);
			GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_I2C2);

			GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
			GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			initialized = true;
			break;

		default:
			break;
	}
}

void I2C::end()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	// Set pins to floating
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;

	switch ((uint32_t) i2c)
	{
		case I2C1_BASE:
			I2C_DeInit(I2C1);

			// Disable clock
			RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;

			// Configure pins
			GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			initialized = false;
			break;

		case I2C2_BASE:
			I2C_DeInit(I2C2);

			// Disable clock
			RCC->APB1ENR &= ~RCC_APB1ENR_I2C2EN;

			// Configure pins
			GPIO_InitStruct.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_10;
			GPIO_Init(GPIOB, &GPIO_InitStruct);

			initialized = false;
			break;

		default:
			break;
	}
}

void I2C::setClock(uint32_t speed)
{
	begin(speed);
}

bool I2C::sendPackets(I2C_PACKET* packets, uint32_t count)
{
	uint32_t idx = 0;
	uint32_t data_offs;
	uint32_t data_left;
	I2C_PACKET* packet = &packets[idx++];
	bool check_busy = true;

	bool ret = false;

	if (!count || !packets || !initialized)
		return false;

	while (busy);
	busy = true;

	while (count)
	{
		if (check_busy)
		{
			ret = checkFlagWithTimeout(I2C_FLAG_BUSY, false, 1000);
			if (!ret)
			{
				packet->result = I2C_Busy;
				break;
			}
		}

		if (packet->op == I2C_Read)
		{
			if (packet->len == 1)
				I2C_AcknowledgeConfig(i2c, DISABLE);
			else
				I2C_AcknowledgeConfig(i2c, ENABLE);
		}

		if (packet->start)
		{
			I2C_GenerateSTART(i2c, ENABLE);
			ret = checkEventWithTimeout(I2C_EVENT_MASTER_MODE_SELECT, true, 1000);
			if (!ret)
			{
				packet->result = I2C_BusError;
				break;
			}

			I2C_Send7bitAddress(i2c, packet->addr, (uint8_t) packet->op);

			ret = false;

			if (packet->op == I2C_Read)
			{
				ret = checkEventWithTimeout(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, true, 1000);
			} else if (packet->op == I2C_Write)
			{
				ret = checkEventWithTimeout(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, true, 1000);
			}

			if (!ret)
			{
				packet->result = I2C_NoDevice;
				reset();
				break;
			}
		}

		if (packet->data && packet->len)
		{
			data_offs = 0;
			data_left = packet->len;
			if (packet->op == I2C_Read)
			{
				while (data_left)
				{
					if (data_left == 1)
					{
						// One byte left, disable ACK
						I2C_AcknowledgeConfig(i2c, DISABLE);

						// Enable STOP generation
						if (packet->stop)
						{
							I2C_GenerateSTOP(i2c, ENABLE);
							check_busy = true;
						} else {
							check_busy = false;
						}
					}

					// Wait for the byte
					ret = checkFlagWithTimeout(I2C_FLAG_RXNE, true, 1000);
					if (!ret)
					{
						packet->result = I2C_Timeout;
						break;
					}

					packet->data[data_offs++] = i2c->DR;

					data_left--;
				}

				if (data_left)
				{
					ret = false;
					break;
				}

			} else if (packet->op == I2C_Write)
			{
				while (data_left)
				{
					i2c->DR = packet->data[data_offs++];

					ret = checkEventWithTimeout(I2C_EVENT_MASTER_BYTE_TRANSMITTING, true, 1000);
					if (!ret)
					{
						packet->result = I2C_BusError;
						break;
					}

					data_left--;
				}

				if (data_left)
				{
					ret = false;
					break;
				}

				ret = checkEventWithTimeout(I2C_EVENT_MASTER_BYTE_TRANSMITTED, true, 1000);
				if (!ret)
				{
					packet->result = I2C_BusError;
					break;
				}

				if (packet->stop)
				{
					I2C_GenerateSTOP(i2c, ENABLE);
					check_busy = true;
				} else {
					check_busy = false;
				}
			}
		}

		packet = &packets[idx++];
		count--;
	}

	busy = false;
	return ret;
}

bool I2C::readRegister(uint8_t addr, uint32_t reg, uint8_t reg_len, uint8_t* data, uint32_t data_len)
{
	I2C_PACKET packet[2];

	if (!initialized)
		return false;

	packet[0].addr = addr;
	packet[0].data = (uint8_t*) &reg;
	packet[0].len = reg_len;
	packet[0].op = I2C_Write;
	packet[0].start = true;
	packet[0].stop = false;

	packet[1].addr = addr;
	packet[1].data = data;
	packet[1].len = data_len;
	packet[1].op = I2C_Read;
	packet[1].start = true;
	packet[1].stop = true;

	return sendPackets(packet, 2);
}

bool I2C::writeRegister(uint8_t addr, uint32_t reg, uint8_t reg_len, uint8_t* data, uint32_t data_len)
{
	I2C_PACKET packet[2];

	if (!initialized)
		return false;

	packet[0].addr = addr;
	packet[0].data = (uint8_t*) &reg;
	packet[0].len = reg_len;
	packet[0].op = I2C_Write;
	packet[0].start = true;
	packet[0].stop = false;

	packet[1].addr = addr;
	packet[1].data = data;
	packet[1].len = data_len;
	packet[1].op = I2C_Write;
	packet[1].start = false;
	packet[1].stop = true;

	return sendPackets(packet, 2);
}

bool I2C::checkEventWithTimeout(uint32_t event, bool value, uint32_t timeout)
{
	startTimeoutCounter();
	while (I2C_CheckEvent(i2c, event) != (ErrorStatus) value)
	{
		if (checkTimeout(timeout))
			return false;
	}

	return true;
}

bool I2C::checkFlagWithTimeout(uint32_t flag, bool value, uint32_t timeout)
{
	startTimeoutCounter();
	while (I2C_GetFlagStatus(i2c, flag) != (FlagStatus) value)
	{
		if (checkTimeout(timeout))
			return false;
	}

	return true;
}

void I2C::startTimeoutCounter()
{
	timeout_counter = GetTickCount();
}

bool I2C::checkTimeout(uint32_t ms)
{
	if (GetTickCount() - timeout_counter >= ms)
		return true;

	return false;
}
