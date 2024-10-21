#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "common.h"
#include "parser.h"
#include "assembler.h"
#include "secondpass.h"

#define OPCODE_NUM 16
#define REGISTERS_NUM 8

/* 15bit max: 0011 1111 1111 1111 (3f ff) (+16383)
 * 15bit min: 0100 0000 0000 0000 (40 00) (-16384)
*/
#define WORD_MAX (0x3fff)
#define WORD_MIN (-0x4000)

#define IMMEDIATE_MAX (0x7ff)
#define IMMEDIATE_MIN (-0x800)

char *registers[] = {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7"};

OpcodeTableEntry opcodeTable[] = {
    {"mov", 0, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 2},
    {"cmp", 1, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 2},
    {"add", 2, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 2},
    {"sub", 3, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 2},
    {"lea", 4, ADDRESSING_1, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 2},
    {"clr", 5, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"not", 6, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"inc", 7, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"dec", 8, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"jmp", 9, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2, 1},
    {"bne", 10, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2, 1},
    {"red", 11, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"prn", 12, ADDRESSING_NONE, ADDRESSING_0|ADDRESSING_1|ADDRESSING_2|ADDRESSING_3, 1},
    {"jsr", 13, ADDRESSING_NONE, ADDRESSING_1|ADDRESSING_2, 1},
    {"rts", 14, ADDRESSING_NONE, ADDRESSING_NONE, 0},
    {"stop", 15, ADDRESSING_NONE, ADDRESSING_NONE, 0}
};

Status labeltable_init(LabelTable* table) {
    table->capacity = INITIAL_CAPACITY;
    table->count = 0;
    table->labels = (LabelTableEntry*)malloc(table->capacity * sizeof(LabelTableEntry));
    if (table->labels == NULL) {
        printf("failed to allocate memory for labels\n");
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

Status labeltable_get_entry(LabelTable* table, const char* label, LabelTableEntry* out) {
    int i = 0;
    for (i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].label_name, label) == 0) {
            *out = table->labels[i];
            return STATUS_SUCCESS;
        }
    }
    return STATUS_FAILURE;
}

bool is_label_duplicate(LabelTable* table, const char* label_name) {
    int i = 0;
    for (i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].label_name, label_name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

Status expand_label_table(LabelTable* table) {
    LabelTableEntry* new_labels = NULL;
    new_labels = (LabelTableEntry*)malloc(table->capacity * 2 * sizeof(LabelTableEntry));
    if (new_labels == NULL) {
        printf("failed to allocate memory for labels\n");
        return STATUS_FAILURE;
    }
    memcpy(new_labels, table->labels, table->capacity * sizeof(LabelTableEntry));
    free(table->labels);
    table->labels = new_labels;
    table->capacity *= 2;
    return STATUS_SUCCESS;
}

/* TODO: support data-labels too (right now we just put 'dc' in the address...) */
/* TODO: support entry and extern labels, and figure out how they need to look.. */
Status assembler_add_label(Assembler* assembler, const char* label_name, LabelType type, CodeOrData code_or_data, const char* filepath, int linenumber) {
    if (is_label_duplicate(&assembler->label_table, label_name)) {
        printf("%s:%d: duplicate label\n", filepath, linenumber);
        return STATUS_FAILURE;
    }

    if (assembler->label_table.count >= assembler->label_table.capacity) {
        if (expand_label_table(&assembler->label_table) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    }

    if (code_or_data == LABEL_DATA) {
        assembler->label_table.labels[assembler->label_table.count].address = assembler->dc;
    } else {
        assembler->label_table.labels[assembler->label_table.count].address = assembler->ic;
    }

    strcpy(assembler->label_table.labels[assembler->label_table.count].label_name, label_name);
    assembler->label_table.labels[assembler->label_table.count].type = type;
    assembler->label_table.labels[assembler->label_table.count].code_or_data = code_or_data;
    assembler->label_table.count++;
    return STATUS_SUCCESS;
}

void labeltable_free(LabelTable* table) {
    free(table->labels);
    table->labels = NULL;
    table->count = 0;
    table->capacity = 0;
}

void update_label_type(LabelTable* table, const char* label_name, LabelType type) {
    int i = 0;
    for (i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].label_name, label_name) == 0) {
            table->labels[i].type = type;
            return;
        }
    }
}

