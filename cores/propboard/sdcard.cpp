/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### sdcard.cpp

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

#include <sdcard.h>

/* SD Commands */
#define CMD_GO_IDLE_STATE					0
#define CMD_SEND_OP_COND					1
#define CMD_ALL_SEND_CID					2
#define CMD_SET_REL_ADDR					3
#define CMD_SET_DSR							4
#define CMD_SDIO_SEN_OP_COND				5
#define CMD_HS_SWITCH						6
#define CMD_SEL_DESEL_CARD					7
#define CMD_HS_SEND_EXT_CSD					8
#define CMD_SEND_CSD						9
#define CMD_SEND_CID						10
#define CMD_READ_DAT_UNTIL_STOP				11
#define CMD_STOP_TRANSMISSION				12
#define CMD_SEND_STATUS						13
#define CMD_HS_BUSTEST_READ					14
#define CMD_GO_INACTIVE_STATE				15
#define CMD_SET_BLOCKLEN					16
#define CMD_READ_SINGLE_BLOCK				17
#define CMD_READ_MULT_BLOCK					18
#define CMD_HS_BUSTEST_WRITE				19
#define CMD_WRITE_DAT_UNTIL_STOP			20
#define CMD_SET_BLOCK_COUNT					23
#define CMD_WRITE_SINGLE_BLOCK				24
#define CMD_WRITE_MULT_BLOCK				25
#define CMD_PROG_CID						26
#define CMD_PROG_CSD						27
#define CMD_SET_WRITE_PROT					28
#define CMD_CLR_WRITE_PROT					29
#define CMD_SEND_WRITE_PROT					30
#define CMD_SD_ERASE_GRP_START				32
#define CMD_SD_ERASE_GRP_END				33
#define CMD_ERASE_GRP_START					35
#define CMD_ERASE_GRP_END					36
#define CMD_ERASE							38
#define CMD_FAST_IO							39
#define CMD_GO_IRQ_STATE					40
#define CMD_LOCK_UNLOCK						42
#define CMD_APP_CMD							55
#define CMD_GEN_CMD							56
#define CMD_NO_CMD							64

/* SD Card Specific commands */
#define CMD_APP_SD_SET_BUSWIDTH				6
#define CMD_SD_APP_STATUS					13
#define CMD_SD_APP_SEND_NUM_WRITE_BLOCKS	22
#define CMD_SD_APP_OP_COND					41
#define CMD_SD_APP_SET_CLR_CARD_DETECT		42
#define CMD_SD_APP_SEND_SCR					51
#define CMD_SDIO_RW_DIRECT					52
#define CMD_SDIO_RW_EXTENDED				53

/* SD Card Specific security commands */
#define CMD_APP_GET_MKB						43
#define CMD_APP_GET_MID						44
#define CMD_APP_SET_CER_RN1					45
#define CMD_APP_GET_CER_RN2					46
#define CMD_APP_SET_CER_RES2				47
#define CMD_APP_GET_CER_RES1				48
#define CMD_APP_SECURE_READ_MULTIPLE_BLOCK	18
#define CMD_APP_SECURE_WRITE_MULTIPLE_BLOCK	25
#define CMD_APP_SECURE_ERASE				38
#define CMD_APP_CHANGE_SECURE_AREA			49
#define CMD_APP_SECURE_WRITE_MKB			48

/* OCR register status flags */
#define OCR_AKE_SEQ_ERROR					0x00000008
#define OCR_ERASE_RESET						0x00002000
#define OCR_CARD_ECC_DISABLED				0x00004000
#define OCR_WP_ERASE_SKIP					0x00008000
#define OCR_CID_CSD_OVERWRITE				0x00010000
#define OCR_STREAM_WRITE_OVERRUN			0x00020000
#define OCR_STREAM_READ_UNDERRUN			0x00040000
#define OCR_GENERAL_UNKNOWN_ERROR			0x00080000
#define OCR_CC_ERROR						0x00100000
#define OCR_CARD_ECC_FAILED					0x00200000
#define OCR_ILL_CMD							0x00400000
#define OCR_COM_CRC_FAILED					0x00800000
#define OCR_LOCK_UNLOCK_FAILED				0x01000000
#define OCR_WRITE_PROT						0x04000000
#define OCR_BAD_ERASE_PARAM					0x08000000
#define OCR_ERASE_SEQ_ERR					0x10000000
#define OCR_BLOCK_LEN_ERR					0x20000000
#define OCR_ADDR_MISALIGNED					0x40000000
#define OCR_ADDR_OUT_OF_RANGE				0x80000000
#define OCR_ERROR_MASK						0xFDFFE008

/* CMD8 Pattern */
#define SD_CMD8_PATTERN						0x000001AA

/* CMD6 Error bits */
#define R6_UNKNOWN_ERROR					0x00002000
#define R6_ILL_CMD							0x00004000
#define R6_CMD_CRC_FAIL						0x00008000
#define R6_ERROR_MASK						0x0000E000

/* SD states */
#define SD_STATE_READY						1
#define SD_STATE_ID							2
#define SD_STATE_STANDBY					3
#define SD_STATE_TRANSFER					4
#define SD_STATE_SENDING					5
#define SD_STATE_RECEIVING					6
#define SD_STATE_PROGRAMMING				7
#define SD_STATE_DISCONNECTED				8
#define SD_STATE_ERROR						0xFF

static SD_CARD_INFO card_info;
static volatile uint32_t sd_busy_count = 0;
static volatile bool sd_sdio_transfer_complete = false;
static volatile bool sd_transfer_error = false;
static volatile bool sd_cmd_irq_flag = false;
static volatile SD_Status sd_transfer_error_code = SD_NO_ERROR;

#if SD_STATS
uint32_t sd_errors = 0;
uint32_t sd_timeouts = 0;
float sd_avg_read_time = 0;
float sd_avg_write_time = 0;
uint32_t sd_max_read_time = 0;
uint32_t sd_max_write_time = 0;
uint32_t sd_max_retries = 0;
uint32_t sd_sectors_read = 0;
uint32_t sd_sectors_written = 0;
volatile uint32_t sd_sta = 0;
#endif // SD_STATS

#define SDIO_MODE_INITIALIZATION			1
#define SDIO_MODE_LOW_SPEED					2
#define SDIO_MODE_HIGH_SPEED				3

#define DISABLE_STATIC_IRQ()	(SDIO->MASK &= 0x5FF) //~0xC003FF)
#define CLEAR_STATIC_IRQ()		(SDIO->ICR = 0x5FF) //0xC003FF)

#define SDIO_CMD_MASK			0x1F

