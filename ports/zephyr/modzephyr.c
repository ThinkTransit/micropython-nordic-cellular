/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Linaro Limited
 * Copyright (c) 2019 NXP
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

#include "py/mpconfig.h"
#if MICROPY_PY_ZEPHYR

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#if defined(CONFIG_PM_DEVICE) && !defined(CONFIG_CONSOLE_SUBSYS)
#include <zephyr/drivers/uart.h>
#include <zephyr/pm/device.h>
#endif

#include "modzephyr.h"
#include "py/runtime.h"

STATIC mp_obj_t mod_is_preempt_thread(void) {
    return mp_obj_new_bool(k_is_preempt_thread());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_is_preempt_thread_obj, mod_is_preempt_thread);

STATIC mp_obj_t mod_current_tid(void) {
    return MP_OBJ_NEW_SMALL_INT(k_current_get());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_current_tid_obj, mod_current_tid);

#ifdef CONFIG_THREAD_ANALYZER
STATIC mp_obj_t mod_thread_analyze(void) {
    thread_analyzer_print();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_thread_analyze_obj, mod_thread_analyze);
#endif

#ifdef CONFIG_SHELL_BACKEND_SERIAL
STATIC mp_obj_t mod_shell_exec(mp_obj_t cmd_in) {
    const char *cmd = mp_obj_str_get_str(cmd_in);
    shell_execute_cmd(shell_backend_uart_get_ptr(), cmd);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_shell_exec_obj, mod_shell_exec);
#endif // CONFIG_SHELL_BACKEND_SERIAL

#if defined(CONFIG_PM_DEVICE) && !defined(CONFIG_CONSOLE_SUBSYS)
STATIC mp_obj_t console_enable() {
    static const struct device *uart_console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    pm_device_action_run(uart_console_dev, PM_DEVICE_ACTION_RESUME);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(console_enable_obj, console_enable);

void console_enable_fn(struct k_work *item) {
    ARG_UNUSED(item);
    console_enable();
}
static K_WORK_DELAYABLE_DEFINE(console_enable_wk, console_enable_fn);

STATIC mp_obj_t console_disable(mp_obj_t seconds) {
    static const struct device *uart_console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    pm_device_action_run(uart_console_dev, PM_DEVICE_ACTION_SUSPEND);
    int delay = mp_obj_get_int(seconds);
    if (delay > 0) {
        k_work_schedule(&console_enable_wk, K_SECONDS(mp_obj_get_int(seconds)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(console_disable_obj, console_disable);

STATIC mp_obj_t console_is_enabled() {
    enum pm_device_state state;
    static const struct device *uart_console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    int ret = pm_device_state_get(uart_console_dev, &state);
    if (ret) {
        mp_raise_OSError(ret);
    }
    return mp_obj_new_bool(state == PM_DEVICE_STATE_ACTIVE);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(console_is_enabled_obj, console_is_enabled);
#endif

STATIC const mp_rom_map_elem_t mp_module_time_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_zephyr) },
    { MP_ROM_QSTR(MP_QSTR_is_preempt_thread), MP_ROM_PTR(&mod_is_preempt_thread_obj) },
    { MP_ROM_QSTR(MP_QSTR_current_tid), MP_ROM_PTR(&mod_current_tid_obj) },
    #ifdef CONFIG_THREAD_ANALYZER
    { MP_ROM_QSTR(MP_QSTR_thread_analyze), MP_ROM_PTR(&mod_thread_analyze_obj) },
    #endif
    #ifdef CONFIG_SHELL_BACKEND_SERIAL
    { MP_ROM_QSTR(MP_QSTR_shell_exec), MP_ROM_PTR(&mod_shell_exec_obj) },
    #endif
    #ifdef CONFIG_DISK_ACCESS
    { MP_ROM_QSTR(MP_QSTR_DiskAccess), MP_ROM_PTR(&zephyr_disk_access_type) },
    #endif
    #ifdef CONFIG_FLASH_MAP
    { MP_ROM_QSTR(MP_QSTR_FlashArea), MP_ROM_PTR(&zephyr_flash_area_type) },
    #endif
    #if defined(CONFIG_PM_DEVICE) && !defined(CONFIG_CONSOLE_SUBSYS)
    { MP_ROM_QSTR(MP_QSTR_console_disable), MP_ROM_PTR(&console_disable_obj) },
    { MP_ROM_QSTR(MP_QSTR_console_enable), MP_ROM_PTR(&console_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_console_is_enabled), MP_ROM_PTR(&console_is_enabled_obj) },
    #endif
};

STATIC MP_DEFINE_CONST_DICT(mp_module_time_globals, mp_module_time_globals_table);

const mp_obj_module_t mp_module_zephyr = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_time_globals,
};

MP_REGISTER_MODULE(MP_QSTR_zephyr, mp_module_zephyr);

#endif // MICROPY_PY_ZEPHYR
