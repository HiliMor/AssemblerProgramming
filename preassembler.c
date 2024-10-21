#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "preassembler.h"
#include "common.h"

Status macrotable_init(MacroTable* table) {
    table->arr_capacity = INITIAL_CAPACITY;
    table->macro_count = 0;
    table->macros = (MacroTableEntry*)malloc(table->arr_capacity * sizeof(MacroTableEntry));
    if (table->macros == NULL) {
        printf("Failed to allocate memory for macros\n");
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

Status add_macro(MacroTable* table, char* name, char* content) {
    int i = 0;
    MacroTableEntry* new_macros = 0;
    
    for (i = 0; i < table->macro_count; i++) {
        if(strcmp(table->macros[i].macro_name , name) == 0){
            return STATUS_FAILURE; /* macro already exist */
        }
    }

    /* if table is full, increase capacity */
    if(table->macro_count >= table->arr_capacity) {
        new_macros = (MacroTableEntry*)malloc(table->arr_capacity * 2 * sizeof(MacroTableEntry));
        if (new_macros == NULL) {
            printf("failed to allocater memory for macros\n");
            return STATUS_FAILURE; 
        }
        memcpy(new_macros, table->macros, table->arr_capacity * sizeof(MacroTableEntry));
        free(table->macros);
        table->arr_capacity *= 2;
        table->macros = new_macros;
    }

    table->macros[table->macro_count].macro_name = my_strdup(name);
    if (table->macros[table->macro_count].macro_name == NULL) {
        printf("failed to allocater memory for macros\n");
        return STATUS_FAILURE; 
    }
    table->macros[table->macro_count].macro_content = my_strdup(content);
    if (table->macros[table->macro_count].macro_content == NULL) {
        printf("failed to allocater memory for macros\n");
        free(table->macros[table->macro_count].macro_name);
        return STATUS_FAILURE; 
    }
    table->macro_count++;
    return STATUS_SUCCESS;
}

char* get_macro_content(MacroTable* table, char* name) {
    int i = 0;
    for (i = 0; i < table->macro_count; i++) {
        if (strcmp(table->macros[i].macro_name , name) == 0) {
            return table->macros[i].macro_content;
        }
    }
    return NULL;
}

void free_macro_table(MacroTable* table) {
    int i = 0;
    
    if (table->macros != NULL) {
        for (i = 0; i < table->macro_count; i++) {
            free(table->macros[i].macro_name);
            free(table->macros[i].macro_content);
        }
        free(table->macros);
        table->macros = NULL;
    }

    table->macro_count = 0;
    table->arr_capacity = 0;
}

Status validate_macro_name(const char* macro_name, const char* input_file_path, int line_number) {
    int i = 0;
    if (!isalpha(macro_name[0])) {
        printf("%s:%d: macro name must start with a letter\n", input_file_path, line_number);
        return STATUS_FAILURE;
    }
    for (i = 0; macro_name[i] != '\0'; i++) {
        if (!isalnum(macro_name[i]) && macro_name[i] != '_') {
            printf("%s:%d: macro name contains invalid characters\n", input_file_path, line_number);
            return STATUS_FAILURE;
        }
    }
    for (i = 0; i < NUM_RESERVED_WORDS; i++) {
        if (strcmp(macro_name, reserved_words[i]) == 0) {
            printf("%s:%d: macro name cannot be a reserved word: %s)\n", input_file_path, line_number, reserved_words[i]);
            return STATUS_FAILURE;
        }
    }
    return STATUS_SUCCESS;
}

/* Assumes that there is only one token. (macros must be on their own line).
   If the token is not a macro, just appends the token to output_bytearray.
   If the token is a macro, appends the macro-content to the output_bytearray */
Status replace_macros_in_line(MacroTable* macro_table, Tokens* tokens, ByteArray* output_bytearray) {
    const char* macro_content = 0;

    macro_content = get_macro_content(macro_table, tokens->tokens[0]);
    if (macro_content != NULL) {
        if (bytearray_append(output_bytearray, (byte*)macro_content, strlen(macro_content)) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    } else {
        if (bytearray_append(output_bytearray, (byte*)tokens->tokens[0], strlen(tokens->tokens[0])) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
        if (bytearray_append(output_bytearray, (byte*)"\n", 1) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }
    }

    return STATUS_SUCCESS;
}



Status preassemble(char* input_file_path) {
    char* output_file_path = 0;
    ByteArray preassembled_bytearray = {0};
    FILE* input_file = 0;
    char* fgets_result = 0;
    byte line[LINEBUFFER_SIZE] = {0};
    Tokens tokens = {0};
    char* current_macro_name = 0;
    ByteArray current_macro_content = {0};
    MacroTable macro_table = {0};
    int line_number = 0;

    if (validate_extension(input_file_path, "as") != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (bytearray_init(&preassembled_bytearray) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (macrotable_init(&macro_table) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    input_file = fopen(input_file_path, "rb");
    if (input_file == NULL) {
        printf("%s: cannot open file\n", input_file_path);
        goto FAILURE;
    }

    while (TRUE) {
        fgets_result = fgets((char*)line, LINEBUFFER_SIZE, input_file);
        line_number++;

        if (fgets_result == NULL && !feof(input_file)) {
            printf("%s: failed reading from file", input_file_path);
            goto FAILURE;
        }
        if (fgets_result == NULL && feof(input_file)) {
            break;
        }

        if (strlen((char*)line) > MAX_LINE_SIZE && line[strlen((char*)line) - 1] != '\n') {
            printf("%s:%d: line too long\n", input_file_path, line_number);
            goto FAILURE;
        }

        tokens_init(&tokens, (char*)line);
        if (tokens.size == 0) {
            continue;
        }

        if (tokens.tokens[0][0] == ';') { /* This is a comment*/
            continue;
        }

        if (strcmp(tokens.tokens[0], "macr") == 0) {
            if (current_macro_name != NULL) {
                printf("%s:%d: nested macro definition\n", input_file_path, line_number);
                goto FAILURE;
            }

            if (tokens.size != 2) {
                printf("%s:%d: invalid macro definition\n", input_file_path, line_number);
                goto FAILURE;
            }

            if (validate_macro_name(tokens.tokens[1], input_file_path, line_number) != STATUS_SUCCESS) {
                goto FAILURE;
            }

            current_macro_name = my_strdup(tokens.tokens[1]);
            if (current_macro_name == NULL) {
                printf("strdup failed\n");
                goto FAILURE;
            }

            if (bytearray_init(&current_macro_content) != STATUS_SUCCESS) {
                goto FAILURE;
            }

            continue;
        }

        if (strcmp(tokens.tokens[0], "endmacr") == 0) {
            if (current_macro_name == NULL) {
                printf("%s:%d: endmacr encountered without macro definition\n", input_file_path, line_number);
                goto FAILURE;
            }

            if (tokens.size != 1) {
                printf("%s:%d: endmacr must be on a separate line\n", input_file_path, line_number);
                goto FAILURE;
            }

            if (bytearray_append(&current_macro_content, (byte*)"", 1) != STATUS_SUCCESS) { /* Add null terminator to the 'current_macro_content' buffer. */
                goto FAILURE;
            }
            if (add_macro(&macro_table, current_macro_name, (char*)current_macro_content.buffer) != STATUS_SUCCESS) {
                printf("%s:%d: macro '%s' already defined\n", input_file_path, line_number, current_macro_name);
                goto FAILURE;
            }

            free(current_macro_name);
            current_macro_name = NULL;
            bytearray_free(&current_macro_content);
            continue;
        }

        if (current_macro_name != NULL) { /* While in a macro, append the line to 'current_macro_contents'. */
            if (bytearray_append(&current_macro_content, line, strlen((char*)line)) != STATUS_SUCCESS) {
                goto FAILURE;
            }
        } else { /* If not in a macro, just append the line to the output-buffer. */
            if (tokens.size != 1) {
                if (bytearray_append(&preassembled_bytearray, line, strlen((char*)line)) != STATUS_SUCCESS) {
                    goto FAILURE;
                }
            } else {
                if (replace_macros_in_line(&macro_table, &tokens, &preassembled_bytearray) != STATUS_SUCCESS) {
                    goto FAILURE;
                }
            }
            
        }
    }

    if (current_macro_name != NULL) {
        printf("%s: unterminated macro\n", input_file_path);
        goto FAILURE;
    }

    fclose(input_file);
    input_file = NULL;

    output_file_path = change_extension(input_file_path, "am");
    if (output_file_path == NULL) {
        printf("failed to change extension of %s", input_file_path);
        goto FAILURE;
    }

    if (write_bytearray_to_file(&preassembled_bytearray, output_file_path) != STATUS_SUCCESS) {
        goto FAILURE;
    }

    if (input_file != NULL) {
        fclose(input_file);
    }
    bytearray_free(&preassembled_bytearray);
    bytearray_free(&current_macro_content);
    free_macro_table(&macro_table);
    free(current_macro_name);
    free(output_file_path);
    return STATUS_SUCCESS;

FAILURE:
    if (input_file != NULL) {
        fclose(input_file);
    }
    bytearray_free(&preassembled_bytearray);
    bytearray_free(&current_macro_content);
    free_macro_table(&macro_table);
    free(current_macro_name);
    free(output_file_path);
    return STATUS_FAILURE;
}
