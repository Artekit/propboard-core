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
	SK6812RGBW
};

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
			int16_t value;
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
			float r_rate;
			float g_rate;
			float b_rate;
			float w_rate;
			uint32_t cycles;
		};
	} params;
} LedStripEffect;

extern "C" void EXTI2_IRQHandler();
extern "C" void DMA2_Stream5_IRQHandler(void);

class LedStrip : public STObject
{
	friend void EXTI2_IRQHandler();
	friend void DMA2_Stream5_IRQHandler(void);

public:
	LedStrip();
	~LedStrip();

	bool begin(uint32_t count, LedStripeType type = WS2812B);
	void end();
	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
	void set(uint32_t index, COLOR color);
	bool update(uint32_t index = 0, bool async = false);
	inline bool updating() { return on_tx; }
	void endUpdate();
	void setMultiplier(float value);
	inline float getMultiplier() { return multiplier; }
	inline bool withEffect() { return effect.active; }
	inline void stopEffect();
	void shimmer(COLOR color, uint8_t amplitude, uint32_t hz, bool random);
	void changeShimmerColor(COLOR color);
	void ramp(COLOR start, COLOR end, uint32_t duration);
	inline void setUpdateLimit(uint32_t limit) { update_limit = limit; }
	inline uint32_t getUpdateLimit() { return update_limit; }
	inline LedStripeType getType() { return led_type; }
	inline uint32_t getLedCount() { return led_count; }

private:

	void setWS2812(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
	void setSK6812RGBW(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
	void setAPA102(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

	inline void packWS2812(uint8_t r, uint8_t g, uint8_t b, uint8_t* ptr, uint32_t* index);
	inline void packAPA102(uint8_t r, uint8_t g, uint8_t b, uint8_t* ptr, uint32_t* index);
	inline void packSK6812RGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, uint8_t* ptr, uint32_t* index);
	void packSingleColor(COLOR color, int16_t multiplier, uint8_t* dest);

	bool updateInternal(uint32_t index = 0, bool async = false);
	bool updateFromEffect();

	uint32_t* packIntoBuffer(uint8_t* dst, uint32_t* bbaddr);
	void swInterrupt();

	void poll();

	bool initialized;
	bool use_single_color;
	uint8_t* buffer;
	uint32_t* bitband_address;
	uint32_t led_count;
	float multiplier;
	uint8_t* code_buffer1;
	uint8_t* code_buffer2;
	uint8_t single_color[4];
	uint32_t* single_color_sbba;
	LedStripeType led_type;
	LedStripEffect effect;
	volatile bool on_tx;
	uint8_t* updating_buffer;
	uint32_t leds_to_update;
	bool send_apa102_end_frame;
	uint32_t update_limit;
};

#endif /* __LEDSTRIP_H__ */
