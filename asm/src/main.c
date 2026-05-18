#include "arena.h"
#include "asm.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "fasm: could not open '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = xmalloc((size_t) len + 1);
    fread(buf, 1, (size_t) len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static const char *flag_value(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        fprintf(stderr, "usage: fasm <input.asm> [-o output.bin]\n");
        return 1;
    }

    const char *input = NULL;
    const char *output = flag_value(argc, argv, "-o");

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            input = argv[i];
            break;
        }
        if (strcmp(argv[i], "-o") == 0) {
            i++;
        }
        if (strcmp(argv[i], "-v") == 0) {
            fprintf(stderr, "ferrite assembler %s\n", CAST_VERSION);
            return 0;
        }
    }
    if (!input) {
        fprintf(stderr, "fasm: no input file\n");
        return 1;
    }

    char out_buf[512];
    if (!output) {
        strncpy(out_buf, input, sizeof(out_buf) - 5);
        out_buf[sizeof(out_buf) - 5] = '\0';
        char *dot = strrchr(out_buf, '.');
        if (dot) {
            *dot = '\0';
        }
        strcat(out_buf, ".bin");
        output = out_buf;
    }

    char *src = read_file(input);

    arena a = {0};
    token_vec tokens = {0};
    stmt_vec stmts = {0};
    asm_error err = {0};

    if (!lex(src, &tokens, &a, &err)) {
        fprintf(stderr, "%s:%d: %s\n", input, err.line, err.msg);
        free(src);
        arena_free(&a);
        vec_free(&tokens);
        return 1;
    }

    if (!parse(&tokens, &stmts, &a, &err)) {
        fprintf(stderr, "%s:%d: %s\n", input, err.line, err.msg);
        free(src);
        arena_free(&a);
        vec_free(&tokens);
        vec_free(&stmts);
        return 1;
    }

    uint8_t *binary = NULL;
    size_t bin_len = 0;
    if (!encode(&stmts, &binary, &bin_len, &err)) {
        fprintf(stderr, "%s:%d: %s\n", input, err.line, err.msg);
        free(src);
        arena_free(&a);
        vec_free(&tokens);
        vec_free(&stmts);
        return 1;
    }

    FILE *f = fopen(output, "wb");
    if (!f) {
        fprintf(stderr, "fasm: could not write '%s'\n", output);
        free(src);
        free(binary);
        arena_free(&a);
        vec_free(&tokens);
        vec_free(&stmts);
        return 1;
    }
    fwrite(binary, 1, bin_len, f);
    fclose(f);

    fprintf(stderr, "%s -> %s (%zu bytes)\n", input, output, bin_len);

    free(src);
    free(binary);
    arena_free(&a);
    vec_free(&tokens);
    vec_free(&stmts);
    return 0;
}