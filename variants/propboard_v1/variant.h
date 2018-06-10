/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### variant.h

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

#ifndef __VARIANT_H__
#define __VARIANT_H__

#include "Arduino.h"
#ifdef __cplusplus
#include "UARTClass.h"
#include "PropMotion.h"
#include "PropAudio.h"
#endif

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#ifdef BETA_BOARD
#define MUTE_ON_PB14
#define HBLED_BETA_SOFT_START
#endif

#define PINS_COUNT 			18
#define NUM_DIGITAL_PINS 	18
#define NUM_ANALOG_INPUTS 	8
#define NUM_IRQ_PINS		10
#define NUM_MAX_EXT_IRQ		22

#define digitalPinToPort(P)        ( g_APinDescription[P].pin_port )
#define digitalPinToBitMask(P)     ( g_APinDescription[P].pin_mask )
#define portOutputRegister(port)   ( &(port->ODR) )
#define portInputRegister(port)    ( &(port->IDR) )
#define digitalPinHasPWM(P)        ( g_APinDescription[P].timer != NULL )

static const uint8_t SDA = 3;
static const uint8_t SCL = 2;

static const uint8_t SS = 6;
static const uint8_t MOSI = 7;
static const uint8_t MISO = 8;
static const uint8_t SCK = 9;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;
static const uint8_t A3 = 7;
static const uint8_t A4 = 8;
static const uint8_t A5 = 9;

static const uint8_t AUDIO_MUTE = 13;
static const uint8_t ACC_IRQ1 = 14;
static const uint8_t ACC_IRQ2 = 15;
static const uint8_t ONOFF_5V = 16;
static const uint8_t ONOFF_3V3 = 17;

#define VARIANT_MAX_LED_CURRENT	1063
#define VARIANT_MAX_LED_OUTPUTS	3

#define VARIANT_PRIO_SDIO				0
#define VARIANT_PRIO_LEDSTRIP_DMA		0
#define VARIANT_PRIO_I2S_DMA			1
//#define VARIANT_PRIO_SDIO				2
#define VARIANT_PRIO_LEDSTRIP_EXTI		3
#define VARIANT_PRIO_LEDSTRIP_APA102	4
#define VARIANT_PRIO_SYSTICK			4
#define VARIANT_PRIO_UART				5
#define VARIANT_PRIO_ST					6
#define VARIANT_PRIO_USER_EXTI			10
#define VARIANT_PRIO_PENDSV				255

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern UARTClass Serial;
extern UARTClass Serial1;
#endif


#endif /* __VARIANT_H__ */
