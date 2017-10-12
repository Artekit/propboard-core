/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### isr.c

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
#include "Reset.h"

extern int sysTickHook(void);
extern void svcHook(void);

static void __halt()
{
	while (1);
}

void HardFault_Handler(void)
{
	__halt();
}

void MemManage_Handler(void)
{
	__halt();
}

void BusFault_Handler(void)
{
	__halt();
}

void UsageFault_Handler(void)
{
	__halt();
}

void SVC_Handler(void)
{
	svcHook();
}
