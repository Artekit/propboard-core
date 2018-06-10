/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2018 Artekit Labs
 * https://www.artekit.eu

### bitmap.h

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

#ifndef __BITMAP_H__
#define __BITMAP_H__

extern uint32_t getRandom(uint32_t min, uint32_t max);

class COLOR
{
public:
	COLOR() :
		r(0), g(0), b(0), w(0)
	{
	}

	COLOR(uint32_t color) :
		r(color & 0xFF),
		g((color >> 8) & 0xFF),
		b((color >> 16) & 0xFF),
		w((color >> 24) & 0xFF)
	{
	}

	COLOR(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) :
		r(r), g(g), b(b), w(w)
	{
	}

	void operator= (const uint32_t color)
	{
		r = (color & 0xFF);
		g = ((color >> 8) & 0xFF);
		b = ((color >> 16) & 0xFF);
		w = ((color >> 24) & 0xFF);
	}

	COLOR operator* (float val)
	{
		if (val > 1)
			val = 1;
		else if (val < 0)
			val = 0;

		COLOR color = COLOR(r * val, g * val, b * val, w * val);
		return color;
	}

	inline void operator*= (float value)
	{
		*this = *this * value;
	}

	inline bool operator== (const COLOR& color)
	{
		return (color.r == r && color.g == g && color.b == b && color.w == w);
	}

	inline bool operator!= (const COLOR& color)
	{
		if (color.r != r || color.g != g || color.b != b || color.w != w)
			return true;
		return false;
	}

	inline bool operator!= (const uint32_t color)
	{
		COLOR match(color);

		if (match.r != r || match.g != g || match.b != b || match.w != w)
			return true;
		return false;
	}

	inline bool operator! ()
	{
		return !notBlack();
	}

	inline bool notBlack() const
	{
		return (r || g || b || w);
	}

	inline uint32_t toInt() const
	{
		return (r | ((uint32_t) g << 8) | ((uint32_t) b << 16) | ((uint32_t) w << 24));
	}

	COLOR blend(COLOR& top, COLOR& bottom, float alpha_top)
	{
		uint32_t r, g, b, w;

		if (alpha_top > 1)
			alpha_top = 1;
		else if (alpha_top < 0)
			alpha_top = 0;

		r = (uint32_t) (top.r * alpha_top) + (uint32_t) (bottom.r * (1 - alpha_top));
		g = (uint32_t) (top.g * alpha_top) + (uint32_t) (bottom.g * (1 - alpha_top));
		b = (uint32_t) (top.b * alpha_top) + (uint32_t) (bottom.b * (1 - alpha_top));
		w = (uint32_t) (top.w * alpha_top) + (uint32_t) (bottom.w * (1 - alpha_top));

		return COLOR(r,g,b,w);
	}

	void blend(const COLOR& other, float amount)
	{
		if (amount > 1)
			amount = 1;
		else if (amount < 0)
			amount = 0;

		r = (uint32_t) (other.r * amount) + (uint32_t) (r * (1 - amount));
		g = (uint32_t) (other.g * amount) + (uint32_t) (g * (1 - amount));
		b = (uint32_t) (other.b * amount) + (uint32_t) (b * (1 - amount));
		w = (uint32_t) (other.w * amount) + (uint32_t) (w * (1 - amount));
	}

	void blend(const COLOR& other, uint8_t amount)
	{
		if (amount > 100)
			amount = 100;

		float val = amount / 100;

		blend(other, val);
	}

	uint8_t r, g, b, w;
};

COLOR randomColor();

/* CIE table generated using Jared Sanson's cie1931.py
 * from http://jared.geek.nz/2013/feb/linear-led-pwm
 */
static const uint8_t cie_lut[256] = {
	0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 3, 3, 3, 3, 3, 3, 3,
	3, 4, 4, 4, 4, 4, 4, 5, 5, 5,
	5, 5, 6, 6, 6, 6, 6, 7, 7, 7,
	7, 8, 8, 8, 8, 9, 9, 9, 10, 10,
	10, 10, 11, 11, 11, 12, 12, 12, 13, 13,
	13, 14, 14, 15, 15, 15, 16, 16, 17, 17,
	17, 18, 18, 19, 19, 20, 20, 21, 21, 22,
	22, 23, 23, 24, 24, 25, 25, 26, 26, 27,
	28, 28, 29, 29, 30, 31, 31, 32, 32, 33,
	34, 34, 35, 36, 37, 37, 38, 39, 39, 40,
	41, 42, 43, 43, 44, 45, 46, 47, 47, 48,
	49, 50, 51, 52, 53, 54, 54, 55, 56, 57,
	58, 59, 60, 61, 62, 63, 64, 65, 66, 67,
	68, 70, 71, 72, 73, 74, 75, 76, 77, 79,
	80, 81, 82, 83, 85, 86, 87, 88, 90, 91,
	92, 94, 95, 96, 98, 99, 100, 102, 103, 105,
	106, 108, 109, 110, 112, 113, 115, 116, 118, 120,
	121, 123, 124, 126, 128, 129, 131, 132, 134, 136,
	138, 139, 141, 143, 145, 146, 148, 150, 152, 154,
	155, 157, 159, 161, 163, 165, 167, 169, 171, 173,
	175, 177, 179, 181, 183, 185, 187, 189, 191, 193,
	196, 198, 200, 202, 204, 207, 209, 211, 214, 216,
	218, 220, 223, 225, 228, 230, 232, 235, 237, 240,
	242, 245, 247, 250, 252, 255,
};

#define RGB(r,g,b) 		(COLOR(r,g,b))
#define RGBW(r,g,b,w)	(COLOR(r,g,b,w))
#define getRed(color)  	(color.r)
#define getGreen(color)	(color.g)
#define getBlue(color)	(color.b)
#define getWhite(color)	(color.w)

#define MAX_RGB_LINE_WIDTH	1920

COLOR alphaBlend(COLOR top, COLOR bottom, float alpha_top);

#endif /* __BITMAP_H__ */
