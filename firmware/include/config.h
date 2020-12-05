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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <fpga.h>

typedef struct
{
    uint16_t offset;
    uint8_t width;
    uint32_t success;
} timing_t;

typedef struct 
{
    uint32_t magic;
    uint32_t idx;
    timing_t timings[32];
    uint8_t reflash;
} config;

void config_clear(config *cfg);
uint32_t config_load(config *cfg);
uint32_t config_add_new(config *cfg, struct glitch_config *new_cfg);
uint32_t config_save(config *cfg);
uint32_t config_reset();

#endif