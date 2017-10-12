/*
 Copyright (c) 2011 Arduino.  All right reserved.
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

#include "Arduino.h"

#ifdef __cplusplus
extern "C" {
#endif

static int _readResolution = 12;
static int _writeResolution = 8;
static uint32_t tim1_pwm_freq = 490;
static uint32_t tim2_pwm_freq = 490;
static uint32_t tim3_pwm_freq = 490;
uint32_t pins_on_pwm = 0;

void analogReadResolution(int res) {
	_readResolution = res;
}

void analogWriteResolution(int res) {
	_writeResolution = res;
}

static inline uint32_t mapResolution(uint32_t value, uint32_t from, uint32_t to)
{
	if (from == to)
		return value;
	if (from > to)
		return value >> (from-to);
	else
		return value << (to-from);
}

static void setFrequency(TIM_TypeDef* tim, uint32_t freq)
{
	uint32_t prescaler = 1;

	if (!freq)
		return;

	while ((SystemCoreClock / prescaler) / freq > 0xFFFF)
		prescaler++;

	tim->CR1 &= ~TIM_CR1_CEN;
	tim->PSC = prescaler - 1;
	tim->ARR = (SystemCoreClock / prescaler) / freq;
	tim->CNT = 0;
	tim->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void configTimer(TIM_TypeDef* tim)
{
	uint32_t freq;

	if (tim == TIM1)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
		freq = tim1_pwm_freq;
	} else if (tim == TIM2)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
		freq = tim2_pwm_freq;
	} else if (tim == TIM3)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
		freq = tim3_pwm_freq;
	} else return;

	setFrequency(tim, freq);
}

void analogSetPwmFrequency(uint32_t ulPin, uint32_t frequency)
{
	if (ulPin > PINS_COUNT)
		return;

	setFrequency(g_APinDescription[ulPin].timer, frequency);

	if (g_APinDescription[ulPin].timer == TIM1)
		tim1_pwm_freq = frequency;
	else if (g_APinDescription[ulPin].timer == TIM2)
		tim2_pwm_freq = frequency;
	else if (g_APinDescription[ulPin].timer == TIM3)
		tim3_pwm_freq = frequency;
}

uint32_t analogRead(uint32_t ulPin)
{
	if (ulPin > PINS_COUNT)
		return 0;

	if (!g_APinDescription[ulPin].adc)
		return 0;

	// Check if the pin is configured as analog
	uint32_t moder = g_APinDescription[ulPin].pin_port->MODER;
	moder >>= g_APinDescription[ulPin].pin_num << 1;
	moder &= 0x03;
	if (moder != 0x03)
		pinMode(ulPin, INPUT_ANALOG);

	while (ADC_GetSoftwareStartConvStatus(g_APinDescription[ulPin].adc) != RESET);

	ADC_RegularChannelConfig(g_APinDescription[ulPin].adc, g_APinDescription[ulPin].adc_ch, 1, ADC_SampleTime_84Cycles);
	ADC_SoftwareStartConv(g_APinDescription[ulPin].adc);

	while (ADC_GetFlagStatus(g_APinDescription[ulPin].adc, ADC_FLAG_EOC) == RESET);

	return g_APinDescription[ulPin].adc->DR;
}

void setTimerChannel(TIM_TypeDef* tim, uint8_t ch, uint8_t enable)
{
	TIM_OCInitTypeDef TIM_OCInitStruct;

	TIM_OCStructInit(&TIM_OCInitStruct);

	if (enable)
	{
		TIM_OCInitStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
		TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
		TIM_OCInitStruct.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
		TIM_OCInitStruct.TIM_OCNPolarity = TIM_OCNPolarity_Low;
		TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_High;
		TIM_OCInitStruct.TIM_OutputNState = TIM_OutputNState_Disable;
		TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
		TIM_OCInitStruct.TIM_Pulse = 0;
	}

	switch (ch)
	{
		case 1:
			TIM_OC1Init(tim, &TIM_OCInitStruct);
			break;
		case 2:
			TIM_OC2Init(tim, &TIM_OCInitStruct);
			break;
		case 3:
			TIM_OC3Init(tim, &TIM_OCInitStruct);
			break;
		case 4:
			TIM_OC4Init(tim, &TIM_OCInitStruct);
			break;
	}

	if (enable)
		TIM_CtrlPWMOutputs(tim, ENABLE);
}

void analogWrite(uint32_t ulPin, uint32_t ulValue)
{
	uint32_t pwm_value;
	GPIO_InitTypeDef GPIO_InitStruct;

	if (ulPin > PINS_COUNT)
		return;

	if (!g_APinDescription[ulPin].timer)
	{
		// Defaults to digital write
		pinMode(ulPin, OUTPUT);
		ulValue = mapResolution(ulValue, _writeResolution, 8);
		if (ulValue < 128)
			digitalWrite(ulPin, LOW);
		else
			digitalWrite(ulPin, HIGH);

		return;
	}

	if (!(g_APinDescription[ulPin].timer->CR1 & TIM_CR1_CEN))
		// Timer is disabled
		configTimer(g_APinDescription[ulPin].timer);

	pwm_value = mapResolution(ulValue, _writeResolution, 16);

	if (!pwm_value)
	{
		pinMode(g_APinDescription[ulPin].pin_num, OUTPUT);
		digitalWrite(g_APinDescription[ulPin].pin_num, 0);
		return;
	}

	pwm_value = (g_APinDescription[ulPin].timer->ARR * pwm_value) / 65535;

	if (!(pins_on_pwm & (1 << ulPin)))
	{
		// Configure pin
		if (g_APinDescription[ulPin].timer == TIM1)
			GPIO_PinAFConfig(g_APinDescription[ulPin].pin_port, g_APinDescription[ulPin].pin_num, GPIO_AF_TIM1);
		else if (g_APinDescription[ulPin].timer == TIM2)
			GPIO_PinAFConfig(g_APinDescription[ulPin].pin_port, g_APinDescription[ulPin].pin_num, GPIO_AF_TIM2);
		else if (g_APinDescription[ulPin].timer == TIM3)
			GPIO_PinAFConfig(g_APinDescription[ulPin].pin_port, g_APinDescription[ulPin].pin_num, GPIO_AF_TIM3);

		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
		GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_InitStruct.GPIO_Pin = g_APinDescription[ulPin].pin_mask;
		GPIO_Init(g_APinDescription[ulPin].pin_port, &GPIO_InitStruct);

		// Configure timer channel
		setTimerChannel(g_APinDescription[ulPin].timer, g_APinDescription[ulPin].timer_ch, 1);
		pins_on_pwm |= 1 << ulPin;
	}

	switch (g_APinDescription[ulPin].timer_ch)
	{
		case 1: g_APinDescription[ulPin].timer->CCR1 = pwm_value; break;
		case 2: g_APinDescription[ulPin].timer->CCR2 = pwm_value; break;
		case 3: g_APinDescription[ulPin].timer->CCR3 = pwm_value; break;
		case 4: g_APinDescription[ulPin].timer->CCR4 = pwm_value; break;
	}
}

#ifdef __cplusplus
}
#endif