bool is_label_in_table(LabelTable* table, const char* label_name) {
    int i = 0;
    for (i = 0; i < table->count; i++) {
        if (strcmp(table->labels[i].label_name, label_name) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

Status get_opcode(const char* tokens, int* out_opcode) {
    int i = 0;
    for (i = 0; i < OPCODE_NUM; i++) {
        if (strcmp(opcodeTable[i].name, tokens) == 0) {
            *out_opcode = opcodeTable[i].code;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_FAILURE;
}

Status handle_entry_directive(ParsedLine* parsed, LabelTable* label_table, const char* filepath, int line_number) {
    char* label = parsed->params[0];
    if (!is_label_in_table(label_table, label)) {
        printf("%s:%d: entry label '%s' not defined\n", filepath, line_number, label);
        return STATUS_FAILURE;
    }

    update_label_type(label_table, label, LABEL_ENTRY);
    return STATUS_SUCCESS;
}

Status handle_extern_directive(ParsedLine* parsed, Assembler* assembler, const char* filepath, int line_number) {
    char* label = parsed->params[0]; 
    if (is_label_in_table(&assembler->label_table, label)) {
        printf("%s:%d: extern label '%s' already defined\n", filepath, line_number, label);
        return STATUS_FAILURE;
    }

    if (assembler_add_label(assembler, label, LABEL_EXTERN, LABEL_CODE, filepath, line_number) != STATUS_SUCCESS) {
        return STATUS_FAILURE;
    }
    return STATUS_SUCCESS;
}

Status handle_data_directive(ParsedLine* parsed, Assembler* assembler, const char* filepath, int line_number) {
    int i = 0;
    long value = 0;
    char* endptr = 0;

    for (i = 0; i < parsed->num_params; i++) {
        /* convert the string to a long integer using strtol */
        value = strtol(parsed->params[i], &endptr, 10);
        
        if (endptr == parsed->params[i] || *endptr != '\0' || value > WORD_MAX || value < WORD_MIN) {
            printf("%s:%d: number out of range or invalid '%s'\n", filepath, line_number, parsed->params[i]);
            return STATUS_FAILURE;
        }

        assembler->data[assembler->dc] = (short)value & 0x7fff;
        assembler->dc++;
    }

    return STATUS_SUCCESS;
}

Status check_string_format(const char* str, const char* filepath, int line_number) {
    if (str[0] != '"' || str[strlen(str) - 1] != '"') {
        printf("%s:%d: invalid string format\n", filepath, line_number);
        return STATUS_FAILURE;
    }
    return STATUS_SUCCESS;
}

/* TODO: support strings with spaces, commas
             label4: .string "abcdm ,,,,,, xyz" */
void handle_string(char* str, Assembler* assembler) {
    int i = 0;
    str++; /* skip the first " "*/
    str[strlen(str) - 1] = '\0'; /* skip the last " "*/

    for (i = 0; str[i] != '\0'; i++) {
        assembler->data[assembler->dc] = (Word)str[i];
        assembler->dc++;
    }
    assembler->data[assembler->dc] = '\0';
    assembler->dc++;
}

Status handle_string_directive(ParsedLine* parsed, Assembler* assembler, const char* filepath, int line_number) {
    char* str = parsed->params[0];

    if (parsed->num_params != 1) { 
        printf("%s:%d: a string must receive only a single paramer. the parameter must not contain spaces or commas", filepath, line_number);
        return STATUS_FAILURE;
    }

    if (check_string_format(str, filepath, line_number) != STATUS_SUCCESS) {
        return STATUS_FAILURE;
    }
    handle_string(str, assembler);

    return STATUS_SUCCESS;
}

Status assembler_init(Assembler* assembler) {
    memset(assembler, '\0', sizeof(Assembler));
    if (labeltable_init(&assembler->label_table) != STATUS_SUCCESS) {
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

void assembler_free(Assembler* assembler) {
  labeltable_free(&assembler->label_table);
  memset(assembler, '\0', sizeof(Assembler));
}


byte get_register_indirect_addressing(const char* param) {
    char* conversion_end = 0;
    long reg_num = 0;
    if (param[0] == '*' && param[1] == 'r') {
        reg_num = strtol(&param[2], &conversion_end, 10); 
        if (conversion_end != &param[2] && reg_num >= 0 && reg_num <= 7) {
            return ADDRESSING_2; 
        }
    }
    return ADDRESSING_NONE;
}

byte get_register_addressing(const char* param) {
    char* conversion_end = 0;
    int reg_num = 0;
    if (param[0] == 'r') {
        reg_num = strtol(&param[1], &conversion_end, 10);
        if (conversion_end != &param[1] && reg_num >= 0 && reg_num <= 7) {
            return ADDRESSING_3; 
        }
    }
    return ADDRESSING_NONE;
}

byte get_addressing_method(const char* param, const char* filepath, int linenumber) {
    if (param[0] == '#') {
        return ADDRESSING_0;
    } else if (strcmp(param, "*r0") == 0 || 
            strcmp(param, "*r1") == 0 ||
            strcmp(param, "*r2") == 0 ||
            strcmp(param, "*r3") == 0 ||
            strcmp(param, "*r4") == 0 ||
            strcmp(param, "*r5") == 0 ||
            strcmp(param, "*r6") == 0 ||
            strcmp(param, "*r7") == 0) {
        return ADDRESSING_2;
    } else if (strcmp(param, "r0") == 0 || 
            strcmp(param, "r1") == 0 ||
            strcmp(param, "r2") == 0 ||
            strcmp(param, "r3") == 0 ||
            strcmp(param, "r4") == 0 ||
            strcmp(param, "r5") == 0 ||
            strcmp(param, "r6") == 0 ||
            strcmp(param, "r7") == 0) {

            return ADDRESSING_3;
    } else if (validate_label_name(param, filepath, linenumber) == STATUS_SUCCESS) {
            return ADDRESSING_1;
    } else {
        return ADDRESSING_NONE;
    }
}

Word make_instruction_word(byte opcode, byte src_addressing, byte dst_addressing, byte are) {
    return 
        opcode << 11 | 
        src_addressing << 7 | 
        dst_addressing << 3 | 
        are;
}

/* Assumes the addressings are either 2 or 3, and the parameters are valid. */
Status prepare_shared_param_word(
    Assembler* assembler, 
    const char* src_param, byte src_addressing, 
    const char* dst_param, byte dst_addressing,
    Word* out,
    const char* filepath, int linenumber) {

    int src_reg_num = 0;
    int dst_reg_num = 0;
    char* endptr = 0;

    if (src_addressing == ADDRESSING_2) {
        src_reg_num = strtol(&src_param[2], &endptr, 10);
        if (endptr == &src_param[2] || src_reg_num < 0 || src_reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, src_param);
            return STATUS_FAILURE;
        }
    } else { /* src_addressing == ADDRESSING_3 */
        src_reg_num = strtol(&src_param[1], &endptr, 10);
        if (endptr == &src_param[1] || src_reg_num < 0 || src_reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, src_param);
            return STATUS_FAILURE;
        }
    }

    if (dst_addressing == ADDRESSING_2) {
        dst_reg_num = strtol(&dst_param[2], &endptr, 10);
        if (endptr == &dst_param[2] || dst_reg_num < 0 || dst_reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, dst_param);
            return STATUS_FAILURE;
        }
    } else { /* dst_addressing == ADDRESSING_3 */
        dst_reg_num = strtol(&dst_param[1], &endptr, 10);
        if (endptr == &dst_param[1] || dst_reg_num < 0 || dst_reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, dst_param);
            return STATUS_FAILURE;
        }
    }

    *out = (src_reg_num << 6) | (dst_reg_num << 3) | ARE_ABSOLUTE;
    return STATUS_SUCCESS;
}

/* Receives a parameter string, and prepares the word for the param */
Status prepare_param_word(Assembler* assembler, const char* str, byte addressing, SrcOrDst src_or_dst, Word* out, const char* filepath, int linenumber) {
    int value = 0;
    char* endptr = 0;
    int reg_num = 0;
    
    /* Immediate Addressing */
    if (addressing == ADDRESSING_0) {
        value = strtol(str + 1, &endptr, 10);
        
        if (endptr == str || *endptr != '\0' || value > IMMEDIATE_MAX || value < IMMEDIATE_MIN) {
            printf("%s:%d: number out of range or invalid '%s'\n", filepath, linenumber, str);
            return STATUS_FAILURE;
        }

        *out = ((value << 3) | ARE_ABSOLUTE) & 0x7fff; /* zero-out the last bit */
        return STATUS_SUCCESS;
    }

    /* Direct Addressing */
    if (addressing == ADDRESSING_1) {        
        *out = 0;
        return STATUS_SUCCESS;
    }

    /* Indirect Register Addressing */
    if (addressing == ADDRESSING_2) {
        reg_num = strtol(&str[2], &endptr, 10);

        if (endptr == &str[2] || reg_num < 0 || reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, str);
            return STATUS_FAILURE;
        }
        
        if (src_or_dst == SOURCE_OPERAND) {
            *out = (reg_num << 6) | ARE_ABSOLUTE;
        } else {
            *out = (reg_num << 3) | ARE_ABSOLUTE;
        }
        return STATUS_SUCCESS;
    }

    /* Direct Register Addressing */
    if (addressing == ADDRESSING_3) {
        reg_num = strtol(&str[1], &endptr, 10);
        if (endptr == &str[1] || reg_num < 0 || reg_num > 7) {
            printf("%s:%d: invalid register '%s'\n", filepath, linenumber, str);
            return STATUS_FAILURE;
        }

        if (src_or_dst == SOURCE_OPERAND) {
            *out = (reg_num << 6) | ARE_ABSOLUTE;
        } else {
            *out = (reg_num << 3) | ARE_ABSOLUTE;
        }
        return STATUS_SUCCESS;
    }
    
    printf("should never happen!\n");
    return STATUS_FAILURE;
}

Status assembler_handle_instruction(Assembler* assembler, ParsedLine* parsed, const char* filepath, int line_number) {
    int opcode = 0;
    OpcodeTableEntry opcode_entry = {0};
    Word param_word = 0;
    Word src_param_word = 0;
    Word dst_param_word = 0;

    if (strlen(parsed->label) > 0) {
        if (assembler_add_label(assembler, parsed->label, LABEL_NONE, LABEL_CODE, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    }
    
    if (get_opcode(parsed->instruction, &opcode) != STATUS_SUCCESS) {
        printf("%s:%d: invalid operation '%s'\n", filepath, line_number, parsed->instruction);
        return STATUS_FAILURE;
    }

    opcode_entry = opcodeTable[opcode];

    if (parsed->num_params != opcode_entry.operands_num) {
        printf("%s:%d: unexpected number of operands for opcode %s\n", filepath, line_number, opcode_entry.name);
        return STATUS_FAILURE;
    }

    /*  TODO: handle instructions with two parameters (source and destination) */
    if (parsed->num_params == 2) {
        byte src_addressing = get_addressing_method(parsed->params[0], filepath, line_number);
        byte dst_addressing = get_addressing_method(parsed->params[1], filepath, line_number);

        if (!(src_addressing & opcode_entry.valid_src_operands) || !(dst_addressing & opcode_entry.valid_dst_operands)) {
            printf("%s:%d: unsupported addressing method for opcode %s\n", filepath, line_number, opcode_entry.name);
            return STATUS_FAILURE;
        }

        assembler->code[assembler->ic] = make_instruction_word(
            opcode,
            src_addressing,
            dst_addressing,
            ARE_ABSOLUTE
        );
        assembler->ic++;

        /* Handle the case of two register operands (shared operand word) */
        if ((src_addressing == ADDRESSING_2 || src_addressing == ADDRESSING_3) && 
            (dst_addressing == ADDRESSING_2 || dst_addressing == ADDRESSING_3)) {
            
            if (prepare_shared_param_word(assembler,
                parsed->params[0], src_addressing,
                parsed->params[1], dst_addressing,
                &param_word,
                filepath, line_number
            ) != STATUS_SUCCESS) {
                return STATUS_FAILURE;
            }
            
            assembler->code[assembler->ic] = param_word;
            assembler->ic++;
        } else { /* Separate words for the operands */
            if (prepare_param_word(assembler, parsed->params[0], src_addressing, SOURCE_OPERAND, &src_param_word, filepath, line_number) != STATUS_SUCCESS) {
                return STATUS_FAILURE;
            }

            if (prepare_param_word(assembler, parsed->params[1], dst_addressing, DEST_OPERAND, &dst_param_word, filepath, line_number) != STATUS_SUCCESS) {
                return STATUS_FAILURE;
            }

            assembler->code[assembler->ic] = src_param_word;
            assembler->ic++;
            assembler->code[assembler->ic] = dst_param_word;
            assembler->ic++;
        }
    }
    else if (parsed->num_params == 1) {
        byte dst_addressing = get_addressing_method(parsed->params[0], filepath, line_number);

        if (!(dst_addressing & opcode_entry.valid_dst_operands)) {
            printf("%s:%d: unsupported addressing method for opcode %s\n", filepath, line_number, opcode_entry.name);
            return STATUS_FAILURE;
        }

        if (prepare_param_word(assembler, parsed->params[0], dst_addressing, DEST_OPERAND, &param_word, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }

        assembler->code[assembler->ic] = make_instruction_word(
            opcode,
            ADDRESSING_NONE,   /* no source operand */
            dst_addressing,
            ARE_ABSOLUTE
        );
        assembler->ic++;

        assembler->code[assembler->ic] = param_word;
        assembler->ic++;
    }
    else if (parsed->num_params == 0) {
        assembler->code[assembler->ic] = make_instruction_word(
            opcode,
            ADDRESSING_NONE,   /* no source operand */
            ADDRESSING_NONE,   /* no destination operand */
            ARE_ABSOLUTE
        );
        assembler->ic++;
    } else {
        printf("%s:%d: unexpected number of parameters\n", filepath, line_number);
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

Status assembler_handle_directive(Assembler* assembler, ParsedLine* parsed, const char* filepath, int line_number) {
    if (strlen(parsed->label) > 0) {
        if (strcmp(parsed->instruction, ".data") != 0 && strcmp(parsed->instruction, ".string") != 0) {
            printf("%s:%d: labels only allowed for .data or .string directives\n", filepath, line_number);
            return STATUS_FAILURE;
        }

        if (assembler_add_label(assembler, parsed->label, LABEL_NONE, LABEL_DATA, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    }
    
    if (strcmp(parsed->instruction, ".data") == 0) {
        if (handle_data_directive(parsed, assembler, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    } else if (strcmp(parsed->instruction, ".string") == 0) {
        if (handle_string_directive(parsed, assembler, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    } else if (strcmp(parsed->instruction, ".extern") == 0) {
        if (handle_extern_directive(parsed, assembler, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    } 
    /* We handle entries in the second pass, when we already know all the addresses of the labels. */

    return STATUS_SUCCESS;
}

Status assembler_firstpass(Assembler* assembler, FILE* input_file, const char* preassembled_path) {
    char* fgets_result = 0;
    int line_number = 0;
    bool is_assembly_successfull = TRUE;
    char line[LINEBUFFER_SIZE] = {0};
    ParsedLine parsed_line = {0};

    while (TRUE) {
        fgets_result = fgets((char*)line, LINEBUFFER_SIZE, input_file);
        line_number++;

        if (fgets_result == NULL && !feof(input_file)) {
            printf("%s: failed reading from file", preassembled_path);
            return STATUS_FAILURE;
        }
        if (fgets_result == NULL && feof(input_file)) {
            break;
        }

        if (is_empty_line(line)) {
            continue;
        }

        if (parse_line(&parsed_line, line, preassembled_path, line_number) != STATUS_SUCCESS) {
            is_assembly_successfull = FALSE;
            continue;
        }

        if (is_directive(parsed_line.instruction)) {
            if (assembler_handle_directive(assembler, &parsed_line, preassembled_path, line_number) != STATUS_SUCCESS) {
                is_assembly_successfull = FALSE;
                continue;
            }
        } else {
            if (assembler_handle_instruction(assembler, &parsed_line, preassembled_path, line_number) != STATUS_SUCCESS) {
                is_assembly_successfull = FALSE;
                continue;
            }
        }
    }

    if (!is_assembly_successfull) {
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

Status assembler_create_obj_file(Assembler* assembler, const char* objfile_path) {
    FILE* objfile = 0;
    int i = 0;
    
    objfile = fopen(objfile_path, "wb");
    if (objfile == NULL) {
        printf("error opening output file: %s\n", objfile_path);
        return STATUS_FAILURE;
    }

    if (fprintf(objfile, "%d %d\n", assembler->code_section_size, assembler->dc) < 0) {
        printf("writing to file %s failed\n", objfile_path);
        fclose(objfile);
        return STATUS_FAILURE;
    }

    /* TODO: should be the same format as requested... */
    for (i = 0; i < assembler->code_section_size; i++) {
        if (fprintf(objfile, "%04d %05o\n", i + LOADING_BASE, assembler->code[i]) < 0) {
            printf("writing to file %s failed\n", objfile_path);
            fclose(objfile);
            return STATUS_FAILURE;
        }
    }
    for (i = 0; i < assembler->dc; i++) {
        if (fprintf(objfile, "%04d %05o\n", i + LOADING_BASE + assembler->code_section_size, assembler->data[i]) < 0) {
            printf("writing to file %s failed\n", objfile_path);
            fclose(objfile);
            return STATUS_FAILURE;
        }
    }

    fclose(objfile);
    return STATUS_SUCCESS;

}

bool has_entries(Assembler* assembler) {
    int i = 0;
    for (i = 0; i < assembler->label_table.count; i++) {
        if (assembler->label_table.labels[i].type == LABEL_ENTRY) {
            return TRUE;
        }
    }
    return FALSE;
}

Status assembler_create_entry_file(Assembler* assembler, char* entryfile_path) {
    FILE* entryfile = 0;
    int i = 0;
    int address = 0;

    if (!has_entries(assembler)) {
        return STATUS_SUCCESS;
    }

    entryfile = fopen(entryfile_path, "wb");
    if (entryfile == NULL) {
        printf("error opening entry file: %s\n", entryfile_path);
        return STATUS_FAILURE;
    }

    for (i = 0; i < assembler->label_table.count; i++) {
        if (assembler->label_table.labels[i].type == LABEL_ENTRY) {
            if (assembler->label_table.labels[i].code_or_data == LABEL_CODE) {
                address = assembler->label_table.labels[i].address + LOADING_BASE;
            } else {
                address = assembler->label_table.labels[i].address + LOADING_BASE + assembler->code_section_size;
            }

            if (fprintf(entryfile, "%s %d\n", assembler->label_table.labels[i].label_name, address) < 0) {
                printf("writing to file %s failed", entryfile_path);
                fclose(entryfile);
                return STATUS_FAILURE;
            }
        }
    }

    fclose(entryfile);
    return STATUS_SUCCESS;
}

Status assembler_create_extern_file(Assembler* assembler, char* externfile_path) {
    FILE* externfile = 0;
    int i = 0;

    if (assembler->extern_table.count == 0) {
        return STATUS_SUCCESS;
    }

    externfile = fopen(externfile_path, "wb");
    if (externfile == NULL) {
        printf("error opening extern file: %s\n", externfile_path);
        return STATUS_FAILURE;
    }

    for (i = 0; i < assembler->extern_table.count; i++) {
        if (fprintf(
                externfile, 
                "%s %d\n", 
                assembler->extern_table.refs[i].label_name,
                assembler->extern_table.refs[i].address) < 0) {
            
            printf("writing to file %s failed", externfile_path);
            fclose(externfile);
            return STATUS_FAILURE;
        }
    }

    fclose(externfile);
    return STATUS_SUCCESS;
}

Status assembler_assemble(Assembler* assembler, char* source_file_path) {
    char* preassembled_path = 0;
    char* objfile_path = 0;
    char* entryfile_path = 0;
    char* externfile_path = 0;
    FILE* input_file = 0;
    Status status = 0;

    preassembled_path = change_extension(source_file_path, "am");
    if (preassembled_path == NULL) {
        goto FAILURE;
    }
    objfile_path = change_extension(source_file_path, "ob");
    if (objfile_path == NULL) {
        goto FAILURE;
    }
    entryfile_path = change_extension(source_file_path, "ent");
    if (entryfile_path == NULL) {
        goto FAILURE;
    }

    externfile_path = change_extension(source_file_path, "ext");
    if (externfile_path == NULL) {
        goto FAILURE;
    }

    input_file = fopen(preassembled_path, "rb");
    if (input_file == NULL) {
        printf("%s: cannot open file\n", preassembled_path);
        goto FAILURE;
    }

    if (assembler_firstpass(assembler, input_file, preassembled_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (fseek(input_file, 0, SEEK_SET) != 0) {
        printf("%s: cannot seek to the beginning of the file\n", preassembled_path);
        goto FAILURE;
    }

    if (assembler->ic + assembler->dc + LOADING_BASE > MAX_MEMORY_SIZE) {
        printf("error: Code and data exceed memory limit\n");
        return STATUS_FAILURE;
    }
    assembler->code_section_size = assembler->ic;
    assembler->ic = 0;

    if (assembler_secondpass(assembler, input_file, preassembled_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (assembler_create_obj_file(assembler, objfile_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (assembler_create_entry_file(assembler, entryfile_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (assembler_create_extern_file(assembler, externfile_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    goto SUCCESS;
SUCCESS:
    status = STATUS_SUCCESS;
    goto CLEANUP;
FAILURE:
    status = STATUS_FAILURE;
CLEANUP:
    if (input_file != NULL) {
        fclose(input_file);
    }
    free(preassembled_path);
    free(objfile_path);
    free(entryfile_path);
    free(externfile_path);
    return status;
}
