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

#if (PTTTL_WAVFILE_GENERATION_STRATEGY < 0) || (PTTTL_WAVFILE_GENERATION_STRATEGY > 2)
#error "Invalid value for PTTTL_WAVFILE_GENERATION_STRATEGY, must in the range 0-2"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#if PTTTL_WAVFILE_GENERATION_STRATEGY == 1
#include <stdlib.h>
#endif


#include "ptttl_to_wav.h"
#include "ptttl_sample_generator.h"
#include "ptttl_common.h"

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

// Helper macro, stores information about an error, which can be retrieved by ptttl_parser_error()
#define ERROR(_parser, _msg)                                        \
{                                                                   \
    _parser->error.error_message = _msg;                            \
    _parser->error.line = _parser->active_stream->line;             \
    _parser->error.column = _parser->active_stream->column;         \
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
 * Initializes sample generator object, and optionally PTTTL parser object.
 *
 * @param generator     Pointer to sample generator object to initialize
 * @param config        Pointer to sample generator config to use
 * @param wave_type     Waveform type for the sample generator to use
 * @param parser        Pointer to parser object
 * @param reinit_parser True if parser object should also be re-initialized
 *
 * @return 0 if successful
 */
static int _init_sample_generation(ptttl_sample_generator_t *generator,
                                   ptttl_sample_generator_config_t *config,
                                   ptttl_waveform_type_e wave_type,
                                   ptttl_parser_t *parser,
                                   bool reinit_parser)
{
    int ret = 0;

    if (reinit_parser)
    {
        ret = ptttl_parse_init(parser, parser->iface);
        if (ret != 0)
        {
            return ret;
        }
    }

    ret = ptttl_sample_generator_create(parser, generator, config);
    if (ret < 0)
    {
        return ret;
    }

    // Set specified waveform type for all channels
    for (uint32_t channel = 0u; channel < parser->channel_count; channel++)
    {
        if (ptttl_sample_generator_set_waveform(generator, channel, wave_type) != 0)
        {
            printf("Failed to set waveform type\n");
            return -1;
        }
    }

    return 0;
}


/**
 * @see ptttl_to_wav.h
 */
int ptttl_to_wav(ptttl_parser_t *parser, FILE *fp, ptttl_sample_generator_config_t *config,
                 ptttl_waveform_type_e wave_type)
{
    ASSERT(NULL != parser);
    ASSERT(NULL != fp);

#if !defined(__BYTE_ORDER__)
    _big_endian = !(*(char *)(int[]){1});
#endif

    wavfile_header_t header;
    ptttl_sample_generator_t generator;
    ptttl_sample_generator_config_t default_config = PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT;
    ptttl_sample_generator_config_t *config_to_use = (config == NULL) ? &default_config : config;

    int ret = _init_sample_generation(&generator, config_to_use, wave_type, parser, false);
    if (ret < 0)
    {
        return ret;
    }

    // Generate 1k samples at a time and write to file, until all samples are generated
    const uint32_t sample_buf_len = 1024;
    uint32_t num_samples = sample_buf_len;

#if PTTTL_WAVFILE_GENERATION_STRATEGY == 0
    int16_t sample_buf[sample_buf_len];

    // First pass of sample generation, we'll generate the header first
    while ((ret = ptttl_sample_generator_generate(&generator, &num_samples, sample_buf)) == 0);
    if (ret < 0)
    {
        return ret;
    }

    int framecount = (int32_t) generator.current_sample;
    _prepare_header(&header, framecount, config_to_use->sample_rate);

    // Write header
    size_t size_written = fwrite(&header, 1u, sizeof(header), fp);
    if (sizeof(header) != size_written)
    {
        ERROR(parser, "Failed to write WAV data");
        return -1;
    }

    // Reset parser / sample generator
    ret = _init_sample_generation(&generator, config_to_use, wave_type, parser, true);
    if (ret < 0)
    {
        return ret;
    }
#elif PTTTL_WAVFILE_GENERATION_STRATEGY == 1
    uint32_t alloc_size = sample_buf_len * 8u;
    int16_t *sample_buf = malloc(alloc_size * sizeof(int16_t));
    if (NULL == sample_buf)
    {
        ERROR(parser, "Failed to allocate memory");
        return -1;
    }

#elif PTTTL_WAVFILE_GENERATION_STRATEGY == 2
    int16_t sample_buf[sample_buf_len];

    // Seek to end of header, we'll generate samples first
    ret = fseek(fp, sizeof(wavfile_header_t), SEEK_SET);
    if (0 != ret)
    {
        ERROR(parser, "Failed to seek within WAV file for writing");
        return -1;
    }
#endif

    int16_t *sample_buf_ptr = &sample_buf[0];

    while ((ret = ptttl_sample_generator_generate(&generator, &num_samples, sample_buf_ptr)) != -1)
    {
        if (_big_endian)
        {
            for (uint32_t i = 0; i < num_samples; i++)
            {
                sample_buf_ptr[i] = cpu_to_le16(sample_buf_ptr[i]);
            }
        }

        if (1 == ret)
        {
            break;
        }
#if (PTTTL_WAVFILE_GENERATION_STRATEGY == 0) || (PTTTL_WAVFILE_GENERATION_STRATEGY == 2)
        size_t size_written = fwrite(&sample_buf, sizeof(uint16_t), num_samples, fp);
        if (num_samples != size_written)
        {
            ERROR(parser, "Failed to write to WAV file");
            return -1;
        }

#elif PTTTL_WAVFILE_GENERATION_STRATEGY == 1
        if ((generator.current_sample + sample_buf_len) > alloc_size)
        {
            alloc_size *= 2u;
            sample_buf = realloc(sample_buf, alloc_size * sizeof(int16_t));
            sample_buf_ptr = &sample_buf[generator.current_sample];
            if (NULL == sample_buf)
            {
                ERROR(parser, "Failed to allocate memory");
                return -1;
            }
        }
        else
        {
            sample_buf_ptr += sample_buf_len;
        }
#endif

        num_samples = sample_buf_len;
    }

    if (ret < 0)
    {
        return ret;
    }

#if PTTTL_WAVFILE_GENERATION_STRATEGY == 2
    // Seek back to beginning and populate header
    ret = fseek(fp, 0u, SEEK_SET);
    if (0 != ret)
    {
        ERROR(parser, "Failed to seek within WAV file for writing");
        return -1;
    }
#endif

#if (PTTTL_WAVFILE_GENERATION_STRATEGY == 2) || (PTTTL_WAVFILE_GENERATION_STRATEGY == 1)
    int32_t framecount = (int32_t) generator.current_sample;
    _prepare_header(&header, framecount, config_to_use->sample_rate);

    // Write header
    size_t size_written = fwrite(&header, 1u, sizeof(header), fp);
    if (sizeof(header) != size_written)
    {
        ERROR(parser, "Failed to write WAV data");
        return -1;
    }
#endif

#if PTTTL_WAVFILE_GENERATION_STRATEGY == 1
    size_written = fwrite(sample_buf, 1u, generator.current_sample * sizeof(int16_t), fp);
    if ((generator.current_sample) != size_written)
    {
        ERROR(parser, "Failed to write WAV data");
        return -1;
    }
#endif

    fflush(fp);

#if PTTTL_WAVFILE_GENERATION_STRATEGY == 1
    free(sample_buf);
#endif

    return 0;
}