#define SDIO_RETRIES			10

static UARTClass* uart_debug = NULL;
static volatile bool debug_enabled = false;

static SD_Status sdTransferBlocksWithDMA(uint32_t sector, const uint8_t* buffer, uint32_t count, bool write) __attribute__ ((optimize(3)));
static SD_Status sdGetR1Response(uint8_t cmd) __attribute__ ((optimize(3)));

void enableSdDebug(UARTClass* uart)
{
	uart_debug = uart;
	debug_enabled = true;
}

void disableSdDebug()
{
	debug_enabled = false;
	uart_debug = NULL;
}

#if SD_DEBUG
static const char* getErrorString(SD_Status code)
{
	switch (code)
	{
		case SD_DATA_CRC_FAIL: return "SD_DATA_CRC_FAIL";
		case SD_CMD_TIMEOUT: return "SD_CMD_TIMEOUT";
		case SD_DATA_TIMEOUT: return "SD_DATA_TIMEOUT";
		case SD_TX_UNDERRUN: return "SD_TX_UNDERRUN";
		case SD_RX_OVERRUN: return "SD_RX_OVERRUN";
		case SD_AKE_SEQ_ERROR: return "SD_AKE_SEQ_ERROR";
		case SD_ERASE_RESET: return "SD_ERASE_RESET";
		case SD_CARD_ECC_DISABLED: return "SD_CARD_ECC_DISABLED";
		case SD_WP_ERASE_SKIP: return "SD_WP_ERASE_SKIP";
		case SD_CID_CSD_OVERWRITE: return "SD_CID_CSD_OVERWRITE";
		case SD_STREAM_WRITE_OVERRUN: return "SD_STREAM_WRITE_OVERRUN";
		case SD_STREAM_READ_UNDERRUN: return "SD_STREAM_READ_UNDERRUN";
		case SD_GENERAL_UNKNOWN_ERROR: return "SD_GENERAL_UNKNOWN_ERROR";
		case SD_CC_ERROR: return "SD_CC_ERROR";
		case SD_CARD_ECC_FAILED: return "SD_CARD_ECC_FAILED";
		case SD_ILL_CMD: return "SD_ILL_CMD";
		case SD_CMD_CRC_FAIL: return "SD_CMD_CRC_FAIL";
		case SD_LOCK_UNLOCK_FAILED: return "SD_LOCK_UNLOCK_FAILED";
		case SD_WRITE_PROT: return "SD_WRITE_PROT";
		case SD_BAD_ERASE_PARAM: return "SD_BAD_ERASE_PARAM";
		case SD_ERASE_SEQ_ERR: return "SD_ERASE_SEQ_ERR";
		case SD_BLOCK_LEN_ERR: return "SD_BLOCK_LEN_ERR";
		case SD_ADDR_MISALIGNED: return "SD_ADDR_MISALIGNED";
		case SD_ADDR_OUT_OF_RANGE: return "SD_ADDR_OUT_OF_RANGE";
		case SD_INVALID_VOLTAGE: return "SD_INVALID_VOLTAGE";
		case SD_NOT_SUPPORTED: return "SD_NOT_SUPPORTED";
		case SD_NOT_POWERED: return "SD_NOT_POWERED";
		case SD_NOT_PRESENT: return "SD_NOT_PRESENT";
		case SD_START_BIT_ERROR: return "SD_START_BIT_ERROR";
		case SD_FIFO_ERROR: return "SD_FIFO_ERROR";
		case SD_BUSY: return "SD_BUSY";
		case SD_ERROR: return "SD_ERROR";
		case SD_NO_ERROR: return "SD_NO_ERROR";
	}

	return "Unknown error code";
}

static void printError(const char* function, SD_Status code, const char* extra)
{
	if (!debug_enabled)
		return;

	uart_debug->print("SdDebug: error \"");
	uart_debug->print(getErrorString(code));
	uart_debug->print("\" in function ");
	if (extra)
	{
		uart_debug->print(function);
		uart_debug->print(" [");
		uart_debug->print(extra);
		uart_debug->println("]");
	} else {
		uart_debug->println(function);
	}
}

static void printErrorRWOp(bool write, SD_Status code, uint32_t sector, uint32_t count, uint32_t retries, const char* msg)
{
	if (!debug_enabled)
		return;

	uart_debug->print("SdDebug: error \"");
	uart_debug->print(getErrorString(code));
	if (write)
		uart_debug->print("\" in write operation ");
	else
		uart_debug->print("\" in read operation ");

	uart_debug->print("Sector: ");
	uart_debug->print(sector);
	uart_debug->print(" count: ");
	uart_debug->print(count);

	if (msg)
	{
		uart_debug->print(" [");
		uart_debug->print(msg);
		uart_debug->print("]");
	}

	uart_debug->print(" Retries left: ");
	uart_debug->println(retries);
}

#else
#define printError(a, b, c)
#define printErrorRWOp(a, b, c, d, f, g)
#endif

static bool sdLock()
{
	if (sd_busy_count)
		return false;

	sd_busy_count++;
	return true;
}

static void sdUnlock()
{
	__disable_irq();

	if (sd_busy_count)
		sd_busy_count--;

	if (Audio.updatePending())
		Activate_PendSV();

	__enable_irq();
}

static void sdInitializePins(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	SYSCFG_CompensationCellCmd(ENABLE);
	while (SYSCFG_GetCompensationCellStatus() != SET);

	// SDIO_D0 = PC8, SDIO_D1 = PC9, SDIO_D2 = PC10
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_Fast_Speed;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	// SDIO_CK = PC12
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	// SDIO_CMD = PD2
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Fast_Speed;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	// SDIO_CD = PA8
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStruct.GPIO_Speed = GPIO_High_Speed;
	GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_PinAFConfig(GPIOD, GPIO_PinSource2, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource8, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource9, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_SDIO);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SDIO);
}

static void sdInitializeDMA(void)
{
	DMA_InitTypeDef DMA_InitStruct;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

	/* Preconfigure DMA2 Stream3 Channel 4 for RX */
	DMA_InitStruct.DMA_Channel = DMA_Channel_4;
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_VeryHigh;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC4;
	DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_INC4;
	DMA_Init(DMA2_Stream3, &DMA_InitStruct);

	/* Preconfigure DMA2 Stream6 Channel 4 for TX */
	DMA_InitStruct.DMA_Channel = DMA_Channel_4;
	DMA_InitStruct.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStruct.DMA_Priority = DMA_Priority_Low;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_INC4;
	DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_INC4;
	DMA_Init(DMA2_Stream6, &DMA_InitStruct);

	NVIC_SetPriority(SDIO_IRQn, VARIANT_PRIO_SDIO);
	NVIC_EnableIRQ(SDIO_IRQn);
}

