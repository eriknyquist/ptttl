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
#include <string.h>
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
    if ((3 != argc) && (4 != argc))
    {
        printf("Usage: %s <PTTTL/RTTTL filename> <output filename> [<waveform_type>]\n\n", argv[0]);
        printf("<waveform_type> is optional, and can be any of the following:\n");
        printf("    sine\n");
        printf("    triangle\n");
        printf("    sawtooth\n");
        printf("    square\n\n");

        return -1;
    }

    ptttl_waveform_type_e wave_type = WAVEFORM_TYPE_SINE;
    if (argc == 4)
    {
        if (strncmp(argv[3], "sine", 4) == 0)
        {
            wave_type = WAVEFORM_TYPE_SINE;
        }
        else if (strncmp(argv[3], "triangle", 8) == 0)
        {
            wave_type = WAVEFORM_TYPE_TRIANGLE;
        }
        else if (strncmp(argv[3], "sawtooth", 8) == 0)
        {
            wave_type = WAVEFORM_TYPE_SAWTOOTH;
        }
        else if (strncmp(argv[3], "square", 6) == 0)
        {
            wave_type = WAVEFORM_TYPE_SQUARE;
        }
        else
        {
            printf("Error: unrecognized waveform type '%s'\n", argv[3]);
            return -1;
        }

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
        ret = ptttl_to_wav(&parser, argv[2], wave_type);
        if (ret < 0)
        {
            ptttl_parser_error_t err = ptttl_parser_error(&parser);
            printf("Error Generating WAV file (%s, line %d, column %d): %s\n", argv[1], err.line,
                   err.column, err.error_message);
        }
    }

    fclose(fp);

    return ret;
}
