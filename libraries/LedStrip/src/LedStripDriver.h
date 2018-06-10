/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2018 Artekit Labs
 * https://www.artekit.eu

### LedStripDriver.h

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

#ifndef __LEDSTRIPDRIVER_H__
#define __LEDSTRIPDRIVER_H__

#include <stm32f4xx.h>
#include <bitmap.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum LedStripeType
{
	WS2812B = 1,
	APA102,
	SK6812RGBW
};

class LedStripData
{
public:
	LedStripData()
	{
		buffer = NULL;
		buffer_size = led_count = 0;
	}

	virtual ~LedStripData()
	{
	}

	bool begin(uint32_t count)
	{
		deallocate();
		if (!allocate(count))
			return false;

		led_count = count;
		return true;
	}

	virtual bool allocate(uint32_t count) = 0;
	virtual void deallocate() = 0;

	virtual void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) = 0;
	virtual void set(uint32_t index, const COLOR& color)
	{
		set(index, getRed(color), getGreen(color), getBlue(color), getWhite(color));
	}

	virtual void setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w) = 0;
	virtual void setRange(uint32_t start, uint32_t end, const COLOR& color)
	{
		setRange(start, end, getRed(color), getGreen(color), getBlue(color), getWhite(color));
	}

	virtual void get(uint32_t index, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w) = 0;
	virtual const COLOR get(uint32_t index)
	{
		uint8_t r, g, b, w;
		get(index, &r, &g, &b, &w);
		return RGBW(r, g, b, w);
	}

	virtual inline uint32_t getBufferAddress() { return (uint32_t) buffer; }
	virtual inline uint32_t getBufferSize() { return (uint32_t) buffer_size; }
	virtual inline uint32_t getLedCount() { return (uint32_t) led_count; }
	virtual inline uint32_t* getBitBandPointer()
	{
		return (uint32_t*) (0x22000000 + ((uint32_t) buffer & 0x1FFFFFFF) * 32);
	}

	virtual void pack(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) = 0;

	virtual void copyInto(LedStripData* data)
	{
		memcpy(data->buffer, buffer, buffer_size);
	}

protected:
	uint8_t* buffer;
	uint32_t buffer_size;
	uint32_t led_count;
};

class LedStripDataWS2812 : public LedStripData
{
	bool allocate(uint32_t count)
	{
		led_count = count;
		uint32_t buffer_size = 3 * led_count;
		buffer = (uint8_t*) malloc(buffer_size);
		if (!buffer)
			return false;

		memset(buffer, 0, buffer_size);
		return true;
	}

	void deallocate()
	{
		if (buffer)
			free(buffer);
		buffer = NULL;
		led_count = 0;
	}

	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
	{
		(void)(w);
		uint8_t data[3];
		uint32_t count = 1;

		if (index > led_count)
			return;

		if (index == 0)
			count = led_count;
		else
			index = (index - 1) * 3;

		pack(data, r, g, b);

		while (count)
		{
			memcpy(buffer + index, data, 3);
			index += 3;
			count--;
		}
	}

	void setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
	{
		(void)(w);
		uint8_t data[3];
		uint32_t index;
		uint32_t count;

		if (!start || !end || start > led_count)
			return;

		if (end > led_count)
			end = led_count;

		count = end - start + 1;

		index = (start - 1) * 3;

		pack(data, r, g, b);

		while (count)
		{
			memcpy(buffer + index, data, 3);
			index += 3;
			count--;
		}
	}

	void get(uint32_t index, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
	{
		(void)(w);
		if (!index || index > led_count)
			return;

		index = (index - 1) * 3;

		uint8_t* ptr = buffer + index;

		if (g) *g = __RBIT(ptr[0]) >> 24;
		if (r) *r = __RBIT(ptr[1]) >> 24;
		if (b) *b = __RBIT(ptr[2]) >> 24;
	}

	void pack(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0)
	{
		(void)(w);
		dst[0] = (__RBIT(g) >> 24);
		dst[1] = (__RBIT(r) >> 24);
		dst[2] = (__RBIT(b) >> 24);
	}
};

class LedStripDataSK6812RGBW : public LedStripData
{
	bool allocate(uint32_t count)
	{
		led_count = count;
		uint32_t buffer_size = 4 * led_count;
		buffer = (uint8_t*) malloc(buffer_size);
		if (!buffer)
			return false;

		memset(buffer, 0, buffer_size);
		return true;
	}

	void deallocate()
	{
		if (buffer)
			free(buffer);
		buffer = NULL;
		led_count = 0;
	}

	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0)
	{
		uint32_t data;
		uint32_t count = 1;
		uint32_t* ptr = (uint32_t*) buffer;

		if (index > led_count)
			return;

		if (index == 0)
			count = led_count;

		pack((uint8_t*) &data, r, g, b, w);
		ptr += (index - 1);

		while (count)
		{
			*ptr++ = data;
			count--;
		}
	}

	void setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
	{
		(void)(w);
		uint32_t data;
		uint32_t* ptr = (uint32_t*) buffer;
		uint32_t count;

		if (!start || !end || start > led_count)
			return;

		if (end > led_count)
			end = led_count;

		count = end - start + 1;
		ptr += start - 1;
		pack((uint8_t*) &data, r, g, b, w);

		while (count)
		{
			*ptr++ = data;
			count--;
		}
	}

