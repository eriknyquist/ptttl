/* afl_fuzz_harness.c
 *
 * Implements a harness for using AFL++ to fuzz the ptttl_parse() function, and to
 * check that ptttl_sample_generator_count_samples() agrees with the sample generator

 * Requires ptttl_parser.c
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"


__AFL_FUZZ_INIT()


/* Inputs that would generate more samples than this are only counted, not generated,
 * to keep fuzzing fast */
#define MAX_GENERATED_SAMPLES (1000000u)

// Crash (so AFL++ reports it) if the sample counter disagrees with the sample generator
#define FUZZ_CHECK(cond) { if (!(cond)) abort(); }


static int buflen = 0;
static int bufpos = 0;

static unsigned char *testcase_buf = NULL;


// ptttl_readchar_t callback to read the next PTTTL/RTTTL source character from stdin
static int _read(char *nextchar)
{
    if (bufpos >= buflen)
    {
        return 1;
    }

    // Provide next character from stdin buf
    *nextchar = (char) testcase_buf[bufpos];
    bufpos++;

    // Return 0 for success, 1 for EOF (no error condition)
    return 0;
}

static int _seek(uint32_t position)
{
    if ((int) position >= buflen)
    {
        return 1;
    }

    bufpos = (int) position;
    return 0;
}

int main(int argc, char *argv[])
{
    __AFL_INIT();

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    const uint32_t sample_buf_len = 8192;
    int16_t sample_buf[sample_buf_len];

    while (__AFL_LOOP(10000))
    {
        uint32_t num_samples = sample_buf_len;
        buflen = __AFL_FUZZ_TESTCASE_LEN;

        /* Reset bufpos to 0-- if ptttl_parse bailed out early on an error,
         * then ptttl_readchar may not have done this */
        bufpos = 0;

        // Empty input will break _ptttl_readchar, and is not useful to test anyway
        if (buflen == 0)
        {
            continue;
        }

        testcase_buf = realloc(testcase_buf, buflen);
        memcpy(testcase_buf, buf, buflen);

        // Parse PTTTL/RTTTL source and produce intermediate representation
        ptttl_parser_t parser;
        ptttl_parser_input_iface_t iface = {.read=_read, .seek=_seek};

        if (ptttl_parse_init(&parser, iface) < 0)
        {
            continue;
        }

        // Count samples, then re-initialize the parser and generate them
        ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;
        uint32_t sample_count = 0u;
        int count_ret = ptttl_sample_generator_count_samples(&parser, &config, MAX_GENERATED_SAMPLES,
                                                             &sample_count);
        ptttl_parser_error_t count_err = ptttl_parser_error(&parser);
        if (count_ret == 1)
        {
            continue;
        }

        FUZZ_CHECK(ptttl_parse_init(&parser, iface) == 0);

        ptttl_sample_generator_t generator;
        uint32_t samples_generated = 0u;
        int ret = ptttl_sample_generator_create(&parser, &generator, &config);
        while (ret == 0)
        {
            num_samples = sample_buf_len;
            ret = ptttl_sample_generator_generate(&generator, &num_samples, sample_buf);
            samples_generated += num_samples;
        }

        if (ret < 0)
        {
            // Counting must fail with exactly the same error as sample generation
            ptttl_parser_error_t gen_err = ptttl_parser_error(&parser);
            FUZZ_CHECK(count_ret == -1);
            FUZZ_CHECK(count_err.error_message != NULL);
            FUZZ_CHECK(strcmp(count_err.error_message, gen_err.error_message) == 0);
            FUZZ_CHECK(count_err.line == gen_err.line);
            FUZZ_CHECK(count_err.column == gen_err.column);
        }
        else
        {
            FUZZ_CHECK(count_ret == 0);
            FUZZ_CHECK(sample_count == samples_generated);
        }
    }

    return 0;
}
