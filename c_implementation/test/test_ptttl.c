#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ptttl_sample_generator.h"

static const char* _testcase_dirs[] =
{
    "test/testcases/polyphonic_success",
    "test/testcases/polyphonic_success_hightempo",
    "test/testcases/invalid_option_key_1",
    "test/testcases/invalid_option_key_2",
    "test/testcases/different_note_lengths",
    "test/testcases/invalid_note_length_line_start",
    "test/testcases/invalid_note_length_line_middle",
    "test/testcases/invalid_note_letter_1",
    "test/testcases/invalid_note_letter_2",
    "test/testcases/invalid_note_letter_3",
    "test/testcases/invalid_default_duration_1",
    "test/testcases/invalid_default_duration_2",
    "test/testcases/invalid_default_duration_3",
    "test/testcases/invalid_default_duration_4",
    "test/testcases/invalid_pause_duration_1",
    "test/testcases/invalid_pause_duration_2",
    "test/testcases/invalid_pause_duration_3",
    "test/testcases/invalid_default_octave_1",
    "test/testcases/invalid_default_octave_2",
    "test/testcases/invalid_default_octave_3",
    "test/testcases/invalid_default_octave_4",
    "test/testcases/invalid_option_format",
    "test/testcases/invalid_octave0_note",
    "test/testcases/invalid_default_vibrato_freq",
    "test/testcases/invalid_default_vibrato_var",
    "test/testcases/invalid_note_vibrato_freq",
    "test/testcases/invalid_note_vibrato_var",
    "test/testcases/invalid_note_vibrato_freq_format",
    "test/testcases/invalid_note_vibrato_var_format",
    "test/testcases/invalid_note_octave_1",
    "test/testcases/invalid_note_octave_2",
    "test/testcases/invalid_note_octave_3",
    "test/testcases/extra_option_comma",
    "test/testcases/extra_option_comma_end",
    "test/testcases/extra_note_comma",
    "test/testcases/extra_note_comma_end",
    "test/testcases/missing_option_comma",
    "test/testcases/missing_note_comma",
    "test/testcases/default_variance_respected",
    "test/testcases/default_frequency_respected",
    "test/testcases/all_piano_keys",
    "test/testcases/double_flat_note",
    "test/testcases/double_pipe",
    "test/testcases/double_semicolon",
    "test/testcases/empty_1",
    "test/testcases/empty_2",
    "test/testcases/option_defaults_explicit",
    "test/testcases/option_defaults_implicit",
    "test/testcases/mismatched_blocks_1",
    "test/testcases/mismatched_blocks_2",
    "test/testcases/malformed_pause_1",
    "test/testcases/malformed_pause_2",
    "test/testcases/bpm_too_large",
    "test/testcases/name_too_long",
    "test/testcases/multiline_name_1",
    "test/testcases/multiline_name_2",
    "test/testcases/afl_testcase_1",
    "test/testcases/bpm_zero",
};

#define NUM_TESTCASES (sizeof(_testcase_dirs) / sizeof(_testcase_dirs[0]))

#define SAMPLE_CHUNK_SIZE (1024)

#define SAMPLE_BUF_SIZE (2500000)
static int16_t _input_sample_buf[SAMPLE_BUF_SIZE];
static int16_t _output_sample_buf[SAMPLE_BUF_SIZE];

// Number of times the "read" iface callback was called (reset per testcase)
static int _char_read_count = 0;

// Furthest input file position the parser advanced to (reset per testcase)
static int _high_watermark = 0;

// Current position in input file (reset per testcase)
static int _input_pos = 0;


// File pointer for RTTTL/PTTTL source file
static FILE *_src_fp = NULL;

// Pointer to file content loaded in memory
static uint8_t *_testcase_buf = NULL;

// Size of file content loaded in memory
static int _buflen = 0;

// Chunk buffer for reading test case info from files
static char _fbuf[8192];
static size_t _fbufsize = 0;
static size_t _fbufpos = 0;

// Forward declaration of iface callbacks
static int _read_mem(char *nextchar);
static int _seek_mem(uint32_t position);
static int _read_file(char *nextchar);
static int _seek_file(uint32_t position);


static ptttl_parser_input_iface_t _ifaces[] =
{
    {.read=_read_mem, .seek=_seek_mem},
    {.read=_read_file, .seek=_seek_file}
};

#define NUM_IFACES (sizeof(_ifaces) / sizeof(_ifaces[0]))


