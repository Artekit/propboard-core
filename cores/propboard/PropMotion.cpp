/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropMotion.cpp

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

#define MMA8452_ADDR 0x38

PropMotion::PropMotion()
{
	fx = fy = fz = 0;
	_x = _y = _z = 0;
	saved_scale = 0;
	initialized = false;
	callback_params[0] = callback_params[1] = NULL;
	callbacks[0] = callbacks[1] = NULL;
}

PropMotion::~PropMotion()
{

}

bool PropMotion::begin(uint8_t scale, uint16_t odr, bool enable)
{
	uint8_t id;

	if (initialized)
		return true;

	// Configure interrupt pins
	pinMode(ACC_IRQ1, INPUT_PULLUP);
	pinMode(ACC_IRQ2, INPUT_PULLUP);

	if (!readRegister(MMA8452_WHO_AM_I, &id))
		return false;

	if (id != 0x2A)
		return false;

	if (!disable())
		return false;

	if (!setScale(scale))
		return false;

	if (!setDataRate(odr))
		return false;

	if (!maskSensorInterrupts())
		return false;

	if (enable && !this->enable())
		return false;

	initialized = true;
	return true;
}

void PropMotion::end()
{

}

bool PropMotion::enable()
{
	uint8_t reg;

	if (!readRegister(MMA8452_CTRL_REG1, &reg))
		return false;

	return writeRegister(MMA8452_CTRL_REG1, reg | 1);
}

bool PropMotion::disable()
{
	uint8_t reg;

	if (!readRegister(MMA8452_CTRL_REG1, &reg))
		return false;

	return writeRegister(MMA8452_CTRL_REG1, reg & ~1);
}