	void get(uint32_t index, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
	{
		(void)(w);
		if (!index || index > led_count)
			return;

		uint8_t* ptr = buffer + ((index - 1) * 4);

		*g = __RBIT(ptr[0]) >> 24;
		*r = __RBIT(ptr[1]) >> 24;
		*b = __RBIT(ptr[2]) >> 24;
		*w = __RBIT(ptr[3]) >> 24;
	}

	void pack(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0)
	{
		// Note: on the datasheet it says that red comes first,
		// but the in strip I have, red and green seem swapped.
		// TODO: verify
		dst[0] = (__RBIT(g) >> 24);
		dst[1] = (__RBIT(r) >> 24);
		dst[2] = (__RBIT(b) >> 24);
		dst[3] = (__RBIT(w) >> 24);
	}
};

class LedStripDataAPA102 : public LedStripData
{
	bool allocate(uint32_t count)
	{
		led_count = count;

		// It's been said around that the end frame has to be (count / 2) bits,
		// and actually I've seen this wrong behavior with a 72 LEDs strip,
		// so make the end frame a little bigger.
		uint32_t end_frame_size = ((count / 2) / 8) + 16;
		if (end_frame_size == 0)
			end_frame_size = 4;

		buffer_size = (4 * count) + 4 + end_frame_size;
		buffer = (uint8_t*) malloc(buffer_size);
		if (!buffer)
			return false;

		memset(buffer, 0, buffer_size);

		// Fill end frame
		memset(buffer + buffer_size - end_frame_size , 0xFF, end_frame_size);
		return true;
	}

	void deallocate()
	{
		if (buffer)
			free(buffer);
		buffer = NULL;
		led_count = 0;
	}

	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
	{
		(void)(w);
		uint32_t count = 1;
		uint32_t* ptr = (uint32_t*) buffer;
		uint32_t data;

		if (index > led_count)
			return;

		if (index == 0)
		{
			index = 1;
			count = led_count;
		}

		pack((uint8_t*) &data, r, g, b);
		ptr += index;

		while (count)
		{
			*ptr++ = data;
			count--;
		}
	}

	void setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
	{
		(void)(w);
		uint32_t data;
		uint32_t* ptr = (uint32_t*) buffer;
		uint32_t count;

		if (!start || !end || start > led_count)
			return;

		if (end > led_count)
			end = led_count;

		count = end - start + 1;
		ptr += start;
		pack((uint8_t*) &data, r, g, b, w);

		while (count)
		{
			*ptr++ = data;
			count--;
		}
	}

	void get(uint32_t index, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w)
	{
		(void)(w);
		if (!index || index > led_count)
			return;

		uint8_t* ptr = buffer;
		ptr += index * 4;

		*b = ptr[1];
		*g = ptr[2];
		*r = ptr[3];
		*w = 0;
	}

	void pack(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0)
	{
		(void)(w);
		dst[0] = 0xFF;
		dst[1] = b;
		dst[2] = g;
		dst[3] = r;
	}
};

extern "C" void DMA2_Stream5_IRQHandler(void);

class LedStripDriver
{
	friend void DMA2_Stream5_IRQHandler(void);

public:
	LedStripDriver();

	bool begin(uint32_t count, LedStripeType type = WS2812B);
	void end();

	void set(uint32_t index, const COLOR& color);
	void set(uint32_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);
	COLOR get(uint32_t index) { return led_data->get(index); }

	void setRange(uint32_t start, uint32_t end, const COLOR& color);
	void setRange(uint32_t start, uint32_t end, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0);

	bool update(uint32_t index = 0, bool async = false);
	void endUpdate();
	void setBrightness(float value);
	inline bool updating() { return busy(); }
	inline LedStripeType getType() { return led_type; }
	inline uint32_t getLedCount() { return led_data ? led_data->getLedCount() : 0; }
	inline void incrementBrightness(float value) { setBrightness(brightness + value); }
	inline void decrementBrightness(float value) { setBrightness(brightness - value); }
	inline float getBrightness() { return brightness; }

protected:

	void packSingleColor(const COLOR& color, int16_t depth, uint8_t* dest);
	uint32_t* packIntoBuffer(uint8_t* dst, uint32_t* bbaddr) __attribute__ ((optimize(3)));

	bool updateInternal(uint32_t index = 0, LedStripData* data = NULL, bool async = false);
	void ledTxHandler() __attribute__ ((optimize(3)));
	bool busy();
	void poll();

	bool initialized;
	bool use_single_color;
	uint32_t* bitband_address;
	float brightness;
	uint8_t* code_buffer1;
	uint8_t* code_buffer2;
	uint8_t single_color[4];
	uint32_t* single_color_sbba;
	LedStripeType led_type;
	volatile bool on_tx;
	uint8_t* updating_buffer;
	volatile uint32_t leds_to_update;
	volatile uint32_t updating_leds;
	bool send_apa102_end_frame;
	LedStripData* updating_data;
	LedStripData* led_data;
};

#endif /* __LEDSTRIPDRIVER_H__ */