// ptttl_readchar_t callback to read the next PTTTL/RTTTL source character from file loaded in memory
static int _read_mem(char *nextchar)
{
    if (_input_pos >= _buflen)
    {
        // No more input for this testcase- return EOF
        return 1;
    }

    // Provide next character from stdin buf
    *nextchar = (char) _testcase_buf[_input_pos];
    _input_pos++;
    _char_read_count++;

    if (_input_pos > _high_watermark)
    {
        _high_watermark = _input_pos;
    }

    // Return 0 for success, 1 for EOF (no error condition)
    return 0;
}

// ptttl_input_iface_t callback to seek to a specific position in file loaded in memory
static int _seek_mem(uint32_t position)
{
    if (_buflen <= (int) position)
    {
        return 1;
    }

    _input_pos = (int) position;
    return 0;
}

// ptttl_input_iface_t callback to read the next PTTTL/RTTTL source character from file on disk
static int _read_file(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, _src_fp);

    if (ret == 1)
    {
        _input_pos++;
        _char_read_count++;
        if (_input_pos > _high_watermark)
        {
            _high_watermark = _input_pos;
        }
    }

    // Return 0 for success, 1 for EOF (no error condition)
    return (int) (1u != ret);
}

// ptttl_input_iface_t callback to seek to a specific position in file on disk
static int _seek_file(uint32_t position)
{
    int ret = fseek(_src_fp, (long) position, SEEK_SET);
    if (feof(_src_fp))
    {
        return 1;
    }

    _input_pos = (int) position;
    return ret;
}

// Used for reading testcase info from files in testcase directory
static size_t _buffered_readchar(FILE *fp, char *c)
{
    if (_fbufsize == _fbufpos)
    {
        _fbufsize = fread(_fbuf, 1, sizeof(_fbuf), fp);
        if (_fbufsize == 0)
        {
            return 0;
        }

        _fbufpos = 0;
    }

    *c = _fbuf[_fbufpos++];
    return 1u;
}

static bool _get_next_noncomment_char(FILE *fp, char *c)
{
    size_t chars_read;
    bool in_comment = false;

    do
    {
        char readchar;
        chars_read = _buffered_readchar(fp, &readchar);
        if (chars_read == 1)
        {
            if (readchar == '#')
            {
                in_comment = true;
            }
            else if ((readchar == '\n') && in_comment)
            {
                in_comment = false;
            }
            else
            {
                if (!in_comment)
                {
                    *c = readchar;
                    return false;
                }
            }
        }
    } while (chars_read == 1);

    // Reached EOF
    return true;
}

static size_t _read_next_line_from_file(FILE *fp, char *output, size_t maxlen)
{
    size_t len = 0u;
    bool line_end_found = false;

    while (len < (maxlen - 1))
    {
        char c;
        bool eof_reached = _get_next_noncomment_char(fp, &c);
        if (eof_reached)
        {
            // If EOF reached, we're done reading
            line_end_found = true;
            break;
        }
        if ('\n' == c)
        {
            if (len > 0)
            {
                // Found a line with some non-whitespace content, break out
                line_end_found = true;
                break;
            }
            else
            {
                // Found a blank line, skip to next character
                continue;
            }
        }

        output[len++] = c;
    }

    output[len] = '\0';
    if (!line_end_found)
    {
        len += 1u;
    }

    return len;
}

static int _load_ints_from_file(const char *filename, FILE *fp, int count, bool ashex, int16_t *output)
{
    int lines_found = 0;

    while (lines_found < count)
    {
        char linebuf[32];
        size_t linelen = _read_next_line_from_file(fp, linebuf, sizeof(linebuf));
        if (linelen == 0u)
        {
            // EOF reached
            break;
        }
        else if (linelen == sizeof(linebuf))
        {
            printf("Error: line longer than %zu found in file %s\n", sizeof(linebuf), filename);
            return -1;
        }

        char *endptr = NULL;
        int base = (ashex) ? 16 : 10;
        long result = strtol(linebuf, &endptr, base);
        if (*endptr != '\0')
        {
            printf("Error: can't convert %s to integer in file %s\n", linebuf, filename);
            return -1;
        }

        *(output++) = (int16_t) result;
        lines_found += 1u;
    }

    return lines_found;
}

