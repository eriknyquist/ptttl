/* ptttl_to_wav.c
 *
 * Converts the output of ptttl_parse() into a WAV file.
 * No dynamic memory allocation, and no loading the entire WAV file in memory.
 *
 * Requires ptttl_parser.c and ptttl_sample_generator.c
 *
 * Requires stdint.h, and fopen/fseek/fwrite from stdio.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#include <stdio.h>
#include <stdint.h>

#include "ptttl_to_wav.h"
#include "ptttl_sample_generator.h"


// Sample width in bits
#define BITS_PER_SAMPLE (16)


/**
 * The header of a wav file Based on:
 * https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
 */
typedef struct
{
    char chunk_id[4];
    int32_t chunk_size;
    char format[4];

    char subchunk1_id[4];
    int32_t subchunk1_size;
    int16_t audio_format;
    int16_t num_channels;
    int32_t sample_rate;
    int32_t byte_rate;
    int16_t block_align;
    int16_t bits_per_sample;

    char subchunk2_id[4];
    int32_t subchunk2_size;
} wavfile_header_t;


// Store a description of the last error
static ptttl_parser_error_t _error = {.line = 0u, .column = 0u, .error_message=NULL};


#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
static int _big_endian = 1;
#else
static int _big_endian = 0;
#endif
#else
static int _big_endian; // Set at runtime
#endif

// Macros to swap byte order if native order is big-endian
#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    // CPU is big endian
    #define cpu_to_le32(val)  __builtin_bswap32(val)
    #define cpu_to_le16(val)  __builtin_bswap16(val)
#else
    // CPU is little endian
    #define cpu_to_le32(val) val
    #define cpu_to_le16(val) val
#endif
#else
    // __BYTE_ORDER__ not defined
    #define cpu_to_le32(val) (_big_endian) ? __builtin_bswap32(val) : val
    #define cpu_to_le16(val) (_big_endian) ? __builtin_bswap16(val) : val
#endif

// Swaps byte order of a signed 16-bit value if native order is big-endian

// Helper macro, stores information about an error, which can be retrieved by ptttl_to_wav_error()
#define ERROR(_parser, _msg)                                \
{                                                           \
    _error.error_message = _msg;                            \
    _error.line = _parser->active_stream->line;             \
    _error.column = _parser->active_stream->column;         \
}

static void _prepare_header(wavfile_header_t *output,
                            int32_t framecount, uint32_t samplerate)
{
    int32_t subchunk2_size = (framecount * BITS_PER_SAMPLE) / 8;

    output->chunk_id[0] = 'R';
    output->chunk_id[1] = 'I';
    output->chunk_id[2] = 'F';
    output->chunk_id[3] = 'F';
    output->chunk_size = cpu_to_le32((4  + (8 + BITS_PER_SAMPLE)) + (8 + subchunk2_size));
    output->format[0] = 'W';
    output->format[1] = 'A';
    output->format[2] = 'V';
    output->format[3] = 'E';
    output->subchunk1_id[0] = 'f';
    output->subchunk1_id[1] = 'm';
    output->subchunk1_id[2] = 't';
    output->subchunk1_id[3] = ' ';
    output->subchunk1_size = cpu_to_le32(BITS_PER_SAMPLE);
    output->subchunk2_size = cpu_to_le32(subchunk2_size);
    output->audio_format = cpu_to_le16(1);
    output->num_channels = cpu_to_le16(1);
    output->sample_rate = cpu_to_le32((int32_t)samplerate);
    output->byte_rate = cpu_to_le32((int32_t)((samplerate * BITS_PER_SAMPLE) / 8));
    output->block_align = cpu_to_le16(BITS_PER_SAMPLE / 8);
    output->bits_per_sample = cpu_to_le16(BITS_PER_SAMPLE);
    output->subchunk2_id[0] = 'd';
    output->subchunk2_id[1] = 'a';
    output->subchunk2_id[2] = 't';
    output->subchunk2_id[3] = 'a';
}

/**
 * @see ptttl_to_wav.h
 */
ptttl_parser_error_t ptttl_to_wav_error(void)
{
    return _error;
}


/**
 * @see ptttl_to_wav.h
 */
int ptttl_to_wav(ptttl_parser_t *parser, const char *wav_filename)
{
    if (NULL == parser)
    {
        return -1;
    }

    if (NULL == wav_filename)
    {
        ERROR(parser, "NULL pointer passed to function");
    }

#if !defined(__BYTE_ORDER__)
    _big_endian = !(*(char *)(int[]){1});
#endif

    ptttl_sample_generator_t generator;
    ptttl_sample_generator_config_t config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;

    int ret = ptttl_sample_generator_create(parser, &generator, &config);
    if (ret < 0)
    {
        _error = ptttl_sample_generator_error();
        return ret;
    }

    FILE *fp = fopen(wav_filename, "wb");
    if (NULL == fp)
    {
        ERROR(parser, "Unable to open WAV file for writing");
        return -1;
    }

    // Seek to end of header, we'll generate samples first
    ret = fseek(fp, sizeof(wavfile_header_t), SEEK_SET);
    if (0 != ret)
    {
        ERROR(parser, "Failed to seek within WAV file for writing");
        fclose(fp);
        return -1;
    }

    // Generate 1k samples at a time and write to file, until all samples are generated
    const uint32_t sample_buf_len = 1024;
    int16_t sample_buf[sample_buf_len];
    uint32_t num_samples = sample_buf_len;

    while ((ret = ptttl_sample_generator_generate(&generator, &num_samples, sample_buf)) != -1)
    {
        if (_big_endian)
        {
            for (uint32_t i = 0; i < num_samples; i++)
            {
                sample_buf[i] = cpu_to_le16(sample_buf[i]);
            }
        }

        size_t size_written = fwrite(&sample_buf, sizeof(uint16_t), num_samples, fp);
        if (num_samples != size_written)
        {
            ERROR(parser, "Failed to write to WAV file");
            fclose(fp);
            return -1;
        }

        if (1 == ret)
        {
            break;
        }

        num_samples = sample_buf_len;
    }

    if (ret < 0)
    {
        _error = ptttl_sample_generator_error();
        fclose(fp);
        return ret;
    }

    // Seek back to beginning and populate header
    ret = fseek(fp, 0u, SEEK_SET);
    if (0 != ret)
    {
        ERROR(parser, "Failed to seek within WAV file for writing");
        fclose(fp);
        return -1;
    }

    wavfile_header_t header;
    int32_t framecount = ((int32_t) generator.current_sample) + 1u;
    _prepare_header(&header, framecount, config.sample_rate);

    // Write header
    size_t size_written = fwrite(&header, 1u, sizeof(header), fp);
    if (sizeof(header) != size_written)
    {
        ERROR(parser, "Failed to write to WAV file");
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}
