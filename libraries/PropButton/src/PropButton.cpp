/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropButton.cpp

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

#include "PropButton.h"
#include <Arduino.h>

PropButton::PropButton()
{
	enabled = false;
	resetEvents();
}

PropButton::~PropButton()
{
	end();
}

void PropButton::begin(uint32_t pin, ButtonType type, uint32_t minimum_debouncer)
{
	if (enabled)
		return;

	button_pin = pin;
	button_type = type;
	debounce = minimum_debouncer;

	if (button_type == ButtonActiveLow)
		pinMode(pin, INPUT_PULLUP);
	else
		pinMode(pin, INPUT_PULLDOWN);

	enabled = 1;
	read_enabled = 1;

	add();
}

void PropButton::end()
{
	enabled = false;
	remove();
	pinMode(button_pin, INPUT);
}

ButtonEvent PropButton::getEvent()
{
	ButtonEvent event;

	if (!event_count)
		return EventNone;

	read_enabled = 0;

	event = events[event_rd++];

	if (event_rd == BUTTON_MAX_EVENTS)
		event_rd = 0;

	event_count--;

	read_enabled = 1;

	return event;
}

void PropButton::addEvent(ButtonEvent event)
{
	if (event_count == BUTTON_MAX_EVENTS)
		return;

	events[event_wr++] = event;
	if (event_wr == BUTTON_MAX_EVENTS)
		event_wr = 0;

	event_count++;
}

void PropButton::resetEvents()
{
	read_enabled = 0;
	event_count = event_rd = event_wr = 0;
	read_enabled = 1;
}

void PropButton::blink(uint32_t pin, uint32_t cycle_ms, uint32_t time_on, ButtonLedType type)
{
	if (!enabled)
		return;

	if (!cycle_ms)
	{
		if (blink_enabled)
			ledOff();

		blink_enabled = false;
		return;
	}

	if (time_on > cycle_ms)
		return;

	pinMode(pin, OUTPUT);

	blink_pin = pin;
	blink_cycle = cycle_ms;
	blink_time_on = time_on;
	blink_counter = 0;
	blink_led_type = type;
	blink_enabled = true;
	ledOn();
}

void PropButton::poll()
{
	uint32_t elapsed = 0;

	if (!enabled)
		return;

	if (read_enabled)
	{
		// PropButton status
		uint8_t status = digitalRead(button_pin);

		if (button_type == ButtonActiveLow)
			status = !status;

		if (status != last_read)
		{
			counter = GetTickCount();
		} else {
			if (asserted != status)
			{
				elapsed = GetTickCount() - counter;
				if (elapsed >= debounce)
				{
					asserted = status;

					if (asserted)
					{
						long_press_detected = 0;
						pressed_time = GetTickCount();
						addEvent(ButtonPressed);
					} else {
						addEvent(ButtonReleased);
						if (!long_press_detected)
							addEvent(ButtonShortPressAndRelease);
					}
				}
			} else {
				if (asserted)
				{
					if (!long_press_detected &&
						long_press_time &&
						GetTickCount() - pressed_time >= long_press_time)
					{
						addEvent(ButtonLongPressed);
						long_press_detected = 1;
					}
				}
			}
		}

		last_read = status;
	}

	// LED blink
	if (blink_enabled)
	{
		blink_counter++;
		if (blink_counter == blink_cycle)
		{
			blink_counter = 0;
			ledOn();
		}

		if (blink_counter == blink_time_on)
			ledOff();
	}
}
