/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### HBLED.cpp

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

#include <string.h>
#include <stdlib.h>
#include <Arduino.h>
#include "HBLED.h"
#include "bitmap.h"

#define HBLED_MAX_CURRENT		VARIANT_MAX_LED_CURRENT

extern uint32_t getRandom(uint32_t min, uint32_t max);
static uint8_t hbled_hw_init = 0;
static uint8_t hbled_ch_init[VARIANT_MAX_LED_OUTPUTS] = {0};

static void initHBLEDHardware()
{
	GPIO_InitTypeDef GPIO_InitStruct;
	TIM_OCInitTypeDef OC_InitStruct;
	TIM_TimeBaseInitTypeDef TimeBase_InitStruct;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	// Configure soft-start pin (PA4)
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	// PA0, PA1 and PA2 are our LED PWM pins
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_TIM5);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM5);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_TIM5);

	TimeBase_InitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TimeBase_InitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TimeBase_InitStruct.TIM_Period = (84000000/1000)-1;
#ifdef HBLED_BETA_SOFT_START
	TimeBase_InitStruct.TIM_Period = (84000000/200)-1;
#endif
	TimeBase_InitStruct.TIM_Prescaler = 0;
	TimeBase_InitStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM5, &TimeBase_InitStruct);

	TIM_OC2PreloadConfig(TIM5, TIM_OCPreload_Enable);

	TIM_OCStructInit(&OC_InitStruct);
	OC_InitStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
	OC_InitStruct.TIM_OCMode = TIM_OCMode_PWM1;
	OC_InitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
	OC_InitStruct.TIM_Pulse = 0;

	TIM5->CCR1 = 0;
	TIM5->CCR2 = 0;
	TIM5->CCR3 = 0;

	TIM_OC1Init(TIM5, &OC_InitStruct);
	TIM_OC2Init(TIM5, &OC_InitStruct);
	TIM_OC3Init(TIM5, &OC_InitStruct);
	TIM_Cmd(TIM5, ENABLE);

#ifndef HBLED_BETA_SOFT_START
	doSoftStart();
#endif
}

void doSoftStart()
{
	// Turn off the LEDs
	TIM5->CCR1 = 0;
	TIM5->CCR2 = 0;
	TIM5->CCR3 = 0;

	// Enable soft-start switch
	GPIOA->BSRRL = GPIO_Pin_4;

	delayMicroseconds(500);

	// Enable PWM output
	TIM5->CCER |= TIM_CCER_CC1E;
	TIM5->CCER |= TIM_CCER_CC2E;
	TIM5->CCER |= TIM_CCER_CC3E;

	// Set 100% PWM
	TIM5->CCR1 = TIM5->ARR + 1;
	TIM5->CCR2 = TIM5->ARR + 1;
	TIM5->CCR3 = TIM5->ARR + 1;

	// Delay
	delayMicroseconds(1500);

	// Set 0% PWM
	TIM5->CCR1 = 0;
	TIM5->CCR2 = 0;
	TIM5->CCR3 = 0;

	delayMicroseconds(2000);

	// Disable soft-start switch
	GPIOA->BSRRH = GPIO_Pin_4;

	// Disable PWM output
	TIM5->CCER &= ~TIM_CCER_CC1E;
	TIM5->CCER &= ~TIM_CCER_CC2E;
	TIM5->CCER &= ~TIM_CCER_CC3E;
}

void deinitHBLEDHardware()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	// Disable timer
	TIM_Cmd(TIM5, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
}

HBLED::HBLED(uint8_t ch)
{
	cc_reg = NULL;
	max_current = 0;
	pwm_percent = 0;
	pwm_value = 0;
	initialized = false;
	multiplier = 1;
	last_value = 0;
	channel = ch;
#ifdef HBLED_BETA_SOFT_START
	first_time = false;
#endif
}

HBLED::HBLED()
{
	cc_reg = NULL;
	max_current = 0;
	pwm_percent = 0;
	pwm_value = 0;
	initialized = false;
	multiplier = 1;
	last_value = 0;
	channel = 0;
#ifdef HBLED_BETA_SOFT_START
	first_time = false;
#endif
}

HBLED::~HBLED()
{
	end();
}

bool HBLED::begin(uint8_t ch, uint16_t current_mA)
{
	channel = ch;
	return begin(current_mA);
}

