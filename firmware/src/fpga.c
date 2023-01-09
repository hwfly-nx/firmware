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
#include <fpga.h>
#include <board.h>
#include <delay.h>
#include <statuscode.h>
#include <string.h>

int fpga_sync_failed = 1;

int payload_not_yet_flashed = 1;

void fpga_init_spi(int prescale)
{
	spi_parameter_struct spi_struct;
	spi_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
	spi_struct.device_mode = SPI_MASTER;
	spi_struct.frame_size = SPI_FRAMESIZE_8BIT;
	spi_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE; // idle is high, pick up at posedge
	spi_struct.nss = SPI_NSS_SOFT; // possibly: we control the slave select manually
	spi_struct.prescale = prescale; // fpga_reset(): div by 4 (SPI_PSC_4), fpga_init() div by 2 (SPI_PSC_2)
	spi_struct.endian = SPI_ENDIAN_MSB;
	spi_i2s_deinit(SPI0);
	spi_init(SPI0, &spi_struct);
	spi_ti_mode_disable(SPI0); // slave select (SS) pin is low the entire time we transmit
	spi_enable(SPI0);
}

void fpga_init()
{
	fpga_init_spi(SPI_PSC_2);
	gpio_bit_reset(FPGA_PWR_EN_PORT, FPGA_PWR_EN_PIN);
	gpio_output_options_set(FPGA_PWR_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FPGA_PWR_EN_PIN);
	gpio_bit_set(FPGA_CS_GPIO_PORT, FPGA_CS_GPIO_PIN);
	gpio_bit_set(FPGA_SCK_GPIO_PORT, FPGA_SCK_GPIO_PIN);
	gpio_bit_set(FPGA_MOSI_GPIO_PORT, FPGA_MOSI_GPIO_PIN);
	gpio_bit_set(FPGA_MISO_GPIO_PORT, FPGA_MISO_GPIO_PIN);
	gpio_output_options_set(FPGA_CS_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FPGA_CS_GPIO_PIN);
	gpio_output_options_set(FPGA_SCK_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FPGA_SCK_GPIO_PIN);
	gpio_output_options_set(FPGA_MOSI_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FPGA_MOSI_GPIO_PIN);
	gpio_output_options_set(FPGA_MISO_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FPGA_MISO_GPIO_PIN);
	gpio_af_set(FPGA_SCK_GPIO_PORT, GPIO_AF_0, FPGA_SCK_GPIO_PIN);
	gpio_af_set(FPGA_MISO_GPIO_PORT, GPIO_AF_0, FPGA_MISO_GPIO_PIN);
	gpio_af_set(FPGA_MOSI_GPIO_PORT, GPIO_AF_0, FPGA_MOSI_GPIO_PIN);
	gpio_mode_set(FPGA_SCK_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, FPGA_SCK_GPIO_PIN);
	gpio_mode_set(FPGA_MISO_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, FPGA_MISO_GPIO_PIN);
	gpio_mode_set(FPGA_MOSI_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, FPGA_MOSI_GPIO_PIN);
	gpio_mode_set(FPGA_STATUS_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, FPGA_STATUS_PIN);
	gpio_mode_set(FPGA_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, FPGA_PWR_EN_PIN);
	gpio_mode_set(FPGA_CS_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, FPGA_CS_GPIO_PIN);
}

// spin as long as we're transmitting in background, then deselect SS (pull high)
void gpioa_set_pin4()
{
	while (SPI_STAT(SPI0) & SPI_STAT_TRANS)
		;
	gpio_bit_set(FPGA_CS_GPIO_PORT, FPGA_CS_GPIO_PIN);
}

// select SS (pull low); before start of SPI transfer
void gpioa_clear_pin4()
{
	gpio_bit_reset(FPGA_CS_GPIO_PORT, FPGA_CS_GPIO_PIN);
}

uint32_t fpga_reset()
{
	fpga_init_spi(8); // SPI_PSC_4 (XXX: weird!)

	gpio_bit_reset(FPGA_PWR_EN_PORT, FPGA_PWR_EN_PIN);
	gpioa_set_pin4();
	delay_us(300);
	gpio_bit_set(FPGA_PWR_EN_PORT, FPGA_PWR_EN_PIN);
	delay_ms(50);
	if (!gpio_input_bit_get(FPGA_STATUS_PORT, FPGA_STATUS_PIN))
		return ERR_FPGA_STATUS_FAIL;

	return OK_FPGA_RESET;
}

void fpga_power_off()
{
	gpio_bit_reset(FPGA_PWR_EN_PORT, FPGA_PWR_EN_PIN);
}

// send len bytes, ignoring any received data
void spi0_send(uint8_t *buf, int len)
{
	for (int i = 0; i < len; i++)
	{
		spi_i2s_data_transmit(SPI0, buf[i]);
		while ((SPI_STAT(SPI0) & (SPI_STAT_TBE | SPI_STAT_RBNE)) != (SPI_STAT_TBE | SPI_STAT_RBNE));
		spi_i2s_data_receive(SPI0);
	}
	while ((SPI_STAT(SPI0) & (SPI_STAT_TRANS | SPI_STAT_TBE | SPI_STAT_RBNE)) != SPI_STAT_TBE);
}

// send len bytes, rewriting the buf with received data
void spi0_spi_transfer_buffer(uint8_t *buf, int len)
{
	for (int i = 0; i < len; i++)
	{
		spi_i2s_data_transmit(SPI0, buf[i]);
		while ((SPI_STAT(SPI0) & (SPI_STAT_TBE | SPI_STAT_RBNE)) != (SPI_STAT_TBE | SPI_STAT_RBNE));
		buf[i] = spi_i2s_data_receive(SPI0);
	}
	while ((SPI_STAT(SPI0) & (SPI_STAT_TRANS | SPI_STAT_TBE | SPI_STAT_RBNE)) != SPI_STAT_TBE);
}

