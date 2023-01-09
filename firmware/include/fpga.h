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

#ifndef __FPGA_H__
#define __FPGA_H__

#include <stdint.h>

extern int fpga_sync_failed;
extern int payload_not_yet_flashed;

void fpga_init();
uint32_t fpga_reset();
void fpga_power_off();

enum FPGA_BUFFER
{
	FPGA_BUFFER_CMD = 0, // 512B, traffic on CMD line
	FPGA_BUFFER_CMD_DATA = 1, // 512B, data from host-->device commands, also toolkit comms
	FPGA_BUFFER_RESP_DATA = 2, // 512B, data from device-->host responses
};
// A word on buffer sizing (implied by the code):
// 0 - 7 (mmc.c), 32 (mmc.c), 512 (glitch.c)
// 1 - 512 (mmc.c), 512 (glitch.c)
// 2 - 512 (glitch.c)

void fpga_select_active_buffer(enum FPGA_BUFFER buffer);
void fpga_reset_device(int do_clock_stuck_glitch);
typedef struct
{
	uint16_t offset; // 12-bit counter marking number of eMMC clock cycles to wait after completed sector 0x13 READ_SINGLE_BLOCK command
	uint8_t subcycle_delay; // 3-bit counter for number of additional pulses at 4x eMMC clock to delay after 'offset' above
	uint8_t width; // glitch pulse width in clock cycles @ 48MHz starting after subcycle_delay.
	uint8_t timeout; // delay as ~1.2ms*timeout value after which glitch_flag:timeout is set when no eMMC bus activity is detected
} glitch_cfg_t;
void fpga_glitch_device(glitch_cfg_t *cfg);
uint8_t fpga_read_glitch_flags();

#define FPGA_MMC_BUSY_SENDING           0x01
#define FPGA_MMC_GLITCH_SUCCESS         0x02
#define FPGA_MMC_GLITCH_TIMEOUT         0x04
#define FPGA_MMC_BUSY_UNKNOWN1          0x08
#define FPGA_MMC_BUSY_LOADER_DATA_RCVD  0x10
#define FPGA_MMC_BUSY_UNKNOWN2          0x20
#define FPGA_MMC_GLITCH_DT_CAPTURED     0x40
#define FPGA_MMC_BUSY_UNKNOWN3          0x80
uint8_t fpga_read_mmc_flags();

uint32_t fpga_read_type();
void fpga_do_mmc_command();

void fpga_read_buffer(uint8_t *buffer, uint32_t size);
void fpga_write_buffer(uint8_t *buffer, uint32_t size);

void fpga_enter_cmd_mode();
void fpga_pre_recv();
void fpga_post_recv();
void fpga_post_send();

#endif
