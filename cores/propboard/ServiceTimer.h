/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### ServiceTimer.h

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

#ifndef __SERVICETIMER_H__
#define __SERVICETIMER_H__

#include <stm32f4xx.h>

#define SERVICE_TIMER_FREQ	1000

extern "C" void TIM1_TRG_COM_TIM11_IRQHandler(void);

class STObject
{
	friend void TIM1_TRG_COM_TIM11_IRQHandler(void);
public:
	STObject();
	virtual ~STObject();

protected:
	void add();
	void remove();
	virtual void poll();
	inline uint32_t getFrequency() { return SERVICE_TIMER_FREQ; }

private:
	STObject* next;
	STObject* prev;
};

void stDeinit();

#endif // __SERVICETIMER_H__