static void sdInitializeSDIO(uint8_t mode, bool wide)
{
	SDIO_InitTypeDef SDIO_InitStruct;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);

	SDIO_InitStruct.SDIO_ClockEdge = SDIO_ClockEdge_Rising;
	SDIO_InitStruct.SDIO_ClockPowerSave = SDIO_ClockPowerSave_Disable;
	SDIO_InitStruct.SDIO_HardwareFlowControl = SDIO_HardwareFlowControl_Disable;
	SDIO_InitStruct.SDIO_BusWide = wide ? SDIO_BusWide_4b : SDIO_BusWide_1b;

	switch (mode)
	{
		case SDIO_MODE_INITIALIZATION:
			SDIO_InitStruct.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
			SDIO_InitStruct.SDIO_ClockDiv = 0x76U;
			break;

		case SDIO_MODE_LOW_SPEED:
			SDIO_InitStruct.SDIO_ClockBypass = SDIO_ClockBypass_Disable;
			SDIO_InitStruct.SDIO_ClockDiv = 0;
			break;

		case SDIO_MODE_HIGH_SPEED:
			SDIO_InitStruct.SDIO_ClockBypass = SDIO_ClockBypass_Enable;
			SDIO_InitStruct.SDIO_ClockDiv = 0;
			break;
	}

	SDIO_Init(&SDIO_InitStruct);
}

static SD_Status sdGetCmd0Result(void)
{
	SD_Status res = SD_NO_ERROR;
	FlagStatus status;
	uint32_t timeout = 0x00010000;

	do
	{
		status = SDIO_GetFlagStatus(SDIO_FLAG_CMDSENT);
		timeout--;
	}
	while (timeout && status == RESET);

	if (!timeout)
		res = SD_CMD_TIMEOUT;
	else
		CLEAR_STATIC_IRQ();

	return res;
}

static SD_Status sdGetR7Response(void)
{
	SD_Status res = SD_NO_ERROR;
	uint32_t timeout = 0x00010000;

	do
	{
		/* It's simpler to use the register here, instead of the ST function */
		if (SDIO->STA & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL))
			break;

		timeout--;
	} while (timeout);

	if (!timeout || (SDIO->STA & SDIO_FLAG_CTIMEOUT))
	{
		res = SD_CMD_TIMEOUT;
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
	} else {
		if (SDIO->STA & SDIO_FLAG_CMDREND)
			SDIO_ClearFlag(SDIO_FLAG_CMDREND);
		else
			/* TODO: Check other errors? */
			res = SD_ERROR;
	}

	return res;
}

static SD_Status sdGetR1Response(uint8_t cmd)
{
	uint32_t r1;
	volatile uint32_t sta;

	/* It's simpler to use the register here, instead of the ST function */
	do
	{
		sta = SDIO->STA;
	}
	while (!(sta & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)));

	if (sta & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SD_CMD_TIMEOUT;
	} else if (sta & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SD_CMD_CRC_FAIL;
	}

	if (SDIO_GetCommandResponse() != cmd)
		return SD_ILL_CMD;

	CLEAR_STATIC_IRQ();

	r1 = SDIO_GetResponse(SDIO_RESP1);

	if ((r1 & OCR_ERROR_MASK) == 0)
		return SD_NO_ERROR;

	if (r1 & OCR_AKE_SEQ_ERROR)
		return SD_AKE_SEQ_ERROR;

	if (r1 & OCR_ERASE_RESET)
		return SD_ERASE_RESET;

	if (r1 & OCR_CARD_ECC_DISABLED)
		return SD_CARD_ECC_DISABLED;

	if (r1 & OCR_WP_ERASE_SKIP)
		return SD_WP_ERASE_SKIP;

	if (r1 & OCR_CID_CSD_OVERWRITE)
		return SD_CID_CSD_OVERWRITE;

	if (r1 & OCR_STREAM_WRITE_OVERRUN)
		return SD_STREAM_WRITE_OVERRUN;

	if (r1 & OCR_STREAM_READ_UNDERRUN)
		return SD_STREAM_READ_UNDERRUN;

	if (r1 & OCR_GENERAL_UNKNOWN_ERROR)
		return SD_GENERAL_UNKNOWN_ERROR;

	if (r1 & OCR_CC_ERROR)
		return SD_CC_ERROR;

	if (r1 & OCR_CARD_ECC_FAILED)
		return SD_CARD_ECC_FAILED;

	if (r1 & OCR_ILL_CMD)
		return SD_ILL_CMD;

	if (r1 & OCR_COM_CRC_FAILED)
		return SD_CMD_CRC_FAIL;

	if (r1 & OCR_LOCK_UNLOCK_FAILED)
		return SD_LOCK_UNLOCK_FAILED;

	if (r1 & OCR_WRITE_PROT)
		return SD_WRITE_PROT;

	if (r1 & OCR_BAD_ERASE_PARAM)
		return SD_BAD_ERASE_PARAM;

	if (r1 & OCR_ERASE_SEQ_ERR)
		return SD_ERASE_SEQ_ERR;

	if (r1 & OCR_BLOCK_LEN_ERR)
		return SD_BLOCK_LEN_ERR;

	if (r1 & OCR_ADDR_MISALIGNED)
		return SD_ADDR_MISALIGNED;

	if (r1 & OCR_ADDR_OUT_OF_RANGE)
		return SD_ADDR_OUT_OF_RANGE;

	return SD_NO_ERROR;
}

static SD_Status sdGetR2Response(void)
{
	while (!(SDIO->STA & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)));

	if (SDIO->STA & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SD_CMD_TIMEOUT;
	}

	if (SDIO->STA & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SD_CMD_CRC_FAIL;
	}

	CLEAR_STATIC_IRQ();
	return SD_NO_ERROR;
}

static SD_Status sdGetR3Response(void)
{
	while (!(SDIO->STA & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)));

	if (SDIO->STA & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SD_CMD_TIMEOUT;
	}

	CLEAR_STATIC_IRQ();
	return SD_NO_ERROR;
}

