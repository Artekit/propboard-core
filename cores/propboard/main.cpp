/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### main.cpp

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

#include "Arduino.h"

extern void setup();
extern void loop();

int main(void)
{
	init();

	srand(GetTickCount() + readBattery());

	setup();

	while (1)
		loop();

	return 0;
}

uint32_t getRandom(uint32_t min, uint32_t max)
{
	uint32_t range;
	uint32_t num;
	uint32_t reject;

	range =  max - min + 1;
	if (!range)
		return 0;

	reject = (RAND_MAX / range) * range;

	do {
		num = rand();
	} while (num >= reject);

	return num % range + min;
}