static int verify_expected_samples(const char *input_filename, uint32_t num_samples_generated)
{
    FILE *fp = NULL;
    if (NULL == (fp = fopen(input_filename, "r")))
    {
        printf("Error: file %s does not exist\n", input_filename);
        return 1;
    }

    uint32_t input_sample_buf_pos = 0u;
    int ret = 0;

    while (1)
    {
        if (input_sample_buf_pos >= SAMPLE_BUF_SIZE)
        {
            printf("Exceeded input sample buffer (%d bytes)\n", SAMPLE_BUF_SIZE);
            fclose(fp);
            return -1;
        }

        ret =_load_ints_from_file(input_filename, fp, SAMPLE_CHUNK_SIZE, false,
                                  _input_sample_buf + input_sample_buf_pos);
        if (ret < 0)
        {
            break;
        }

        input_sample_buf_pos += ret;
        if (ret < SAMPLE_CHUNK_SIZE)
        {
            break;
        }
    }

    fclose(fp);

    if (ret >= 0)
    {
        if (num_samples_generated != input_sample_buf_pos)
        {
            printf("Error: generated %u samples, but %s contains %u samples\n",
                   num_samples_generated, input_filename, input_sample_buf_pos);
            return -1;
        }

        for (uint32_t i = 0; i < num_samples_generated; i++)
        {
            if (_input_sample_buf[i] != _output_sample_buf[i])
            {
                printf("Error : expected value %d for sample #%d (from %s), but generated value was %d\n",
                       _input_sample_buf[i], i + 1, input_filename, _output_sample_buf[i]);
                return -1;
            }
        }
    }

    return 0;
}

