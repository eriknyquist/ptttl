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
#include "ptttl_parser.h"


__AFL_FUZZ_INIT()
__AFL_COVERAGE()
__AFL_COVERAGE_START_OFF()


static int buflen = 0;
static int bufpos = 0;

static unsigned char *testcase_buf = NULL;


// ptttl_readchar_t callback to read the next PTTTL/RTTTL source character from stdin
static int _ptttl_readchar(char *nextchar)
{
    int ret = 0;

    // Provide next character from stdin buf
    *nextchar = (char) testcase_buf[bufpos];

    if (bufpos == (buflen - 1))
    {
        // Last char of input for this testcase- reset buffer position and return EOF
        bufpos = 0;
        ret = 1;
    }
    else
    {
        bufpos++;
    }

    // Return 0 for success, 1 for EOF (no error condition)
    return ret;
}

int main(int argc, char *argv[])
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(10000))
    {
        buflen = __AFL_FUZZ_TESTCASE_LEN;
        testcase_buf = buf;
        bufpos = 0;

        if (buflen < 5)
        {
            continue;
        }

        // Parse PTTTL/RTTTL source and produce intermediate representation
        ptttl_output_t output = {0};

        __AFL_COVERAGE_ON();
        int ret = ptttl_parse(_ptttl_readchar, &output);
        __AFL_COVERAGE_OFF();

        if (ret != 0)
        {
            ptttl_parser_error_t err = ptttl_parser_error();
            printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        }
    }

    return 0;
}
