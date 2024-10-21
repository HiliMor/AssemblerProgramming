.PHONY: all

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -ansi -pedantic

# Target executable
TARGET = a.out

# Source files
SRCS = main.c assembler.c preassembler.c secondpass.c parser.c common.c

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) -I. $(SRCS)

# Clean up build files
clean:
	rm -f $(TARGET)