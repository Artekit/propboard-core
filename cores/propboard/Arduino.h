/*
  Arduino.h - Main include file for the Arduino SDK
  Copyright (c) 2005-2013 Arduino Team.  All right reserved.
  Copyright (c) 2017 Artekit Labs

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef Arduino_h
#define Arduino_h

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// some libraries and sketches depend on this
// AVR stuff, assuming Arduino.h or WProgram.h
// automatically includes it...
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "binary.h"
#include "itoa.h"

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#include <chip.h>
#include "wiring_constants.h"

#define UNUSED(x) (void)(x)

#define VARIANT_MCK	84000000

extern uint32_t GetTickCount();

#define clockCyclesPerMicrosecond() ( SystemCoreClock / 1000000L )
#define clockCyclesToMicroseconds(a) ( ((a) * 1000L) / (SystemCoreClock / 1000L) )
#define microsecondsToClockCycles(a) ( (a) * (SystemCoreClock / 1000000L) )

void yield(void);

/* sketch */
extern void setup( void ) ;
extern void loop( void ) ;

#define NIRQ ((uint8_t) -1)		// Not an interrupt

typedef void (*voidFuncPtr)( void ) ;

/* Define attribute */
#if defined   ( __CC_ARM   ) /* Keil uVision 4 */
    #define WEAK (__attribute__ ((weak)))
#elif defined ( __ICCARM__ ) /* IAR Ewarm 5.41+ */
    #define WEAK __weak
#elif defined (  __GNUC__  ) /* GCC CS */
    #define WEAK __attribute__ ((weak))
#endif

typedef struct _PinDescription
{
	GPIO_TypeDef* pin_port;
	uint32_t pin_num;
	uint32_t pin_mask;
	ADC_TypeDef* adc;
	uint8_t adc_ch;
	TIM_TypeDef* timer;
	uint8_t timer_ch;
	uint8_t irq_num;
	uint8_t irq_port_num;
	uint8_t has_peripheral;
} PinDescription ;

/* Pins table to be instantiated into variant.cpp */
extern const PinDescription g_APinDescription[] ;

#ifdef __cplusplus
} // extern "C"

#include "WCharacter.h"
#include "WString.h"
// #include "Tone.h"
#include "WMath.h"
#include "HardwareSerial.h"
// #include "wiring_pulse.h"

// PropBoard specifics
#include "i2c.h"
#include "PropAudio.h"
#include "HBLED.h"
#include "RawChainPlayer.h"
#include "WavChainPlayer.h"
#include "WavPlayer.h"
#include "PropMotion.h"
#include "wm8523.h"
#include "sdcard.h"
#include "PropPower.h"

#endif // __cplusplus

// Include board variant
#include <variant.h>
#include <ff.h>
#include "wiring.h"
#include "wiring_digital.h"
#include "wiring_analog.h"
#include "wiring_shift.h"
#include "WInterrupts.h"

#endif // Arduino_h
