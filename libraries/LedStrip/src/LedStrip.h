/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### LedStrip.h

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

#ifndef __LEDSTRIP_H__
#define __LEDSTRIP_H__

#include <stm32f4xx.h>
#include <ServiceTimer.h>
#include <bitmap.h>
#include "LedStripDriver.h"

#define LED_STRIP_BUFFERS		2
#define LED_STRIP_BUFFER_LEN	24

typedef enum
{
	LedStripEffectNone = 0,
	LedStripEffectShimmer,
	LedStripEffectRamp,
} LedStripEffectType;

typedef struct _LedStripEffect
{
	LedStripEffectType type;
	volatile bool active;
	uint32_t ticks;
	uint32_t tick_count;
	union
	{
		struct
		{
			uint8_t low;
			int16_t depth;
			uint8_t step;
			uint32_t update_count;
			uint32_t update_every;
			COLOR color;
			bool update;
			bool up;
			bool random;
		} shimmer;

		struct
		{
			float r_step;
			float g_step;
			float b_step;
			float w_step;
			float r_val;
			float g_val;
			float b_val;
			float w_val;
			COLOR end;
			uint32_t cycles;
		} ramp;
	} params;
} LedStripEffect;

class LedStrip : public LedStripDriver, STObject
{

public:
	LedStrip();

	void shimmer(COLOR color, uint8_t amplitude, uint32_t hz, bool random);
	void ramp(COLOR start, COLOR end, uint32_t duration);
	void changeShimmerColor(COLOR color);
	void stopEffect();
	inline bool withEffect() { return effect.active; }

private:
	void poll();
	bool updateFromEffect();

	LedStripEffect effect;
};

#endif /* __LEDSTRIP_H__ */