static SD_Status sdGetR6Response(uint8_t cmd, uint32_t* rca)
{
	while (!(SDIO->STA & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)));

	if (SDIO->STA & SDIO_FLAG_CTIMEOUT)
	{
		SDIO_ClearFlag(SDIO_FLAG_CTIMEOUT);
		return SD_CMD_TIMEOUT;
	}

	if (SDIO->STA & SDIO_FLAG_CCRCFAIL)
	{
		SDIO_ClearFlag(SDIO_FLAG_CCRCFAIL);
		return SD_CMD_CRC_FAIL;
	}

	if (SDIO_GetCommandResponse() != cmd)
		return SD_ILL_CMD;

	CLEAR_STATIC_IRQ();

	uint32_t r1 = SDIO_GetResponse(SDIO_RESP1);

	if (!(r1 & R6_ERROR_MASK))
	{
		*rca = r1;//(uint32_t) (r1 >> 16);
	} else {
		if (r1 & R6_UNKNOWN_ERROR)
			return SD_ERROR;

		if (r1 & R6_ILL_CMD)
			return SD_ILL_CMD;

		if (r1 & R6_CMD_CRC_FAIL)
			return SD_CMD_CRC_FAIL;
	}

	return SD_NO_ERROR;
}

static SD_Status sdPowerOn(void)
{
	SD_Status status;
	uint32_t sd_type = 0;
	uint32_t resp1;
	SDIO_CmdInitTypeDef sdio_cmd;

	/* Disable clock */
	SDIO_ClockCmd(DISABLE);

	/* Power on the card */
	SDIO_SetPowerState(SDIO_PowerState_ON);

	delay(5);

	/* Enable clock */
	SDIO_ClockCmd(ENABLE);

	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;

	/* CMD0 = Idle state */
	sdio_cmd.SDIO_Argument = 0;
	sdio_cmd.SDIO_CmdIndex = CMD_GO_IDLE_STATE;
	sdio_cmd.SDIO_Response = SDIO_Response_No;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetCmd0Result();
	if (status != SD_NO_ERROR)
	{
		printError("sdPowerOn", status, "sdGetCmd0Result");
		return status;
	}

	/* CMD8 */
	sdio_cmd.SDIO_Argument = SD_CMD8_PATTERN;
	sdio_cmd.SDIO_CmdIndex = CMD_HS_SEND_EXT_CSD;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	SDIO_SendCommand(&sdio_cmd);

	/* CMD8 response is R7 */
	if (sdGetR7Response() == SD_NO_ERROR)
	{
		/* SDHC type */
		sd_type = 0x40000000;
		card_info.card_type = SD_CARD_TYPE_STD_V2;
	}

	/* CMD55 */
	sdio_cmd.SDIO_Argument = 0;
	sdio_cmd.SDIO_CmdIndex = CMD_APP_CMD;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_APP_CMD);

	if (status == SD_NO_ERROR)
	{
		/* SD */
		uint32_t voltage = 0, retries = 0xFFFF;

		sdio_cmd.SDIO_Response = SDIO_Response_Short;
		sdio_cmd.SDIO_Wait = SDIO_Wait_No;
		sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;

		do
		{
			/* CMD55 */
			sdio_cmd.SDIO_Argument = 0;
			sdio_cmd.SDIO_CmdIndex = CMD_APP_CMD;
			SDIO_SendCommand(&sdio_cmd);

			status = sdGetR1Response(CMD_APP_CMD);

			if (status != SD_NO_ERROR)
			{
				printError("sdPowerOn", status, "sdGetR1Response");
				return status;
			}

			/* CMD41 */
			sdio_cmd.SDIO_Argument = 0x80100000 | sd_type; /* Bit 20: 3.2V 3.3V */
			sdio_cmd.SDIO_CmdIndex = CMD_SD_APP_OP_COND;
			SDIO_SendCommand(&sdio_cmd);

			status = sdGetR3Response();

			if (status != SD_NO_ERROR)
			{
				printError("sdPowerOn", status, "sdGetR3Response");
				return status;
			}

			resp1 = SDIO_GetResponse(SDIO_RESP1);

			voltage = ((resp1 >> 31) == 1);
			retries--;

		} while (!voltage && retries);

		if (!retries)
		{
			printError("sdPowerOn", SD_INVALID_VOLTAGE, NULL);
			return SD_INVALID_VOLTAGE;
		}

		if (resp1 & 0x40000000)
		{
			/* High capacity */
			card_info.card_type = SD_CARD_TYPE_HC;
		}
	} else {
		/* MMC not supported */
		printError("sdPowerOn", SD_NOT_SUPPORTED, NULL);
		return SD_NOT_SUPPORTED;
	}

	return SD_NO_ERROR;
}

static SD_Status sdStartAndSelect(void)
{
	SDIO_CmdInitTypeDef sdio_cmd;
	SD_Status status;

	if (SDIO_GetPowerState() == 0)
	{
		printError("sdStartAndSelect", SD_NOT_POWERED, NULL);
		return SD_NOT_POWERED;
	}

	if (card_info.card_type != SD_CARD_TYPE_STD_V1_1 &&
		card_info.card_type != SD_CARD_TYPE_STD_V2 &&
		card_info.card_type != SD_CARD_TYPE_HC)
	{
		printError("sdStartAndSelect", SD_NOT_SUPPORTED, NULL);
		return SD_NOT_SUPPORTED;
	}

	/* CMD2 */
	sdio_cmd.SDIO_Argument = 0;
	sdio_cmd.SDIO_CmdIndex = CMD_ALL_SEND_CID;
	sdio_cmd.SDIO_Response = SDIO_Response_Long;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR2Response();

	if (status != SD_NO_ERROR)
	{
		printError("sdStartAndSelect", status, "sdGetR2Response CID");
		return status;
	}

	card_info.cid.value[0] = SDIO_GetResponse(SDIO_RESP4);
	card_info.cid.value[1] = SDIO_GetResponse(SDIO_RESP3);
	card_info.cid.value[2] = SDIO_GetResponse(SDIO_RESP2);
	card_info.cid.value[3] = SDIO_GetResponse(SDIO_RESP1);

	/* CMD3 */
	sdio_cmd.SDIO_CmdIndex = CMD_SET_REL_ADDR;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR6Response(CMD_SET_REL_ADDR, &card_info.rca.value);

	if (status != SD_NO_ERROR)
	{
		printError("sdStartAndSelect", status, "sdGetR6Response(CMD_SET_REL_ADDR)");
		return status;
	}

	sdio_cmd.SDIO_Argument = (uint32_t) (card_info.rca.fields.rca << 16);
	sdio_cmd.SDIO_CmdIndex = CMD_SEND_CSD;
	sdio_cmd.SDIO_Response = SDIO_Response_Long;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR2Response();

	if (status != SD_NO_ERROR)
	{
		printError("sdStartAndSelect", status, "sdGetR2Response CSD");
		return status;
	}

	card_info.csd.value[0] = SDIO_GetResponse(SDIO_RESP4);
	card_info.csd.value[1] = SDIO_GetResponse(SDIO_RESP3);
	card_info.csd.value[2] = SDIO_GetResponse(SDIO_RESP2);
	card_info.csd.value[3] = SDIO_GetResponse(SDIO_RESP1);

	if (card_info.card_type == SD_CARD_TYPE_HC)
	{
		card_info.capacity = ((card_info.csd.fields_v2.c_sizel | (card_info.csd.fields_v2.c_sizeh << 16)) + 1) * 512 * 1024;
		card_info.block_size = 512;
	} else {
		card_info.capacity = ((card_info.csd.fields_v1.c_sizel | (card_info.csd.fields_v1.c_sizeh << 2)) + 1);
		card_info.capacity *= (1 << (card_info.csd.fields_v1.c_size_mult + 2));
		card_info.block_size = 1 << card_info.csd.fields_v1.read_data_length;
		card_info.capacity *= card_info.block_size;
	}

	/* CMD7 - Select SD card */
	sdio_cmd.SDIO_Argument = (uint32_t) (card_info.rca.fields.rca << 16);
	sdio_cmd.SDIO_CmdIndex = CMD_SEL_DESEL_CARD;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_SEL_DESEL_CARD);
	if (status)
	{
		printError("sdStartAndSelect", status, "sdGetR1Response(CMD_SEL_DESEL_CARD)");
	}

	return status;
}

