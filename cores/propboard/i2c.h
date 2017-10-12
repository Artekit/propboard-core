/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### i2c.h

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

#ifndef __I2C_H__
#define __I2C_H__

#include "Arduino.h"

enum I2C_OP
{
	I2C_Write = 0,
	I2C_Read,
};

enum I2C_RESULT
{
	I2C_NoError = 0,
	I2C_Busy,
	I2C_BusError,
	I2C_Timeout,
	I2C_NoDevice,
};

typedef struct
{
    uint16_t addr;		// Device address
    I2C_OP op;			// Operation = I2C_OP::I2C_Read or I2C_OP::I2C_Write
    uint8_t* data;		// In/Out data buffer
    uint32_t len;		// Data buffer length
    I2C_RESULT result;	// Result of the operation
    bool stop;			// Send a STOP condition after this packet
    bool start;			// START + device addres procedure
} I2C_PACKET;

class I2C
{
	friend void I2C1_EV_IRQHandler();
	friend void I2C2_EV_IRQHandler();

public:
	I2C(I2C_TypeDef* i2c_reg);
	~I2C();

	void begin(uint32_t speed);
	void end();
	void reset();
	void setClock(uint32_t speed);
	bool sendPackets(I2C_PACKET* packets, uint32_t count);
	bool readRegister(uint8_t addr, uint32_t reg, uint8_t reg_len, uint8_t* data, uint32_t data_len);
	bool writeRegister(uint8_t addr, uint32_t reg, uint8_t reg_len, uint8_t* data, uint32_t data_len);
	inline bool isBusy() { return busy; }

private:
	I2C_TypeDef* i2c;
	uint32_t freq;
	bool initialized;
	volatile bool busy;
	volatile bool in_transaction;
	I2C_PACKET* current_packet;
	uint32_t timeout_counter;

	void initialize();
	void onInterrupt();
	void startTimeoutCounter();
	bool checkTimeout(uint32_t ms);
	bool checkEventWithTimeout(uint32_t event, bool value, uint32_t timeout);
	bool checkFlagWithTimeout(uint32_t flag, bool value, uint32_t timeout);
};

extern I2C i2c1;

#endif /* __I2C_H__ */
