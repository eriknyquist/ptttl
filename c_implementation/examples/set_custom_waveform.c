/* set_custom_waveform.c
 *
 * Sample main.c showing how to read RTTTL/PTTTL source from a file, provide a
 * custom waveform generator function to be used by the sample generator, generate
 * PCM audio samples, and print the sample values to stdout.
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
#include "ptttl_parser.h"
#include "ptttl_sample_generator.h"

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

/**
 * Triangle wave generator
 *
 * @param x   Phase; current position on the waveform, in turns (0.0 through 1.0)
 * @param p   Wave frequency in Hz
 * @param s   Sampling rate in Hz
 *
 * @return Value for the given phase, in the range -1.0 through 1.0
 */
static float _triangle_generator(float x, float p, unsigned int s)
{
    (void) p; // Unused
    (void) s; // Unused

    int ix = (int)x;
    float t = x - ix;
    if (t < 0.0f) t += 1.0f;

    // build [-1..+1..-1]
    if (t < 0.5f)
    {
        return t * 4.0f - 1.0f; // rise from -1 to +1
    }
    else
    {
        return 3.0f - t * 4.0f; // fall from +1 back to -1
    }
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

    // Iterate over all channel indices
    for (uint32_t i = 0u; i < parser.channel_count; i++)
    {
        // Use custom waveform generator for each channel in the input text
        ret = ptttl_sample_generator_set_custom_waveform(&generator, i, _triangle_generator);
        if (ret < 0)
        {
            printf("Unable to set waveform type to square\n");
            fclose(fp);
            return ret;
        }
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
