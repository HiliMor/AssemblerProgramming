#ifndef _PARSER_H
#define _PARSER_H

#include "common.h"

typedef struct {
    char label[LINEBUFFER_SIZE]; /* optional label */
    char instruction[LINEBUFFER_SIZE]; /* instruction or directive */
    int num_params;
    char params[LINEBUFFER_SIZE][LINEBUFFER_SIZE];
} ParsedLine;

bool is_empty_line(const char* line);
Status validate_label_name(const char* label, const char* file_path, int line_number);

Status parse_params(ParsedLine* parsed, const char* line, const char* filepath, int line_number);
Status parse_line(ParsedLine* parsed, char* line, const char* filepath, int line_number);

bool is_directive(const char* word);

#endif
