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

#include <payload.h>
#include <mmc.h>
#include <string.h>
#include <leds.h>
#include <statuscode.h>
#include "erista_bct.h"
#include "mariko_bct.h"
#include "payload.h"

enum STATUSCODE flash_payload(uint8_t *cid, enum DEVICE_TYPE cpu_type)
{
	leds_set_pattern(&lp_flash_payload);
	uint32_t ret = ERR_FLASH_PAYLOAD_FAIL;
	int retry = 6;
	while (--retry)
	{
		ret = mmc_initialize(cid);
		if (ret)
			continue;

		if (cpu_type == DEVICE_TYPE_ERISTA)
		{
			ret = mmc_check_and_if_different_write(0, erista_bct, sizeof(erista_bct));
			if (ret)
				continue;
			ret = mmc_check_and_if_different_write(0x20, erista_bct, sizeof(erista_bct));
			if (ret)
				continue;
		}
		else
		{
			// Check and replace 1st BCT with custom one if needed.
			ret = mmc_check_and_if_different_write(0, mariko_bct, sizeof(mariko_bct));
			if (ret)
				continue;

			// Check and replace 2nd BCT with custom one if needed.
			ret = mmc_check_and_if_different_write(0x20, mariko_bct, sizeof(mariko_bct));
			if (ret)
				continue;

			// Check and replace 3rd BCT with official one if header is wrong.
			ret = mmc_check_and_if_header_different_write_all(0x40, bct_mariko_1500, sizeof(bct_mariko_1500));
			if (ret)
				continue;

			// Check and replace 4th BCT with official one if header is wrong.
			ret = mmc_check_and_if_header_different_write_all(0x60, bct_mariko_1500, sizeof(bct_mariko_1500));
			if (ret)
				continue;
		}

		ret = mmc_check_and_if_different_write(0x1F80, payload, sizeof(payload));
		if (!ret)
			return OK_FLASH_SUCCESS;
	}

	if (ret && ret != OK_FLASH_SUCCESS)
		leds_set_pattern(&lp_err_emmc);

	return ret;
}

enum STATUSCODE erase_payload()
{
	leds_set_pattern(&lp_flash_payload);
	uint32_t ret = ERR_FLASH_PAYLOAD_FAIL;
	int retry = 6;
	while (--retry)
	{
		ret = mmc_initialize(0);
		if (!ret)
		{
			ret = mmc_copy(0, 0x40, 0x2800);
			if (!ret)
			{
				ret = mmc_copy(0x20, 0x60, 0x2800);
				if (!ret)
				{
					ret = mmc_erase(0x1F80, 0x4000);
					if (!ret)
						return OK_FLASH_SUCCESS;
				}
			}
		}
	}

	if (ret && ret != OK_FLASH_SUCCESS)
		leds_set_pattern(&lp_err_emmc);

	return ret;
}