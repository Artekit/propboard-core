/*
  Copyright (c) 2011-2012 Arduino.  All right reserved.
  Copyright (c) 2017 Artekit Labs.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "WInterrupts.h"

typedef void (*interruptCB)(void);

static interruptCB irq_funcs[16] = {0};
static volatile uint8_t exti9_5 = 0;
static volatile uint8_t exti15_10 = 0;

static IRQn_Type getIrqNum(uint8_t irq)
{
	if (irq < 5)
		return EXTI0_IRQn + irq;
	else if (irq < 10)
		return EXTI9_5_IRQn;
	else
		return EXTI15_10_IRQn;
}

void attachInterrupt(uint32_t pin, void (*callback)(void), uint32_t mode)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	IRQn_Type irq;

	if (pin > PINS_COUNT)
		return;

	if (g_APinDescription[pin].irq_num == NIRQ)
		return;

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

	SYSCFG_EXTILineConfig(g_APinDescription[pin].irq_port_num, g_APinDescription[pin].pin_num);

	EXTI_InitStruct.EXTI_Line = g_APinDescription[pin].pin_mask;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	irq = getIrqNum(g_APinDescription[pin].irq_num);

	__disable_irq();

	irq_funcs[g_APinDescription[pin].irq_num] = callback;

	if (irq == EXTI9_5_IRQn)
	{
		if (exti9_5 == 0)
			NVIC_ClearPendingIRQ(irq);

		exti9_5++;
	}
	else if (irq == EXTI15_10_IRQn)
	{
		if (exti15_10 == 0)
			NVIC_ClearPendingIRQ(irq);

		exti15_10++;
	} else {
		NVIC_ClearPendingIRQ(irq);
	}

	EXTI_ClearITPendingBit(g_APinDescription[pin].pin_mask);
	NVIC_SetPriority(irq, VARIANT_PRIO_USER_EXTI);
	NVIC_EnableIRQ(irq);

	__enable_irq();
}

void detachInterrupt(uint32_t pin)
{
	IRQn_Type irq;
	EXTI_InitTypeDef EXTI_InitStruct;

	if (pin > PINS_COUNT)
		return;

	if (g_APinDescription[pin].irq_num == NIRQ)
		return;

	if (irq_funcs[g_APinDescription[pin].irq_num] == NULL)
		return;

	irq = getIrqNum(g_APinDescription[pin].irq_num);

	EXTI_InitStruct.EXTI_Line = g_APinDescription[pin].pin_mask;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_LineCmd = DISABLE;
	EXTI_Init(&EXTI_InitStruct);

	__disable_irq();
	irq_funcs[g_APinDescription[pin].irq_num] = NULL;

	if (irq == EXTI9_5_IRQn)
	{
		if (exti9_5 && --exti9_5 == 0)
			NVIC_DisableIRQ(irq);
	}
	else if (irq == EXTI15_10_IRQn)
	{
		if (exti15_10 && --exti15_10 == 0)
			NVIC_DisableIRQ(irq);
	} else {
		NVIC_DisableIRQ(irq);
	}
	__enable_irq();
}

void maskInterrupt(uint32_t pin)
{
	if (pin > PINS_COUNT)
		return;

	if (g_APinDescription[pin].irq_num == NIRQ)
		return;

	EXTI->EMR &= ~(1 << g_APinDescription[pin].irq_num);
}

void unmaskInterrupt(uint32_t pin)
{
	if (pin > PINS_COUNT)
		return;

	if (g_APinDescription[pin].irq_num == NIRQ)
		return;

	EXTI->EMR |= (1 << g_APinDescription[pin].irq_num);
}

static void EXTI_IRQHandler(uint32_t line_num)
{
	if (EXTI->PR & (1 << line_num))
	{
		EXTI->PR = 1 << line_num;
		if (irq_funcs[line_num])
			(irq_funcs[line_num])();
	}
}

void EXTI0_IRQHandler(void)
{
	EXTI_IRQHandler(0);
}

void EXTI1_IRQHandler(void)
{
	EXTI_IRQHandler(1);
}

void EXTI3_IRQHandler(void)
{
	EXTI_IRQHandler(3);
}

void EXTI4_IRQHandler(void)
{
	EXTI_IRQHandler(4);
}

void EXTI9_5_IRQHandler()
{
	EXTI_IRQHandler(5);
	EXTI_IRQHandler(6);
	EXTI_IRQHandler(7);
	EXTI_IRQHandler(8);
	EXTI_IRQHandler(9);
}

void EXTI15_10_IRQHandler()
{
	EXTI_IRQHandler(10);
	EXTI_IRQHandler(11);
	EXTI_IRQHandler(12);
	EXTI_IRQHandler(13);
	EXTI_IRQHandler(14);
	EXTI_IRQHandler(15);
}
