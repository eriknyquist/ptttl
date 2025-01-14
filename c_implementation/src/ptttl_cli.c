/* ptttl_cli.c
 *
 * Sample main.c which implements a command-line tool for converting PTTTL/RTTTL
 * source into .wav file, illustrating how to use ptttl_parser.c and ptttl_to_wav.c.
 *
 * Requires ptttl_parser.c, ptttl_sample_generator.c, and ptttl_to_wav.c
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"

// File pointer for RTTTL/PTTTL source file
static FILE *fp = NULL;

// ptttl_input_iface_t callback to read the next PTTTL/RTTTL source character from the open file
static int _read(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, fp);

    // Return 0 for success, 1 for EOF (no error condition)
    return (int) (1u != ret);
}

// ptttl_input_iface_t callback to seek to a specific position in the open file
static int _seek(uint32_t position)
{
    int ret = fseek(fp, (long) position, SEEK_SET);
    if (feof(fp))
    {
        return 1;
    }

    return ret;
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

    // Create and initialize PTTTL parser object
    ptttl_parser_t parser;
    ptttl_parser_input_iface_t iface = {.read=_read, .seek=_seek};

    int ret = ptttl_parse_init(&parser, iface);
    if (0 > ret)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
    }

    if (0 == ret)
    {
        // Parse PTTTL/RTTTL source and convert to .wav file
        ret = ptttl_to_wav(&parser, argv[2]);
        if (ret < 0)
        {
            ptttl_parser_error_t err = ptttl_to_wav_error();
            printf("Error Generating WAV file (%s, line %d, column %d): %s\n", argv[1], err.line,
                   err.column, err.error_message);
        }
    }

    fclose(fp);

    return ret;
}
