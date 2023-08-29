# Makefile for building ptttl_cli sample program

BUILD_DIR      := build
SRC_DIR        := src
FUZZ_DIR       := fuzz_testing
OBJ_DIR        := $(BUILD_DIR)/obj

PTTTL_CLI_PROG := ptttl_cli
FUZZ_PROG      := afl_fuzz_parser
CLI_BIN        := $(BUILD_DIR)/$(PTTTL_CLI_PROG)
FUZZ_BIN       := $(BUILD_DIR)/$(FUZZ_PROG)

RM             := rm -f
MKDIR          := mkdir

CFLAGS := -Wall -pedantic -I$(SRC_DIR)
#CFLAGS += -g -O0 -pg -no-pie

AFL_CC := afl-clang-fast

default: ptttl_cli

make_build_dir:
	$(MKDIR) $(BUILD_DIR)
	$(MKDIR) $(OBJ_DIR)

build_ptttl_objs:

ptttl_cli:
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_cli.c -o $(OBJ_DIR)/ptttl_cli.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/ptttl_cli.o -o $(CLI_BIN) -lm

afl_fuzz_harness: CC=$(AFL_CC)
afl_fuzz_harness: CFLAGS += -O2
afl_fuzz_harness:
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(AFL_CC) $(CFLAGS) -c $(FUZZ_DIR)/afl_fuzz_harness.c -o $(OBJ_DIR)/afl_fuzz_harness.o
	$(AFL_CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/afl_fuzz_harness.o -o $(FUZZ_BIN)

clean:
	$(RM) $(OBJ_DIR)/ptttl_parser.o
	$(RM) $(OBJ_DIR)/ptttl_sample_generator.o
	$(RM) $(OBJ_DIR)/ptttl_to_wav.o
	$(RM) $(OBJ_DIR)/ptttl_cli.o
	$(RM) $(OBJ_DIR)/afl_fuzz_harness.o
	$(RM) $(CLI_BIN) $(FUZZ_BIN)
