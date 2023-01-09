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

#ifndef __BOARD_H__
#define __BOARD_H__

// PC13 - GND?
// PA0 - GND?
#define FPGA_STATUS_PORT			GPIOA
#define FPGA_STATUS_PIN				GPIO_PIN_1
#define SWITCH_LITE_ADC_GPIO_PORT	GPIOA
#define SWITCH_LITE_ADC_GPIO_PIN	GPIO_PIN_2

#define CONSOLE_STATE_ADC_PORT		GPIOA
#define CONSOLE_STATE_ADC_PIN		GPIO_PIN_3
#define FPGA_CS_GPIO_PORT			GPIOA
#define FPGA_CS_GPIO_PIN			GPIO_PIN_4
#define FPGA_SCK_GPIO_PORT		  	GPIOA
#define FPGA_SCK_GPIO_PIN			GPIO_PIN_5
#define FPGA_MISO_GPIO_PORT			GPIOA
#define FPGA_MISO_GPIO_PIN			GPIO_PIN_6
#define FPGA_MOSI_GPIO_PORT			GPIOA
#define FPGA_MOSI_GPIO_PIN			GPIO_PIN_7
#define SWITCH_ERISTA_ADC_GPIO_PORT	GPIOB
#define SWITCH_ERISTA_ADC_GPIO_PIN	GPIO_PIN_0
#define SWITCH_MARIKO_ADC_GPIO_PORT	GPIOB
#define SWITCH_MARIKO_ADC_GPIO_PIN	GPIO_PIN_1
// PB2 - BOOT1
#define FPGA_PWR_EN_PORT			GPIOB
#define FPGA_PWR_EN_PIN				GPIO_PIN_10
// PB11 - GND?

#define LIS3DH_CS_GPIO_PORT			GPIOB
#define LIS3DH_CS_GPIO_PIN			GPIO_PIN_12
#define LIS3DH_SCK_GPIO_PORT		GPIOB
#define LIS3DH_SCK_GPIO_PIN			GPIO_PIN_13
#define LIS3DH_MISO_GPIO_PORT		GPIOB
#define LIS3DH_MISO_GPIO_PIN		GPIO_PIN_14
#define LIS3DH_MOSI_GPIO_PORT		GPIOB
#define LIS3DH_MOSI_GPIO_PIN		GPIO_PIN_15
#define CK_OUT_GPIO_PORT			GPIOA
#define CK_OUT_GPIO_PIN				GPIO_PIN_8
#define USB_POWER_DETECT_GPIO_PORT	GPIOA
#define USB_POWER_DETECT_GPIO_PIN	GPIO_PIN_9
#define LED_BLUE_GPIO_PORT			GPIOA
#define LED_BLUE_GPIO_PIN			GPIO_PIN_10
// PA11 - USB_DM
// PA12 - USB_DP
// PA13 - SWD_IO
#define DEBUG_GPIO_PORT				GPIOF
#define DEBUG_GPIO_PIN				GPIO_PIN_6
#define MCU_RESET_CONFIG_PORT		GPIOF
#define MCU_RESET_CONFIG_PIN		GPIO_PIN_7

// PA14 - SWD_CLK
// PA15 - ???
#define BOARDID_A_GPIO_PORT			GPIOB
#define BOARDID_A_GPIO_PIN			GPIO_PIN_3
// PB4 - NTRST - GND?
#define BOARDID_B_GPIO_PORT			GPIOB
#define BOARDID_B_GPIO_PIN			GPIO_PIN_5
#define USART0_TX_GPIO_PORT			GPIOB
#define USART0_TX_GPIO_PIN			GPIO_PIN_6
#define USART0_RX_GPIO_PORT			GPIOB
#define USART0_RX_GPIO_PIN			GPIO_PIN_7
#define LED_RED_GPIO_PORT			GPIOB
#define LED_RED_GPIO_PIN			GPIO_PIN_8
#define LED_GREEN_GPIO_PORT			GPIOB
#define LED_GREEN_GPIO_PIN			GPIO_PIN_9

#endif
