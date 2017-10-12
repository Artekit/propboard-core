/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropPower.h

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

#ifndef __PROPPOWER_H__
#define __PROPPOWER_H__

#include <stm32f4xx.h>

#define power5V(enable) { digitalWrite(ONOFF_5V, enable ? 1 : 0); }
#define power3V3(enable) { digitalWrite(ONOFF_3V3, enable ? 1 : 0); }

void enterLowPowerMode(uint32_t pin, uint32_t mode, bool motion_enabled = false);
uint16_t readBattery();

#endif /* __PROPPOWER_H__ */
