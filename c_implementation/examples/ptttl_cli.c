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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"

static char *_input_filename = NULL;
static ptttl_waveform_type_e _wave_type = WAVEFORM_TYPE_SINE;
static char *_output_filename = NULL;

static struct option _long_options[] =
{
    {"wave-type", required_argument, NULL, 'w'},
    {"output-filename", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
};

// File pointer for RTTTL/PTTTL source file
static FILE *rfp = NULL;

static void _print_usage(void)
{
    printf("\n");
    printf("USAGE:\n\n");
    printf("ptttl_cli [OPTIONS] input_filename\n");
    printf("\nOPTIONS:\n\n");
    printf("-w --wave-type [sine|triangle|square|sawtooth]  Waveform type (default: sine)\n");
    printf("-o --output-filename [string]                   Output filename (default: print to stdout)\n");
    printf("\n");
}

static int _parse_args(int argc, char *argv[])
{
    int ch;

    while ((ch = getopt_long(argc, argv, "w:o:", _long_options, NULL)) != -1)
    {
        switch (ch)
        {
            case 'w':
            {
                if (strncmp(optarg, "sine", 4) == 0)
                {
                    _wave_type = WAVEFORM_TYPE_SINE;
                }
                else if (strncmp(optarg, "triangle", 8) == 0)
                {
                    _wave_type = WAVEFORM_TYPE_TRIANGLE;
                }
                else if (strncmp(optarg, "sawtooth", 8) == 0)
                {
                    _wave_type = WAVEFORM_TYPE_SAWTOOTH;
                }
                else if (strncmp(optarg, "square", 6) == 0)
                {
                    _wave_type = WAVEFORM_TYPE_SQUARE;
                }
                else
                {
                    printf("Error: unrecognized waveform type '%s'\n", optarg);
                    return -1;
                }
                break;
            }
            case 'o':
            {
                _output_filename = optarg;
                break;
            }
            default:
                return -1;
        }
    }

    if (argc < (optind + 1))
    {
        printf("Missing positional arguments\n");
        return -1;
    }

    _input_filename = argv[optind];

    return 0;
}

// ptttl_input_iface_t callback to read the next PTTTL/RTTTL source character from the open file
static int _read(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, rfp);

    // Return 0 for success, 1 for EOF (no error condition)
    return (int) (1u != ret);
}

// ptttl_input_iface_t callback to seek to a specific position in the open file
static int _seek(uint32_t position)
{
    int ret = fseek(rfp, (long) position, SEEK_SET);
    if (feof(rfp))
    {
        return 1;
    }

    return ret;
}


int main(int argc, char *argv[])
{
    if (1 == argc)
    {
        _print_usage();
        return -1;
    }

    if (_parse_args(argc, argv) != 0)
    {
        _print_usage();
        return -1;
    }

    rfp = fopen(_input_filename, "rb");
    if (NULL == rfp)
    {
        printf("Unable to open input file for reading: %s\n", argv[1]);
        return -1;
    }

    FILE *wfp;

    if (_output_filename == NULL)
    {
        wfp = stdout;
    }
    else
    {
        wfp = fopen(_output_filename, "wb");
        if (NULL == wfp)
        {
            printf("Unable to open output file for writing: %s\n", argv[2]);
            fclose(rfp);
            return -1;
        }
    }

    // Create and initialize PTTTL parser object, sample generator config object
    ptttl_parser_t parser;
    ptttl_parser_input_iface_t iface = {.read=_read, .seek=_seek};
    ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;

    int ret = ptttl_parse_init(&parser, iface);
    if (0 > ret)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
    }

    if (0 == ret)
    {
        // Parse PTTTL/RTTTL source and convert to .wav file
        ret = ptttl_to_wav(&parser, wfp, &config, _wave_type);
        if (ret < 0)
        {
            ptttl_parser_error_t err = ptttl_parser_error(&parser);
            printf("Error Generating WAV file (%s, line %d, column %d): %s\n", argv[1], err.line,
                   err.column, err.error_message);
        }
    }

    fclose(rfp);
    fclose(wfp);

    return ret;
}
