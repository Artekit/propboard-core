/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2018 Artekit Labs
 * https://www.artekit.eu

### bitmap.cpp

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
#include <Arduino.h>
#include "bitmap.h"

COLOR alphaBlend(COLOR top, COLOR bottom, float alpha_top)
{
	uint32_t r, g, b, w;

	r = (uint32_t) (getRed(top) * alpha_top) + (uint32_t) (getRed(bottom) * (1 - alpha_top));
	g = (uint32_t) (getGreen(top) * alpha_top) + (uint32_t) (getGreen(bottom) * (1 - alpha_top));
	b = (uint32_t) (getBlue(top) * alpha_top) + (uint32_t) (getBlue(bottom) * (1 - alpha_top));
	w = (uint32_t) (getWhite(top) * alpha_top) + (uint32_t) (getWhite(bottom) * (1 - alpha_top));

	return RGBW(r,g,b,w);
}
