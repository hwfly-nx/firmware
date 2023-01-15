/*
 * Copyright (c) 2020 Spacecraft-NX
 * Copyright (c) 2022 HWFLY-NX
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

#include "mmc_defs.h"
#include <adc.h>
#include <board_id.h>
#include <config.h>
#include <statuscode.h>
#include <device.h>
#include <fpga.h>
#include <glitch.h>
#include <glitch_heuristic.h>
#include <leds.h>
#include <mmc_sniffer.h>
#include <payload.h>
#include <sdio.h>
#include <string.h>
#include <timer.h>

#define ASSERTZERO(cond) { int __test; do { __test = cond; if (__test) return __test; } while (0); }
#define MAX_GLITCH_WIDTH 85
#define MIN_GLITCH_WIDTH 15
#define START_GLITCH_WIDTH ((MAX_GLITCH_WIDTH + MIN_GLITCH_WIDTH) / 2)

enum STATUSCODE glitch_prepare(logger *lgr, session_info_t *session_info, unsigned int *adc_goal);
enum STATUSCODE glitch_reuse_offsets(logger *lgr, session_info_t *session_info, unsigned int adc_goal);
enum STATUSCODE glitch_search_new_offset(logger *lgr, session_info_t *session_info, unsigned int adc_goal);

enum GLITCH_RESULT_TYPE glitch_attempt(logger *lgr, session_info_t *session_info, glitch_cfg_t *glitch_cfg);
enum STATUSCODE flash_payload_and_update_config(logger *lgr, session_info_t *session_info);

int read_glitch_result(uint8_t *buf)
{
	if (fpga_read_mmc_flags() & FPGA_MMC_GLITCH_DT_CAPTURED)
	{
		fpga_select_active_buffer(FPGA_BUFFER_CMD);
		fpga_read_buffer(buf, 512);
		int datalen = buf[0x10];

		fpga_select_active_buffer(FPGA_BUFFER_RESP_DATA);
		fpga_read_buffer(buf, 512);
		return datalen;
	}
	return 0;
}

enum STATUSCODE glitch(logger *lgr, session_info_t *session_info, bool is_training)
{
	lgr->start();
	leds_set_pattern_delayed(is_training ? &lp_train_prepare : &lp_glitch_prepare, 300);
	timer2_init();

	enum STATUSCODE result;
	for (;;)
	{
		unsigned int adc_goal;
		result = glitch_prepare(lgr, session_info, &adc_goal);
		if (result != OK)
			break;

		session_info->power_threshold_reached_us = timer2_get_total();

		// Check if payload must be flashed
		config_t cfg;
		bool flash_payload = config_load(&cfg) != OK_CONFIG || cfg.reflash;
		if (flash_payload)
		{
			result = flash_payload_and_update_config(lgr, session_info);
			if (result != OK_FLASH_SUCCESS)
				break;
		}

		lgr->glitching_started();
		leds_set_pattern(is_training ? &lp_train_glitching : &lp_glitch_glitching);
		result = glitch_reuse_offsets(lgr, session_info, adc_goal);
		if (result != OK_GLITCH_SUCCESS)
			result = glitch_search_new_offset(lgr, session_info, adc_goal);

		break;
	}

	lgr->end();

	// Set LED to color indicative of glitch result
	switch (result)
	{
		case OK_GLITCH_SUCCESS:
			leds_set_pattern(&lp_glitch_done);
			break;
		case ERR_GLITCH_NO_EMMC_COMM:
			leds_set_pattern(&lp_err_emmc);
			break;
		case ERR_GLITCH_TOO_MANY_ATTEMPTS:
			leds_set_pattern(&lp_err_exhausted);
			break;
		case ERR_ADC_WAIT_TIMEOUT:
		case ERR_UNKNOWN_DEVICE:
			leds_set_pattern(&lp_err_adc);
			break;
		default:
			leds_set_pattern(&lp_err_unknown);
			break;
	}

	return result;
}

enum STATUSCODE glitch_prepare(logger *lgr, session_info_t *session_info, unsigned int *adc_goal)
{
	session_info->device_type = detect_device_type();
	session_info->board_id = board_id_get();
	session_info->fpga_type = fpga_read_type();

	lgr->device_type(session_info->device_type);
	struct adc_param adc_min_values = {0};
	ASSERTZERO(init_device_specific_adc(session_info->device_type, &adc_min_values));

	// Wait until console reaches state where we can begin to attempt glitching
	*adc_goal = 0;
	session_info->was_the_device_reset = 0;
	int ret;
	do
	{
		uint16_t adc_read;
		ret = adc_wait_for_min_value(lgr, adc_min_values.glitch_threshold, &adc_read);

		if (adc_read >= adc_min_values.glitch_threshold)
		{
			*adc_goal = adc_min_values.glitch_threshold;
			return 0;
		}
		else if (adc_read >= adc_min_values.poweron_threshold)
		{
			*adc_goal = adc_min_values.poweron_threshold;
			return 0;
		}

		// Reset to retry. Perform longer reset every 2nd try.
		fpga_reset_device(session_info->was_the_device_reset & 1);
		session_info->was_the_device_reset++;
	} while (!ret || session_info->was_the_device_reset < 5);

	return ret;
}

enum STATUSCODE glitch_reuse_offsets(logger *lgr, session_info_t *session_info, unsigned int adc_goal)
{
	config_t cfg;
	config_load(&cfg);
	bool fatal_abort = false;
	session_info->glitch_attempt = 0;

	// Loop through known glitch configs that worked in the past
	for (int i = 0; i < cfg.count && !fatal_abort; ++i)
	{
		// Load stored config
		glitch_cfg_t glitch_cfg;
		glitch_cfg.offset = cfg.timings[i].offset;
		glitch_cfg.width = cfg.timings[i].width;
		glitch_cfg.subcycle_delay = 0;
		glitch_cfg.timeout = 100;

		// Allow each config to be rejected by the heuristic 3x before moving on
		const unsigned int retries_per_config = 3;
		for (int j = 0; !fatal_abort && j < retries_per_config; ++j)
		{
			// Initialize heuristic which will inform how to adjust pulse width
			// and when to move on to next offset.
			glitch_heuristic_t heuristic = {0};
			bool next_offset = false;
			do
			{
				// Wait until device is ready to be glitched, reset if necessary.
				if (adc_wait_eoc_read() < adc_goal)
				{
					ASSERTZERO(adc_wait_for_min_value(lgr, adc_goal, 0));
					session_info->adc_goal_reached_us = timer2_get_total();
				}

				// Perform glitch attempt and add result to heuristic
				enum GLITCH_RESULT_TYPE res = glitch_attempt(lgr, session_info, &glitch_cfg);
				if (res == GLITCH_RESULT_SUCCESS)
					return OK_GLITCH_SUCCESS;

				heuristic_add_result(&heuristic, res);

				// Query heuristic for advice on how to continue
				int width_adjust, offset_adjust;
				heuristic_advice(&heuristic, &fatal_abort, &next_offset, &width_adjust, &offset_adjust);
				glitch_cfg.width += width_adjust;
				glitch_cfg.offset += offset_adjust;
				glitch_cfg.subcycle_delay = (glitch_cfg.subcycle_delay + 1) & 3;
			} while (!fatal_abort && !next_offset);
		}
	}

	// Exhausted options
	return ERR_GLITCH_TOO_MANY_ATTEMPTS;
}

enum STATUSCODE glitch_search_new_offset(logger *lgr, session_info_t *session_info, unsigned int adc_goal)
{
	const uint16_t erista_offsets[] = {825, 830, 835, 840, 845, 850, 855, 860, 865, 870, 875, 880, 885, 890, 895, 900, 905};
	const uint16_t mariko_offsets[] = {800, 805, 810, 815, 820, 825, 830, 835, 840, 845, 850, 855, 860, 865, 870, 875, 880};

	const uint16_t *offsets;
	unsigned int offsets_count;
	if (session_info->device_type == DEVICE_TYPE_ERISTA)
	{
		offsets = erista_offsets;
		offsets_count = sizeof(erista_offsets) / sizeof(erista_offsets[0]);
	}
	else
	{
		offsets = mariko_offsets;
		offsets_count = sizeof(mariko_offsets) / sizeof(mariko_offsets[0]);
	}

	int offset_idx = offsets_count / 2; // Start in the center of window; this helps converging to good pulse width quickly.
	glitch_cfg_t glitch_cfg;
	glitch_cfg.width = START_GLITCH_WIDTH;
	glitch_cfg.subcycle_delay = 0;
	glitch_cfg.timeout = 100;

	bool fatal_abort = false;
	bool reflash = false;

	const unsigned int max_glitch_attempts = 1200;
	for (session_info->glitch_attempt = 0; !fatal_abort && session_info->glitch_attempt <= max_glitch_attempts; )
	{
		glitch_cfg.offset = offsets[offset_idx];

		// Initialize heuristic which will inform how to adjust pulse width
		// and when to move on to next offset.
		glitch_heuristic_t heuristic = {0};
		bool next_offset = false;
		do
		{
			// Poor heuristic to determine whether to reflash payload to BOOT0..
			reflash |= (session_info->glitch_attempt % 400) == 0;
			if (reflash)
			{
				reflash = false;
				enum STATUSCODE flash_result = flash_payload_and_update_config(lgr, session_info);
				if (flash_result != OK_FLASH_SUCCESS)
					return flash_result;
			}

			// Wait until device is ready to be glitched, reset if necessary.
			if (adc_wait_eoc_read() < adc_goal)
			{
				ASSERTZERO(adc_wait_for_min_value(lgr, session_info->device_type == DEVICE_TYPE_LITE ? adc_goal : adc_goal - 100, 0));
				session_info->adc_goal_reached_us = timer2_get_total();
			}

			// Perform glitch attempt and add result to heuristic
			enum GLITCH_RESULT_TYPE res = glitch_attempt(lgr, session_info, &glitch_cfg);
			if (res == GLITCH_RESULT_SUCCESS)
				return OK_GLITCH_SUCCESS;

			heuristic_add_result(&heuristic, res);

			// Query heuristic for advice on how to continue
			int width_adjust, offset_adjust;
			heuristic_advice(&heuristic, &fatal_abort, &next_offset, &width_adjust, &offset_adjust);
			glitch_cfg.width += width_adjust;
			glitch_cfg.offset += offset_adjust;
			glitch_cfg.subcycle_delay = (glitch_cfg.subcycle_delay + 1) & 3;

			if (glitch_cfg.width > MAX_GLITCH_WIDTH || glitch_cfg.width < MIN_GLITCH_WIDTH)
			{
				// Reflash & recenter width
				reflash = true;
				glitch_cfg.width = START_GLITCH_WIDTH;
				break;
			}
		} while (!fatal_abort && !next_offset && session_info->glitch_attempt < max_glitch_attempts);

		offset_idx = (offset_idx + 1) % offsets_count;

	} // for

	return ERR_GLITCH_TOO_MANY_ATTEMPTS;
}

enum GLITCH_RESULT_TYPE glitch_attempt(logger *lgr, session_info_t *session_info, glitch_cfg_t *glitch_cfg)
{
	// Attempt single glitch attempt with given parameters
	// and categorize outcome using eMMC bus monitoring.
	session_info->glitch_attempt++;
	fpga_glitch_device(glitch_cfg);
	uint8_t mmc_flags;
	uint8_t glitch_flags;
	do
	{
		mmc_flags = fpga_read_mmc_flags();
		glitch_flags = fpga_read_glitch_flags();
	} while (!(mmc_flags & (FPGA_MMC_GLITCH_SUCCESS | FPGA_MMC_GLITCH_TIMEOUT)));

	uint8_t data[512];
	int datalen = read_glitch_result(data);

	if (mmc_flags & FPGA_MMC_GLITCH_SUCCESS)
	{
		// Success bit set. Skip eMMC traffic analysis.
		session_info->glitch_complete_us = timer2_get_total();
		lgr->glitch_result(glitch_cfg, GLITCH_RESULT_SUCCESS, mmc_flags, datalen, data, glitch_flags);

		// Confirm glitch success by awaiting command over eMMC bus.
		// This detects false-positives.
		fpga_enter_cmd_mode();
		unsigned int max_flag_reads = 200000; // takes about 1s
		unsigned int flag_reads;
		for (flag_reads = 0; flag_reads < max_flag_reads; flag_reads++)
		{
			if (fpga_read_mmc_flags() & FPGA_MMC_BUSY_LOADER_DATA_RCVD)
			{
				// read buffer so flags get cleared
				fpga_select_active_buffer(FPGA_BUFFER_CMD_DATA);
				data[0] = 0;
				fpga_read_buffer(data, 512);
				fpga_post_recv();
				break;
			}
		}

		if (flag_reads > 0)
		{
			session_info->glitch_confirm_us = timer2_get_total();
			session_info->flag_reads_before_glitch_confirmed = flag_reads;
			session_info->total_time_us = timer_get_global_total();
			session_info->glitch_cfg = *glitch_cfg;

			// Update config in flash
			config_t cfg;
			config_load(&cfg);
			if (config_add_new(&cfg, glitch_cfg) == 0x900D0007)
			{
				lgr->new_config_and_save(glitch_cfg, config_save(&cfg));
			}
			return GLITCH_RESULT_SUCCESS;
		}
		else
		{
			led_pattern_t blink_yellow = {blink, 0xC0, 0xFF, 0x00};
			leds_override(500, &blink_yellow);
			return GLITCH_RESULT_FAIL_TIMEOUT;
		}
	}
	else
	{
		// Analyse eMMC bus traffic to categorize glitch attempt result
		mmc_sniff_parser_ctx ctx;
		mmc_sniff_parser_init(&ctx, data, datalen);
		enum GLITCH_RESULT_TYPE glitch_res = (datalen >= 5) ? GLITCH_RESULT_FAIL_TIMEOUT : GLITCH_RESULT_FAIL_NO_EMMC_COMMS;
		enum MMC_SNIFFER_PACKET_TYPE sniffer_result;
		do
		{
			sniffer_result = mmc_sniff_parser_parse(&ctx);
			if (sniffer_result == MMC_SNIFF_PKT_TYPE_COMMAND && (ctx.cmd == MMC_READ_SINGLE_BLOCK || ctx.cmd == MMC_GO_IDLE_STATE))
				glitch_res = GLITCH_RESULT_FAILED_MMC;
		} while (sniffer_result != MMC_SNIFF_PKT_TYPE_INVALID);

		lgr->glitch_result(glitch_cfg, glitch_res, mmc_flags, datalen, data, glitch_flags);
		return glitch_res;
	}
}

static bool g_payload_flash_attempted = false;
enum STATUSCODE flash_payload_and_update_config(logger *lgr, session_info_t *session_info)
{
	// Prevent doing this multiple times per session. Failure always indicates improper wiring.
	if (!g_payload_flash_attempted)
	{
		g_payload_flash_attempted = true;

		// Clear flag from config if it was set
		config_t cfg;
		config_load(&cfg);
		if (cfg.reflash)
		{
			cfg.reflash = 0;
			config_save(&cfg);
		}

		led_pattern_t prev = leds_get_pattern();
		uint8_t cid[16];
		enum STATUSCODE result = flash_payload(cid, session_info->device_type);
		lgr->payload_flash_res_and_cid(result, cid);

		if (result == OK_FLASH_SUCCESS)
			session_info->payload_flashed = 1;

		leds_set_pattern(&prev);
		return result;
	}
	return OK_FLASH_SUCCESS; // skipped since we've been successful before
}
