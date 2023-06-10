/* ptttl_cli.c
 *
 * Sample main.c which implements a command-line tool for converting PTTTL/RTTTL
 * source into .wav file, illustrating how to use ptttl_parser.c and ptttl_to_wav.c.
 *
 * Requires ptttl_parser.c, ptttl_sample_generator.c, and ptttl_to_wav.c
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"

// File pointer for RTTTL/PTTTL source file
static FILE *fp = NULL;

// ptttl_readchar_t callback to read the next PTTTL/RTTTL source character from a file
static int _ptttl_readchar(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, fp);

    // Return 0 for success, 1 for EOF, and -1 for error
    return (1u == ret) ? 0 : 1;
}

int main(int argc, char *argv[])
{
    if (3 != argc)
    {
        printf("Usage: %s <PTTTL/RTTTL filename> <output filename>\n", argv[0]);
        return -1;
    }

    fp = fopen(argv[1], "rb");
    if (NULL == fp)
    {
        printf("Unable to open file %s\n", argv[1]);
        return -1;
    }

    // Parse PTTTL/RTTTL source and produce intermediate representation
    ptttl_output_t output;
    int ret = ptttl_parse(_ptttl_readchar, &output);
    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error();
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        return -1;
    }

    fclose(fp);

    // Convert intermediate representation to .wav file
    ret = ptttl_to_wav(&output, argv[2]);
    if (ret < 0)
    {
        printf("Error generating WAV file: %s\n", ptttl_to_wav_error());
        return ret;
    }

    return 0;
}
