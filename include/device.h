/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct device device;

struct device {
    uint32_t base;
    uint32_t size;
    uint32_t (*read)(device *dev, uint32_t addr);
    void (*write)(device *dev, uint32_t addr, uint32_t val);
    // returns cause of interrupt, or 0 if no interrupt
    int32_t (*tick)(device *dev);
};

static inline int device_contains(device *dev, uint32_t addr) {
    return addr >= dev->base && addr < dev->base + dev->size;
}

// uart

#define UART_RX_BUF_SIZE 256

typedef struct {
    device dev;
    uint8_t rx_buf[UART_RX_BUF_SIZE];
    size_t rx_head;
    size_t rx_tail;
    int pending_irq;
} uart;

void uart_init(uart *uart);
void uart_push_rx(uart *uart, uint8_t byte);