static SD_Status sdSet4BitsMode(void)
{
	SDIO_CmdInitTypeDef sdio_cmd;
	SD_Status status;

	// CMD55
	sdio_cmd.SDIO_Argument = (uint32_t) (card_info.rca.fields.rca << 16);
	sdio_cmd.SDIO_CmdIndex = CMD_APP_CMD;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_APP_CMD);
	if (status != SD_NO_ERROR)
	{
		printError("sdSet4BitsMode", status, "sdGetR1Response(CMD_APP_CMD)");
		return status;
	}

	// ACMD6
	sdio_cmd.SDIO_Argument = 2;
	sdio_cmd.SDIO_CmdIndex = CMD_APP_SD_SET_BUSWIDTH;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_APP_SD_SET_BUSWIDTH);

	if (status != SD_NO_ERROR)
	{
		printError("sdSet4BitsMode", status, "sdGetR1Response(CMD_APP_SD_SET_BUSWIDTH)");
		return status;
	}

	// Set Block Size
	sdio_cmd.SDIO_Argument = (uint32_t) 512;
	sdio_cmd.SDIO_CmdIndex = CMD_SET_BLOCKLEN;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_SET_BLOCKLEN);
	if (status)
	{
		printError("sdSet4BitsMode", status, "sdGetR1Response(CMD_SET_BLOCKLEN)");
	}

	return status;
}

static SD_Status sdGetSCR()
{
	SDIO_CmdInitTypeDef sdio_cmd;
	SDIO_DataInitTypeDef sdio_data;
	uint32_t* ptr = card_info.scr.value;

	SD_Status status;

	sdio_cmd.SDIO_Argument = 8;
	sdio_cmd.SDIO_CmdIndex = CMD_SET_BLOCKLEN;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_SET_BLOCKLEN);
	if (status != SD_NO_ERROR)
		return status;

	sdio_cmd.SDIO_Argument = (uint32_t) (card_info.rca.fields.rca << 16);
	sdio_cmd.SDIO_CmdIndex = CMD_APP_CMD;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_APP_CMD);
	if (status != SD_NO_ERROR)
		return status;

	sdio_data.SDIO_DataLength = 8;
	sdio_data.SDIO_DataBlockSize = SDIO_DataBlockSize_8b;
	sdio_data.SDIO_DPSM = SDIO_DPSM_Enable;
	sdio_data.SDIO_TransferMode = SDIO_TransferMode_Block;
	sdio_data.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	sdio_data.SDIO_DataTimeOut = 0xFFFFFFFF;
	SDIO_DataConfig(&sdio_data);

	sdio_cmd.SDIO_Argument = 0;
	sdio_cmd.SDIO_CmdIndex = CMD_SD_APP_SEND_SCR;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_SD_APP_SEND_SCR);
	if (status != SD_NO_ERROR)
		return status;

	do
	{
		if (SDIO->STA & (SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND))
			break;

		if (SDIO->STA & SDIO_FLAG_RXDAVL)
			*ptr++ = SDIO->FIFO;
	} while (true);

	if (SDIO->STA & SDIO_FLAG_DTIMEOUT)
		return SD_DATA_TIMEOUT;

	if (SDIO->STA & SDIO_FLAG_RXOVERR)
		return SD_RX_OVERRUN;

	if (SDIO->STA & SDIO_FLAG_DCRCFAIL)
		return SD_DATA_CRC_FAIL;

	CLEAR_STATIC_IRQ();

	// Swap
	uint32_t low = card_info.scr.value[0];
	uint32_t high = card_info.scr.value[1];

	card_info.scr.value[1] = ((low & 0xFF) << 24) | ((low & 0xFF00) << 8) |
							 ((low & 0xFF0000) >> 8) | ((low & 0xFF000000) >> 24);

	card_info.scr.value[0] = ((high & 0xFF) << 24) | ((high & 0xFF00) << 8) |
						 	 ((high & 0xFF0000) >> 8) | ((high & 0xFF000000) >> 24);

	return SD_NO_ERROR;
}

static SD_Status sdSetBlockLength(uint32_t len)
{
	SDIO_CmdInitTypeDef sdio_cmd;

	sdio_cmd.SDIO_Argument = (uint32_t) len;
	sdio_cmd.SDIO_CmdIndex = CMD_SET_BLOCKLEN;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	return sdGetR1Response(CMD_SET_BLOCKLEN);
}

static SD_Status sdStopTransmission()
{
	SDIO_CmdInitTypeDef sdio_cmd;

	sdio_cmd.SDIO_Argument = 0;
	sdio_cmd.SDIO_CmdIndex = CMD_STOP_TRANSMISSION;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	return sdGetR1Response(CMD_STOP_TRANSMISSION);
}

