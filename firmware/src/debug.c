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

#include <bootloader.h>
#include <delay.h>
#include <leds.h>
#include <fpga.h>
#include <board_id.h>
#include <device.h>
#include <adc.h>
#include <glitch.h>
#include <clock.h>
#include <payload.h>
#include <timer.h>
#include <sdio.h>
#include <statuscode.h>
#include <string.h>
#include <sprintf.h>
#include <stdarg.h>

struct bootloader_usb *g_usb;

void dbglog(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	if (g_usb)
	{
		int len = vsprintf((char *) g_usb->send_buffer, fmt, ap);
		if (len)
			g_usb->send_data(len);
	}

	va_end(ap);
}

void dbg_logger_start()
{
	dbglog("# Diagnose report:\n");
}

void dbg_logger_device_type(enum DEVICE_TYPE dt)
{
	if (dt == DEVICE_TYPE_ERISTA)
		dbglog("Device type: Erista (V1)\n");
	else if (dt == DEVICE_TYPE_MARIKO)
		dbglog("Device type: Mariko (V2)\n");
	else if (dt == DEVICE_TYPE_LITE)
		dbglog("Device type: Lite\n");
	else
		dbglog("Device type: unknown\n");
}

void dbg_logger_glitching_started()
{
	dbglog("Glitching started\n");
}

void dbg_logger_payload_flash_res_and_cid(uint32_t status, uint8_t *cid)
{
	if (status == 0x900D0008)
	{
		dbglog("# CID: ");
		for (int i = 0; i < 16; ++i)
		{
			dbglog("%02X", cid[i]);
		}
		dbglog(" ");
		switch (cid[0])
		{
			case 0x15:
				dbglog("Samsung");
				if (!memcmp(&cid[3], "BJTD4R", 6))
					dbglog(" KLMBG2JETD-B041 32GB");
				else if (!memcmp(&cid[3], "BJNB4R", 6))
					dbglog(" KLMBG2JENB-B041 32GB");
				break;
			case 0x11:
				dbglog("Toshiba");
				if (!memcmp(&cid[3], "032G32", 6))
					dbglog(" THGBMHG8C2LBAIL 32GB");
				break;
			case 0x90:
				dbglog("Hynix");
				if (!memcmp(&cid[3], "hB8aP>", 6))
					dbglog(" H26M62002JPR 32GB");
				break;
		}
		dbglog("\n");
	}
	dbglog("# Status: %X\n", status);
}

void dbg_logger_new_config_and_save(glitch_cfg_t *new_cfg, int save_ret)
{
	dbglog("new cfg: [%d, %d.%d] save res: %x\n", new_cfg->offset, new_cfg->width, new_cfg->subcycle_delay, save_ret);
}

void dbg_logger_glitch_result(glitch_cfg_t *new_cfg, uint8_t glitch_res, uint8_t mmc_flags, unsigned int datalen, uint8_t *data, uint8_t glitch_flags)
{
	dbglog("glitch info: [%d, %d, %d] {%d} %x %x ", new_cfg->offset, new_cfg->width, new_cfg->subcycle_delay, glitch_res, mmc_flags, glitch_flags);
	for (int i = 0; i < datalen; ++i)
	{
		dbglog("%02X", data[i]);
	}
	dbglog("\n");
}

void dbg_logger_end()
{
	dbglog("Done.\n");
}

void dbg_logger_adc(uint32_t value)
{
	dbglog("adc: %d (flag %x)\n", value & 0xFFFF, value >> 16);
}

void dbg_logger_stats(uint32_t attempt, uint16_t offset, uint8_t width, uint8_t subcycle, uint8_t needs_reflash)
{
	dbglog("Attempt: %d, offset: %d, width: %d, subcycle: %d,  needs_reflash: %d\n", offset, width, subcycle, needs_reflash);
}

logger dbg_logger =
{
	dbg_logger_start,
	dbg_logger_device_type,
	dbg_logger_glitching_started,
	dbg_logger_payload_flash_res_and_cid,
	dbg_logger_new_config_and_save,
	dbg_logger_glitch_result,
	dbg_logger_end,
	dbg_logger_adc,
	dbg_logger_stats
};

int wait_for_power_on(enum DEVICE_TYPE *pdt)
{
	enum DEVICE_TYPE device = detect_device_type();
	*pdt = device;
	struct adc_param ap;

	int ret = init_device_specific_adc(device, &ap);
	if (ret)
		return ret;

	ret = adc_wait_for_min_value(&dbg_logger, ap.poweron_threshold, 0);
	if (ret)
		return ret;

	return OK_FPGA_RESET;
}

