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
#include "exception.h"
#include "mem.h"
#include <stdint.h>

typedef enum {
    STEP_OK = 0,
    STEP_HALTED = 1,
} step_result;

typedef struct {
    uint32_t gpr[16]; // r0 always 0
    float fpr[16];    // ieee754
    uint32_t pc;
    uint32_t csr[8];
    uint32_t fetch_pc; // current pc
    int in_trap;       // double fault
} cpu;

void cpu_init(cpu *cpu);
step_result cpu_step(cpu *cpu, bus *bus);

static inline uint32_t cpu_ring(const cpu *cpu) {
    return cpu->csr[CSR_STATUS] & 0x3;
}
static inline ring cpu_ring_e(const cpu *cpu) {
    return (ring) (cpu->csr[CSR_STATUS] & 0x3);
}