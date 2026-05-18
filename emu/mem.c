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

#include "mem.h"
#include "isa.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

typedef enum { REGION_ROM, REGION_RAM, REGION_MMIO } region;

static region region_of(uint32_t addr) {
    if (addr <= ROM_END) {
        return REGION_ROM;
    }
    if (addr <= RAM_END) {
        return REGION_RAM;
    }
    return REGION_MMIO;
}

int bus_init(bus *b, const uint8_t *rom, size_t rom_len, const uint8_t *image, size_t image_len,
             uint32_t load_addr) {
    memset(b, 0, sizeof(*b));

    // copy rom
    size_t rlen = rom_len < ROM_SIZE ? rom_len : ROM_SIZE;
    memcpy(b->rom, rom, rlen);

    // allocate ram
    b->ram = xmalloc(RAM_SIZE);
    memset(b->ram, 0, RAM_SIZE);

    // copy ram image
    if (image && image_len > 0) {
        // check load address is valid
        if (load_addr < RAM_BASE || load_addr > RAM_END) {
            return -1;
        }
        size_t offset = load_addr - RAM_BASE;

        // check if image_len exceeds ram size
        size_t ilen = image_len < (RAM_SIZE - offset) ? image_len : (RAM_SIZE - offset);

        // copy image to ram
        memcpy(b->ram + offset, image, ilen);
    }

    b->ndevices = 0;
    return 0;
}

void bus_free(bus *b) {
    free(b->ram);
    b->ram = nullptr;
    b->ndevices = 0;
}

void bus_add_device(bus *b, device *dev) {
    if (b->ndevices >= MAX_DEVICES) {
        die("too many devices");
    }
    b->devices[b->ndevices++] = dev;
}

static device *find_device(bus *b, uint32_t addr) {
    for (size_t i = 0; i < b->ndevices; i++) {
        if (device_contains(b->devices[i], addr)) {
            return b->devices[i];
        }
    }
    return nullptr;
}

int32_t bus_tick(bus *b) {
    for (size_t i = 0; i < b->ndevices; i++) {
        device *dev = b->devices[i];
        if (dev->tick) {
            int32_t irq = dev->tick(dev);
            if (irq >= 0) {
                return irq;
            }
        }
    }
    return -1;
}

// raw reads no permission checking

static uint8_t raw_read8(bus *b, uint32_t addr) {
    switch (region_of(addr)) {
        case REGION_ROM: {
            uint32_t off = addr - ROM_BASE;
            return off < ROM_SIZE ? b->rom[off] : 0;
        }
        case REGION_RAM: {
            uint32_t off = addr - RAM_BASE;
            return off < RAM_SIZE ? b->ram[off] : 0;
        }
        case REGION_MMIO: {
            uint32_t aligned = addr & ~3u;
            uint32_t off = addr & 3;
            device *dev = find_device(b, aligned);
            if (!dev) {
                return 0;
            }
            uint32_t word = dev->read(dev, aligned);
            return (word >> (off * 8)) & 0xFF;
        }
    }
    return 0;
}

static void raw_write8(bus *b, uint32_t addr, uint8_t val) {
    switch (region_of(addr)) {
        case REGION_ROM:
            break; // permision check shouldve caught this already
        case REGION_RAM: {
            uint32_t off = addr - RAM_BASE;
            if (off < RAM_SIZE) {
                b->ram[off] = val;
            }
            break;
        }
        case REGION_MMIO: {
            uint32_t aligned = addr & ~3u;
            uint32_t off = addr & 3;
            device *dev = find_device(b, aligned);
            if (!dev) {
                break;
            }
            uint32_t word = dev->read(dev, aligned);
            word &= ~(0xFFu << (off * 8));
            word |= (uint32_t) val << (off * 8);
            dev->write(dev, aligned, word);
            break;
        }
    }
}

static int check_read(uint32_t addr, ring r, uint32_t epc, exception *exc) {
    switch (region_of(addr)) {
        case REGION_ROM:
        case REGION_RAM:
            return 1;
        case REGION_MMIO:
            if (r == RING_SUPERVISOR) {
                return 1;
            }
            *exc = exc_fault_priv(epc);
            return 0;
    }
    return 0;
}

static int check_write(uint32_t addr, ring r, uint32_t epc, exception *exc) {
    switch (region_of(addr)) {
        case REGION_ROM:
            *exc = exc_fault_mem(epc);
            return 0;
        case REGION_RAM:
            return 1;
        case REGION_MMIO:
            if (r == RING_SUPERVISOR) {
                return 1;
            }
            *exc = exc_fault_priv(epc);
            return 0;
    }
    return 0;
}