// send {0x24, subcmd, value}
void transfer_spi0_24_byte(uint8_t subcmd, uint8_t value)
{
	uint8_t buf[3];

	buf[0] = 0x24;
	buf[1] = subcmd;
	buf[2] = value;

	gpioa_clear_pin4();
	spi0_send(buf, sizeof(buf));
	gpioa_set_pin4();
}

// send {0x24, subcmd, value0, value1}
void transfer_spi0_24_word(uint8_t subcmd, uint16_t value)
{
	uint8_t buf[4];

	buf[0] = 0x24;
	buf[1] = subcmd;
	buf[2] = value;
	buf[3] = value >> 8;

	gpioa_clear_pin4();
	spi0_send(buf, sizeof(buf));
	gpioa_set_pin4();
}

// send {0x26, subcmd}, return buf[2]
uint8_t transfer_spi0_26_byte(uint8_t subcmd)
{
	uint8_t buf[3];
	buf[0] = 0x26;
	buf[1] = subcmd;
	gpioa_clear_pin4();
	spi0_spi_transfer_buffer(buf, sizeof(buf));
	gpioa_set_pin4();
	return buf[2];
}

// send {0x24, 0x6, value}
void transfer_spi0_24_6(uint8_t value)
{
	transfer_spi0_24_byte(0x6, value);
}

// send {0x24, 0x5, buffer}
// fpga.h says:
// FPGA_BUFFER_CMD = 0, // traffic on CMD line
// FPGA_BUFFER_CMD_DATA = 1, // data from host-->device commands, also toolkit comms
// FPGA_BUFFER_RESP_DATA = 2, // data from device-->host responses
void fpga_select_active_buffer(enum FPGA_BUFFER buffer)
{
	transfer_spi0_24_byte(0x5, buffer);
}

// send bunch of {0x24, 6, ...}
//
// do_clock_stuck_glitch is IMO incorrect. It forces CMD line low for about 2s,
// which gets the BPSP somehow stuck.
void fpga_reset_device(int do_clock_stuck_glitch)
{
	transfer_spi0_24_6(0x80);
	delay_ms(2);
	transfer_spi0_24_6(0);
	if (do_clock_stuck_glitch == 1)
	{
		delay_ms(15);
		transfer_spi0_24_6(0x40);
		delay_ms(2000);
		transfer_spi0_24_6(0);
		delay_ms(1);
	}
}

// send bunch of {0x24, 0x[12368], ...}
void fpga_glitch_device(glitch_cfg_t *cfg)
{
	transfer_spi0_24_6(0);
	transfer_spi0_24_word(0x1, cfg->offset);
	transfer_spi0_24_byte(0x2, cfg->width);
	transfer_spi0_24_byte(0x3, cfg->timeout);
	transfer_spi0_24_byte(0x8, cfg->subcycle_delay);
	transfer_spi0_24_6(0x80);
	delay_ms(1u);
	transfer_spi0_24_6(0x10);
}

// send {0x26, 0xA}, read back one byte
uint8_t fpga_read_glitch_flags()
{
	return transfer_spi0_26_byte(0xA);
}

// send {0x26, 0xB}, read back one byte
uint8_t fpga_read_mmc_flags()
{
	return transfer_spi0_26_byte(0xB);
}

// send {0xEE, 0x00, 0x00, 0x00, 0x00}, read back four bytes as the FPGA id
uint32_t fpga_read_type()
{
	uint8_t buf[5];
	buf[0] = 0xEE;
	gpioa_clear_pin4();
	spi0_spi_transfer_buffer(buf, sizeof(buf));
	gpioa_set_pin4();
	uint32_t res;
	memcpy(&res, buf + 1, sizeof(uint32_t));
	return res;
}

// send {0x54}
void fpga_do_mmc_command()
{
	uint8_t cmd = 0x54;
	gpioa_clear_pin4();
	spi0_send(&cmd, 1);
	gpioa_set_pin4();
}

// send {0xBA}, then read size into the buffer
void fpga_read_buffer(uint8_t *buffer, uint32_t size)
{
	uint8_t cmd = 0xBA;
	gpioa_clear_pin4();
	spi0_send(&cmd, 1);
	spi0_spi_transfer_buffer(buffer, size);
	gpioa_set_pin4();
}

// send {0xBC}, then write size from the buffer
void fpga_write_buffer(uint8_t *buffer, uint32_t size)
{
	uint8_t cmd = 0xBC;
	gpioa_clear_pin4();
	spi0_send(&cmd, 1);
	spi0_send(buffer, size);
	gpioa_set_pin4();
}

// send {0x24, 0x6, 0x4} then {0x24, 0x6, 0x1}
void fpga_enter_cmd_mode()
{
	transfer_spi0_24_6(4);
	transfer_spi0_24_6(1);
}

// spinloop send {0x26, 0xB} until FPGA_MMC_BUSY_LOADER_DATA_RCVD clears
void fpga_pre_recv()
{
	while (!(fpga_read_mmc_flags() & FPGA_MMC_BUSY_LOADER_DATA_RCVD));
}

// send {0x24, 0x6, 5}
void fpga_post_recv()
{
	transfer_spi0_24_6(5);
}

// send {0x24, 0x6, 3}
void fpga_post_send()
{
	transfer_spi0_24_6(3);
}
