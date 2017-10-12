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

#define LED_STRIP_BUFFERS		2
#define LED_STRIP_BUFFER_LEN	24

enum LedStripeType
{
	WS2812B = 1,
	APA102,
};

typedef enum
{
	LedStripEffectNone = 0,
	LedStripEffectShimmer,
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
			int16_t value;
			uint8_t step;
			uint32_t update_count;
			uint32_t update_every;
			uint8_t r, g, b;
			COLOR color;
			bool update;
			bool up;
			bool random;
		} shimmer;
	} params;
} LedStripEffect;

extern "C" void EXTI2_IRQHandler();

class LedStrip : public STObject
{
	friend void EXTI2_IRQHandler();
public:
	LedStrip();
	~LedStrip();

	bool begin(uint32_t count, LedStripeType type = WS2812B);
	void end();
	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
	void set(uint32_t index, COLOR color);
	bool update(uint32_t index = 0, bool async = false);
	bool updating();
	void endUpdate();
	void poll();
	void setMultiplier(float value);
	inline float getMultiplier() { return multiplier; }
	inline bool withEffect() { return effect.active; }
	inline void stopEffect();
	void shimmer(COLOR color, uint8_t amplitude, uint32_t hz, bool random);

private:

	void setWS2812B(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
	void setAPA102(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
	inline void Period800kHZ()__attribute__((optimize("-O3")));
	inline void packWS2812B(uint8_t r, uint8_t g, uint8_t b, uint8_t* buffer, uint32_t* index);
	bool updateInternal(uint32_t index = 0, bool async = false);
	bool updateFromEffect();

	uint32_t* packIntoBuffer(uint8_t* dst, uint32_t* bbaddr);
	void swInterrupt();

	bool initialized;
	bool use_single_color;
	uint8_t* buffer;
	uint8_t* next_tx;
	uint32_t* bitband_address;
	uint32_t led_count;
	float multiplier;
	uint8_t update_buffer1[72];
	uint8_t update_buffer2[72];
	uint8_t single_color[3];
	uint32_t* single_color_sbba;
	LedStripeType led_type;
	LedStripEffect effect;
};

#endif /* __LEDSTRIP_H__ */