int bus_fetch32(bus *b, uint32_t addr, uint32_t epc, uint32_t *out, exception *exc) {
    if (addr & 3) {
        *exc = exc_fault_mem(epc);
        return 0;
    }
    switch (region_of(addr)) {
        case REGION_ROM:
        case REGION_RAM:
            *out = (uint32_t) raw_read8(b, addr) | (uint32_t) raw_read8(b, addr + 1) << 8 |
                   (uint32_t) raw_read8(b, addr + 2) << 16 |
                   (uint32_t) raw_read8(b, addr + 3) << 24;
            return 1;
        case REGION_MMIO:
            *exc = exc_fault_mem(epc);
            return 0;
    }
    return 0;
}

// safe reads
int bus_read8(bus *b, uint32_t addr, ring r, uint32_t epc, uint8_t *out, exception *exc) {
    if (!check_read(addr, r, epc, exc)) {
        return 0;
    }
    *out = raw_read8(b, addr);
    return 1;
}

int bus_read16(bus *b, uint32_t addr, ring r, uint32_t epc, uint16_t *out, exception *exc) {
    if (addr & 1) {
        *exc = exc_fault_mem(epc);
        return 0;
    }
    if (!check_read(addr, r, epc, exc)) {
        return 0;
    }
    if (region_of(addr) == REGION_MMIO) {
        device *dev = find_device(b, addr & ~3u);
        if (!dev) {
            *out = 0;
            return 1;
        }
        uint32_t word = dev->read(dev, addr & ~3u);
        uint32_t off = (addr & 2) * 8;
        *out = (word >> off) & 0xFFFF;
        return 1;
    }
    *out = (uint16_t) raw_read8(b, addr) | (uint16_t) raw_read8(b, addr + 1) << 8;
    return 1;
}

int bus_read32(bus *b, uint32_t addr, ring r, uint32_t epc, uint32_t *out, exception *exc) {
    if (addr & 3) {
        *exc = exc_fault_mem(epc);
        return 0;
    }
    if (!check_read(addr, r, epc, exc)) {
        return 0;
    }
    if (region_of(addr) == REGION_MMIO) {
        device *dev = find_device(b, addr);
        *out = dev ? dev->read(dev, addr) : 0;
        return 1;
    }
    *out = (uint32_t) raw_read8(b, addr) | (uint32_t) raw_read8(b, addr + 1) << 8 |
           (uint32_t) raw_read8(b, addr + 2) << 16 | (uint32_t) raw_read8(b, addr + 3) << 24;
    return 1;
}

int bus_write8(bus *b, uint32_t addr, ring r, uint32_t epc, uint8_t val, exception *exc) {
    if (!check_write(addr, r, epc, exc)) {
        return 0;
    }
    raw_write8(b, addr, val);
    return 1;
}

int bus_write16(bus *b, uint32_t addr, ring r, uint32_t epc, uint16_t val, exception *exc) {
    if (addr & 1) {
        *exc = exc_fault_mem(epc);
        return 0;
    }
    if (!check_write(addr, r, epc, exc)) {
        return 0;
    }
    if (region_of(addr) == REGION_MMIO) {
        device *dev = find_device(b, addr & ~3u);
        if (!dev) {
            return 1;
        }
        uint32_t aligned = addr & ~3u;
        uint32_t off = (addr & 2) * 8;
        uint32_t word = dev->read(dev, aligned);
        word &= ~(0xFFFFu << off);
        word |= (uint32_t) val << off;
        dev->write(dev, aligned, word);
        return 1;
    }
    raw_write8(b, addr, val & 0xFF);
    raw_write8(b, addr + 1, val >> 8);
    return 1;
}

int bus_write32(bus *b, uint32_t addr, ring r, uint32_t epc, uint32_t val, exception *exc) {
    if (addr & 3) {
        *exc = exc_fault_mem(epc);
        return 0;
    }
    if (!check_write(addr, r, epc, exc)) {
        return 0;
    }
    if (region_of(addr) == REGION_MMIO) {
        device *dev = find_device(b, addr);
        if (dev) {
            dev->write(dev, addr, val);
        }
        return 1;
    }
    raw_write8(b, addr, (val) & 0xFF);
    raw_write8(b, addr + 1, (val >> 8) & 0xFF);
    raw_write8(b, addr + 2, (val >> 16) & 0xFF);
    raw_write8(b, addr + 3, (val >> 24) & 0xFF);
    return 1;
}
