/*
 * Copyright (c) 2016 Linaro
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/console/uart_console.h>
#include <zephyr/sys/printk.h>
#include "zephyr_getchar.h"

extern int mp_interrupt_char;
void mp_sched_keyboard_interrupt(void);
void mp_hal_signal_event(void);
void mp_hal_wait_sem(struct k_sem *sem, uint32_t timeout_ms);

static struct k_sem uart_sem;
#define UART_BUFSIZE 256
static uint8_t uart_ringbuf[UART_BUFSIZE];
static uint8_t i_get, i_put, i_size;

static int console_irq_input_hook(uint8_t ch) {
    int i_next = (i_put + 1) & (UART_BUFSIZE - 1);
    if (i_next == i_get) {
        printk("UART buffer overflow - char dropped\n");
        return 1;
    }
    if (ch == mp_interrupt_char) {
        mp_hal_signal_event();
        mp_sched_keyboard_interrupt();
        return 1;
    } else {
        uart_ringbuf[i_put] = ch;
        i_put = i_next;
        i_size++;
    }
    // printk("%x\n", ch);
    k_sem_give(&uart_sem);
    // Try to avoid buffer overflows
    if (i_size > (UART_BUFSIZE - 50)) {
        k_msleep(1);
    }
    return 1;
}

uint8_t zephyr_getchar(void) {
    uint8_t c;

    while (zephyr_getchar_timeout(-1, &c) != 0) { }
    return c;
}

int zephyr_getchar_timeout(uint32_t timeout, uint8_t *c) {
    mp_hal_wait_sem(&uart_sem, timeout);
    if (k_sem_take(&uart_sem, K_NO_WAIT) != 0) {
        return -ENODEV;
    }
    unsigned int key = irq_lock();
    *c = uart_ringbuf[i_get++];
    i_get &= UART_BUFSIZE - 1;
    i_size--;
    irq_unlock(key);
    return 0;
}

void zephyr_getchar_init(void) {
    k_sem_init(&uart_sem, 0, UINT_MAX);
    uart_console_in_debug_hook_install(console_irq_input_hook);
    // All NULLs because we're interested only in the callback above
    uart_register_input(NULL, NULL, NULL);
}
