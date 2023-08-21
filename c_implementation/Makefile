# Makefile for building ptttl_cli sample program

BUILD_DIR   := build
OBJ_DIR     := $(BUILD_DIR)/obj
OUTPUT_BIN  := $(BUILD_DIR)/ptttl_cli

RM          := rm
MKDIR       := mkdir

CFLAGS := -Wall -pedantic

default: ptttl_cli

make_build_dir:
	@$(MKDIR) $(BUILD_DIR)
	@$(MKDIR) $(OBJ_DIR)

build_objs:
	$(CC) $(CFLAGS) -c ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c ptttl_cli.c -o $(OBJ_DIR)/ptttl_cli.o

ptttl_cli: build_objs
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/ptttl_cli.o -o $(OUTPUT_BIN) -lm

clean:
	$(RM) $(OBJ_DIR)/ptttl_parser.o
	$(RM) $(OBJ_DIR)/ptttl_sample_generator.o
	$(RM) $(OBJ_DIR)/ptttl_to_wav.o
	$(RM) $(OBJ_DIR)/ptttl_cli.o
	$(RM) $(OUTPUT_BIN)
