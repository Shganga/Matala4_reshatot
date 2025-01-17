#######################################
#   Makefile for the ping program.    #
#   Updated by: Your Name             #
#   Date: 2025-01-17                  #
#######################################

# Use the gcc compiler.
CC = gcc

# Compiler flags for warnings, errors, and C standard.
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic

# Command to remove files.
RM = rm -f

# Source and header files.
SRC = ping.c
HEADERS = ping.h

# Executable name.
EXE = ping

# Phony targets - not actual files.
.PHONY: all clean run run_debug

# Default target: Build the executable.
all: $(EXE)

# Build the executable.
$(EXE): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC)

# Run the program with an example command.
# Adjust the arguments as needed.
run: $(EXE)
	sudo ./$(EXE) -a 1.1.1.1 -t 4 -c 4

# Run the program with debugging enabled.
run_debug: $(EXE)
	sudo gdb --args ./$(EXE) -a 1.1.1.1 -t 4 -c 4

# Clean up the compiled files.
clean:
	$(RM) $(EXE)