int reset_device_and_wait_for_power_on()
{
	enum DEVICE_TYPE  device = detect_device_type();
	fpga_reset_device(0);

	struct adc_param ap;
	int ret = init_device_specific_adc(device, &ap);
	if (ret)
		return ret;

	ret = adc_wait_for_min_value(&dbg_logger, ap.poweron_threshold, 0);
	if (ret)
		return ret;

	return OK_FPGA_RESET;
}

void debug_led_blink_success()
{
	// Green for 2s, then back to USB indicator
	led_pattern_t green = {solid, 0x00, 0xFF, 0x00};
	leds_set_pattern(&green);
	leds_set_pattern_delayed(&lp_usb, 2000);
}

void debug_main(struct bootloader_usb *usb)
{
	g_usb = usb;

	delay_init();
	clocks_init();
	timer_global_init();
	leds_set_pattern(&lp_usb);
	leds_init();
	clock_output_init();
	fpga_init();
	board_id_init();

	uint8_t first = 1;
	while (1)
	{
		int received_len;
		if (!first)
		{
			do
			{
				g_usb->wait_till_ready();
				received_len = g_usb->receive_data();
			}
			while (!received_len);
		}
		else
		{
			received_len = 1;
			first = 0;
		}
		dbglog("> %c\n", g_usb->receive_buffer[0]);

		switch (g_usb->receive_buffer[0])
		{
			case 'v':
			{
				enum BOARD_ID bid = board_id_get();
				if (bid == BOARD_ID_CORE)
					dbglog("# Board ID: SX Core, ");
				else if (bid == BOARD_ID_LITE)
					dbglog("# Board ID: SX Lite, ");
				else
					dbglog("# Board ID: Unknown, ");

				fpga_reset();
				uint32_t fpga_type = fpga_read_type();
				char fpgaid[5];
				memcpy(fpgaid, &fpga_type, 4);
				fpgaid[4] = '\0';
				dbglog(" FPGA ID: %s\n", fpgaid);
				break;
			}
			case 'p':
			{
				dbglog("# Programming eMMC payload...\n");
				enum STATUSCODE status = fpga_reset();
				uint8_t cid[16];
				if (status == OK_FPGA_RESET)
				{
					enum DEVICE_TYPE dt;
					status = wait_for_power_on(&dt);
					if (status == OK_FPGA_RESET)
					{
						status = flash_payload(cid, dt);
						if (status == OK_FLASH_SUCCESS)
							debug_led_blink_success();
					}
					else
						leds_set_pattern(&lp_err_adc);
				}
				else
					leds_set_pattern(&lp_err_adc);

				dbg_logger_payload_flash_res_and_cid(status, cid);
				if (status == ERR_UNKNOWN_DEVICE)
					dbglog("# Please make sure console is powered on\n");
				break;
			}
			case 'd':
			{
				dbglog("# Diagnosing...\n");

				enum STATUSCODE status = fpga_reset();
				session_info_t si = {0};
				if (status == OK_FPGA_RESET)
					status = glitch(&dbg_logger, &si, false);

				dbglog("# Diagnose status: %08X\n", status);
				if (status == ERR_UNKNOWN_DEVICE)
					dbglog("# Please make sure console is powered on\n");
				else if (status == OK_GLITCH_SUCCESS)
				{
					debug_led_blink_success();
					dbglog("# Success!\n");
#if 0
					fpga_enter_cmd_mode();
					dbglog("# Wait for command...\n");

					uint8_t recv_buffer[512];
					uint8_t resp_buffer[512];
					do
					{
						fpga_pre_recv();
						fpga_select_active_buffer(FPGA_BUFFER_CMD_DATA);
						fpga_read_buffer(recv_buffer, sizeof(recv_buffer));
						fpga_post_recv();
						dbglog("# Got command: %x %x %x\n", recv_buffer[0], recv_buffer[1], recv_buffer[2]);
						if (recv_buffer[0] != 0xAA)
						{
							resp_buffer[0] = ~recv_buffer[0];
							*(uint32_t *) &resp_buffer[1] = 0x70000000;
							fpga_select_active_buffer(FPGA_BUFFER_CMD_DATA);
							fpga_write_buffer(resp_buffer, sizeof(resp_buffer));
							fpga_post_send();
						}
					}
					while (recv_buffer[0] != 0xBB);
#endif
				}
				break;
			}

			case 's':
			{
				dbglog("# Diagnosing into SDIO handler...\n");

				enum STATUSCODE status = fpga_reset();
				session_info_t si = {0};
				if (status == OK_FPGA_RESET)
					status = glitch(&dbg_logger, &si, false);

				dbglog("# Diagnose status: %08X\n", status);
				if (status == ERR_UNKNOWN_DEVICE)
					dbglog("# Please make sure console is powered on\n");
				else if (status == OK_GLITCH_SUCCESS)
				{
					debug_led_blink_success();
					dbglog("# Running SDIO handler!\n");
					sdio_handler();
					dbglog("Deep sleep received, SDIO handler done\n");
					leds_set_pattern(&lp_usb);
				}
				break;
			}

			case 'b':
			{
				dbglog("# Boot\n");
				enum STATUSCODE status = fpga_reset();
				if (status == OK_FPGA_RESET)
					status = reset_device_and_wait_for_power_on();
				dbglog("# Status: %08X\n", status);
				if (status == OK_FPGA_RESET)
				{
					debug_led_blink_success();
					dbglog("# Success!\n");
				}
				else
					leds_set_pattern(&lp_err_adc);
				break;
			}
			case 'r':
			{
				dbglog("# Resetting to factory settings...\n");
				dbglog("# Status: %08X\n", config_reset());
				leds_set_pattern(&lp_config_reset);
				leds_set_pattern_delayed(&lp_usb, 2000);
				break;
			}
			case 't':
			{
				dbglog("# Training...\n");
				uint32_t status = fpga_reset();
				if (status == OK_FPGA_RESET)
				{
					int trains_left = 50;
					session_info_t si = {0};
					do
					{
						status = glitch(&dbg_logger, &si, true);
						if (status == OK_GLITCH_SUCCESS)
						{
							trains_left--;
							leds_override(100 , &lp_glitch_done);
							dbglog("Train step successful; steps left: %d\n", trains_left);
						}
					} while (trains_left && status == OK_GLITCH_SUCCESS);

					if (trains_left == 0)
					{
						dbglog("# Success; all training attempts completed!\n");
					}
				}
				if (status == ERR_UNKNOWN_DEVICE)
				{
					dbglog("# Please make sure console is powered on\n");
				}
				break;
			}
			case 'c':
			{
				config_t cfg;
				uint32_t status = config_load(&cfg);
				dbglog("# Status: %08X\n", status);
				if (status == OK_CONFIG)
				{
					dbglog("# Config count: %d\n", cfg.count);
					for (int i = 0; i < cfg.count; ++i)
						dbglog("# %02d: [%d, %d] %d\n", i, cfg.timings[i].offset, cfg.timings[i].width, cfg.timings[i].success);
				}
				break;
			}
			case 'e':
			{
				dbglog("# Erasing eMMC payload...\n");
				uint32_t status = fpga_reset();
				if (status == OK_FPGA_RESET)
				{
					enum DEVICE_TYPE dt;
					status = wait_for_power_on(&dt);
					if (status == OK_FPGA_RESET)
					{
						status = erase_payload();
						if (status == OK_FLASH_SUCCESS)
							debug_led_blink_success();
					}
					dbglog("# Status: %08X\n", status);
				}
				else
				{
					dbglog("# Please make sure console is powered on\n");
					leds_set_pattern(&lp_err_adc);
				}
				break;
			}
			case 'x':
			{
				SCB->VTOR = 0x8000000;
				typedef  void  (*app_func) ();
				app_func *application = (app_func *) (0x8000000 + 4);
				(*application)(usb);
				break;
			}
			case 'h':
			{
				dbglog("# Debug menu keys overview\n");
				dbglog("   'v'  Show modchip board info\n");
				dbglog("   'd'  Diagnose single boot\n");
				dbglog("   's'  Boot into SDIO handler\n");
				dbglog("   'b'  Boot OFW\n");
				dbglog("   't'  (Re-)train modchip\n");
				dbglog("   'c'  Show timing configuration table\n");
				dbglog("   'r'  Reset timing configuration table\n");
				dbglog("   'p'  Program eMMC with embedded payload\n");
				dbglog("   'e'  Erase eMMC BOOT0 payload\n");
				dbglog("   'x'  Jump to bootloader\n");
				dbglog("   'h'  Show this help text\n");
				dbglog("# ========================\n");
				break;
			}

			default:
				dbglog("Unrecognized input. Press 'h' for help.\n");
				break;
		}
	}
}
