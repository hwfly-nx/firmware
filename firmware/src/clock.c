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

#include <gd32f3x0.h>
#include <board.h>

void clocks_init()
{
	// FPGA, ADC (HDH), CK_OUT, USB_POWER_DETECT, LED
	rcu_periph_clock_enable(RCU_GPIOA);

	// FPGA, ADC (HAC/HAD), LIS3DH, BOARD_ID, LED
	rcu_periph_clock_enable(RCU_GPIOB);

	// FPGA
	rcu_periph_clock_enable(RCU_SPI0);

	// FPGA Sync
	rcu_periph_clock_enable(RCU_GPIOF);
}

// This clock is used by FPGA for its operation
void clock_output_init()
{
	// Original:
	// The `1` as second parameter is weird, because that messes up SCS
	// (setting it to "Reserved" if my reading of
	// libs/CMSIS/system_gd32f3x0.c is right)
	// rcu_ckout_config(RCU_CKOUTSRC_CKPLL_DIV2, 1);

	// This works. And IMO is correct:
	rcu_ckout_config(RCU_CKOUTSRC_CKPLL_DIV2, RCU_CKOUT_DIV1);

	// A few more for educational purposes:
	// If you stop the clock completely (see below), the FPGA is unable to
	// count (the glitch clock doesn't work, no emmc flags either, probably
	// the emmc comms is gone too)
	// rcu_ckout_config(RCU_CKOUTSRC_NONE, RCU_CKOUT_DIV1);
	//
	// Slowing it to 0.5 (below), glitching takes forever, but eventually
	// succeeds.
	// rcu_ckout_config(RCU_CKOUTSRC_CKPLL_DIV2, RCU_CKOUT_DIV2);

	gpio_mode_set(CK_OUT_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, CK_OUT_GPIO_PIN);
	gpio_af_set(CK_OUT_GPIO_PORT, GPIO_AF_0, CK_OUT_GPIO_PIN);
}
