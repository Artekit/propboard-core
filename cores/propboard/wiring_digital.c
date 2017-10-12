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

extern uint32_t pins_on_pwm;
extern void setTimerChannel(TIM_TypeDef* tim, uint8_t ch, uint8_t enable);

extern void pinMode(uint32_t ulPin, uint32_t ulMode)
{
	if (ulPin > PINS_COUNT)
		return;

	if (pins_on_pwm & (1 << ulPin))
	{
		setTimerChannel(g_APinDescription[ulPin].timer, g_APinDescription[ulPin].timer_ch, 0);
		pins_on_pwm &= ~(1 << ulPin);
	}

	g_APinDescription[ulPin].pin_port->PUPDR &= ~(0x03 << (g_APinDescription[ulPin].pin_num << 1));
	g_APinDescription[ulPin].pin_port->MODER &= ~(0x03 << (g_APinDescription[ulPin].pin_num << 1));

	switch (ulMode)
    {
        case INPUT:
        	break;

        case INPUT_PULLUP:
        	g_APinDescription[ulPin].pin_port->PUPDR |= 0x01 << (g_APinDescription[ulPin].pin_num << 1);
        	break;

        case INPUT_PULLDOWN:
        	g_APinDescription[ulPin].pin_port->PUPDR |= 0x02 << (g_APinDescription[ulPin].pin_num << 1);
           	break;

        case INPUT_ANALOG:
        	g_APinDescription[ulPin].pin_port->MODER |= 0x03 << (g_APinDescription[ulPin].pin_num << 1);
        	break;

        case OUTPUT:
        	g_APinDescription[ulPin].pin_port->MODER |= 0x01 << (g_APinDescription[ulPin].pin_num << 1);
        	break;

        default:
        	return;
    }
}

extern void digitalWrite(uint32_t ulPin, uint32_t ulVal)
{
	if (ulPin > PINS_COUNT)
		return;

	if ((g_APinDescription[ulPin].pin_port->MODER & (0x03 << (g_APinDescription[ulPin].pin_num << 1))) != 0)
	{
		// It's not an input, set/reset pin
		if (ulVal)
			g_APinDescription[ulPin].pin_port->BSRRL = g_APinDescription[ulPin].pin_mask;
		else
			g_APinDescription[ulPin].pin_port->BSRRH = g_APinDescription[ulPin].pin_mask;
	} else {
		// It's an input, enable or disable pull-up
		if (ulVal)
			g_APinDescription[ulPin].pin_port->PUPDR |= 0x01 << (g_APinDescription[ulPin].pin_num * 2);
		else
			g_APinDescription[ulPin].pin_port->PUPDR &= ~(0x03 << (g_APinDescription[ulPin].pin_num * 2));
	}
}

extern int digitalRead(uint32_t ulPin)
{
	return ((g_APinDescription[ulPin].pin_port->IDR & g_APinDescription[ulPin].pin_mask) != 0);
}

#ifdef __cplusplus
}
#endif
