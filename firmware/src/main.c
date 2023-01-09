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
#include <delay.h>
#include <board.h>
#include <statuscode.h>
#include <leds.h>
#include <fpga.h>
#include <board_id.h>
#include <adc.h>
#include <glitch.h>
#include <clock.h>
#include <sdio.h>
#include <timer.h>
#include <session_info.h>

void systick_irq_config(void)
{
	SysTick->CTRL |= (1UL << 1);
	nvic_priority_group_set(NVIC_PRIGROUP_PRE2_SUB2);
	nvic_irq_enable((uint8_t)SysTick_IRQn, 1U, 0U);
}

void systick_irq_disable(void)
{
	SysTick->CTRL &= ~(1UL << 1);
	nvic_irq_disable((uint8_t)SysTick_IRQn);
}

void enter_sleep()
{
	systick_irq_disable();
	leds_off();
	fpga_power_off();
	while (1)
		pmu_to_standbymode(WFI_CMD);
}

void firmware_main()
{
	delay_init();
	systick_irq_config();
	timer_global_init();
	clocks_init();
	leds_init();
	clock_output_init();
	fpga_init();
	board_id_init();
	adc_init(CONSOLE_STATE_ADC_PORT, CONSOLE_STATE_ADC_PIN, 3);
	g_session_info.startup_adc_value = adc_wait_eoc_read();

	int syncAttempt = 100;
	SCB->CCR = SCB->CCR & ~(1 << 3); // no hardfault on UA

	while (1)
	{
		gpio_mode_set(MCU_RESET_CONFIG_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, MCU_RESET_CONFIG_PIN);
		delay_ms(5u);
		int pinWhilePulledUp = gpio_input_bit_get(MCU_RESET_CONFIG_PORT, MCU_RESET_CONFIG_PIN);

		// configure GPIO to input with pull-down
		gpio_mode_set(MCU_RESET_CONFIG_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, MCU_RESET_CONFIG_PIN);
		delay_ms(5u);
		int pinWhilePulledDown = gpio_input_bit_get(MCU_RESET_CONFIG_PORT, MCU_RESET_CONFIG_PIN);

		if (pinWhilePulledDown && pinWhilePulledUp && syncAttempt < 95)
		{
			fpga_sync_failed = 1;
			break;
		}
		if (pinWhilePulledUp == 1) // FPGA was driving it high
		{
			break;
		}
		if (syncAttempt == 0)
		{
			config_reset();
			leds_set_pattern(&lp_config_reset);
			while (1) ;
		}
		--syncAttempt;
	}

	if (fpga_reset() != OK_FPGA_RESET)
	{
		leds_set_pattern(&lp_err_fpga);
		while (1);
	}

	if (g_session_info.startup_adc_value < 1596)
	{
		config_t cfg;
		if (config_load(&cfg) == ERR_CONFIG_NOT_FILLED)
		{
			int trains_left = 50;
			uint32_t status;
			do
			{
				session_info_t local_si = {0};
				status = glitch(&null_logger, &local_si, true);
				if (status == OK_GLITCH_SUCCESS)
				{
					trains_left--;
					leds_override(100, &lp_glitch_done); // green blink
				}
			} while (trains_left && (status != ERR_UNKNOWN_DEVICE && status != ERR_MMC_STATE_UNEXPECTED_NOT_IDENT));
		}

		enum STATUSCODE status = glitch(&null_logger, &g_session_info, false);
		systick_irq_disable();

		if (status == OK_GLITCH_SUCCESS)
			sdio_handler();

		fpga_power_off(); // so cannot interfere with eMMC
		if (status == OK_GLITCH_SUCCESS)
		{
			leds_set_pattern_delayed(&lp_off, 2000);
			delay_ms(2000);
		}
		else
			delay_ms(5000); // slightly longer LED visible for error
	}
	enter_sleep();
}
