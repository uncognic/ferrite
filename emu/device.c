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

#include "device.h"
#include "isa.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// UART
static uint32_t uart_read(device *dev, uint32_t addr) {
    uart *u = (uart *) dev;
    switch (addr) {
        case UART_RX: {
            if (u->rx_head == u->rx_tail) {
                return 0xFF; // empty
            }
            uint8_t byte = u->rx_buf[u->rx_head];
            u->rx_head = (u->rx_head + 1) % UART_RX_BUF_SIZE;
            return byte;
        }
        case UART_STATUS: {
            int rx_ready = u->rx_head != u->rx_tail;
            return (uint32_t) rx_ready | (1u << 1); // tx is always ready
        }
        default:
            return 0;
    }
}

static void uart_write(device *dev, uint32_t addr, uint32_t val) {
    (void) dev;
    if (addr == UART_TX) {
        uint8_t byte = (uint8_t) val;
        fwrite(&byte, sizeof(byte), 1, stdout);
        fflush(stdout);
    }
}

static int32_t uart_tick(device *dev) {
    uart *u = (uart *) dev;
    if (u->pending_irq >= 0) {
        int32_t irq = u->pending_irq;
        u->pending_irq = -1;
        return irq;
    }
    return -1;
}

void uart_init(uart *u) {
    u->dev.base = UART_TX;
    u->dev.size = 12;
    u->dev.read = uart_read;
    u->dev.write = uart_write;
    u->dev.tick = uart_tick;
    u->rx_head = 0;
    u->rx_tail = 0;
    u->pending_irq = -1;
}

void uart_push_rx(uart *u, uint8_t byte) {
    size_t next = (u->rx_tail + 1) % UART_RX_BUF_SIZE;
    if (next == u->rx_head) {
        return;
    }
    u->rx_buf[u->rx_tail] = byte;
    u->rx_tail = next;
    u->pending_irq = CAUSE_INT_UART_RX;
}