bool HBLED::begin(uint16_t current_mA)
{
	if (!hbled_hw_init)
	{
		// First configuration
		initHBLEDHardware();
		hbled_hw_init = 1;
	}

	if (channel < 1 || channel > VARIANT_MAX_LED_OUTPUTS)
		return false;

	if (hbled_ch_init[channel - 1])
		return true;

	switch (channel)
	{
		case 1:
			cc_reg = &TIM5->CCR1;
			TIM5->CCER |= TIM_CCER_CC1E;
			break;

		case 2:
			cc_reg = &TIM5->CCR2;
			TIM5->CCER |= TIM_CCER_CC2E;
			break;
			
		case 3:
			cc_reg = &TIM5->CCR3;
			TIM5->CCER |= TIM_CCER_CC3E;
			break;
	}

	if (current_mA > HBLED_MAX_CURRENT)
		current_mA = HBLED_MAX_CURRENT;
	
	hbled_ch_init[channel - 1] = 1;
	max_current = current_mA;
	initialized = true;

	return true;
}

void HBLED::end()
{

	if (channel)
	{
		if (!hbled_ch_init[channel - 1])
			return;

		// Disable CC output
		TIM5->CCER &= ~(1 << (channel - 1));
		hbled_ch_init[channel - 1] = 0;
	}

	if (led_effect.active)
		remove();

	initialized = false;
}

inline void HBLED::setPWM(uint32_t value)
{
	*cc_reg = value;

	// TODO remove these debug variables
	pwm_value = value;
	pwm_percent = (value * 100) / (TIM5->ARR + 1);
}

void HBLED::setCurrent(uint16_t current_mA)
{
	uint32_t pwm;

	if (current_mA > max_current)
		current_mA = max_current;

#ifdef HBLED_BETA_SOFT_START
	if (first_time == 0)
	{
		// Hack to overcome the soft-start of the LED2001
		// present in the BETA version of the board (v1.5)
		first_time = 1;
		pwm = TIM5->ARR + 1;
		setPWM(pwm);
		delayMicroseconds(1100);
		setPWM(0);
	}
#endif // HBLED_BETA_SOFT_START

	pwm = (current_mA * (TIM5->ARR + 1)) / HBLED_MAX_CURRENT;
	setPWM(pwm);
}

void HBLED::setWithLUT(uint8_t value)
{
	uint8_t val = cie_lut[value];

	val *= multiplier;

	uint16_t current = (val * max_current) / 255;
	setCurrent(current);
}

void HBLED::setValue(uint8_t value)
{
	if (withEffect())
		stopEffect();

	setWithLUT(value);
	last_value = value;
}

uint16_t HBLED::getCurrent()
{
	uint32_t pwm = *cc_reg;
	return (uint16_t) (pwm * HBLED_MAX_CURRENT / (TIM5->ARR + 1));
}

uint8_t HBLED::getValue()
{
	if (!max_current)
		return 0;

	uint16_t current = getCurrent();
	return (uint8_t) (current * 255 / max_current);
}

void HBLED::poll()
{
	if (!led_effect.active)
		return;

	switch(led_effect.type)
	{
		case LedEffectShimmer:
			if (led_effect.params.shimmer.random)
			{
				if (++led_effect.tick_count == led_effect.ticks)
				{
					led_effect.tick_count = 0;
					led_effect.params.shimmer.value = getRandom(led_effect.params.shimmer.minimum,
																led_effect.params.shimmer.maximum);
				}
			} else {
				if (led_effect.params.shimmer.up)
				{
					led_effect.params.shimmer.value += led_effect.params.shimmer.step;
					if (led_effect.params.shimmer.value >= led_effect.params.shimmer.maximum)
					{
						led_effect.params.shimmer.up = false;
						led_effect.params.shimmer.value = led_effect.params.shimmer.maximum;
					}
				} else {
					led_effect.params.shimmer.value -= led_effect.params.shimmer.step;
					if (led_effect.params.shimmer.value <= led_effect.params.shimmer.minimum)
					{
						led_effect.params.shimmer.value = led_effect.params.shimmer.minimum;
						led_effect.params.shimmer.up = true;
					}
				}
			}

			setWithLUT((uint8_t) led_effect.params.shimmer.value);
			break;

		case LedEffectFlash:

			// Time to toggle the LED?
			if (++led_effect.tick_count == led_effect.ticks)
			{
				led_effect.tick_count = 0;

				// Check in the LED is OFF
				if (!led_effect.params.flash.on)
				{
					// Turn the LED ON with current_on current
					setWithLUT(led_effect.params.flash.on_value);
					led_effect.params.flash.on = true;
				} else {
					//  Turn the LED OFF
					setWithLUT(led_effect.params.flash.off_value);
					led_effect.params.flash.on = false;
				}
			}
				
			// If effect is not infinite
			if (!led_effect.params.flash.infinite)
			{
				// Decrement time-left counter
				led_effect.params.flash.duration--;

				if (led_effect.params.flash.duration == 0)
				{
					// End of effect
					led_effect.active = 0;
					setWithLUT(led_effect.params.flash.off_value);
					remove();
				}
			}
			break;

		case LedEffectRamp:
			if (led_effect.params.ramp.up)
			{
				led_effect.params.ramp.value += led_effect.params.ramp.step;
				if (led_effect.params.ramp.value > led_effect.params.ramp.end)
					led_effect.params.ramp.value = led_effect.params.ramp.end;
			} else {
				led_effect.params.ramp.value -= led_effect.params.ramp.step;
				if (led_effect.params.ramp.value < led_effect.params.ramp.end)
					led_effect.params.ramp.value = led_effect.params.ramp.end;
			}

			setWithLUT(led_effect.params.ramp.value);

			if (--led_effect.params.ramp.duration == 0)
			{
				setWithLUT(led_effect.params.ramp.end);
				led_effect.active = false;
				remove();
			}
			break;

		case LedEffectNone:
			break;
	}
}

