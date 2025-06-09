/* gen_samples_from_mem.c
 *
 * Sample main.c showing how to read RTTTL/PTTTL source from memory, generate PCM
 * audio samples, and print the sample values to stdout.
 *
 * Requires ptttl_parser.c and ptttl_sample_generator.c
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "ptttl_parser.h"
#include "ptttl_sample_generator.h"


// Tracks position in input text
static uint32_t _input_pos = 0;

// Size of file content loaded in memory
static uint32_t _buflen = 0;

// Pointer to file content loaded in memory
static char *_file_buf = NULL;


// ptttl_input_iface_t callback to read the next PTTTL/RTTTL source character from file loaded in memory
static int _read(char *nextchar)
{
    if (_input_pos >= _buflen)
    {
        // No more input- return EOF
        return 1;
    }

    // Provide next character from stdin buf
    *nextchar = (char) _file_buf[_input_pos];
    _input_pos++;

    // Return 0 for success, 1 for EOF (no error condition)
    return 0;
}

// ptttl_input_iface_t callback to seek to a specific position in file loaded in memory
static int _seek(uint32_t position)
{
    if (_buflen <= position)
    {
        return 1;
    }

    _input_pos = position;
    return 0;
}

// Get the size of an open file
static int _get_file_size(FILE *fp)
{
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        return -1;
    }

    long size = ftell(fp);
    if ((size < 0) || (INT_MAX < size))
    {
        return -1;
    }

    return (int) size;
}

int main(int argc, char *argv[])
{
    if (2 != argc)
    {
        printf("Usage: %s <PTTTL/RTTTL filename>\n", argv[0]);

        return -1;
    }

    fp = fopen(argv[1], "rb");
    if (NULL == fp)
    {
        printf("Unable to open file %s\n", argv[1]);
        return -1;
    }

    int size = _get_file_size(fp);
    if (size < 0)
    {
        printf("Unable to compute size of file %s\n", argv[1]);
        fclose(fp);
        return -1;
    }

    // Figure out size of input file and malloc() space for it
    _buflen = (uint32_t) size;
    _file_buf = malloc(_buflen);
    if (NULL == _file_buf)
    {
        printf("Unable to allocate memory to store file %s\n", argv[1]);
        return -1;
    }

    // Read the entire file into memory
    (void) fread(_file_buf, 1, _buflen, fp);

    // Create and initialize PTTTL parser object
    ptttl_parser_t parser;
    ptttl_parser_input_iface_t iface = {.read=_read, .seek=_seek};
    int ret = ptttl_parse_init(&parser, iface);
    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        fclose(fp);
        return ret;
    }

    // Create and initialize PTTTL sample generator object
    ptttl_sample_generator_t generator;
    ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;
    ret = ptttl_sample_generator_create(&parser, &generator, &config);
    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        fclose(fp);
        return ret;
    }

    // Generate 8k samples at a time and print each sample value
    const uint32_t sample_buf_len = 8192;
    int16_t sample_buf[sample_buf_len];
    uint32_t num_samples = sample_buf_len;

    while ((ret = ptttl_sample_generator_generate(&generator, &num_samples, sample_buf)) == 0)
    {
        for (uint32_t i = 0; i < num_samples; i++)
        {
            printf("%d\n", sample_buf[i]);
        }
    }

    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error(&parser);
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
    }

    fclose(fp);

    return ret;
}
