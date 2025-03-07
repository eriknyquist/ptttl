# Makefile for building ptttl_cli sample program

BUILD_DIR      := build
SRC_DIR        := src
TEST_DIR       := test
FUZZ_DIR       := fuzz_testing
OBJ_DIR        := $(BUILD_DIR)/obj
TEST_OBJ_DIR   := $(TEST_DIR)/obj

PTTTL_CLI_PROG := ptttl_cli
FUZZ_PROG      := afl_fuzz_harness
TEST_PROG      := test_ptttl
CLI_BIN        := $(BUILD_DIR)/$(PTTTL_CLI_PROG)
FUZZ_BIN       := $(BUILD_DIR)/$(FUZZ_PROG)
TEST_BIN       := $(TEST_DIR)/$(TEST_PROG)

RM             := rm -f
MKDIR          := mkdir -p

CFLAGS := -Wall -pedantic -I$(SRC_DIR)

AFL_CC := afl-clang-fast

default: ptttl_cli

make_build_dirs:
	$(MKDIR) $(BUILD_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(TEST_OBJ_DIR)

ptttl_cli: make_build_dirs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_cli.c -o $(OBJ_DIR)/ptttl_cli.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/ptttl_cli.o -o $(CLI_BIN)

debug: CFLAGS += -O0 -g -fanalyzer -fsanitize=address -fsanitize=undefined
debug: ptttl_cli

afl_fuzz_harness: CC=$(AFL_CC)
afl_fuzz_harness: CFLAGS += -O2
afl_fuzz_harness: make_build_dir
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c $(FUZZ_DIR)/afl_fuzz_harness.c -o $(OBJ_DIR)/afl_fuzz_harness.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/afl_fuzz_harness.o -o $(FUZZ_BIN)

test: CFLAGS += -O0 -g3 -fsanitize=address -fsanitize=undefined
test: make_build_dirs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(TEST_OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(TEST_OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(TEST_DIR)/test_ptttl.c -o $(TEST_OBJ_DIR)/test_ptttl.o
	$(CC) $(CFLAGS) $(TEST_OBJ_DIR)/ptttl_parser.o $(TEST_OBJ_DIR)/ptttl_sample_generator.o $(TEST_OBJ_DIR)/test_ptttl.o -o $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(RM) $(OBJ_DIR)/ptttl_parser.o
	$(RM) $(OBJ_DIR)/ptttl_sample_generator.o
	$(RM) $(OBJ_DIR)/ptttl_to_wav.o
	$(RM) $(OBJ_DIR)/ptttl_cli.o
	$(RM) $(OBJ_DIR)/afl_fuzz_harness.o
	$(RM) $(CLI_BIN) $(FUZZ_BIN)
