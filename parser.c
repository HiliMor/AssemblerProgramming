#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

#define MAX_LABEL_LENGTH 31
#define DIRECTIVES_NUM 4

char *directives[] = {".data", ".string", ".entry", ".extern"};

/* chek if directive */
bool is_directive(const char* word) {
    int i = 0;

    for (i = 0; i < DIRECTIVES_NUM; i++) {
        if (strcmp(word, directives[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

bool is_empty_line(const char* line) {
    int i = 0;
    while (is_whitespace(line[i])) {
        i++;
    }
    return (line[i] == '\0');
}

Status validate_label_name(const char* label, const char* file_path, int line_number) {
    int len = strlen(label);
    int i = 0;

    if (len == 0) {
        printf("%s:%d: empty label not allowed\n", file_path, line_number);
        return STATUS_FAILURE;
    }

    if (len > MAX_LABEL_LENGTH) {
        printf("%s:%d: label too long\n", file_path, line_number);
        return STATUS_FAILURE;
    }

    if (!isalpha(label[0])) {
        printf("%s:%d: label must start with a letter\n", file_path, line_number);
        return STATUS_FAILURE;
    }

    for (i = 0; i < len; i++) {
        if (!isalnum(label[i])) {
            printf("%s:%d: label contains invalid characters\n", file_path, line_number);
            return STATUS_FAILURE;
        }
    }

    for (i = 0; i < NUM_RESERVED_WORDS; i++) {
        if (strcmp(label, reserved_words[i]) == 0) {
            printf("%s:%d: label cannot be a reserved word\n", file_path, line_number);
            return STATUS_FAILURE;
        }
    }

    return STATUS_SUCCESS;
}

/* line should point to the start of the first parameter */
Status parse_params(ParsedLine* parsed, const char* line, const char* filepath, int line_number) {
    int i = 0;
    int j = 0;

    while (line[i] != '\0') {
        while (is_whitespace(line[i])) {
            i++;
        }

        j = 0;
        while(line[i] != '\0' && line[i] != ',' && !is_whitespace(line[i])) {
            parsed->params[parsed->num_params][j] = line[i];
            i++;
            j++;
        }

        while (is_whitespace(line[i])) {
            i++;
        }

        if (j == 0) {
            printf("%s:%d: invalid parameter structure\n", filepath, line_number);
            return STATUS_FAILURE;
        }

        parsed->num_params++;

        if (line[i] == '\0') {
            break;
        }

        if (line[i] == ',') {
            i++;
            continue;
        }

        /* If we found another letter, but there was no ',' before that (e.g. "a b c")*/
        printf("%s:%d: invalid parameter structure\n", filepath, line_number);
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

Status parse_line(ParsedLine* parsed, char* line, const char* filepath, int line_number) {    
    int i = 0;
    int j = 0;
    char token[LINEBUFFER_SIZE] = {0};

    memset(parsed, 0, sizeof(ParsedLine));

    /* Read the first token into 'token' (terminated by whitespace).
     * If it ends with a ':' , copy it into 'parsed->label'.
     * Otherwise, copy it into parsed->instruction. */

    while (is_whitespace(line[i])) { /* Skip whitespaces */
        i++;
    }

    j = 0;
    while (line[i] != '\0' && !is_whitespace(line[i])) {
        token[j] = line[i];
        i++;
        j++;
    }

    if (token[strlen(token) - 1] == ':') { /* The first token is a label: read the instruction. */
        strcpy(parsed->label, token);
        parsed->label[strlen(token) - 1] = '\0'; /* Remove the colon, we don't need it anymore. */

        if (validate_label_name(parsed->label, filepath, line_number) != STATUS_SUCCESS) {
            return STATUS_FAILURE;
        }

        while (is_whitespace(line[i])) { /* Skip whitespaces */
            i++;
        }
        
        j = 0;
        while (line[i] != '\0' && !is_whitespace(line[i])) {
            parsed->instruction[j] = line[i];
            i++;
            j++;
        }

        if (strlen(parsed->instruction) == 0) {
            printf("%s:%d: no instruction or directive found\n", filepath, line_number);
            return STATUS_FAILURE;
        }

    } else { /* The first token is an instruction */
        strcpy(parsed->instruction, token);
    }

    while (is_whitespace(line[i])) { /* Skip whitespaces. */
        i++;
    }

    if (line[i] != '\0') {
        if (parse_params(parsed, line + i, filepath, line_number)) {
            return STATUS_FAILURE;
        }
    }

    return STATUS_SUCCESS;
}
