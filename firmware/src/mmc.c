/*
 * Copyright (c) 2020 Spacecraft-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <mmc.h>
#include <fpga.h>
#include <delay.h>
#include <statuscode.h>
#include "mmc_defs.h"
#include "sd.h"

int crc7(uint8_t *buffer, int size)
{
	uint8_t crc = 0;
	for (int i = 0; i < size; i++)
	{
		uint8_t c = buffer[i];
		for (int j = 0; j < 8; j++)
		{
			crc <<= 1;
			if ((crc ^ c) & 0x80)
				crc ^= 9;
			c <<= 1;
		}
		crc &= 0x7Fu;
	}
	return crc;
}

int mmc_send_command(uint32_t cmd, uint32_t argument, uint32_t *res, uint8_t *io)
{
	uint8_t data[7];
	data[0] = cmd | 0x40;
	*(uint32_t *) &data[1] = __builtin_bswap32(argument);
	data[5] = (crc7(data, 5) << 1) | 1;
	data[6] = 1;

	switch (cmd)
	{
		case MMC_READ_SINGLE_BLOCK:
			data[6] = 3;
			break;

		case MMC_WRITE_BLOCK:
			data[6] = 5;
			if (io)
			{
				fpga_select_active_buffer(FPGA_BUFFER_CMD_DATA);
				fpga_write_buffer(io, 512);
			}
			break;

		case MMC_ALL_SEND_CID:
		case MMC_SEND_CSD:
			data[6] |= 8;
			break;
	}

	fpga_select_active_buffer(FPGA_BUFFER_CMD);
	fpga_write_buffer(data, 7);
	fpga_do_mmc_command();

	int retry = 2000;
	while (fpga_read_mmc_flags() & FPGA_MMC_BUSY_SENDING)
	{
		if (!--retry)
			return -1;
		delay_us(50);
	}

	fpga_select_active_buffer(FPGA_BUFFER_CMD);
	uint8_t tmp[32];
	fpga_read_buffer(tmp, 32);

	if (res)
	{
		// MMC_SEND_CSD?
		if (cmd == MMC_ALL_SEND_CID)
			memcpy(res, &tmp[1], 15);
		else
			*res = __builtin_bswap32(*(uint32_t *) &tmp[1]);
	}

	if (cmd == MMC_READ_SINGLE_BLOCK && io)
	{
		fpga_select_active_buffer(FPGA_BUFFER_CMD_DATA);
		fpga_read_buffer(io, 512);
	}

	return 0;
}

uint32_t mmc_initialize(uint8_t *cid)
{
	fpga_reset_device(1);

	uint32_t res;
	if (mmc_send_command(MMC_GO_IDLE_STATE, 0, 0, 0))
		return ERR_MMC_GO_IDLE_FAILED;

	if (!mmc_send_command(MMC_SEND_OP_COND, 0, 0, 0))
	{
		int retry = 100;
		while (!mmc_send_command(MMC_SEND_OP_COND, SD_OCR_CCS | SD_OCR_VDD_18, &res, 0))
		{
			if ((res & 0xFF000000) == (MMC_CARD_BUSY | SD_OCR_CCS))
				break;

			delay_ms(10);
			if (!--retry)
				return ERR_MMC_SEND_OP_COND_FAILED;
		}
	}

	uint32_t cid_res[4];
	if (mmc_send_command(MMC_ALL_SEND_CID, 0, cid_res, 0))
		return ERR_MMC_SEND_CID_FAILED;

	if (cid)
	{
		memcpy(cid, cid_res, 16);
		cid[15] = (crc7(cid, 15) << 1);
	}

	if (mmc_send_command(MMC_SET_RELATIVE_ADDR, 2 << 16, &res, 0))
		return ERR_MMC_SET_RELATIVE_ADDR_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_IDENT)
		return ERR_MMC_STATE_UNEXPECTED_NOT_IDENT;

	if (mmc_send_command(MMC_SEND_CSD, 2 << 16, 0, 0))
		return ERR_MMC_SEND_CSD_FAILED;

	if (mmc_send_command(MMC_SELECT_CARD, 2 << 16, &res, 0))
		return ERR_MMC_SELECT_CARD_FAILED;

	if (R1_CURRENT_STATE(res) != (R1_STATE_IDENT | R1_STATE_READY))
		return ERR_MMC_STATE_NOT_IDENT_OR_READY;

	if (mmc_send_command(MMC_SEND_STATUS, 2 << 16, &res, 0))
		return ERR_MMC_SEND_STATUS_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_TRAN)
		return ERR_MMC_STATE_UNEXPECTED_NOT_TRAN1;

	if (mmc_send_command(MMC_SET_BLOCKLEN, 512, &res, 0))
		return ERR_MMC_SET_BLOCKLEN_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_TRAN)
		return ERR_MMC_STATE_UNEXPECTED_NOT_TRAN2;

	if (mmc_send_command(MMC_SWITCH, (MMC_SWITCH_MODE_WRITE_BYTE << 24) | (EXT_CSD_PART_CONFIG << 16) | (1 << 8), &res, 0))
		return ERR_MMC_SWITCH_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_TRAN)
		return ERR_MMC_STATE_UNEXPECTED_NOT_TRAN3;

	return 0;
}

uint32_t mmc_read(uint32_t offset, uint8_t *block)
{
	uint32_t res;
	int ret = mmc_send_command(MMC_READ_SINGLE_BLOCK, offset, &res, block);
	if (ret)
		return ERR_MMC_READ_SINGLE_BLOCK_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_TRAN)
		return ERR_MMC_STATE_UNEXPECTED_NOT_TRAN4;

	return 0;
}

uint32_t mmc_write(uint32_t offset, const uint8_t *block)
{
	uint32_t res;
	if (mmc_send_command(MMC_WRITE_BLOCK, offset, &res, (uint8_t *) block))
		return ERR_MMC_WRITE_SINGLE_BLOCK_FAILED;

	if (R1_CURRENT_STATE(res) != R1_STATE_TRAN)
		return ERR_MMC_STATE_UNEXPECTED_NOT_TRAN5;

	return 0;
}

uint32_t mmc_check_and_if_different_write(uint32_t offset, const uint8_t *buffer, uint32_t len)
{
	uint8_t tmp[512];
	memset(tmp, 0, sizeof(tmp));
	len = (len + sizeof(tmp) - 1) / sizeof(tmp);

	for (int i = 0; i < len; i++)
	{
		uint32_t status = mmc_read(offset + i, tmp);
		if (status)
			return status;
		if (memcmp(tmp, &buffer[i * sizeof(tmp)], sizeof(tmp)))
		{
			status = mmc_write(offset + i, &buffer[i * sizeof(tmp)]);
			if (status)
				return status;
		}
	}

	return 0;
}

uint32_t mmc_check_and_if_header_different_write_all(uint32_t offset, const uint8_t *buffer, uint32_t len)
{
	uint8_t tmp[512];
	memset(tmp, 0, sizeof(tmp));
	len = (len + sizeof(tmp) - 1) / sizeof(tmp);

	// Read header.
	uint32_t status = mmc_read(offset, tmp);
	if (status)
		return status;

	// Skip bad block table and check if signature doesn't match.
	if (memcmp(&tmp[0x10], &buffer[0x10], 0x100))
	{
		for (int i = 0; i < len; i++)
		{
			uint32_t status = mmc_read(offset + i, tmp);
			if (status)
				return status;
			if (memcmp(tmp, &buffer[i * sizeof(tmp)], sizeof(tmp)))
			{
				status = mmc_write(offset + i, &buffer[i * sizeof(tmp)]);
				if (status)
					return status;
			}
		}
	}

	return 0;
}

uint32_t mmc_copy(uint32_t dest, uint32_t source, uint32_t len)
{
	uint8_t tmp[512];
	len = (len + sizeof(tmp) - 1) / sizeof(tmp);

	for (int i = 0; i < len; i++)
	{
		uint32_t status = mmc_read(source + i, tmp);
		if (status)
			return status;
		status = mmc_write(dest + i, tmp);
		if (status)
			return status;
	}

	return 0;
}

uint32_t mmc_erase(uint32_t offset, uint32_t len)
{
	uint8_t tmp[512];
	memset(tmp, 0, sizeof(tmp));
	len = (len + sizeof(tmp) - 1) / sizeof(tmp);

	for (int i = 0; i < len; i++)
	{
		uint32_t status = mmc_write(offset + i, tmp);
		if (status)
			return status;
	}

	return 0;
}