static SD_Status sdSetHighSpeed()
{
	SD_Status status;
	SDIO_CmdInitTypeDef sdio_cmd;
	SDIO_DataInitTypeDef sdio_data;
	uint8_t functions[64];
	uint32_t* ptr = (uint32_t*) functions;
	uint32_t offs = 0;

	SDIO->DCTRL = 0;
	status = sdSetBlockLength(64);
	if (status != SD_NO_ERROR)
		return status;

	sdio_data.SDIO_DataLength = 64;
	sdio_data.SDIO_DataBlockSize = SDIO_DataBlockSize_64b;
	sdio_data.SDIO_DPSM = SDIO_DPSM_Enable;
	sdio_data.SDIO_TransferMode = SDIO_TransferMode_Block;
	sdio_data.SDIO_TransferDir = SDIO_TransferDir_ToSDIO;
	sdio_data.SDIO_DataTimeOut = 0xFFFFFFFF;
	SDIO_DataConfig(&sdio_data);

	sdio_cmd.SDIO_Argument = (uint32_t) 0x80FFFF01; // Group 1 functions
	sdio_cmd.SDIO_CmdIndex = CMD_HS_SWITCH;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_HS_SWITCH);
	if (status != SD_NO_ERROR)
		return status;

	do
	{
		if (SDIO->STA & (SDIO_FLAG_RXOVERR | SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_DBCKEND))
			break;

		if (SDIO->STA & SDIO_FLAG_RXDAVL)
		{
			*ptr++ = SDIO->FIFO;
			offs += 4;
			if (offs == 64)
				break;
		}

	} while (true);

	if (SDIO->STA & SDIO_FLAG_DTIMEOUT)
		return SD_DATA_TIMEOUT;

	if (SDIO->STA & SDIO_FLAG_RXOVERR)
		return SD_RX_OVERRUN;

	if (SDIO->STA & SDIO_FLAG_DCRCFAIL)
		return SD_DATA_CRC_FAIL;

	CLEAR_STATIC_IRQ();

	if ((functions[13] & 2) != 2)
		return SD_NOT_SUPPORTED;

	return SD_NO_ERROR;
}

bool sdPresent()
{
	if (GPIOA->IDR & GPIO_Pin_8)
		return false;

	return true;
}

SD_Status sdInitialize(void)
{
	SD_Status status;
	bool high_speed = false;
	bool wide_bus = false;

	sdInitializePins();

	if (!sdPresent())
	{
		printError("sdInitialize", SD_NOT_PRESENT, NULL);
		return SD_NOT_PRESENT;
	}

	sdInitializeDMA();
	sdInitializeSDIO(SDIO_MODE_INITIALIZATION, false);	/* Low speed */

	status = sdPowerOn();

	if (status != SD_NO_ERROR)
		return status;

	status = sdStartAndSelect();
	if (status != SD_NO_ERROR)
		return status;

	status = sdGetSCR();
	if (status != SD_NO_ERROR)
		return status;

	/* Check if the card can go high speed */
	if (card_info.scr.bits.specs >= 1)
	{
		status = sdSetHighSpeed();
		if (status == SD_NO_ERROR)
			high_speed = true;
		else if (status != SD_NOT_SUPPORTED)
			return status;
	}

	/* Check if the card accepts 4-bit mode */
	if (card_info.scr.bits.bus_width & 0x04)
	{
		status = sdSet4BitsMode();
		if (status != SD_NO_ERROR)
			return status;

		wide_bus = true;
	}

	if (high_speed)
		sdInitializeSDIO(SDIO_MODE_HIGH_SPEED, wide_bus);	/* 48Mhz */
	else
		sdInitializeSDIO(SDIO_MODE_LOW_SPEED, wide_bus);	/* 24Mhz */

	status = sdSetBlockLength(512);
	return status;
}

void sdDeinitialize()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	NVIC_DisableIRQ(SDIO_IRQn);

	/* Disable clock */
	SDIO_ClockCmd(DISABLE);

	/* Power the card off */
	SDIO_SetPowerState(SDIO_PowerState_OFF);

	SDIO_DeInit();

	// SDIO_D0 = PC8, SDIO_D1 = PC9, SDIO_D2 = PC10,SDIO_D3 = PC11
	// SDIO_CK = PC12, SDIO_CMD = PD2, SDIO_CD = PA8
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOD, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
}

SD_Status sdGetStatus(bool lock)
{
	SD_Status status;
	SDIO_CmdInitTypeDef sdio_cmd;

	if (!sdPresent())
		return SD_NOT_PRESENT;

	if (lock && !sdLock())
		return SD_BUSY;

	// CMD55
	sdio_cmd.SDIO_Argument = (uint32_t) (card_info.rca.fields.rca << 16);
	sdio_cmd.SDIO_CmdIndex = CMD_SEND_STATUS;
	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	SDIO_SendCommand(&sdio_cmd);

	status = sdGetR1Response(CMD_SEND_STATUS);

	if (lock)
		sdUnlock();

	return status;
}

static SD_Status sdSendBlockRxCmd(uint32_t sector, bool multi)
{
	SDIO_CmdInitTypeDef sdio_cmd;
	SD_Status status;

	if (multi)
		sdio_cmd.SDIO_CmdIndex = CMD_READ_MULT_BLOCK;
	else
		sdio_cmd.SDIO_CmdIndex = CMD_READ_SINGLE_BLOCK;

	if (card_info.card_type != SD_CARD_TYPE_HC)
		sector <<= 9;

	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	sdio_cmd.SDIO_Argument = (uint32_t) sector;
	SDIO_SendCommand(&sdio_cmd);

	if (multi)
		status = sdGetR1Response(CMD_READ_MULT_BLOCK);
	else
		status = sdGetR1Response(CMD_READ_SINGLE_BLOCK);

	return status;
}

static SD_Status sdSendBlockTxCmd(uint32_t sector, bool multi)
{
	SDIO_CmdInitTypeDef sdio_cmd;
	SD_Status status;

	if (multi)
		sdio_cmd.SDIO_CmdIndex = CMD_WRITE_MULT_BLOCK;
	else
		sdio_cmd.SDIO_CmdIndex = CMD_WRITE_SINGLE_BLOCK;

	if (card_info.card_type != SD_CARD_TYPE_HC)
		sector <<= 9;

	sdio_cmd.SDIO_Response = SDIO_Response_Short;
	sdio_cmd.SDIO_Wait = SDIO_Wait_No;
	sdio_cmd.SDIO_CPSM = SDIO_CPSM_Enable;
	sdio_cmd.SDIO_Argument = (uint32_t) sector;
	SDIO_SendCommand(&sdio_cmd);

	if (multi)
		status = sdGetR1Response(CMD_WRITE_MULT_BLOCK);
	else
		status = sdGetR1Response(CMD_WRITE_SINGLE_BLOCK);

	return status;
}

static bool sdTransferDone()
{
	uint32_t response;
	SD_Status status;

	status = sdGetStatus(false);

	if (status == SD_NO_ERROR)
	{
		response = SDIO_GetResponse(SDIO_RESP1);
		response = (response >> 9) & 0x0F;

		if (response == SD_STATE_TRANSFER)
			// SD is back to transfer state. Transfer is done.
			return true;
	}

	return false;
}

