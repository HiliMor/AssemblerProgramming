#ifndef _PREASSEMBLER_H
#define _PREASSEMBLER_H

#include "common.h"

typedef struct macrotableentry_t {
    char* macro_name;
    char* macro_content;
} MacroTableEntry;

typedef struct {
    MacroTableEntry* macros;
    int macro_count;
    int arr_capacity;
} MacroTable;

Status preassemble(char* input_file_path);

#endif
