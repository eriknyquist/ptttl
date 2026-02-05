/* afl_fuzz_harness.c
 *
 * Implements a harness for using AFL++ to fuzz the ptttl_parse() function

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
    if (buflen > (int) position)
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

        ptttl_sample_generator_t generator;
        ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;
        if (ptttl_sample_generator_create(&parser, &generator, &config) < 0)
        {
            continue;
        }

        while (ptttl_sample_generator_generate(&generator, &num_samples, sample_buf) == 0);
    }

    return 0;
}