static bool sdAbortTransmission(bool multi)
{
	uint32_t ticks;

	/*
	if (sdTransferDone())
		return true;
	*/

	ticks = GetTickCount();

	while (true)
	{
		if (multi)
		{
			if (sdStopTransmission() == SD_NO_ERROR)
				multi = false;
		}

		if (sdTransferDone())
			return true;

		if (GetTickCount() - ticks >= 1000)
			return false;
	}
/*
	if (multi)
	{
		while (sdStopTransmission() != SD_NO_ERROR)
		{
			if (GetTickCount() - ticks >= 1000)
				return false;
		}
	} else {
		sdStopTransmission();
	}

	while (!sdTransferDone())
	{
		if (GetTickCount() - ticks >= 1000)
			return false;
	}
*/
	return true;
}

static SD_Status sdTransferBlocksWithDMA(uint32_t sector, const uint8_t* buffer, uint32_t count, bool write)
{
	uint8_t retries = SDIO_RETRIES;
	uint32_t ticks;
	SD_STAT(uint32_t transfer_time = micros());

	while (retries)
	{
		// Reset SDIO data control and interrupts
		SDIO->DCTRL = 0;

		DMA2->LIFCR = 0x0F400000;							// Clear DMA2 Stream3 pending interrupts
		DMA2_Stream3->CR = 0;								// Disable DMA2 Stream3
		while (DMA2_Stream3->CR & DMA_SxCR_EN);
		DMA2_Stream3->M0AR = (uint32_t) buffer;				// Program source address
		DMA2_Stream3->PAR = (uint32_t) &SDIO->FIFO;			// Program destination address
		DMA2_Stream3->NDTR = 0;								// Counter doens't care if flow control is enabled

		if (((uint32_t) buffer & 0x0F) == 0)
		{
			DMA2_Stream3->FCR = DMA_FIFOMode_Enable			|
								DMA_FIFOThreshold_Full;

			DMA2_Stream3->CR = 	DMA_Channel_4				|
								DMA_MemoryInc_Enable		|
								DMA_PeripheralDataSize_Word	|
								DMA_MemoryDataSize_Word		|
								DMA_PeripheralBurst_INC4	|
								DMA_MemoryBurst_INC4		|
								DMA_Priority_VeryHigh		|
								DMA_SxCR_PFCTRL;

		} else if (((uint32_t) buffer & 0x07) == 0)
		{
			DMA2_Stream3->FCR = DMA_FIFOMode_Enable 		|
								DMA_FIFOThreshold_Full;

			DMA2_Stream3->CR = 	DMA_Channel_4 				|
								DMA_MemoryInc_Enable 		|
								DMA_PeripheralDataSize_Word |
								DMA_PeripheralBurst_INC4 	|
								DMA_MemoryDataSize_HalfWord	|
								DMA_MemoryBurst_INC4		|
								DMA_Priority_VeryHigh		|
								DMA_SxCR_PFCTRL;

		} else if (((uint32_t) buffer & 0x03) == 0)
		{
			DMA2_Stream3->FCR = DMA_FIFOMode_Enable			|
								DMA_FIFOThreshold_1QuarterFull;

			DMA2_Stream3->CR = 	DMA_Channel_4				|
								DMA_MemoryInc_Enable		|
								DMA_PeripheralDataSize_Word	|
								DMA_PeripheralBurst_INC4	|
								DMA_MemoryDataSize_Byte		|
								DMA_MemoryBurst_INC4		|
								DMA_Priority_VeryHigh		|
								DMA_SxCR_PFCTRL;
		} else {

			DMA2_Stream3->FCR = DMA_FIFOMode_Enable			|
								DMA_FIFOThreshold_HalfFull;

			DMA2_Stream3->CR = 	DMA_Channel_4				|
								DMA_MemoryInc_Enable		|
								DMA_PeripheralDataSize_Word	|
								DMA_MemoryDataSize_Byte		|
								DMA_PeripheralBurst_INC4	|
								DMA_MemoryBurst_Single		|
								DMA_Priority_VeryHigh		|
								DMA_SxCR_PFCTRL;
		}

		if (write)
			DMA2_Stream3->CR |= DMA_DIR_MemoryToPeripheral;
		else
			DMA2_Stream3->CR |= DMA_DIR_PeripheralToMemory;

		DMA2_Stream3->CR |= DMA_SxCR_EN;

		// Reset our status/error flags
		sd_cmd_irq_flag = sd_transfer_error = sd_sdio_transfer_complete = false;

		SD_STAT(sd_sta = 0);
		SDIO->DTIMER = 0xFFFFFFFF;
		SDIO->DLEN = count * 512;
		SDIO->MASK = SDIO_MASK_DATAENDIE | SDIO_MASK_DTIMEOUTIE | SDIO_MASK_TXUNDERRIE |
					 SDIO_MASK_DCRCFAILIE | SDIO_MASK_RXOVERRIE;

		if (!write)
		{
			SDIO->DCTRL = SDIO_DCTRL_DMAEN | SDIO_DataBlockSize_512b | SDIO_TransferMode_Block | SDIO_DPSM_Enable | SDIO_TransferDir_ToSDIO;
			sd_transfer_error_code = sdSendBlockRxCmd(sector, (count > 1));
		} else {
			sd_transfer_error_code = sdSendBlockTxCmd(sector, (count > 1));
		}

		if (sd_transfer_error_code != SD_NO_ERROR)
		{
			printErrorRWOp(write, sd_transfer_error_code, sector, count, retries, "Send block cmd");
			SD_STAT(sd_errors++);
			SDIO->DCTRL = 0;
			sdAbortTransmission(count > 1);
			retries--;
			continue;
		}

		if (write)
			SDIO->DCTRL = SDIO_DCTRL_DMAEN | SDIO_DataBlockSize_512b | SDIO_TransferMode_Block | SDIO_DPSM_Enable | SDIO_TransferDir_ToCard;

		ticks = GetTickCount();
		while (!sd_sdio_transfer_complete)
		{
			if (sd_transfer_error)
				break;

			if (GetTickCount() - ticks >= 1000)
			{
				// A second has passed, declare timeout
				SD_STAT(sd_timeouts++);
				sd_transfer_error = true;
				sd_transfer_error_code = SD_DATA_TIMEOUT;
				break;
			}
		}

		if (sd_transfer_error || sd_transfer_error_code != SD_NO_ERROR)
		{
			printErrorRWOp(write, sd_transfer_error_code, sector, count, retries, "While waiting for completion");
			SD_STAT(if (sd_transfer_error_code != SD_DATA_TIMEOUT) sd_errors++);
			sdAbortTransmission(count > 1);
			retries--;
			continue;
		}

		// Wait while the SDIO TX/RX finishes
		ticks = GetTickCount();
		uint32_t sdio_status_flag = (write) ? SDIO_STA_TXACT : SDIO_STA_RXACT;
		while (true)
		{
			if (!(SDIO->STA & sdio_status_flag))
			{
				if (SDIO->DCOUNT == 0)
					break;
			}

			if (GetTickCount() - ticks >= 1000)
			{
				// A second has passed, declare timeout
				printErrorRWOp(write, SD_DATA_TIMEOUT, sector, count, retries, "While waiting for SDIO->STA completion");
				SD_STAT(sd_timeouts++);
				sd_transfer_error = true;
				sd_transfer_error_code = SD_DATA_TIMEOUT;
				sdAbortTransmission(count > 1);
				retries--;
				break;
			}
		}

		if (sd_transfer_error_code)
			continue;

		ticks = GetTickCount();
		while (true)
		{
			if (DMA2->LISR & DMA_LISR_TCIF3)
				break;

			if (DMA2->LISR & DMA_LISR_TEIF3)
			{
				sd_transfer_error = true;
				sd_transfer_error_code = SD_FIFO_ERROR;
				break;
			}

			if (GetTickCount() - ticks >= 1000)
			{
				// A second has passed, declare timeout
				SD_STAT(sd_timeouts++);
				sd_transfer_error = true;
				sd_transfer_error_code = SD_DATA_TIMEOUT;
				retries--;
				break;
			}
		}

		DMA_ClearFlag(DMA2_Stream3, DMA_FLAG_TCIF3 | DMA_FLAG_FEIF3);

		if (sd_transfer_error || sd_transfer_error_code != SD_NO_ERROR)
		{
			printErrorRWOp(write, sd_transfer_error_code, sector, count, retries, "While waiting for DMA");
			SD_STAT(sd_timeouts++);
			sdAbortTransmission(count > 1);
			retries--;
			continue;
		}

		sdAbortTransmission(count > 1);

		CLEAR_STATIC_IRQ();
		break;
	}

#if SD_STATS
	if (retries != SDIO_RETRIES)
	{
		uint32_t retries_count = SDIO_RETRIES - retries;
		if (retries_count > sd_max_retries)
			sd_max_retries = retries_count;
	}

	if (write)
	{
		if (sd_transfer_error_code == SD_NO_ERROR)
		{
			uint32_t write_time = micros() - transfer_time;
			if (write_time > sd_max_write_time)
				sd_max_write_time = write_time;

			if (count)
				sd_avg_write_time = ((sd_sectors_written * sd_avg_write_time) + write_time) / (sd_sectors_written + count);

			sd_sectors_written += count;
		}

	} else {
		if (sd_transfer_error_code == SD_NO_ERROR)
		{
			uint32_t read_time = micros() - transfer_time;
			if (read_time > sd_max_read_time)
				sd_max_read_time = read_time;

			if (count)
				sd_avg_read_time = ((sd_sectors_read * sd_avg_read_time) + read_time) / (sd_sectors_read + count);

			sd_sectors_read += count;
		}
	}
#endif // SD_STATS

	return sd_transfer_error_code;
}

