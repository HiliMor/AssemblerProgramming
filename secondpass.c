#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "parser.h"
#include "secondpass.h"

Status assembler_secondpasss_prepare_label_word(Assembler* assembler, const char* label, Word* out, const char* filepath, int line_number) {
    LabelTableEntry label_entry = {0};

    if (labeltable_get_entry(&assembler->label_table, label, &label_entry) != STATUS_SUCCESS) {
        printf("%s:%d: label not found\n", filepath, line_number);
        return STATUS_FAILURE;
    }

    if (label_entry.type == LABEL_EXTERN) {
        *out = ARE_EXTERNAL;
        
        strcpy(assembler->extern_table.refs[assembler->extern_table.count].label_name, label_entry.label_name);
        assembler->extern_table.refs[assembler->extern_table.count].address = assembler->ic + LOADING_BASE;
        assembler->extern_table.count++;

        return STATUS_SUCCESS;
    }

    if (label_entry.code_or_data == LABEL_CODE) {
        *out = ((label_entry.address + LOADING_BASE) << 3) | ARE_RELATIVE;
    } else { /* Data label */
        *out = ((label_entry.address + LOADING_BASE + assembler->code_section_size) << 3) | ARE_RELATIVE;
    }

    return STATUS_SUCCESS;
}

/* We need to do less validations, since the first pass validated *most* of the things, except the labels. */
Status assembler_secondpass_handle_instruction(Assembler* assembler, ParsedLine* parsed, const char* filepath, int line_number) {
    Word src_param_word = 0;
    Word dst_param_word = 0;


    assembler->ic++; /* instruction word already prepared */

    if (parsed->num_params == 2) {
        /* TODO: implement two params (second pass)*/
        byte src_addressing = get_addressing_method(parsed->params[0], filepath, line_number);
        byte dst_addressing = get_addressing_method(parsed->params[1], filepath, line_number);

        if ((src_addressing == ADDRESSING_2 || src_addressing == ADDRESSING_3) && 
                (dst_addressing == ADDRESSING_2 || dst_addressing == ADDRESSING_3)) {
            assembler->ic += 1;
            return STATUS_SUCCESS;
        }

        if (src_addressing == ADDRESSING_1) {
            if (assembler_secondpasss_prepare_label_word(assembler, parsed->params[0], &src_param_word, filepath, line_number) != STATUS_SUCCESS) {
                return STATUS_FAILURE;
            }

            assembler->code[assembler->ic] = src_param_word;
            assembler->ic++;
        } else {
            assembler->ic++;
        }

        if (dst_addressing == ADDRESSING_1) {
            if (assembler_secondpasss_prepare_label_word(assembler, parsed->params[1], &dst_param_word, filepath, line_number) != STATUS_SUCCESS) {
                return STATUS_FAILURE;
            }

            assembler->code[assembler->ic] = dst_param_word;
            assembler->ic++;
        } else {
            assembler->ic++;
        }

        return STATUS_SUCCESS;
    } else if (parsed->num_params == 1) {
        byte dst_addressing = get_addressing_method(parsed->params[0], filepath, line_number);

        if (dst_addressing != ADDRESSING_1) {
            assembler->ic += 1;
            return STATUS_SUCCESS;
        }

        if (assembler_secondpasss_prepare_label_word(assembler, parsed->params[0], &dst_param_word, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }

        assembler->code[assembler->ic] = dst_param_word;
        assembler->ic++;
        return STATUS_SUCCESS;
    } else if (parsed->num_params == 0) {
        return STATUS_SUCCESS;
    }

    return STATUS_FAILURE;
}

/* We only handle "entries" - by marking the labeltable as entries */
Status assembler_secondpass_handle_directive(Assembler* assembler, ParsedLine* parsed, const char* filepath, int line_number) {
    int i = 0;

    if (strcmp(parsed->instruction, ".entry") != 0) {
        return STATUS_SUCCESS;
    }

    if (parsed->num_params != 1) {
        printf("%s:%d: .entry directive must have exactly one parameter\n", filepath, line_number);
        return STATUS_FAILURE;
    }

    for (i = 0; i < assembler->label_table.count; i++) {
        if (strcmp(parsed->params[0], assembler->label_table.labels[i].label_name) == 0) {
            if (assembler->label_table.labels[i].type == LABEL_EXTERN) {
                printf("%s:%d: cannot mark an extern label as entry\n", filepath, line_number);
                return STATUS_FAILURE;
            }
            if (assembler->label_table.labels[i].type == LABEL_ENTRY) {
                printf("%s:%d: label already marked as entry\n", filepath, line_number);
                return STATUS_FAILURE;
            }

            assembler->label_table.labels[i].type = LABEL_ENTRY;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_FAILURE;
}


Status assembler_secondpass(Assembler* assembler, FILE* input_file, const char* preassembled_path) {
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
            if (assembler_secondpass_handle_directive(assembler, &parsed_line, preassembled_path, line_number) != STATUS_SUCCESS) {
                is_assembly_successfull = FALSE;
                continue;
            }
            continue; 
        }

        if (assembler_secondpass_handle_instruction(assembler, &parsed_line, preassembled_path, line_number) != STATUS_SUCCESS) {
            is_assembly_successfull = FALSE;
            continue;
        }
    }

    if (!is_assembly_successfull) {
        return STATUS_FAILURE;
    }
    return STATUS_SUCCESS;
}
