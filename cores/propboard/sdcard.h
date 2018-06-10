/***************************************************************************
 * Artekit PropBoard
 * https://www.artekit.eu/products/devboards/propboard
 *
 * Written by Ivan Meleca
 * Copyright (c) 2017 Artekit Labs
 * https://www.artekit.eu

### sdcard.h

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

#ifndef __SDCARD_H__
#define __SDCARD_H__

#include "Arduino.h"

#ifndef SD_DEBUG
#define SD_DEBUG 0
#endif

#ifndef SD_STATS
#define SD_STATS 0
#endif

#if SD_STATS
#define SD_STAT(x)	x
#else
#define SD_STAT(x)
#endif // SD_STATS

#define SD_CARD_TYPE_STD_V1_1	1
#define SD_CARD_TYPE_STD_V2		2
#define SD_CARD_TYPE_HC			3
#define SD_CARD_TYPE_IO			4
#define SD_CARD_TYPE_COMBO		5

typedef enum
{
	SD_NO_ERROR = 0,
	SD_DATA_CRC_FAIL = 1,
	SD_CMD_TIMEOUT,
	SD_DATA_TIMEOUT,
	SD_TX_UNDERRUN,
	SD_RX_OVERRUN,

	/* OCR errors */
	SD_AKE_SEQ_ERROR,
	SD_ERASE_RESET,
	SD_CARD_ECC_DISABLED,
	SD_WP_ERASE_SKIP,
	SD_CID_CSD_OVERWRITE,
	SD_STREAM_WRITE_OVERRUN,
	SD_STREAM_READ_UNDERRUN,
	SD_GENERAL_UNKNOWN_ERROR,
	SD_CC_ERROR,
	SD_CARD_ECC_FAILED,
	SD_ILL_CMD,
	SD_CMD_CRC_FAIL,
	SD_LOCK_UNLOCK_FAILED,
	SD_WRITE_PROT,
	SD_BAD_ERASE_PARAM,
	SD_ERASE_SEQ_ERR,
	SD_BLOCK_LEN_ERR,
	SD_ADDR_MISALIGNED,
	SD_ADDR_OUT_OF_RANGE,

	/* Generic errors */
	SD_INVALID_VOLTAGE,
	SD_NOT_SUPPORTED,
	SD_NOT_POWERED,
	SD_NOT_PRESENT,
	SD_START_BIT_ERROR,
	SD_FIFO_ERROR,
	SD_BUSY,
	SD_ERROR = 100
} SD_Status;

typedef enum
{
	CURRENT_STATE_IDLE = 0,
	CURRENT_STATE_READY,
	CURRENT_STATE_IDENT,
	CURRENT_STATE_STBY,
	CURRENT_STATE_TRAN,
	CURRENT_STATE_DATA,
	CURRENT_STATE_RCV,
	CURRENT_STATE_PRG,
	CURRENT_STATE_DIS,
} SD_Current_State;

typedef union __attribute__((packed))
{
    struct {
    	uint8_t always_1:1;
    	uint8_t crc:7;
    	uint8_t manufacturing_date[2];
    	uint8_t serial_number[4];
    	uint8_t revision;
    	uint8_t name[5];
    	uint8_t oem_id[2];
    	uint8_t manufacturer_id;
    } fields;

    uint32_t value[4];
} CID_Register;


typedef union
{
	struct
	{
		struct
		{
			uint8_t reserved1:2;		// status bits: 12:0
			uint8_t reserved2:1;
			uint8_t ake_seq_error:1;
			uint8_t reserved3:1;
			uint8_t app_cmd:1;
			uint8_t reserved4:2;
			uint8_t ready_for_data:1;
			uint8_t state:4;
			uint8_t error:1;			// status bits: 19
			uint8_t cc_error:1;			// status bits: 22
			uint8_t card_ecc_failed:1;	// status bits: 23
		} card_status_bits;

		uint16_t rca;
	} fields;

	uint32_t value;
} RCA_Response;

