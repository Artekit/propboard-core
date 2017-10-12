/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropPower.cpp

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

#include <Arduino.h>

static void doShutdown(bool motion_enabled)
{
	// Disable LEDs PWM
	deinitHBLEDHardware();

	// Stop audio
	Audio.end();

	// Unmount SD
	f_mount(NULL, "", 0);

	// Shutdown SD
	sdDeinitialize();

	// Service timer
	stDeinit();

	Serial.end();
	Serial1.end();

	// Switch off 5V
	power5V(false);

	if (!motion_enabled)
	{
		// Accelerometer in stand-by
		Motion.disable();

		// Switch off 3.3V
		power3V3(false);
	}

	SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
}

void enterLowPowerMode(uint32_t pin, uint32_t mode, bool motion_enabled)
{
	EXTI_InitTypeDef EXTI_InitStruct;

	// We'll enter STOP LPLV mode, all clocks will be disabled
	// but we still need to disable external peripherals.
	// At STOP mode exit, we will produce a SW reset.

	if (pin > PINS_COUNT)
		return;

	if (g_APinDescription[pin].irq_num == NIRQ)
		return;

	// Detach any interrupt function
	detachInterrupt(pin);

	doShutdown(motion_enabled);

	SYSCFG_EXTILineConfig(g_APinDescription[pin].irq_port_num, g_APinDescription[pin].pin_num);

	switch (mode)
	{
		case FALLING:
			EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
			break;

		case RISING:
			EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
			break;

		case CHANGE:
			EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
			break;

		default:
			return;
	}

	EXTI_InitStruct.EXTI_Line = g_APinDescription[pin].pin_mask;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Event;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	__disable_irq();

	/* Enter STOP mode */
	PWR_EnterSTOPMode(PWR_LowPowerRegulator_ON, PWR_STOPEntry_WFE);

	/* On exit, do a system reset */
	NVIC_SystemReset();
}

uint16_t readBattery()
{
	uint16_t battery;
	uint16_t vrefint;

	// Read VREFINT
	while (ADC_GetSoftwareStartConvStatus(ADC1) != RESET);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_17, 1, ADC_SampleTime_84Cycles);
	ADC_SoftwareStartConv(ADC1);
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

	vrefint = ADC1->DR;

	while (ADC_GetSoftwareStartConvStatus(ADC1) != RESET);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_10, 1, ADC_SampleTime_84Cycles);
	ADC_SoftwareStartConv(ADC1);
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

	battery = ADC1->DR;
	battery = battery * 1210 / vrefint;

	return battery / 0.233f;
}