static int verify_error(ptttl_parser_error_t err, const char *error_path)
{
    FILE *fp = fopen(error_path, "r");
    if (NULL == fp)
    {
        printf("Error: encountered the following error, but unable to access %s:\n", error_path);
        printf("  Message: %s\n", err.error_message);
        printf("  Line   : %d\n", err.line);
        printf("  Column : %d\n", err.column);
        return -1;
    }

    char expected_err_msg[128];
    size_t linelen = _read_next_line_from_file(fp, expected_err_msg, sizeof(expected_err_msg));
    if (0u == linelen)
    {
        printf("Error: EOF reached in %s before expected error message was found\n", error_path);
        fclose(fp);
        return -1;
    }
    else if (sizeof(expected_err_msg) == linelen)
    {
        printf("Error: expected error messages in %s was longer than %zu\n", error_path, linelen);
        fclose(fp);
        return -1;
    }

    int16_t count_values[2u] = {0u};
    int ints_found = _load_ints_from_file(error_path, fp, 2u, false, count_values);
    if (ints_found != 2)
    {
        printf("Error: expected 2 integers in %s, but only found %d\n", error_path, ints_found);
        fclose(fp);
        return -1;
    }

    int expected_line_num = count_values[0];
    int expected_col_num = count_values[1];

    if ((memcmp(expected_err_msg, err.error_message, linelen) != 0) ||
        (expected_line_num != err.line) ||
        (expected_col_num != err.column))
    {
        printf("Error: expected the following error information\n");
        printf("  Message : %s\n", expected_err_msg);
        printf("     Line : %d\n", expected_line_num);
        printf("   Column : %d\n", expected_col_num);
        printf("But saw the following error information instead\n");
        printf("  Message : %s\n", err.error_message);
        printf("     Line : %d\n", err.line);
        printf("   Column : %d\n", err.column);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int _run_testcase(const char *testcase_dir, ptttl_parser_input_iface_t *iface)
{
    // Build paths for testcase input/output files
    char source_path[128];
    char error_path[128];
    char expected_samples_path[128];

    _fbufpos = 0;
    _fbufsize = 0;

    (void) snprintf(source_path, sizeof(source_path), "%s/source.txt", testcase_dir);
    (void) snprintf(error_path, sizeof(error_path), "%s/expected_error.txt", testcase_dir);
    (void) snprintf(expected_samples_path, sizeof(expected_samples_path), "%s/expected_samples.txt", testcase_dir);

    _src_fp = NULL;
    _src_fp = fopen(source_path, "r");
    if (NULL == _src_fp)
    {
        printf("Unable to open file %s\n", source_path);
        return -1;
    }

    // Create and initialize PTTTL parser object
    ptttl_parser_t parser;
    uint32_t output_sample_buf_pos = 0u;
    bool samples_mismatch = false;

    int ret = ptttl_parse_init(&parser, *iface);
    if (ret == 0)
    {
        ptttl_sample_generator_t generator;
        ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;

        ret = ptttl_sample_generator_create(&parser, &generator, &config);
        if (ret == 0)
        {
            uint32_t num_samples = SAMPLE_CHUNK_SIZE;
            while ((ret = ptttl_sample_generator_generate(&generator,
                                                          &num_samples,
                                                          _output_sample_buf + output_sample_buf_pos)) != -1)
            {
                output_sample_buf_pos += num_samples;
                if (ret == 1)
                {
                    ret = 0;
                    break;
                }
            }

            if (ret == 0)
            {
                samples_mismatch = (verify_expected_samples(expected_samples_path, output_sample_buf_pos) != 0);
            }
        }
    }

    fclose(_src_fp);

    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        return verify_error(err, error_path);
    }

    FILE *fp = fopen(error_path, "r");
    if (NULL != fp)
    {
        printf("Encountered no error, but an error was expected as per %s\n", error_path);
        fclose(fp);
        return -1;
    }

    if (samples_mismatch)
    {
        return -1;
    }

    return 0;
}

// Loads source file in the given testcase directory
static int _load_source_file(const char *testcase_dir)
{
    char source_path[128];
    (void) snprintf(source_path, sizeof(source_path), "%s/source.txt", testcase_dir);
    FILE *fp = fopen(source_path, "r");
    if (fp == NULL)
    {
        printf("Unable to open file %s\n", source_path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        printf("Unable to seek in file %s\n", source_path);
        fclose(fp);
        return -1;
    }

    _buflen = (int) ftell(fp);
    if (_buflen < 0)
    {
        printf("Failed to get size of file %s\n", source_path);
        fclose(fp);
        return -1;
    }

    _testcase_buf = malloc(_buflen);
    if (_testcase_buf == NULL)
    {
        printf("Unable to allocate memory for file %s\n", source_path);
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        printf("Unable to seek in file %s\n", source_path);
        fclose(fp);
        return -1;
    }

    size_t chars_read = fread(_testcase_buf, 1, _buflen, fp);
    if (chars_read != (size_t) _buflen)
    {
        printf("Expecting %d bytes, but only %zu bytes read from %s\n",
               _buflen, chars_read, source_path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

// Frees the memory for source file loaded in memory
static void _unload_source_file(void)
{
    if (_testcase_buf != NULL)
    {
        free(_testcase_buf);
        _testcase_buf = NULL;
    }
}

int main(void)
{
    int failures = 0;
    int tests = 0;

    for (int i = 0; i < NUM_TESTCASES; i++)
    {
        const char *testcase_dir = _testcase_dirs[i];

        if (_load_source_file(_testcase_dirs[i]) != 0)
        {
            printf("Unable to load source file in %s\n", _testcase_dirs[i]);
            return -1;
        }

        // Test name is the "basename" of the testcase directory path-- find last slash
        const char *testcase_name = testcase_dir;
        const char *last_slash = testcase_dir;
        while (*testcase_name != '\0')
        {
            if (*testcase_name == '/')
            {
                last_slash = testcase_name;
            }

            testcase_name++;
        }

        if (last_slash == testcase_dir)
        {
            printf("Error: testcase dir '%s' contains no forward slashes\n", testcase_dir);
            return -1;
        }

        testcase_name = last_slash + 1;
        if (*testcase_name == '\0')
        {
            printf("Error: testcase dir '%s' ends with a forward slash\n", testcase_dir);
            return -1;
        }

        for (int iface = 0; iface < NUM_IFACES; iface++)
        {
            // Run the test case
            int result = _run_testcase(testcase_dir, &_ifaces[iface]);
            tests += 1;

            float _overread_percentage = ((float) _char_read_count) / (((float) _high_watermark) / 100.0f);
            char passfail_str[128];
            (void) snprintf(passfail_str, sizeof(passfail_str), "Test %s (%s) %s",
                            testcase_name,
                            (_ifaces[iface].read == _read_mem) ? "mem" : "file",
                            (result == 0) ? "PASSED" : "FAILED");

            printf("%-55s %d/%d : %.2f%%\n", passfail_str, _high_watermark, _char_read_count, _overread_percentage);

            if (result != 0)
            {
                failures += 1;
            }

            _unload_source_file();
            _char_read_count = 0;
            _high_watermark = 0;
            _input_pos = 0;
            _buflen = 0;
        }
    }

    printf("\nRan %d tests, ", tests);
    if (failures == 0)
    {
        printf("All OK\n\n");
    }
    else
    {
        printf("%d failures\n\n", failures);
    }

    return failures;
}