typedef union __attribute__((packed))
{
	struct
	{
		uint32_t always_1:1;
		uint32_t crc:7;
		uint32_t reserved1:2;
		uint32_t file_format:2;
		uint32_t tmp_write_protect:1;
		uint32_t perm_write_protect:1;
		uint32_t copy_flag:1;
		uint32_t file_format_grp:1;
		uint32_t reserved2:5;
		uint32_t write_bl_partial:1;
		uint32_t write_bl_len:4;
		uint32_t r2w_factor:3;
		uint32_t reserved3:2;
		uint32_t wp_grp_enable :1;

		uint32_t wp_grp_size:7;
		uint32_t erase_sector_size:7;
		uint32_t erase_single_block:1;
		uint32_t reserved4:1;
		uint32_t c_sizel:16;

		uint32_t c_sizeh:6;
		uint32_t reserved5:6;
		uint32_t dsr_imp:1;
		uint32_t read_blk_misalign:1;
		uint32_t write_blk_misalign:1;
		uint32_t read_bl_partial:1;
		uint32_t read_bl_len:4;
		uint32_t ccc:12;
		uint32_t trans_speed:8;
		uint32_t nsac :8;
		uint32_t taac :8;
		uint32_t reserved6:6;
		uint32_t csd_structure:2;
	} fields_v2;

	struct
	{
		uint32_t always_1:1;
		uint32_t crc:7;
		uint32_t reserved1:2;
		uint32_t file_format:2;
		uint32_t tmp_write_protect:1;
		uint32_t perm_write_protect:1;
		uint32_t copy_flag:1;
		uint32_t file_format_grp:1;
		uint32_t reserved2:5;
		uint32_t write_bl_partial:1;
		uint32_t write_bl_len:4;
		uint32_t r2w_factor:3;
		uint32_t reserved3:2;
		uint32_t wp_grp_enable:1;
		uint32_t wp_grp_size:7;
		uint32_t erase_sector_size:7;
		uint32_t erase_single_block:1;
		uint32_t c_size_mult:3;
		uint32_t vdd_w_cur_max:3;
		uint32_t vdd_w_cur_min:3;
		uint32_t vdd_r_cur_max:3;
		uint32_t vdd_r_cur_min:3;
		uint32_t c_sizel:2;
		uint32_t c_sizeh:10;
		uint32_t reserved4:2;
		uint32_t dsr_imp:1;
		uint32_t read_block_misalignment:1;
		uint32_t write_block_misalignment:1;
		uint32_t partial_blocks_allowed:1;
		uint32_t read_data_length:4;
		uint32_t ccc:12;
		uint32_t max_data_rate:8;
		uint32_t nsac:8;
		uint32_t taac:8;
		uint32_t reserved5:6;
		uint32_t csd_structure:2;
	} fields_v1;

	uint32_t value[4];
} CSD_Register;

typedef union
{
	struct
	{
		uint32_t reserved1;
		uint32_t reserved2:16;
		uint32_t bus_width:4;
		uint32_t security:3;
		uint32_t data_stat_after_erase:1;
		uint32_t specs:4;
		uint32_t structure:4;
	} bits;

	uint32_t value[2];
} SCR_Register;

typedef union
{
	struct
	{
		uint32_t reserved1:2;
		uint32_t reserved2:1;
		uint32_t ake_seq_error:1;
		uint32_t reserved3:1;
		uint32_t app_cmd:1;
		uint32_t reserved4:2;
		uint32_t ready_for_data:1;
		SD_Current_State current_state:4;
		uint32_t erase_reset:1;
		uint32_t card_ecc_disabled:1;
		uint32_t wp_erase_skip:1;
		uint32_t csd_overwrite:1;
		uint32_t reserved5:1;
		uint32_t reserved6:1;
		uint32_t error:1;
		uint32_t cc_error:1;
		uint32_t card_ecc_failed:1;
		uint32_t illegal_command:1;
		uint32_t com_crc_error:1;
		uint32_t lock_unlock_failed:1;
		uint32_t card_is_locked:1;
		uint32_t wp_violation:1;
		uint32_t erase_param:1;
		uint32_t erase_seq_error:1;
		uint32_t block_len_error:1;
		uint32_t address_error:1;
		uint32_t out_of_range:1;
	} fields;

	uint32_t value;
} SD_Card_Status;


typedef struct sdCardInfo
{
	uint8_t card_type;
	uint32_t address;
	uint64_t capacity;
	uint16_t block_size;
	CSD_Register csd;
	CID_Register cid;
	SCR_Register scr;
	RCA_Response rca;

} SD_CARD_INFO;


#ifdef __cplusplus
extern "C"
{
#endif

SD_Status sdInitialize();
void sdDeinitialize();
SD_Status sdGetStatus(bool lock = true);
SD_Status sdReadBlocks(uint32_t sector, uint8_t* buffer, uint32_t count);
SD_Status sdWriteBlocks(uint32_t sector, const uint8_t* buffer, uint32_t count);
uint8_t sdIsBusy();
void enableSdDebug(UARTClass* uart);
void disableSdDebug();
bool sdPresent();

#if SD_STATS
#include "UARTClass.h"

void sdPrintDebug(UARTClass* uart);
#endif // SD_STATS

#ifdef __cplusplus
}
#endif

#endif /* __SDCARD_H__ */