bool PropMotion::setScale(uint32_t scale)
{
	uint8_t reg;

	bool enabled = isEnabled();

	if (scale != 2 && scale != 4 && scale != 8)
		return false;

	if (enabled && !disable())
		return false;

	if (!readRegister(MMA8452_XYZ_DATA_CFG, &reg))
		return false;

	reg &= ~0x03;
	reg |= scale >> 2;

	if (!writeRegister(MMA8452_XYZ_DATA_CFG, reg))
		return false;

	/* Set high-res */
	if (!readRegister(MMA8452_CTRL_REG2, &reg))
		return false;

	reg &= ~0x03;
	reg |= 0x02;

	if (!writeRegister(MMA8452_CTRL_REG2, reg))
		return false;

	saved_scale = scale;

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::setDataRate(uint32_t data_rate)
{
	uint8_t reg;

	bool enabled = isEnabled();

	if (enabled && !disable())
		return false;

	if (!readRegister(MMA8452_CTRL_REG1, &reg))
		return false;

	reg &= ~0x38;

	switch (data_rate)
	{
		case 800: break;
		case 400: reg |= (1 << 3); break;
		case 200: reg |= (2 << 3); break;
		case 100: reg |= (3 << 3); break;
		case 50: reg |= (4 << 3); break;
		case 12: reg |= (5 << 3); break;
		case 6: reg |= (6 << 3); break;
		case 1: reg |= (7 << 3); break;

		default:
			return false;
	}

	if (!writeRegister(MMA8452_CTRL_REG1, reg))
		return false;

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::dataReady()
{
	uint8_t reg;

	if (!readRegister(MMA8452_STATUS, &reg))
		return false;

	// ZYXDR bit
	return (reg & 0x08);
}

bool PropMotion::maskSensorInterrupts()
{
	if (!writeRegister(MMA8452_CTRL_REG4, 0))
		return false;

	return true;
}

bool PropMotion::configDataReady(MotionInterrupt irq)
{
	bool enabled = isEnabled();

	if (enabled && !disable())
		return false;

	if (!enableSensorInterrupt(MMA8452_IRQ_DRDY, irq))
		return false;

	if (irq == MotionInterrupt1)
		EXTI_ClearITPendingBit(EXTI_Line7);
	else if (irq == MotionInterrupt2)
		EXTI_ClearITPendingBit(EXTI_Line6);

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::configAnyMotion(Axis axis, float force_g, uint32_t time, MotionInterrupt irq)
{
	uint8_t value = 0;
	uint8_t odr = 0;
	bool enabled;

	enabled = isEnabled();

	if (enabled && !disable())
		return false;

	// Enable high resolution
	if (!readRegister(MMA8452_CTRL_REG2, &value))
		return false;

	if ((value & 0x03) != 0x02)
	{
		value &= ~0x03;
		value |= 0x02;

		if (!writeRegister(MMA8452_CTRL_REG2, value))
			return false;
	}

	// Set axis (ZEFE, YEFE, XEFE) and motion detection (OAE)
	value = ((axis & 0x07) << 3) | 0x40;

	if (irq == MotionInterruptNone)
		// No irq chosen, set latch mode
		value |= 0x80;

	if (!writeRegister(MMA8452_FF_MT_CFG, value))
		return false;

	// Configure threshold
	value = (uint8_t) (force_g / 0.063f) & 0x7F;

	if (!writeRegister(MMA8452_FF_MT_THS, value))
		return false;

	// Get the current ODR
	if (!readRegister(MMA8452_CTRL_REG1, &odr))
		return false;

	odr = odr >> 4;

	// Configure time interval
	if (odr == 0)
	{
		// ODR = 800
		if (time > 319)
			time = 319;

		value = time / 1.25f;
	} else {

		if (time > 638)
			time = 638;

		value = time / 2.5f;
	}

	// Configure count
	if (!writeRegister(MMA8452_FF_MT_COUNT, value))
		return false;

	if (irq)
	{
		if (!enableSensorInterrupt(MMA8452_IRQ_FF_MT, irq))
			return false;
	} else {
		if (!disableSensorInterrupt(MMA8452_IRQ_FF_MT))
			return false;
	}

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::setLowNoiseMode(bool enable)
{
	uint8_t value = 0;

	if (!readRegister(MMA8452_CTRL_REG1, &value))
		return false;

	if (enable)
	{
		if (!(value & 0x04))
		{
			value |= 0x04;

			if (!writeRegister(MMA8452_CTRL_REG1, value))
				return false;
		}
	} else {
		if (value & 0x04)
		{
			value &= ~0x04;
			if (!writeRegister(MMA8452_CTRL_REG1, value))
				return false;
		}
	}

	return true;
}

bool PropMotion::setHighPassFilter(bool enable)
{
	uint8_t value = 0;

	if (!readRegister(MMA8452_XYZ_DATA_CFG, &value))
		return false;

	if (enable)
	{
		if (!(value & 0x10))
		{
			value |= 0x10;

			if (!writeRegister(MMA8452_XYZ_DATA_CFG, value))
				return false;
		}
	} else {
		if (value & 0x10)
		{
			value &= ~0x10;
			if (!writeRegister(MMA8452_XYZ_DATA_CFG, value))
				return false;
		}
	}

	return true;
}

bool PropMotion::configTransient(Axis axis, float force_g, uint32_t time, MotionInterrupt irq)
{
	uint8_t value = 0;
	uint8_t odr;
	bool enabled;

	if (irq > 2)
		return false;

	enabled = isEnabled();

	if (enabled && !disable())
		return false;

	// Enable high resolution
	if (!readRegister(MMA8452_CTRL_REG2, &value))
		return false;

	if ((value & 0x03) != 0x02)
	{
		value &= ~0x03;
		value |= 0x02;

		if (!writeRegister(MMA8452_CTRL_REG2, value))
			return false;
	}

	if (!readRegister(MMA8452_HP_FILTER_CUTOFF, &value))
		return false;

	value &= ~0x03;
	value |= 0x03;

	if (!writeRegister(MMA8452_HP_FILTER_CUTOFF, value))
		return false;

	// Configure XTEFE, YTEFE and ZTEFE with high-pass filter
	value = (axis & 0x07) << 1;

	if (irq == MotionInterruptNone)
		// No irq chosen, set latch mode
		value |= (1 << 4);

	if (!writeRegister(MMA8452_TRANSIENT_CFG, value))
		return false;

	// Configure threshold
	value = ((uint8_t) (force_g / 0.063f)) & 0x7F;

	if (!writeRegister(MMA8452_TRANSIENT_THS, value))
		return false;

	// Get the current ODR
	if (!readRegister(MMA8452_CTRL_REG1, &odr))
		return false;

	odr = odr >> 4;

	// Configure time interval
	if (odr == 0)
	{
		// ODR = 800
		if (time > 319)
			time = 319;

		value = time / 1.25f;
	} else {

		if (time > 638)
			time = 638;

		value = time / 2.5f;
	}

	if (!writeRegister(MMA8452_TRANSIENT_COUNT, value))
		return false;

	if (irq)
	{
		if (!enableSensorInterrupt(MMA8452_IRQ_TRANSIENT, irq))
			return false;
	} else {
		if (!disableSensorInterrupt(MMA8452_IRQ_TRANSIENT))
			return false;
	}

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::configPulse(Axis axis, float force_g, uint32_t time, uint32_t latency, MotionInterrupt irq)
{
	uint8_t value = 0;
	uint8_t odr;
	bool enabled;

	if (irq > 2)
		return false;

	enabled = isEnabled();

	if (enabled && !disable())
		return false;

	// Enable low-pass filter for pulse detection
	if (!readRegister(MMA8452_HP_FILTER_CUTOFF, &value))
		return false;

	if (!(value & 0x10))
	{
		value |= 0x10;
		if (!writeRegister(MMA8452_HP_FILTER_CUTOFF, value))
			return false;
	}

	// Enable high resolution
	if (!readRegister(MMA8452_CTRL_REG2, &value))
		return false;

	if ((value & 0x03) != 0x02)
	{
		value &= ~0x03;
		value |= 0x02;

		if (!writeRegister(MMA8452_CTRL_REG2, value))
			return false;
	}

	value = 0;

	// Set axis (ZSPEFE, YSPEFE, XSPEFE) and motion detection (OAE)
	if (axis & AxisX)
		value |= 1;

	if (axis & AxisY)
		value |= 1 << 2;

	if (axis & AxisZ)
		value |= 1 << 4;

	if (irq == 0)
		// No irq chosen, set latch mode
		value |= 0x40;

	if (!writeRegister(MMA8452_PULSE_CFG, value))
		return false;

	// Configure threshold
	value = (uint8_t) (force_g / 0.063f) & 0x7F;

	if (axis & AxisX)
	{
		if (!writeRegister(MMA8452_PULSE_THSX, value))
			return false;
	}

	if (axis & AxisY)
	{
		if (!writeRegister(MMA8452_PULSE_THSY, value))
			return false;
	}

	if (axis & AxisZ)
	{
		if (!writeRegister(MMA8452_PULSE_THSZ, value))
			return false;
	}

	// Get the current ODR
	if (!readRegister(MMA8452_CTRL_REG1, &odr))
		return false;

	odr = odr >> 4;

	// Configure time interval
	if (odr == 0)
	{
		// ODR = 800
		if (time > 319)
			time = 319;

		value = time / 1.25f;
	} else {

		if (time > 638)
			time = 638;

		value = time / 2.5f;
	}

	if (!writeRegister(MMA8452_PULSE_TMLT, value))
		return false;

	// Configure latency
	if (odr == 0)
	{
		// ODR = 800
		value = latency / 2.5f;
	} else {
		value = latency / 5;
	}

	if (!writeRegister(MMA8452_PULSE_LTCY, value))
		return false;

	if (irq)
	{
		if (!enableSensorInterrupt(MMA8452_IRQ_PULSE, irq))
			return false;
	} else {
		if (!disableSensorInterrupt(MMA8452_IRQ_PULSE))
			return false;
	}

	if (enabled)
		return enable();

	return true;
}

bool PropMotion::attachInterrupt(MotionInterrupt irq, MotionInterruptCallback* callback)
{
	uint32_t pin;

	switch (irq)
	{
		case MotionInterrupt1:
			pin = ACC_IRQ1;
			break;

		case MotionInterrupt2:
			pin = ACC_IRQ2;
			break;

		default:
			return false;
	}

	if (callback)
		::attachInterrupt(pin, callback, FALLING);
	else
		detachInterrupt(pin);

	return true;
}

bool PropMotion::attachInterruptWithParam(MotionInterrupt irq,
										  MotionInterruptCallbackWithParam* callback,
										  void* param)
{
	uint32_t pin;
	MotionInterruptCallbackWithParam** callback_ptr;
	void** callback_param_ptr;
	MotionInterruptCallback* stub_ptr;

	switch (irq)
	{
		case MotionInterrupt1:
			pin = ACC_IRQ1;
			callback_ptr = &callbacks[0];
			callback_param_ptr = &callback_params[0];
			stub_ptr = PropMotion::interrup1Stub;
			break;

		case MotionInterrupt2:
			pin = ACC_IRQ2;
			callback_ptr = &callbacks[1];
			callback_param_ptr = &callback_params[1];
			stub_ptr = PropMotion::interrup2Stub;
			break;

		default:
			return false;
	}

	if (!callback)
	{
		detachInterrupt(pin);
	} else {
		__disable_irq();
		*callback_ptr = callback;
		*callback_param_ptr = param;
		__enable_irq();

		::attachInterrupt(pin, stub_ptr, FALLING);
	}

	return true;
}

bool PropMotion::enableSensorInterrupt(uint8_t interrupt, MotionInterrupt irq_pin)
{
	uint8_t reg;

	// Configure on which pin of the sensor the interrupt will be signaled
	if (!readRegister(MMA8452_CTRL_REG5, &reg))
		return false;

	switch (irq_pin)
	{
		case MotionInterrupt1:
			reg |= interrupt;
			break;

		case MotionInterrupt2:
			reg &= ~interrupt;
			break;

		default:
			return false;
	}

	if (!writeRegister(MMA8452_CTRL_REG5, reg))
		return false;

	// Enable interrupt
	if (!readRegister(MMA8452_CTRL_REG4, &reg))
		return false;

	reg |= interrupt;

	if (!writeRegister(MMA8452_CTRL_REG4, reg))
		return false;

	return true;
}

bool PropMotion::disableSensorInterrupt(uint8_t interrupt)
{
	uint8_t reg;

	if (!readRegister(MMA8452_CTRL_REG4, &reg))
		return false;
	reg &= ~interrupt;
	return writeRegister(MMA8452_CTRL_REG4, reg);
}

MotionEvent PropMotion::getInterruptSource()
{
	uint8_t reg = 0;

	readRegister(MMA8452_INT_SOURCE, &reg);
	return reg;
}

uint8_t PropMotion::getAnyMotionSource()
{
	uint8_t reg;

	if (!readRegister(MMA8452_FF_MT_SRC, &reg))
		return 0;

	if (!(reg & MotionDetected))
		return 0;

	return reg;
}

uint8_t PropMotion::getTransientSource()
{
	uint8_t reg;

	if (!readRegister(MMA8452_TRANSIENT_SRC, &reg))
		return 0;

	if (!(reg & MotionTransientDetected))
		return 0;

	// Remove EA
	reg &= 0x3F;

	return reg;
}

uint8_t PropMotion::getPulseSource()
{
	uint8_t reg;

	if (!readRegister(MMA8452_PULSE_SRC, &reg))
		return 0;

	if (!(reg & MotionPulseDetected))
		return 0;

	// Remove EA and DPE
	reg &= 0x77;

	return reg;
}

bool PropMotion::isEnabled()
{
	uint8_t reg;

	if (!readRegister(MMA8452_CTRL_REG1, &reg))
		return false;

	return (reg & 0x01);
}

bool PropMotion::writeRegister(uint8_t reg, uint8_t value)
{
	return i2c1.writeRegister(MMA8452_ADDR, reg, 1, &value, 1);
}

bool PropMotion::readRegister(uint8_t reg, uint8_t* value)
{
	return i2c1.readRegister(MMA8452_ADDR, reg, 1, value, 1);
}

bool PropMotion::readRegisters(uint8_t reg, uint8_t* values, uint8_t len)
{
	return i2c1.readRegister(MMA8452_ADDR, reg, 1, values, len);
}

bool PropMotion::read()
{
	uint8_t values[6];

	if (!readRegisters(MMA8452_OUT_X_MSB, values, 6))
		return false;

	_x = ((int16_t) (values[0] << 8 | (values[1])) >> 4);
	_y = ((int16_t) (values[2] << 8 | (values[3])) >> 4);
	_z = ((int16_t) (values[4] << 8 | (values[5])) >> 4);

	if (saved_scale)
	{
		fx = _x * (float) saved_scale / (4096.0f / 2);
		fy = _y * (float) saved_scale / (4096.0f / 2);
		fz = _z * (float) saved_scale / (4096.0f / 2);
	}

	return true;
}

bool PropMotion::read(float* x, float* y, float* z)
{
	if (!x || !y || !z)
		return false;

	if (!read())
		return false;

	*x = fx;
	*y = fy;
	*z = fz;

	return true;
}

bool PropMotion::read(int16_t* x, int16_t* y, int16_t* z)
{
	if (!x || !y || !z)
		return false;

	if (!read())
		return false;

	*x = _x;
	*y = _y;
	*z = _z;

	return true;
}

void PropMotion::interrup1Stub()
{
	if (Motion.callbacks[0] != NULL)
	{
		Motion.callbacks[0](Motion.callback_params[0]);
	}
}

void PropMotion::interrup2Stub()
{
	if (Motion.callbacks[1] != NULL)
	{
		Motion.callbacks[1](Motion.callback_params[1]);
	}
}
