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
#include "device.h"
#include "exception.h"
#include <stddef.h>
#include <stdint.h>

#define ROM_SIZE (ROM_END - ROM_BASE + 1) // 64 kb
#define RAM_SIZE (RAM_END - RAM_BASE + 1) // 2 gb

#define MAX_DEVICES 16

typedef enum {
    RING_SUPERVISOR = 0,
    RING_USER = 1,
} ring;

typedef struct {
    uint8_t rom[ROM_SIZE];
    uint8_t *ram;
    device *devices[MAX_DEVICES];
    size_t ndevices;
} bus;

int bus_init(bus *bus, const uint8_t *rom, size_t rom_len, const uint8_t *ram_image,
             size_t ram_image_len, uint32_t ram_load_addr);
void bus_free(bus *bus);
void bus_add_device(bus *bus, device *dev);

// tick all devices
int32_t bus_tick(bus *bus);

// fetch instruction
int bus_fetch32(bus *bus, uint32_t addr, uint32_t epc, uint32_t *out, exception *exc);

// reads
int bus_read8(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint8_t *out, exception *exc);
int bus_read16(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint16_t *out, exception *exc);
int bus_read32(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint32_t *out, exception *exc);

// writes
int bus_write8(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint8_t val, exception *exc);
int bus_write16(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint16_t val, exception *exc);
int bus_write32(bus *bus, uint32_t addr, ring ring, uint32_t epc, uint32_t val, exception *exc);