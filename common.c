#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

const char* reserved_words[NUM_RESERVED_WORDS] = {
    "mov", "cmp", "add", "sub", "lea", "clr", "not", "inc", "dec", 
    "jmp", "bne", "red", "prn", "jsr", "rts", "stop",
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "macr", "endmacr", 
    "data", "string", "entry", "extern"
};

bool is_whitespace(char ch) {
    return ch == ' ' || ch == '\n' || ch == '\t';
}

void tokens_init(Tokens* tokens, char* line) {
  int i = 0;
  int token_start_index = -1;
  int token_length = 0;
  int line_len = strlen(line);

  tokens->size = 0;
  memset(tokens->tokens, 0, LINEBUFFER_SIZE * LINEBUFFER_SIZE);

  for (i = 0; i < line_len; i++) {
        if (token_start_index == -1) { /* The case where we are not reading a token right now */
            if (is_whitespace(line[i])) {
                continue;
            } else {
                token_start_index = i;
                continue;
            }
        } 

        /* We are reading a token right now */
        if (is_whitespace(line[i])) {
            token_length = i - token_start_index;
            memcpy(tokens->tokens[tokens->size], line + token_start_index, token_length);
            tokens->size += 1;
            token_start_index = -1;
        } else {
            continue;
        }   
  }

  if (token_start_index != -1) {
    token_length = i - token_start_index;
    memcpy(tokens->tokens[tokens->size], line + token_start_index, token_length);
    tokens->size += 1;
  }
}

Status bytearray_init(ByteArray* bytearray) {
    bytearray->buffer = malloc(1024);
    if (bytearray->buffer == NULL) {
      printf("failed to initialize bytearray");
      return STATUS_FAILURE;
    }

    bytearray->size = 0;
    bytearray->capacity = 1024;
    return STATUS_SUCCESS;
}

void bytearray_free(ByteArray* bytearray) {
  free(bytearray->buffer);
  bytearray->buffer = NULL;
  bytearray->size = 0;
  bytearray->capacity = 0;
}

Status bytearray_append(ByteArray* bytearray, byte* bytes_to_append, int size_to_append) {
    byte* new_buffer = NULL;

    while (bytearray->size + size_to_append > bytearray->capacity) {
        new_buffer = malloc(2 * bytearray->capacity);
        if (new_buffer == NULL) {
            printf("malloc failed\n");
            return STATUS_FAILURE;
        }

        memcpy(new_buffer, bytearray->buffer, bytearray->size);
        free(bytearray->buffer);
        bytearray->buffer = new_buffer;
        bytearray->capacity *= 2;
    }

    memcpy(bytearray->buffer + bytearray->size, bytes_to_append, size_to_append);
    bytearray->size += size_to_append;
    return STATUS_SUCCESS;
}

Status write_bytearray_to_file(ByteArray* bytearray, char* file_path) {
    FILE* file = 0;

    file = fopen(file_path, "wb");
    if (file == NULL) {
        printf("failed to open file %s for writing\n", file_path);
        return STATUS_FAILURE;
    }

    if (fwrite(bytearray->buffer, 1, bytearray->size, file) != bytearray->size) {
        printf("failed to write to file %s\n", file_path);
        fclose(file);
        return STATUS_FAILURE;
    }

    fclose(file);
    return STATUS_SUCCESS;
}


Status validate_extension(char* str, char* extension) {
  if (strlen(str) <= strlen(extension)) {
    printf("%s: invalid extension (should be %s)\n", str, extension);
    return STATUS_FAILURE;
  }

  if (str[strlen(str) - strlen(extension) - 1] != '.') {
    printf("%s: invalid extension (should be %s)\n", str, extension);
    return STATUS_FAILURE;
  }

  if (strcmp(str + strlen(str) - strlen(extension), extension) != 0) {
        printf("%s: invalid extension (should be %s)\n", str, extension);
        return STATUS_FAILURE;
    }

    return STATUS_SUCCESS;
}

char* my_strdup(const char* src) {
    char* dest = (char*)malloc(strlen(src) + 1);
    if (dest != NULL) {
        strcpy(dest, src);
    }
    return dest;
}

char* base_name(char* path){
    char* result = NULL;
    int size = 0;
    int i = 0;

    size = strlen(path);
    result = malloc(size + 1);
    if (result == NULL) {
        printf("base_name: malloc failed\n");
        return NULL;
    }
    memcpy(result, path, size + 1);

    for (i = size - 1; i >= 0; i--) {
        if (result[i] == '.') {
            result[i] = '\0';
            return result;
        }
    }

    printf("base_name: no '.' charachter found\n");
    free(result);
    return NULL;
}

char* change_extension(char* path, char* new_extension) {
    char* base = 0;
    char* result = 0;

    base = base_name(path);
    if (base == NULL) {
        return NULL;
    }

    result = malloc(strlen(base) + strlen(new_extension) + 2); /* extra space for a '.' and a '\0' */
    if (result == NULL) {
        printf("change_extension: malloc failed\n");
        free(base);
        return NULL;
    }

    strcpy(result, base);
    strcat(result, ".");
    strcat(result, new_extension);

    free(base);
    return result;
}
