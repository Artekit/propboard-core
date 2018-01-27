/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2018 Artekit Labs
 * https://www.artekit.eu

### AudioUtil.cpp

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

#include "AudioUtil.h"

void PCM16ToFloat(int16_t* src, float* dst, uint32_t samples)
{
	for (uint16_t i = 0; i < samples; i++)
	{
		*dst++ = (float) *src++ / (float) 0x8000;
	}
}

void floatToPCM16(float* src, int16_t* dst, uint32_t samples)
{
	for (uint16_t i = 0; i < samples; i++)
	{
		*dst++ = *src++ * 0x8000;
	}
}
