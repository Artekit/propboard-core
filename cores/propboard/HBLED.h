/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### HBLED.h

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

#ifndef __HBLED_H__
#define __HBLED_H__

#include <stm32f4xx.h>
#include "ServiceTimer.h"
#include <variant.h>
#include <ff.h>
#include "bitmap.h"

typedef enum
{
	LedEffectNone = 0,
	LedEffectShimmer,
	LedEffectFlash,
	LedEffectRamp,
} LedEffectType;

typedef struct _hbled_effect
{
	LedEffectType type;
	volatile bool active;
	uint32_t ticks;
	uint32_t tick_count;
	union
	{
		struct
		{
			float step;
			uint8_t maximum;
			uint8_t minimum;
			bool up; 
			bool random;
			float value;
		} shimmer;
		
		struct
		{
			uint32_t duration;
			bool infinite;
			bool on;
			uint8_t on_value;
			uint8_t off_value;
		} flash;
		
		struct
		{
			float step;
			uint32_t interval;
			uint32_t counter;
			float value;
			uint8_t end;
			uint32_t duration;
			bool up;
		} ramp;

	} params;
} HBLED_EFFECT;

class HBLED : public STObject
{
public:
		
	HBLED();
	HBLED(uint8_t ch);
	~HBLED();

	bool begin(uint16_t current_mA);
	bool begin(uint8_t ch, uint16_t current_mA);
	void end();
	void setCurrent(uint16_t current_mA);
	void setValue(uint8_t value);
	uint16_t getCurrent();
	uint8_t getValue();
	void setPercent(uint8_t percent);
	void setFrequency(uint32_t value);
	float setMultiplier(float multp);
	inline float getMultiplier() { return multiplier; }
	
	void ramp(uint8_t start, uint8_t end, uint32_t duration);
	void shimmer(uint8_t lower, uint8_t upper, uint8_t hz, uint8_t random);
	void flash(uint8_t on, uint8_t off, uint8_t hz, uint32_t duration);
	inline bool withEffect() { return led_effect.active; }
	inline void stopEffect() { remove(); led_effect.active = false; }

private:
	
	void setPWM(uint32_t value);
	void setWithLUT(uint8_t value);
	void poll();

	volatile uint32_t* volatile cc_reg;
	HBLED_EFFECT led_effect;
	uint32_t max_current;
	uint8_t channel;

	bool initialized;
	uint8_t last_value;
	float multiplier;

	uint32_t pwm_value;
	uint8_t pwm_percent;

#ifdef HBLED_BETA_SOFT_START
	bool first_time;
#endif
};

void deinitHBLEDHardware();
void setPwmFrequency(uint32_t value);
void doSoftStart();

#endif /* __HBLED_H__ */
