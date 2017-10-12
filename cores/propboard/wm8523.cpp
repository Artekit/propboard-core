/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### wm8523.cpp

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

#include "wm8523.h"

#define WM8523_ADDRESS	0x34

#define WM8523_REG_DEVICE_ID		0x00
#define WM8523_REG_REVISION			0x01
#define WM8523_REG_PSCTRL1			0x02
#define WM8523_REG_AIF_CTRL1		0x03
#define WM8523_REG_AIF_CTRL2		0x04
#define WM8523_REG_DAC_CTRL3		0x05
#define WM8523_REG_DAC_GAINL		0x06
#define WM8523_REG_DAC_GAINR		0x07
#define WM8523_REG_ZERO_DETECT		0x08

static bool wm8523ReadRegister(uint8_t reg, uint16_t* value)
{
	if (i2c1.readRegister(WM8523_ADDRESS, reg, 1, (uint8_t*) value, 2))
	{
		*value = __REV16(*value);
		return true;
	}

	return false;
}

static bool wm8523WriteRegister(uint8_t reg, uint16_t value, bool verify = true)
{
	uint16_t read_value;
	uint16_t rev_value = __REV16(value);

	if (!i2c1.writeRegister(WM8523_ADDRESS, reg, 1, (uint8_t*) &rev_value, 2))
		return false;
	
	if (verify)
	{
		if (!wm8523ReadRegister(reg, &read_value) || read_value != value)
			return false;			
	}
	
	return true;
}

bool wm8523Verify()
{
	uint16_t value;

	// Read ID
	if (!wm8523ReadRegister(WM8523_REG_DEVICE_ID, &value) || value != 0x8523)
		return false;
	
	return true;
}

bool wm8523SetPower(uint16_t power_mode)
{
	if (power_mode > WM8523_POWER_UNMUTE)
		return false;
	
	return wm8523WriteRegister(WM8523_REG_PSCTRL1, power_mode);
}

bool wm8523SoftReset()
{
	return wm8523WriteRegister(WM8523_REG_DEVICE_ID, 0, false);
}

bool wm8523SetDeEmpahsis(bool on)
{
	uint16_t value;

	// Read ID
	if (!wm8523ReadRegister(WM8523_REG_AIF_CTRL1, &value))
		return false;

	if (on)
		value |= 1 << 8;
	else
		value &= ~(1 << 8);

	if (!wm8523WriteRegister(WM8523_REG_AIF_CTRL1, value))
		return false;
	
	return true;
}

bool wm8523Config(WM8523_CONFIG* config)
{
	uint16_t value;
	
	// Configure AIF_CTRL1
	value = config->de_emphasis << 8;
	value |= config->mode << 7;
	
	if (config->if_format == WM8523_IF_DSP)
		value |= config->dsp_mode << 6;
	else
		value |= config->lrclk_polarity << 6;
	
	value |= config->bclk_pol << 5;
	
	switch (config->word_lenght)
	{
		case 16: break;
		case 20: value |= 1 << 3; break;
		case 24: value |= 2 << 3; break;
		case 32: value |= 3 << 3; break;
		default:
			return false;
	}
	
	value |= config->if_format;
	
	if (!wm8523WriteRegister(WM8523_REG_AIF_CTRL1, value))
		return false;
	
	// Configure AIF_CTRL2
	value = config->dac_op << 6;
	
	if (config->mode == WM8523_MODE_MASTER)
	{
		value |= config->bclk_div << 3;
	}
	
	value |= config->mclock_ratio;
	
	if (!wm8523WriteRegister(WM8523_REG_AIF_CTRL2, value))
		return false;
	
	// Configure DAC_CTRL3
	value = config->vol_up_ramp << 1;
	value |= config->vol_down_ramp;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_CTRL3, value))
		return false;
	
	return true;
}

bool wm8523Init(void)
{
	WM8523_CONFIG config;
	
	// Reset
	if (!wm8523SoftReset())
		return false;

	// Check chip-id
	if (!wm8523Verify())
		return false;

	config.mode 			= WM8523_MODE_SLAVE;
	config.lrclk_polarity 	= WM8523_NORMAL_POLARITY;
	config.if_format 		= WM8523_IF_I2S;
	config.dac_op			= WM8523_DAC_OP_STEREO;
	config.mclock_ratio		= WM8523_MCLOCK_RATIO_256FS;
	config.bclk_pol			= WM8523_BCLK_RISING_EDGE;
	config.vol_down_ramp	= 1;
	config.vol_up_ramp		= 1;
	config.de_emphasis 		= 0;
	config.word_lenght 		= 32;
	
	if (!wm8523Config(&config))
		return false;
	
	// Set volume to 0dB
	if (!wm8523SetVolume(0))
		return false;
	
	// Power up and unmute
	return wm8523SetPower(WM8523_POWER_UNMUTE);
}

bool wm8523SetVolume(float db)
{
	uint16_t value;
	
	// 0 = -100.00 dB
	// 400 = 0.00 dB
	// 448 = 12 dB
	
	if (db > 12)
		db = 12;
	
	if (db < -100)
		db = -100;
	
	if (db < 0)
	{
		value = 400 - (db / -0.25f);
	} else if (db > 0)
	{
		value = (db / 0.25f) + 400;
	} else {
		value = 400;
	}
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINL, value))
		return false;
	
	value |= 1 << 9;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINR, value, false))
		return false;
	
	return true;
}

bool wm8523VolumeStepUp()
{
	uint16_t value;

	if (!wm8523ReadRegister(WM8523_REG_DAC_GAINL, &value))
		return false;
	
	if (value == 448)	// 448 = 12 dB
		return true;
	
	value--;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINL, value))
		return false;
	
	value |= 1 << 9;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINR, value))
		return false;
	
	return true;
}

bool wm8523VolumeStepDown()
{
	uint16_t value;

	if (!wm8523ReadRegister(WM8523_REG_DAC_GAINL, &value))
		return false;
	
	if (value == 0)	//  = -100 dB
		return true;
	
	value++;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINL, value))
		return false;
	
	value |= 1 << 9;
	
	if (!wm8523WriteRegister(WM8523_REG_DAC_GAINR, value))
		return false;
	
	return true;
}
