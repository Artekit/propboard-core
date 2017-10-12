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

#ifndef _UART_CLASS_
#define _UART_CLASS_

#include "HardwareSerial.h"
#include "RingBuffer.h"
#include <stm32f4xx.h>

#define SERIAL_8N1 UARTClass::Mode_8N1
#define SERIAL_8E1 UARTClass::Mode_8E1
#define SERIAL_8O1 UARTClass::Mode_8O1
#define SERIAL_8M1 UARTClass::Mode_8M1
#define SERIAL_8S1 UARTClass::Mode_8S1


class UARTClass : public HardwareSerial
{
  public:
    enum UARTModes {
      Mode_8N1 = 0,
      Mode_8E1 = 1,
      Mode_8O1 = 2,
	  Mode_9N1 = 3,
      Mode_9E1 = 4,
      Mode_9O1 = 5,
    };
    UARTClass(USART_TypeDef* pUart, IRQn_Type dwIrq, RingBuffer* pRx_buffer, RingBuffer* pTx_buffer);

    void begin(uint32_t dwBaudRate);
    void begin(uint32_t dwBaudRate, UARTModes config);
    void end();
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    void flush(void);
    size_t write(uint8_t c);
    using Print::write; // pull in write(str) and write(buf, size) from Print

    void setInterruptPriority(uint32_t priority);
    uint32_t getInterruptPriority();

    void IrqHandler(void);

    operator bool() { return true; }; // UART always active

  protected:
    void init(const uint32_t dwBaudRate, const uint32_t config);

    RingBuffer *_rx_buffer;
    RingBuffer *_tx_buffer;

    USART_TypeDef* _pUart;
    IRQn_Type _dwIrq;
    bool _initialized;
};

#endif // _UART_CLASS_
