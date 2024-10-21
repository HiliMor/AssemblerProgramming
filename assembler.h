#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "common.h"

#define MAX_WORDS_IN_OBJFILE 4096

#define LOADING_BASE 100

#define ARE_ABSOLUTE 4 /* 0b100*/
#define ARE_RELATIVE 2 /* 0b010*/
#define ARE_EXTERNAL 1 /* 0b001*/

#define ADDRESSING_NONE 0 /*0b0000*/ 
#define ADDRESSING_0    1 /*0b0001*/ /*immediate*/
#define ADDRESSING_1    2 /*0b0010*/ /*direct*/
#define ADDRESSING_2    4 /*0b0100*/ /*indirect-register*/
#define ADDRESSING_3    8 /*0b1000*/ /*direct-register*/

                          /*mask = 0b0000*/
                          /*addr = 0b0010*/
                          /*     & */
                          /*     = 0b0000*/



typedef enum {
    SOURCE_OPERAND,
    DEST_OPERAND
} SrcOrDst;

typedef struct {
    char* name;
    byte code;
    byte valid_src_operands; /* bitfield */
    byte valid_dst_operands; /* bitfield */
    int operands_num;
} OpcodeTableEntry;

typedef struct {
    char label_name[LINEBUFFER_SIZE];
    LabelType type;
    CodeOrData code_or_data;
    /* this is the ic/dc. when writing to a RELATIVE operand, add LOADING_BASE to this value. (and maybe code_section_size)*/
    int address;  
} LabelTableEntry;

typedef struct {
    LabelTableEntry* labels;
    int count;
    int capacity;
} LabelTable;

typedef struct {
    char label_name[LINEBUFFER_SIZE];
    int address; /* The address in the codesection (already includes LOADING_BASE) */
} ExternTableEntry;

typedef struct {
    ExternTableEntry refs[MAX_WORDS_IN_OBJFILE];
    int count;
} ExternTable;

typedef struct assembler_t {
  Word code[MAX_WORDS_IN_OBJFILE];
  Word data[MAX_WORDS_IN_OBJFILE];
  int ic;
  int dc;
  int code_section_size;
  LabelTable label_table;
  ExternTable extern_table; /* All the references to externs in the code. filled during second pass */
  
} Assembler;

Status assembler_init(Assembler* assembler);
void assembler_free(Assembler* assembler);

/* The assembler assumes that the .am file was not tampered with, and pre-assembly was successfull. */
Status assembler_assemble(Assembler* assembler, char* source_file_path);

Status get_opcode(const char* tokens, int* out_opcode);

extern OpcodeTableEntry opcodeTable[];

Status labeltable_get_entry(LabelTable* table, const char* label, LabelTableEntry* out);
byte get_addressing_method(const char* param, const char* filepath, int linenumber);

#endif
