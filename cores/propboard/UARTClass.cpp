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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "UARTClass.h"
#include <variant.h>

extern uint32_t pins_on_pwm;
extern "C" void setTimerChannel(TIM_TypeDef* tim, uint8_t ch, uint8_t enable);

// Constructors ////////////////////////////////////////////////////////////////

UARTClass::UARTClass(USART_TypeDef* pUart, IRQn_Type dwIrq, RingBuffer* pRx_buffer, RingBuffer* pTx_buffer)
{
	_rx_buffer = pRx_buffer;
	_tx_buffer = pTx_buffer;

	_pUart=pUart;
	_dwIrq=dwIrq;

	_initialized = false;
}

// Public Methods //////////////////////////////////////////////////////////////

void UARTClass::begin(uint32_t dwBaudRate)
{
	begin(dwBaudRate, Mode_8N1);
}

void UARTClass::begin(uint32_t dwBaudRate, UARTModes config)
{
	init(dwBaudRate, config);
}

void UARTClass::init(uint32_t dwBaudRate, uint32_t modeReg)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	USART_InitTypeDef USART_InitStructure;
	uint16_t parity;

	if (modeReg == Mode_8N1 || Mode_9N1)
		parity = USART_Parity_No;
	else if (modeReg == Mode_8O1 || Mode_9O1)
		parity = USART_Parity_Odd;
	else if (modeReg == Mode_8E1 || Mode_9E1)
		parity = USART_Parity_Even;
	else return;

	USART_InitStructure.USART_BaudRate = dwBaudRate;
	USART_InitStructure.USART_WordLength = (modeReg <= Mode_8O1) ? USART_WordLength_8b : USART_WordLength_9b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = parity;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	/* Enable the clock to the selected UART */
	if (_pUart == USART1)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

		GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
		GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
		GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
		GPIO_Init(GPIOA, &GPIO_InitStruct);

		USART_Init(USART1, &USART_InitStructure);
	}
	else if (_pUart == USART6)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

		// Check if pin 1 is on PWM
		if (pins_on_pwm & 2)
		{
			setTimerChannel(g_APinDescription[1].timer, g_APinDescription[1].timer_ch, 0);
			pins_on_pwm &= ~2;
		}

		GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_USART6);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource11, GPIO_AF_USART6);

		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_7;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
		GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
		GPIO_InitStruct.GPIO_Speed = GPIO_Low_Speed;
		GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
		GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
	else return;

	USART_Init(_pUart, &USART_InitStructure);

	// Make sure both ring buffers are initialized back to empty.
	_rx_buffer->_iHead = _rx_buffer->_iTail = 0;
	_tx_buffer->_iHead = _tx_buffer->_iTail = 0;
	
	/* Enable RX interrupt */
	USART_ITConfig(_pUart, USART_IT_RXNE, ENABLE);
	setInterruptPriority(5);
	NVIC_SetPriority(_dwIrq, VARIANT_PRIO_UART);
    NVIC_EnableIRQ(_dwIrq);

    /* Enable */
    USART_Cmd(_pUart, ENABLE);

    _initialized = true;
}

void UARTClass::end()
{
	if (_initialized)
	{
		_initialized = false;

		// Clear any received data
		_rx_buffer->_iHead = _rx_buffer->_iTail;

		// Wait for any outstanding data to be sent
		flush();

		USART_Cmd(_pUart, DISABLE);

		if (_pUart == USART1)
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);
		else if (_pUart == USART6)
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);

		// Disable UART interrupt in NVIC
		NVIC_DisableIRQ( _dwIrq );
	}
}

void UARTClass::setInterruptPriority(uint32_t priority)
{
	NVIC_SetPriority(_dwIrq, priority & 0x0F);
}

uint32_t UARTClass::getInterruptPriority()
{
	return NVIC_GetPriority(_dwIrq);
}

int UARTClass::available( void )
{
	return (uint32_t)(SERIAL_BUFFER_SIZE + _rx_buffer->_iHead - _rx_buffer->_iTail) % SERIAL_BUFFER_SIZE;
}

int UARTClass::availableForWrite(void)
{
	int head = _tx_buffer->_iHead;
	int tail = _tx_buffer->_iTail;
	if (head >= tail) return SERIAL_BUFFER_SIZE - 1 - head + tail;
	return tail - head - 1;
}

int UARTClass::peek( void )
{
	if ( _rx_buffer->_iHead == _rx_buffer->_iTail )
		return -1;

	return _rx_buffer->_aucBuffer[_rx_buffer->_iTail];
}

int UARTClass::read( void )
{
	// if the head isn't ahead of the tail, we don't have any characters
	if ( _rx_buffer->_iHead == _rx_buffer->_iTail )
		return -1;

	uint8_t uc = _rx_buffer->_aucBuffer[_rx_buffer->_iTail];
	_rx_buffer->_iTail = (unsigned int)(_rx_buffer->_iTail + 1) % SERIAL_BUFFER_SIZE;
	return uc;
}

void UARTClass::flush( void )
{
	if (!_initialized)
		return;

	while (_tx_buffer->_iHead != _tx_buffer->_iTail); //wait for transmit data to be sent

	// Wait for transmission to complete
	while (!(_pUart->SR & USART_SR_TC));
}

size_t UARTClass::write( uint8_t uc_data )
{
	if (!_initialized)
		return 0;

	// Is the hardware currently busy?
	if ((!(_pUart->SR & USART_SR_TC)) || (_tx_buffer->_iTail != _tx_buffer->_iHead))
	{
		// If busy we buffer
		unsigned int l = (_tx_buffer->_iHead + 1) % SERIAL_BUFFER_SIZE;
		while (_tx_buffer->_iTail == (int32_t) l)
			; // Spin locks if we're about to overwrite the buffer. This continues once the data is sent

		_tx_buffer->_aucBuffer[_tx_buffer->_iHead] = uc_data;
		_tx_buffer->_iHead = l;
		// Make sure TX interrupt is enabled
		USART_ITConfig(_pUart, USART_IT_TXE, ENABLE);
	} else {
		// Bypass buffering and send character directly
		_pUart->DR = uc_data;
	}

	return 1;
}

void UARTClass::IrqHandler( void )
{
	uint8_t c;
	
	if (USART_GetFlagStatus(_pUart, USART_FLAG_RXNE))
	{
		c = _pUart->DR;
		_rx_buffer->store_char(c);
	}

	if (USART_GetFlagStatus(_pUart, USART_FLAG_TXE))
	{
		if (_tx_buffer->_iTail != _tx_buffer->_iHead)
		{
			_pUart->DR = _tx_buffer->_aucBuffer[_tx_buffer->_iTail];
			 _tx_buffer->_iTail = (unsigned int)(_tx_buffer->_iTail + 1) % SERIAL_BUFFER_SIZE;
		} else {
			// Mask off transmit interrupt so we don't get it anymore
			USART_ITConfig(_pUart, USART_IT_TXE, DISABLE);
		}
	}
}
