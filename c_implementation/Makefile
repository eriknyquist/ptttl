# Makefile for building ptttl_cli sample program

BUILD_DIR      := build
SRC_DIR        := src
TEST_DIR       := test
FUZZ_DIR       := fuzz_testing
EXAMPLES_DIR   := examples
OBJ_DIR        := $(BUILD_DIR)/obj
TEST_OBJ_DIR   := $(TEST_DIR)/obj

SAMPLES_FILE_BIN  := $(BUILD_DIR)/gen_samples_from_file
SAMPLES_MEM_BIN   := $(BUILD_DIR)/gen_samples_from_mem
GEN_CONFIG_BIN    := $(BUILD_DIR)/generator_config
SET_WAVE_BIN      := $(BUILD_DIR)/set_waveform
CUSTOM_WAVE_BIN   := $(BUILD_DIR)/set_custom_waveform
CLI_BIN           := $(BUILD_DIR)/ptttl_cli
FUZZ_BIN          := $(BUILD_DIR)/afl_fuzz_harness
TEST_BIN          := $(TEST_DIR)/test_ptttl

RM             := rm -rf
MKDIR          := mkdir -p

CFLAGS := -Wall -pedantic -I$(SRC_DIR)

AFL_CC := afl-clang-fast

default: examples

make_build_dirs:
	$(MKDIR) $(BUILD_DIR)
	$(MKDIR) $(OBJ_DIR)
	$(MKDIR) $(TEST_OBJ_DIR)

examples: make_build_dirs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/ptttl_cli.c -o $(OBJ_DIR)/ptttl_cli.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/gen_samples_from_file.c -o $(OBJ_DIR)/gen_samples_from_file.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/gen_samples_from_file.c -o $(OBJ_DIR)/gen_samples_from_mem.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/generator_config.c -o $(OBJ_DIR)/generator_config.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/set_waveform.c -o $(OBJ_DIR)/set_waveform.o
	$(CC) $(CFLAGS) -c $(EXAMPLES_DIR)/set_custom_waveform.c -o $(OBJ_DIR)/set_custom_waveform.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/ptttl_cli.o -o $(CLI_BIN)
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/gen_samples_from_file.o -o $(SAMPLES_FILE_BIN)
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/gen_samples_from_mem.o -o $(SAMPLES_MEM_BIN)
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/generator_config.o -o $(GEN_CONFIG_BIN)
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/set_waveform.o -o $(SET_WAVE_BIN)
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/set_custom_waveform.o -o $(CUSTOM_WAVE_BIN)

debug: CFLAGS += -O0 -g -fsanitize=address,undefined
debug: examples

afl_fuzz_harness: CC=$(AFL_CC)
afl_fuzz_harness: CFLAGS += -O2
afl_fuzz_harness: make_build_dirs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_to_wav.c -o $(OBJ_DIR)/ptttl_to_wav.o
	$(CC) $(CFLAGS) -c $(FUZZ_DIR)/afl_fuzz_harness.c -o $(OBJ_DIR)/afl_fuzz_harness.o
	$(CC) $(CFLAGS) $(OBJ_DIR)/ptttl_parser.o $(OBJ_DIR)/ptttl_sample_generator.o $(OBJ_DIR)/ptttl_to_wav.o $(OBJ_DIR)/afl_fuzz_harness.o -o $(FUZZ_BIN)

fuzz: afl_fuzz_harness
fuzz:
	export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 && afl-fuzz -t 2000 -i $(FUZZ_DIR)/testcase_corpus/ -o $(FUZZ_DIR)/afl_findings -- $(FUZZ_BIN)

test: CFLAGS += -O0 -g3 -fsanitize=address,undefined
test: make_build_dirs
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_parser.c -o $(TEST_OBJ_DIR)/ptttl_parser.o
	$(CC) $(CFLAGS) -c $(SRC_DIR)/ptttl_sample_generator.c -o $(TEST_OBJ_DIR)/ptttl_sample_generator.o
	$(CC) $(CFLAGS) -c $(TEST_DIR)/test_ptttl.c -o $(TEST_OBJ_DIR)/test_ptttl.o
	$(CC) $(CFLAGS) $(TEST_OBJ_DIR)/ptttl_parser.o $(TEST_OBJ_DIR)/ptttl_sample_generator.o $(TEST_OBJ_DIR)/test_ptttl.o -o $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(RM) $(BUILD_DIR)
