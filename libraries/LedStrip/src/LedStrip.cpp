/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### LedStrip.cpp

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

#include "LedStrip.h"
#include <SPI.h>

extern uint32_t getRandom(uint32_t min, uint32_t max);

LedStrip::LedStrip()
{
}

bool LedStrip::updateFromEffect()
{
	return updateInternal(0, NULL, true);
}

void LedStrip::ramp(COLOR start, COLOR end, uint32_t duration)
{
	if (!duration || start == end)
		return;

	if (effect.active)
		stopEffect();

	effect.type = LedStripEffectRamp;
	effect.tick_count = 0;
	effect.ticks = 20; // update every 20ms
	effect.params.ramp.end = end.toInt();
	effect.params.ramp.cycles = duration / effect.ticks;
	if (effect.params.ramp.cycles == 0)
		effect.params.ramp.cycles = 1;

	float val;
	val = (((float) getRed(start) - (float) getRed(end)) / effect.params.ramp.cycles) * -1.0f;
	effect.params.ramp.r_step = val;

	val = (((float) getGreen(start) - (float) getGreen(end)) / effect.params.ramp.cycles) * -1.0f;
	effect.params.ramp.g_step = val;

	val = (((float) getBlue(start) - (float) getBlue(end)) / effect.params.ramp.cycles) * -1.0f;
	effect.params.ramp.b_step = val;

	val = (((float) getWhite(start) - (float) getWhite(end)) / effect.params.ramp.cycles) * -1.0f;
	effect.params.ramp.w_step = val;

	effect.params.ramp.r_val = getRed(start);
	effect.params.ramp.g_val = getGreen(start);
	effect.params.ramp.b_val = getBlue(start);
	effect.params.ramp.w_val = getWhite(start);

	use_single_color = true;
	effect.active = true;
	add();
}

void LedStrip::shimmer(COLOR color, uint8_t amplitude, uint32_t hz, bool random)
{
	uint32_t cycle, half_cycle;

	if (!hz || !amplitude)
		return;

	if (amplitude > 100)
		amplitude = 100;

	if (effect.active)
		stopEffect();

	effect.type = LedStripEffectShimmer;
	effect.tick_count = 0;
	effect.ticks = 1;

	cycle = getFrequency() / hz;
	half_cycle = cycle / 2;

	effect.params.shimmer.depth = 100;
	effect.params.shimmer.low = 100 - amplitude;
	effect.params.shimmer.random = random;

	if (random)
	{
		effect.ticks = cycle;
	} else {
		effect.params.shimmer.step = amplitude / half_cycle;
		if (effect.params.shimmer.step == 0)
		{
			effect.ticks = half_cycle / amplitude;
			effect.params.shimmer.step = 1;
		}

		effect.params.shimmer.update_count = 0;
		effect.params.shimmer.update_every = 10;
	}

	effect.params.shimmer.color = color.toInt();

	use_single_color = true;
	effect.active = true;
	add();
}

void LedStrip::poll()
{
	if (!effect.active)
		return;

	effect.tick_count++;

	switch (effect.type)
	{
		case LedStripEffectShimmer:
			if (effect.params.shimmer.random)
			{
				if (effect.tick_count >= effect.params.shimmer.update_every && !busy())
				{
					if (effect.tick_count >= effect.ticks)
					{
						effect.tick_count = 0;
						effect.params.shimmer.depth = getRandom(effect.params.shimmer.low, 100);
					}

					COLOR color(effect.params.shimmer.color);
					packSingleColor(color, effect.params.shimmer.depth, single_color);
					updateFromEffect();
				}
			} else {
				effect.params.shimmer.update_count++;
				if (effect.tick_count == effect.ticks)
				{
					effect.tick_count = 0;

					if (effect.params.shimmer.up)
					{
						effect.params.shimmer.depth += effect.params.shimmer.step;
						if (effect.params.shimmer.depth >= 100)
						{
							effect.params.shimmer.depth = 100;
							effect.params.shimmer.up = false;
							effect.params.shimmer.update_count = effect.params.shimmer.update_every;
						}
					} else {
						effect.params.shimmer.depth -= effect.params.shimmer.step;
						if (effect.params.shimmer.depth <= effect.params.shimmer.low)
						{
							effect.params.shimmer.depth = effect.params.shimmer.low;
							effect.params.shimmer.up = true;
							effect.params.shimmer.update_count = effect.params.shimmer.update_every;
						}
					}

					if (effect.params.shimmer.update_count >= effect.params.shimmer.update_every && !busy())
					{
						effect.params.shimmer.update_count = 0;
						COLOR color(effect.params.shimmer.color);
						packSingleColor(color, effect.params.shimmer.depth, single_color);
						updateFromEffect();
					}
				}
			}
			break;

		case LedStripEffectRamp:
			if (effect.tick_count == effect.ticks)
			{
				effect.tick_count = 0;
				effect.params.ramp.r_val += effect.params.ramp.r_step;
				effect.params.ramp.g_val += effect.params.ramp.g_step;
				effect.params.ramp.b_val += effect.params.ramp.b_step;
				effect.params.ramp.w_val += effect.params.ramp.w_step;

				if (effect.params.ramp.cycles)
					effect.params.ramp.cycles--;

				if (!busy())
				{
					COLOR color;

					if (effect.params.ramp.cycles > 1)
					{
						color = COLOR((uint8_t) effect.params.ramp.r_val,
						             (uint8_t) effect.params.ramp.g_val,
									 (uint8_t) effect.params.ramp.b_val,
									 (uint8_t) effect.params.ramp.w_val);
					} else if (effect.params.ramp.cycles == 1)
					{
						color = effect.params.ramp.end;
					} else {
						effect.active = false;
						remove();
						return;
					}

					packSingleColor(color, 100, single_color);
					updateFromEffect();
				}
			}
			break;

		case LedStripEffectNone:
			break;
	}
}

void LedStrip::stopEffect()
{
	if (effect.active)
	{
		remove();
		effect.active = false;
	}
}
