/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### variant.cpp

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

#include "variant.h"
#include <sys/types.h>

RingBuffer rx_bufferSerial;
RingBuffer tx_bufferSerial;
RingBuffer rx_bufferSerial1;
RingBuffer tx_bufferSerial1;
UARTClass Serial(USART1, USART1_IRQn, &rx_bufferSerial, &tx_bufferSerial);
UARTClass Serial1(USART6, USART6_IRQn, &rx_bufferSerial1, &tx_bufferSerial1);
PropMotion Motion = PropMotion::instance();
PropAudio Audio = PropAudio::instance();
FATFS fs;

extern uint32_t program_offset;
extern "C" void __libc_init_array(void);

#define WITH_BOOTLOADER	1

static const uint8_t signature[] __attribute__ ((section(".signature"), used )) =
{
	"AAKK-60E2-485F-B5B2-4F593B57FF21"
};

const PinDescription g_APinDescription[] =
{
	// Pin 0 (USART6 RX)
	{ GPIOC,  7,  GPIO_Pin_7, NULL,  0, NULL, 0, NIRQ, 0, 1 },
	// Pin 1 (USART6 TX)
	{ GPIOA, 11, GPIO_Pin_11, NULL,  0, TIM1, 4,   11, 0, 1 },
	// Pin 2 (SCL)
	{ GPIOB, 10, GPIO_Pin_10, NULL,  0, NULL, 0,   10, 1, 1 },
	// Pin 3 (SDA)
	{ GPIOB,  3,  GPIO_Pin_3, NULL,  0, NULL, 0,    3, 1, 1 },
	// Pin 4
	{ GPIOB,  0,  GPIO_Pin_0, ADC1,  8, TIM3, 3,    0, 1, 0 },
	// Pin 5
	{ GPIOB,  1,  GPIO_Pin_1, ADC1,  9, TIM3, 4,    1, 1, 0 },
	// Pin 6
	{ GPIOA,  3,  GPIO_Pin_3, ADC1,  3, TIM2, 4,    3, 0, 0 },
	// Pin 7
	{ GPIOC,  4,  GPIO_Pin_4, ADC1, 14, NULL, 0,    4, 2, 0 },
	// Pin 8
	{ GPIOC,  5,  GPIO_Pin_5, ADC1, 15, NULL, 0,    5, 2, 0 },
	// Pin 9 (SS) (A2)
	{ GPIOA,  7,  GPIO_Pin_7, ADC1,  7, NULL, 0, NIRQ, 0, 1 },
	// Pin 10 (MOSI)
	{ GPIOB,  5,  GPIO_Pin_5, NULL,  0, TIM3, 2, NIRQ, 0, 1 },
	// Pin 11 (MISO) (A3)
	{ GPIOA,  6,  GPIO_Pin_6, ADC1,  6, TIM3, 1, NIRQ, 0, 1 },
	// Pin 12 (CLK) (A4)
	{ GPIOA,  5,  GPIO_Pin_5, ADC1,  5, TIM2, 1, NIRQ, 0, 1 },
	// Pin 13 (AUDIO_MUTE)
#ifdef MUTE_ON_PB14
	{ GPIOB, 14, GPIO_Pin_14, NULL,  0, NULL, 0, NIRQ, 0, 0 },
#else
	{ GPIOA, 15, GPIO_Pin_15, NULL,  0, NULL, 0, NIRQ, 0, 0 },
#endif
	// PIN 14 (Accelerometer IRQ 1)
	{ GPIOB, 7,   GPIO_Pin_7, NULL,  0, NULL, 0,    7, 1, 0 },
	// PIN 15 (Accelerometer IRQ 2)
	{ GPIOB, 6,   GPIO_Pin_6, NULL,  0, NULL, 0,    6, 1, 0 },
	// PIN 16 (5V On/Off)
	{ GPIOC, 1,   GPIO_Pin_1, NULL,  0, NULL, 0, NIRQ, 0, 0 },
	// PIN 17 (3.3V On/Off)
	{ GPIOC, 2,   GPIO_Pin_2, NULL,  0, NULL, 0, NIRQ, 0, 0 },
};

extern "C" void USART1_IRQHandler(void)
{
	Serial.IrqHandler();
}

extern "C" void USART6_IRQHandler(void)
{
	Serial1.IrqHandler();
}

static void initADC()
{
	ADC_InitTypeDef ADC_InitStruct;
	ADC_CommonInitTypeDef ADCCommon_InitStruct;
	GPIO_InitTypeDef GPIO_InitStruct;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	ADCCommon_InitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADCCommon_InitStruct.ADC_Mode = ADC_Mode_Independent;
	ADCCommon_InitStruct.ADC_Prescaler = ADC_Prescaler_Div4;
	ADCCommon_InitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADCCommon_InitStruct);

	ADC_StructInit(&ADC_InitStruct);
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStruct.ADC_NbrOfConversion = 1;
	ADC_InitStruct.ADC_ScanConvMode = ENABLE;
	ADC_Init(ADC1, &ADC_InitStruct);
	ADC_Cmd(ADC1, ENABLE);
	ADC_TempSensorVrefintCmd(ENABLE);

	// Configure battery AD (PC0)
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static inline void initClocks()
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
						   RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD,
						   ENABLE);
}

void init(void)
{
#if WITH_BOOTLOADER
	// SystemInit() was already called by the bootloader, so...
	// Initialize FPU (usually done by SystemInit())
	#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
	#endif

	// And reallocate vector table
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, ((uint32_t) &program_offset) - NVIC_VectTab_FLASH);
#else
	/* Call the clock system initialization function.*/
	SystemInit();
#endif

	/* Call static constructors */
	__libc_init_array();

	SysTick_Config((SystemCoreClock/1000) - 1);
	NVIC_SetPriority(SysTick_IRQn, VARIANT_PRIO_SYSTICK);

	initClocks();

	// Configure all pins as inputs (no pull-up/down)
	for (unsigned i = 0; i < PINS_COUNT; i++)
		pinMode(i, INPUT);

	// Configure 3.3V and 5V power pins
	pinMode(ONOFF_3V3, OUTPUT);
	pinMode(ONOFF_5V, OUTPUT);

	power5V(false);
	power3V3(false);

	// Audio mute
	pinMode(AUDIO_MUTE, OUTPUT);
	digitalWrite(AUDIO_MUTE, 0);

	delay(100);
	power5V(true);
	power3V3(true);

	delay(50);

	i2c1.begin(400000);

	initADC();

	f_mount(&fs, "", 0);
}
