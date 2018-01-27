/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropButton.h

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

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <ServiceTimer.h>
#include <wiring_digital.h>

#define BUTTON_MAX_EVENTS	5

typedef enum
{
	ButtonActiveLow = 0,
	ButtonActiveHigh
} ButtonType;

typedef enum
{
	ButtonLedActiveLow = 0,
	ButtonLedActiveHigh
} ButtonLedType;

typedef enum
{
	EventNone = 0,
	ButtonPressed,
	ButtonReleased,
	ButtonLongPressed,
	ButtonShortPressAndRelease,
} ButtonEvent;

class PropButton : public STObject
{
public:
	PropButton();
	~PropButton();

	void begin(uint32_t pin, ButtonType type = ButtonActiveHigh, uint32_t minimum_debouncer = 25);
	void end();
	inline bool pressed() { return asserted == 1; }
	inline bool released() { return asserted != 1; }
	inline void setLongPressTime(uint32_t ms) { long_press_time = ms; };
	ButtonEvent getEvent();
	void resetEvents();
	void blink(uint32_t pin, uint32_t cycle_ms, uint32_t time_on, ButtonLedType type = ButtonLedActiveHigh);

protected:
	void poll();
	void addEvent(ButtonEvent event);

private:
	ButtonEvent events[BUTTON_MAX_EVENTS];
	uint8_t event_count;
	uint8_t event_rd;
	uint8_t event_wr;

	volatile uint8_t read_enabled;
	uint8_t enabled;

	volatile uint8_t asserted;
	uint32_t debounce;
	uint32_t pressed_time;
	uint32_t long_press_time;
	uint8_t last_read;
	uint8_t long_press_detected;
	uint32_t counter;
	ButtonType button_type;
	uint32_t button_pin;

	uint8_t blink_enabled;
	uint32_t blink_pin;
	uint32_t blink_cycle;
	uint32_t blink_time_on;
	uint32_t blink_counter;
	ButtonLedType blink_led_type;

	inline void ledOn() {
		digitalWrite(blink_pin, blink_led_type == ButtonLedActiveHigh ? 1 : 0);
	}

	inline void ledOff() {
		digitalWrite(blink_pin, blink_led_type == ButtonLedActiveHigh ? 0 : 1);
	}
};

#endif // __BUTTON_H__
