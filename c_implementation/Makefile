# Makefile for building ptttl_cli sample program

BUILD_DIR   := build
SRC_DIR     := src
FUZZ_DIR    := fuzz_testing
OBJ_DIR     := $(BUILD_DIR)/obj
CLI_BIN     := $(BUILD_DIR)/ptttl_cli
FUZZ_BIN    := $(BUILD_DIR)/afl_fuzz_harness

RM          := rm -f
MKDIR       := mkdir

CFLAGS := -Wall -pedantic -I$(SRC_DIR)
#CFLAGS += -g -O0 -pg -no-pie

AFL_CC := afl-clang-fast

default: ptttl_cli

make_build_dir:
	$(MKDIR) $(BUILD_DIR)
	$(MKDIR) $(OBJ_DIR)

build_ptttl_objs:
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o

ptttl_cli: build_ptttl_objs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_cli.c -o $(OBJ_DIR)/ptttl_cli.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/ptttl_cli.o -o $(CLI_BIN) -lm

fuzz_parser: CC=$(AFL_CC)
fuzz_parser: CFLAGS += -O2
fuzz_parser: build_ptttl_objs
	$(AFL_CC) $(CFLAGS) -c $(FUZZ_DIR)/afl_fuzz_harness.c -o $(OBJ_DIR)/afl_fuzz_harness.o
	$(AFL_CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/afl_fuzz_harness.o -o $(FUZZ_BIN)

clean:
	$(RM) $(OBJ_DIR)/ptttl_parser.o
	$(RM) $(OBJ_DIR)/ptttl_sample_generator.o
	$(RM) $(OBJ_DIR)/ptttl_to_wav.o
	$(RM) $(OBJ_DIR)/ptttl_cli.o
	$(RM) $(OBJ_DIR)/afl_fuzz_harness.o
	$(RM) $(CLI_BIN) $(FUZZ_BIN)
