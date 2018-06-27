/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### PropMotion.h

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

#ifndef __PROPMOTION_H__
#define __PROPMOTION_H__

#include <Arduino.h>

#define MMA8452_STATUS				0x00
#define MMA8452_OUT_X_MSB			0x01
#define MMA8452_OUT_X_LSB			0x02
#define MMA8452_OUT_Y_MSB			0x03
#define MMA8452_OUT_Y_LSB			0x04
#define MMA8452_OUT_Z_MSB			0x05
#define MMA8452_OUT_Z_LSB			0x06
#define MMA8452_SYSMOD				0x0B
#define MMA8452_INT_SOURCE			0x0C
#define MMA8452_WHO_AM_I			0x0D
#define MMA8452_XYZ_DATA_CFG		0x0E
#define MMA8452_HP_FILTER_CUTOFF	0x0F
#define MMA8452_PL_STATUS			0x10
#define MMA8452_PL_CFG				0x11
#define MMA8452_PL_COUNT			0x12
#define MMA8452_PL_BF_ZCOMP			0x13
#define MMA8452_P_L_THS_REG			0x14
#define MMA8452_FF_MT_CFG			0x15
#define MMA8452_FF_MT_SRC			0x16
#define MMA8452_FF_MT_THS			0x17
#define MMA8452_FF_MT_COUNT			0x18
#define MMA8452_TRANSIENT_CFG		0x1D
#define MMA8452_TRANSIENT_SRC		0x1E
#define MMA8452_TRANSIENT_THS		0x1F
#define MMA8452_TRANSIENT_COUNT		0x20
#define MMA8452_PULSE_CFG			0x21
#define MMA8452_PULSE_SRC			0x22
#define MMA8452_PULSE_THSX			0x23
#define MMA8452_PULSE_THSY			0x24
#define MMA8452_PULSE_THSZ			0x25
#define MMA8452_PULSE_TMLT			0x26
#define MMA8452_PULSE_LTCY			0x27
#define MMA8452_PULSE_WIND			0x28
#define MMA8452_ASLP_COUNT			0x29
#define MMA8452_CTRL_REG1			0x2A
#define MMA8452_CTRL_REG2			0x2B
#define MMA8452_CTRL_REG3			0x2C
#define MMA8452_CTRL_REG4			0x2D
#define MMA8452_CTRL_REG5			0x2E
#define MMA8452_OFF_X				0x2F
#define MMA8452_OFF_Y				0x30
#define MMA8452_OFF_Z				0x31

#define MMA8452_IRQ_ASLP			0x80
#define MMA8452_IRQ_TRANSIENT		0x20
#define MMA8452_IRQ_LNDPRT			0x10
#define MMA8452_IRQ_PULSE			0x08
#define MMA8452_IRQ_FF_MT			0x04
#define MMA8452_IRQ_DRDY			0x01

#define AxisNone					0x00
#define	AxisX						0x01
#define	AxisY						0x02
#define	AxisZ						0x04
#define AxisAll 					(AxisX | AxisY | AxisZ)

#define	MotionEventNone				0x00
#define MotionEventDataReady		0x01
#define MotionEventAnyMotion		0x04
#define MotionEventPulse			0x08
#define MotionEventTransient		0x12

#define MotionNegativeX				0x01
#define MotionOnX					0x02
#define MotionNegativeY				0x04
#define MotionOnY					0x08
#define MotionNegativeZ				0x10
#define MotionOnZ					0x20
#define MotionDetected				0x40

#define MotionTransientNegativeX	0x01
#define MotionTransientOnX			0x02
#define MotionTransientNegativeY	0x04
#define MotionTransientOnY			0x08
#define MotionTransientNegativeZ	0x10
#define MotionTransientOnZ			0x20
#define MotionTransientDetected		0x40

#define MotionPulseNegativeX		0x01
#define MotionPulseNegativeY		0x02
#define MotionPulseNegativeZ		0x04
#define MotionDoublePulse			0x08
#define MotionPulseOnX				0x10
#define MotionPulseOnY				0x20
#define MotionPulseOnZ				0x40
#define MotionPulseDetected			0x80

typedef uint32_t MotionEvent;
typedef uint8_t Axis;

typedef enum
{
	MotionInterruptNone,
	MotionInterrupt1,
	MotionInterrupt2
} MotionInterrupt;

typedef void (MotionInterruptCallback)();
typedef void (MotionInterruptCallbackWithParam)(void*);

extern "C" void EXTI9_5_IRQHandler();

class PropMotion
{
	friend void EXTI9_5_IRQHandler();

public:
	~PropMotion();

	bool begin(uint8_t scale = 8, uint16_t odr = 200, bool enable = true);
	void end();
	bool enable();
	bool disable();
	bool setScale(uint32_t scale);
	bool setDataRate(uint32_t data_rate);
	bool setHighPassFilter(bool enable);
	bool setLowNoiseMode(bool enable);
	bool dataReady();

	bool configDataReady(MotionInterrupt irq);
	bool configAnyMotion(Axis axis, float force_g, uint32_t time, MotionInterrupt irq = MotionInterruptNone);
	bool configTransient(Axis axis, float force_g, uint32_t time, MotionInterrupt irq = MotionInterruptNone);
	bool configPulse(Axis axis, float force_g, uint32_t time, uint32_t latency, MotionInterrupt irq = MotionInterruptNone);

	// Interrupt handling
	bool attachInterrupt(MotionInterrupt irq, MotionInterruptCallback* callback);
	bool attachInterruptWithParam(MotionInterrupt irq, MotionInterruptCallbackWithParam* callback, void* param);
	MotionEvent getInterruptSource();

	// Event polling
	uint8_t getAnyMotionSource();
	uint8_t getTransientSource();
	uint8_t getPulseSource();

	bool isEnabled();

	// Data access
	bool writeRegister(uint8_t reg, uint8_t value);
	bool readRegister(uint8_t reg, uint8_t* value);
	bool readRegisters(uint8_t reg, uint8_t* values, uint8_t len);
	bool read(float* x, float* y, float* z);
	bool read(int16_t* x, int16_t* y, int16_t* z);

	static PropMotion& instance()
	{
		static PropMotion singleton;
		return singleton;
	}

private:
	PropMotion();

	bool read();
	bool maskSensorInterrupts();
	bool enableSensorInterrupt(uint8_t interrupt, MotionInterrupt irq_pin);
	bool disableSensorInterrupt(uint8_t interrupt);

	float fx, fy, fz;
	int16_t _x, _y, _z;
	uint32_t saved_scale;
	bool initialized;

	MotionInterruptCallbackWithParam* callbacks[2];
	void* callback_params[2];

	static void interrup1Stub();
	static void interrup2Stub();
};

extern PropMotion Motion;

#endif /* __PROPMOTION_H__ */