SD_Status sdReadBlocks(uint32_t sector, uint8_t* buffer, uint32_t count)
{
	SD_Status ret;

	if (!sdLock())
		return SD_BUSY;

	ret = sdTransferBlocksWithDMA(sector, buffer, count, false);

	sdUnlock();

	return ret;
}

SD_Status sdWriteBlocks(uint32_t sector, const uint8_t* buffer, uint32_t count)
{
	SD_Status ret;

	if (!sdLock())
		return SD_BUSY;

	ret = sdTransferBlocksWithDMA(sector, buffer, count, true);

	sdUnlock();

	return ret;
}

extern "C" void SDIO_IRQHandler(void)
{
	SD_STAT(sd_sta = SDIO->STA);
	sd_cmd_irq_flag = true;

	if (SDIO->STA & SDIO_IT_DATAEND)
	{
		SDIO->ICR = SDIO_IT_DATAEND;
		sd_sdio_transfer_complete = true;
	} else if (SDIO->STA & SDIO_IT_DCRCFAIL)
	{
		SDIO->ICR = SDIO_IT_DCRCFAIL;
		sd_transfer_error_code = SD_DATA_CRC_FAIL;
		sd_transfer_error = true;
	} else if (SDIO->STA & SDIO_IT_DTIMEOUT)
	{
		SDIO->ICR = SDIO_IT_DTIMEOUT;
		sd_transfer_error_code = SD_DATA_TIMEOUT;
		sd_transfer_error = true;
	} else if (SDIO->STA & SDIO_IT_RXOVERR)
	{
		SDIO->ICR = SDIO_IT_RXOVERR;
		sd_transfer_error_code = SD_RX_OVERRUN;
		sd_transfer_error = true;
	} else if (SDIO->STA & SDIO_IT_TXUNDERR)
	{
		SDIO->ICR = SDIO_IT_TXUNDERR;
		sd_transfer_error = true;
	} else if (SDIO->STA & SDIO_IT_STBITERR)
	{
		SDIO->ICR = SDIO_IT_STBITERR;
		sd_transfer_error_code = SD_START_BIT_ERROR;
		sd_transfer_error = true;
	}

	// Disable all SDIO interrupts
	DISABLE_STATIC_IRQ();
}

uint8_t sdIsBusy()
{
	return (sd_busy_count > 0);
}



#if SD_STATS
void sdPrintDebug(UARTClass* uart)
{
	uart->println("\r\nSD statistics");
	uart->println("----------------\r\n");
	uart->print("SD errors: ");
	uart->println(sd_errors);
	uart->print("SD timeouts: ");
	uart->println(sd_timeouts);
	uart->print("Max. retries: ");
	uart->println(sd_max_retries);
	uart->print("Sectors read: ");
	uart->println(sd_sectors_read);
	uart->print("Average read time: ");
	uart->print(sd_avg_read_time);
	uart->println(" uS");
	uart->print("Max. read time: ");
	uart->print(sd_max_read_time);
	uart->println(" uS");
	uart->print("Sectors written: ");
	uart->println(sd_sectors_written);
	uart->print("Average write time: ");
	uart->print(sd_avg_write_time);
	uart->println(" uS");
	uart->print("Max. write time: ");
	uart->print(sd_max_write_time);
	uart->println(" uS");
}
#endif // SD_STATS
