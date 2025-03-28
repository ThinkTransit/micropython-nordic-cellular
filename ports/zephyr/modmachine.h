/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Jim Mussared
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H
#define MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H

#include "py/obj.h"
#include "py/mphal.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>

typedef struct _machine_pin_obj_t {
    mp_obj_base_t base;
    qstr name;
    const struct device *port;
    gpio_pin_t pin;
} machine_pin_obj_t;

typedef struct _machine_hard_spi_obj_t {
    mp_obj_base_t base;
    qstr name;
    const struct device *device;
    struct spi_config config;
} machine_hard_spi_obj_t;

typedef struct _machine_hard_i2c_obj_t {
    mp_obj_base_t base;
    qstr name;
    const struct device *device;
    bool restart;
} machine_hard_i2c_obj_t;

extern const mp_obj_dict_t machine_pin_cpu_pins_locals_dict;
extern const mp_obj_dict_t machine_pin_board_pins_locals_dict;

extern const mp_obj_type_t machine_pin_type;
extern const mp_obj_type_t machine_i2c_type;
extern const mp_obj_type_t machine_spi_type;
extern const mp_obj_type_t machine_uart_type;

// Include all of the devicetree objects
#include "genhdr/machine_defs.h"

MP_DECLARE_CONST_FUN_OBJ_0(machine_info_obj);

void machine_pin_deinit(void);

#endif // MICROPY_INCLUDED_ZEPHYR_MODMACHINE_H
