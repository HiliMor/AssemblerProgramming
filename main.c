#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"
#include "preassembler.h"
#include "assembler.h"

int main(int argc, char **argv) {
    int i = 0;
    Assembler assembler = {0};
    int status = 0;

    if (argc < 2) {
        printf("usage: a.out <file1.as> <file2.as> ... <fileN.as>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (assembler_init(&assembler) != STATUS_SUCCESS) {
            return 1;
        }

        if (preassemble(argv[i]) != STATUS_SUCCESS) {
            status = 1;
            assembler_free(&assembler);
            continue;
        }

        if (assembler_assemble(&assembler, argv[i]) != STATUS_SUCCESS) {
            status = 1;
            assembler_free(&assembler);
            continue;
        }

        assembler_free(&assembler);
    }

    return status;
}
