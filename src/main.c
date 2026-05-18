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

#include "cpu.h"
#include "device.h"
#include "mem.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *load_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        die("could not open '%s'", path);
    }
    fseek(f, 0, SEEK_END);
    *len = (size_t) ftell(f);
    rewind(f);
    uint8_t *buf = xmalloc(*len);
    if (fread(buf, 1, *len, f) != *len) {
        die("could not read '%s'", path);
    }
    fclose(f);
    return buf;
}

static const char *flag_value(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return argv[i + 1];
        }
    }
    return nullptr;
}

static bool has_flag(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

static void print_trace(const cpu *c) {
    fprintf(stderr, "pc=%08x ", c->pc);
    for (int i = 0; i < 16; i++) {
        if (c->gpr[i]) {
            fprintf(stderr, "R%d=%08x ", i, c->gpr[i]);
        }
    }
    fprintf(stderr, "\n");
}

static void print_regs(const cpu *c) {
    fprintf(stderr, "REGISTERS\n");
    for (int i = 0; i < 16; i++) {
        fprintf(stderr, "  R%-2d = %08x  (%d)\n", i, c->gpr[i], (int32_t) c->gpr[i]);
    }
    for (int i = 0; i < 16; i++) {
        if (c->fpr[i] != 0.0f) {
            fprintf(stderr, "  F%-2d = %f\n", i, c->fpr[i]);
        }
    }
}

int main(int argc, char **argv) {
    bool trace = has_flag(argc, argv, "--trace");
    bool regs = has_flag(argc, argv, "--regs");
    const char *fw_path = flag_value(argc, argv, "--firmware");
    const char *prog_path = flag_value(argc, argv, "--program");

    // if only one binary was provided, use it as the firmware
    const char *legacy = nullptr;
    if (!fw_path && !prog_path) {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] != '-') {
                legacy = argv[i];
                break;
            }
        }
    }

    if (!fw_path && !legacy) {
        fprintf(stderr,
                "usage: ferrite [--firmware <fw.bin>] [--program <prog.bin>] [--trace] [--regs]\n");
        return 1;
    }
    if (prog_path && !fw_path) {
        fprintf(stderr, "error: --program requires --firmware\n");
        return 1;
    }

    size_t rom_len = 0;
    uint8_t *rom = load_file(fw_path ? fw_path : legacy, &rom_len);

    size_t img_len = 0;
    uint8_t *img = NULL;
    uint32_t load_addr = 0;
    if (prog_path) {
        img = load_file(prog_path, &img_len);
        load_addr = 0x00011000;
    }

    uart u;
    uart_init(&u);

    bus b;
    if (bus_init(&b, rom, rom_len, img, img_len, load_addr) < 0) {
        die("failed to initialize bus");
    }
    bus_add_device(&b, (device *) &u);

    cpu c;
    cpu_init(&c);

    free(rom);
    free(img);

    for (;;) {
        if (trace) {
            fflush(stdout);
            print_trace(&c);
        }
        step_result r = cpu_step(&c, &b);
        if (r == STEP_HALTED) {
            fflush(stdout);
            if (trace || regs) {
                print_regs(&c);
            }
            fprintf(stderr, "[ferrite] halted at pc=%08x\n", c.pc);
            break;
        }
    }

    bus_free(&b);
    return 0;
}
