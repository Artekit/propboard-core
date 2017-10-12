/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### wm8523.h

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

#ifndef __WM8523_H__
#define __WM8523_H__

#include <Arduino.h>

#define WM8523_POWER_OFF			0x00
#define WM8523_POWER_DOWN			0x01
#define WM8523_POWER_MUTE			0x02
#define WM8523_POWER_UNMUTE			0x03

#define WM8523_MODE_SLAVE			0x00
#define WM8523_MODE_MASTER			0x01

#define WM8523_NORMAL_POLARITY		0x00
#define WM8523_INVERTED_POLARITY	0x01
#define WM8523_BCLK_RISING_EDGE		0x00
#define WM8523_BCLK_FALLING_EDGE	0x01

#define WM8523_IF_RIGHT_JUSTIFIED	0x00
#define WM8523_IF_LEFT_JUSTIFIED	0x01
#define WM8523_IF_I2S				0x02
#define WM8523_IF_DSP				0x03

#define WM8523_IF_DSP_MODE_A		0x00
#define WM8523_IF_DSP_MODE_B		0x01

#define WM8523_DAC_OP_STEREO		0x00
#define WM8523_DAC_OP_MONO_LEFT		0x01
#define WM8523_DAC_OP_MONO_RIGHT	0x02
#define WM8523_DAC_OP_MONO_MIX		0x03

#define WM8523_MCLOCK_RATIO_AUTO	0x00
#define WM8523_MCLOCK_RATIO_128FS	0x01
#define WM8523_MCLOCK_RATIO_192FS	0x02
#define WM8523_MCLOCK_RATIO_256FS	0x03
#define WM8523_MCLOCK_RATIO_384FS	0x04
#define WM8523_MCLOCK_RATIO_512FS	0x05
#define WM8523_MCLOCK_RATIO_768FS	0x06
#define WM8523_MCLOCK_RATIO_1152FS	0x07

#define WM8523_BCLKDIV_MCLK_8		0x00
#define WM8523_BCLKDIV_MCLK_4		0x01
#define WM8523_BCLKDIV_32FS			0x02
#define WM8523_BCLKDIV_64FS			0x03
#define WM8523_BCLKDIV_128FS		0x04

typedef struct _wm8523_config
{
	uint8_t mode;				/* Master, slave.
								 *
								 * WM8523_MODE_SLAVE
								 * WM8523_MODE_MASTER
								 */

	uint8_t lrclk_polarity;		/* Normal, inverted.
								 *
								 * WM8523_NORMAL_POLARITY
								 * WM8523_INVERTED_POLARITY
								 */

	uint8_t dsp_mode;			/* A - B, use when if_format = WM8523_IF_DSP.
								 *
								 * WM8523_IF_DSP_MODE_A
								 * WM8523_IF_DSP_MODE_B
								 */
	
	uint8_t de_emphasis;		/* 1 = enabled, 0 = disabled */

	uint8_t word_lenght;		/* 16, 20, 24 or 32 (bits). */

	uint8_t if_format;			/* Right-left justified, I2S, DSP.
								 *
								 * WM8523_IF_RIGHT_JUSTIFIED
								 * WM8523_IF_LEFT_JUSTIFIED
								 * WM8523_IF_I2S
								 * WM8523_IF_DSP
								 */

	uint8_t dac_op;				/* Stereo, mono (L/R), monomix.
								 * WM8523_DAC_OP_STEREO
								 * WM8523_DAC_OP_MONO_LEFT
								 * WM8523_DAC_OP_MONO_RIGHT
								 * WM8523_DAC_OP_MONO_MIX
								 */

	uint8_t mclock_ratio;		/* Auto, 128 to 1152 FS.
								 *
								 * WM8523_MCLOCK_RATIO_AUTO
								 * WM8523_MCLOCK_RATIO_128FS
								 * WM8523_MCLOCK_RATIO_192FS
								 * WM8523_MCLOCK_RATIO_256FS
								 * WM8523_MCLOCK_RATIO_384FS
								 * WM8523_MCLOCK_RATIO_512FS
								 * WM8523_MCLOCK_RATIO_768FS
								 * WM8523_MCLOCK_RATIO_1152FS
								 */

	uint8_t bclk_pol;			/* BCLK polarity, when mode = WM8523_MODE_MASTER.
								 *
								 * WM8523_NORMAL_POLARITY
								 * WM8523_INVERTED_POLARITY
								 *
								 * BCLK edge, when mode = WM8523_MODE_SLAVE
								 *
								 * WM8523_BCLK_RISING_EDGE
								 * WM8523_BCLK_FALLING_EDGE							
								 */

	uint8_t bclk_div;			/* BCLK div. Use when mode = WM8523_MODE_MASTER
								 *
								 * WM8523_BCLKDIV_MCLK_8
								 * WM8523_BCLKDIV_MCLK_4
								 * WM8523_BCLKDIV_32FS
								 * WM8523_BCLKDIV_64FS
								 * WM8523_BCLKDIV_128FS
								 */

	uint8_t vol_up_ramp;		/* Volume-up ramp. 1 = enabled, 0 = disabled */

	uint8_t vol_down_ramp;		/* Volume-dowm ramp. 1 = enabled, 0 = disabled */
	
} WM8523_CONFIG;

bool wm8523Init(void);
bool wm8523SetPower(uint16_t power_mode);
bool wm8523Verify();
bool wm8523SoftReset();
bool wm8523Config(WM8523_CONFIG* config);
bool wm8523SetVolume(float db);
bool wm8523VolumeStepUp();
bool wm8523VolumeStepDown();
bool wm8523SetDeEmpahsis(bool on);

#endif /* __WM8523_H__ */