void HBLED::ramp(uint8_t start, uint8_t end, uint32_t duration)
{
	float increment = 0;
	uint32_t range;
	float duration_ticks;

	if (start == end || !duration)
		return;

	range = abs(start - end);

	duration_ticks = duration * (1000 / getFrequency());
	if (duration_ticks < 1)
		return;

	setValue(start);

	increment = (float) range / (float) duration;

	led_effect.type = LedEffectRamp;
	led_effect.params.ramp.step = increment;
	led_effect.params.ramp.up = (end > start);
	led_effect.params.ramp.value = start;
	led_effect.params.ramp.end = end;
	led_effect.params.ramp.counter = 0;
	led_effect.params.ramp.duration = duration_ticks;
	led_effect.active = true;

	add();
}

void HBLED::shimmer(uint8_t lower, uint8_t upper, uint8_t hz, uint8_t random)
{
	float increment = 0;
	uint32_t ticks_in_cycle = 0;
	uint32_t ticks_in_half_cycle = 0;
	uint32_t difference;
	uint8_t initial_value;

	if (lower >= upper || !hz)
		return;

	if (hz > 100)
		hz = 100;

	setValue(lower);

	led_effect.type = LedEffectShimmer;
	led_effect.params.shimmer.maximum = upper;
	led_effect.params.shimmer.minimum = lower;
	led_effect.params.shimmer.random = random;

	if (!random)
	{
		ticks_in_cycle = getFrequency() / hz;
		ticks_in_half_cycle = ticks_in_cycle / 2;
		difference = upper - lower;
		increment = (float) difference / (float) ticks_in_half_cycle;

		led_effect.params.shimmer.step = increment;
		led_effect.params.shimmer.up = true;
	} else {
		led_effect.ticks = getFrequency() / hz;
		led_effect.tick_count = 0;
	}

	initial_value = getValue();
	if (initial_value < led_effect.params.shimmer.minimum)
		initial_value = led_effect.params.shimmer.minimum;

	led_effect.params.shimmer.value = initial_value;
	led_effect.active = true;

	add();
}

void HBLED::flash(uint8_t on, uint8_t off, uint8_t hz, uint32_t duration)
{
	if (!hz)
		return;

	setValue(on);

	led_effect.type = LedEffectFlash;
	led_effect.params.flash.duration = duration * (1000 / getFrequency());
	led_effect.params.flash.infinite = (duration == 0);
	led_effect.params.flash.on = true;
	led_effect.params.flash.on_value = on;
	led_effect.params.flash.off_value = off;
	led_effect.ticks = ((getFrequency() / hz) / 2) + 1;
	led_effect.tick_count = 0;
	led_effect.active = true;

	add();
}

float HBLED::setMultiplier(float multp)
{
	float prev;

	if (multp > 1)
		multp = 1;
	else if (multp < 0)
		multp = 0;

	prev = multiplier;
	multiplier = multp;

	if (!withEffect())
		setWithLUT(last_value);

	return prev;
}

void HBLED::setPercent(uint8_t percent)
{
	uint32_t value;

	value = (percent * TIM5->ARR + 1) / 100;
	setPWM(value);
}

void setPwmFrequency(uint32_t value)
{
	if (!value)
		return;

	uint32_t pwm_value = 84000000 / value;
	TIM5->ARR = pwm_value;
	TIM5->CNT = 0;
}
