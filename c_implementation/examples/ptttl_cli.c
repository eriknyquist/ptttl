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

static char *_input_buf = NULL;
static uint32_t _input_pos = 0u;
static uint32_t _buflen = 0u;

static struct option _long_options[] =
{
    {"wave-type", required_argument, NULL, 'w'},
    {"output-filename", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
};

// File pointer for RTTTL/PTTTL source file
static FILE *rfp = NULL;

static void _print_usage(void)
{
    printf("\n");
    printf("USAGE:\n\n");
    printf("ptttl_cli [OPTIONS] [input_filename]\n");
    printf("\nIf no input file is given, input will be read from stdin.\n");
    printf("If no output file is given, output will be written to stdout.\n\n");
    printf("\nOPTIONS:\n\n");
    printf("-w --wave-type [sine|triangle|square|sawtooth]  Waveform type (default: sine)\n");
    printf("-o --output-filename [string]                   Output filename (default: print to stdout)\n");
    printf("-h --help                                       Show this output and exit\n");
    printf("\n");
}

static int _parse_args(int argc, char *argv[])
{
    int ch;

    while ((ch = getopt_long(argc, argv, "w:o:h", _long_options, NULL)) != -1)
    {
        switch (ch)
        {
            case 'h':
                return 1;
                break;

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

    if (argc > optind)
    {
        _input_filename = argv[optind];
    }

    return 0;
}

// ptttl_readchar_t callback to read the next PTTTL/RTTTL source character from file loaded in memory
static int _read_mem(char *nextchar)
{
    if (_input_pos >= _buflen)
    {
        // No more input- return EOF
        return 1;
    }

    // Provide next character from buffer
    *nextchar = _input_buf[_input_pos];
    _input_pos++;

    // Return 0 for success, 1 for EOF (no error condition)
    return 0;
}

// ptttl_input_iface_t callback to seek to a specific position in file loaded in memory
static int _seek_mem(uint32_t position)
{
    if (_buflen <= position)
    {
        return 1;
    }

    _input_pos = position;
    return 0;
}

// ptttl_input_iface_t callback to read the next PTTTL/RTTTL source character from the open file
static int _read_file(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, rfp);

    // Return 0 for success, 1 for EOF (no error condition)
    return (int) (1u != ret);
}

// ptttl_input_iface_t callback to seek to a specific position in the open file
static int _seek_file(uint32_t position)
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
    int args_ret = _parse_args(argc, argv);
    if (args_ret != 0)
    {
        _print_usage();
        return args_ret < 0 ? -1 : 0;
    }

    ptttl_parser_input_iface_t iface;

    if (_input_filename == NULL)
    {
        // If reading from stdin, need to read input file contents into malloc'd buffer
        size_t total_buflen = 1024;
        const size_t chunk_size = 512;
        _input_buf = malloc(total_buflen);
        if (NULL == _input_buf)
        {
            printf("Failed to allocate memory\n");
            return -1;
        }

        size_t size_read = 0u;
        size_t bufpos = 0u;

        while ((size_read = fread(_input_buf + bufpos, 1, chunk_size, stdin)) == chunk_size)
        {
            bufpos += size_read;
            if ((bufpos + chunk_size) > total_buflen)
            {
                total_buflen *= 2u;
                _input_buf = realloc(_input_buf, total_buflen);
                if (NULL == _input_buf)
                {
                    printf("Failed to allocate memory\n");
                    return -1;
                }
            }
        }

        _buflen = (uint32_t) (bufpos + size_read);
        iface.read = _read_mem;
        iface.seek = _seek_mem;
    }
    else
    {
        // If reading from regular file, we can just operate directly on the file pointer
        rfp = fopen(_input_filename, "rb");
        if (NULL == rfp)
        {
            printf("Unable to open input file for reading: %s\n", _input_filename);
            return -1;
        }

        iface.read = _read_file;
        iface.seek = _seek_file;
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
            printf("Unable to open output file for writing: %s\n", _output_filename);
            fclose(rfp);
            return -1;
        }
    }

    // Create and initialize PTTTL parser object, sample generator config object
    ptttl_parser_t parser;
    ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;

    int ret = ptttl_parse_init(&parser, iface);
    if (0 > ret)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error (line %d, column %d): %s\n", err.line, err.column, err.error_message);
    }

    if (0 == ret)
    {
        // Parse PTTTL/RTTTL source and convert to .wav file
        ret = ptttl_to_wav(&parser, wfp, &config, _wave_type);
        if (ret < 0)
        {
            ptttl_parser_error_t err = ptttl_parser_error(&parser);
            printf("Error (line %d, column %d): %s\n", err.line, err.column, err.error_message);
        }
    }

    if (NULL != _input_filename)
    {
        fclose(rfp);
    }

    if (NULL != _output_filename)
    {
        fclose(wfp);
    }

    if (NULL != _input_buf)
    {
        free(_input_buf);
    }

    return ret;
}
