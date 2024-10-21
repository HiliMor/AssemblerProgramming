#ifndef _SECONDPASS_H
#define _SECONDPASS_H

#include <stdio.h>
#include <stdlib.h>
#include "assembler.h"


Word make_instruction_word(byte opcode, byte src_addressing, byte dst_addressing, byte are);
Status prepare_param_word(Assembler* assembler, const char* str, byte addressing, SrcOrDst src_or_dst, Word* out, const char* filepath, int linenumber);
Status assembler_secondpass(Assembler* assembler, FILE* input_file, const char* preassembled_path);
  

#endif
