/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### ServiceTimer.cpp

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

static STObject stHead;
static STObject* pTail = &stHead;
bool stInitialized = false;

static void stInit(void)
{
	TIM_TimeBaseInitTypeDef TimeBase_InitStruct;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, ENABLE);

	TimeBase_InitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TimeBase_InitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TimeBase_InitStruct.TIM_Period = 42000 - 1;
	TimeBase_InitStruct.TIM_Prescaler = 1;
	TimeBase_InitStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM11, &TimeBase_InitStruct);

	NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
	NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, VARIANT_PRIO_ST);

	TIM_ITConfig(TIM11, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM11, ENABLE);
	stInitialized = true;
}

void stDeinit()
{
	TIM_ITConfig(TIM11, TIM_IT_Update, DISABLE);
	TIM_Cmd(TIM11, DISABLE);
	NVIC_DisableIRQ(TIM1_TRG_COM_TIM11_IRQn);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, DISABLE);
}

STObject::STObject()
{
	prev = next = NULL;
}

STObject::~STObject()
{

}

void STObject::add()
{
	if (!stInitialized)
		stInit();

	__disable_irq();
	// Not in list?
	if (!prev)
	{
		prev = pTail;
		next = NULL;
		pTail->next = this;
		pTail = this;
	}
	__enable_irq();
}

void STObject::remove()
{
	__disable_irq();
	// In list?
	if (this->prev)
	{
		this->prev->next = this->next;

		if (this == pTail)
			pTail = this->prev;
		else
			this->next->prev = this->prev;

		this->next = this->prev = NULL;
	}
	__enable_irq();
}

void STObject::poll()
{
}

extern "C" void TIM1_TRG_COM_TIM11_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM11, TIM_IT_Update))
	{
		TIM_ClearFlag(TIM11, TIM_FLAG_Update);

		STObject* list = stHead.next;
		while (list)
		{
			list->poll();
			list = list->next;
		}
	}
}
