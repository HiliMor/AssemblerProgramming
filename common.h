#ifndef _COMMON_H
#define _COMMON_H

#define TRUE 1
#define FALSE 0

#define MAX_LINE_SIZE 80
#define MAX_MEMORY_SIZE 4096
#define LINEBUFFER_SIZE (MAX_LINE_SIZE + 2)
#define INITIAL_CAPACITY 1024

#define NUM_RESERVED_WORDS 30
extern const char* reserved_words[NUM_RESERVED_WORDS];

typedef unsigned char Status;
#define STATUS_SUCCESS 0
#define STATUS_FAILURE 1

typedef unsigned char bool;
typedef unsigned char byte;

/* a word the 15 least-significant bits in an 'unsigned short'*/
typedef unsigned short Word;

typedef struct bytearray_t {
    byte* buffer;
    int capacity;
    int size;
} ByteArray;

typedef enum {
    LABEL_NONE,
    LABEL_ENTRY,
    LABEL_EXTERN
} LabelType;

typedef enum {
    LABEL_CODE,
    LABEL_DATA
} CodeOrData;

Status bytearray_init(ByteArray* bytearray);
void bytearray_free(ByteArray* bytearray);
Status bytearray_append(ByteArray* bytearray, byte* bytes_to_append, int size_to_append);

typedef struct {
    char tokens[LINEBUFFER_SIZE][LINEBUFFER_SIZE]; /* A 2d-array to hold tokens. rows are tokens, cols are token characthers. */
    int size;
} Tokens;

/* Reads the line 'line' and splits is to tokens from whitespaces.
 * e.g. "   hello   world"  -> ["hello", "world"].
 * the caller must call tokens_free on the 'tokens' struct. */
void tokens_init(Tokens* tokens, char* line);

Status write_bytearray_to_file(ByteArray* bytearray, char* file_path);

/* returns a new string, identical to the input, but without an extension.
 * e.g. testdata/example1.as -> testdata/example1 /
 * returns NULL on failure (e.g. if no '.' was found)
 * the caller must call free() on the returned pointer.
 * */
char* base_name(char* path);

/* returns a new string, identical to the input, but with a new extension.
 * e.g. change_extension("testdata/example1.as", "txt") -> "testdata/example1.txt"
 * returns NULL on failure (e.g. if no '.' was found)
 * the caller must call free() on the returned pointer.
 * */
char* change_extension(char* path, char* new_extension);
Status validate_extension(char* str, char* extension);

bool is_whitespace(char ch);

char* my_strdup(const char* src);

#endif
