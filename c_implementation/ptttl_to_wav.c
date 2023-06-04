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
 * Erik Nyquist 2023
 */

#include <stdio.h>
#include <stdint.h>

#include "ptttl_to_wav.h"
#include "ptttl_sample_generator.h"

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

/**
 * WAV header data with all fixed/known values populated
 */
static wavfile_header_t _default_header =
{
    .chunk_id = {'R', 'I', 'F', 'F'},
    .chunk_size = 0,
    .format = {'W', 'A', 'V', 'E'},

    .subchunk1_id = {'f', 'm', 't', ' '},
    .subchunk1_size = BITS_PER_SAMPLE,
    .audio_format = 1,
    .num_channels = 1,
    .sample_rate = 0u,
    .byte_rate = 0u,
    .block_align = BITS_PER_SAMPLE / 8,
    .bits_per_sample = BITS_PER_SAMPLE,

    .subchunk2_id = {'d', 'a', 't', 'a'},
    .subchunk2_size = 0
};


#define ERROR(error_msg) (_error = error_msg)


static const char *_error = NULL;

/**
 * @see ptttl_to_wav.h
 */
const char *ptttl_to_wav_error(void)
{
    return _error;
}


/**
 * @see ptttl_to_wav.h
 */
int ptttl_to_wav(ptttl_output_t *parsed_ptttl, const char *wav_filename)
{
    if ((NULL == parsed_ptttl) || (NULL == wav_filename))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    ptttl_sample_generator_t generator = PTTTL_SAMPLE_GENERATOR_DEFAULT;
    int ret = ptttl_sample_generator_create(parsed_ptttl, &generator);
    if (ret < 0)
    {
        _error = ptttl_sample_generator_error();
        return ret;
    }

    FILE *fp = fopen(wav_filename, "wb");
    if (NULL == fp)
    {
        ERROR("Unable to open WAV file for writing");
        return -1;
    }

    // Seek to end of header, we'll generate samples first
    ret = fseek(fp, sizeof(wavfile_header_t), SEEK_SET);
    if (0 != ret)
    {
        ERROR("Failed to seek within WAV file for writing");
        return -1;
    }

    // Generate all samples and write to file
    int16_t sample = 0;
    while ((ret = ptttl_sample_generator_generate(parsed_ptttl, &generator, &sample)) == 0)
    {
        size_t size_written = fwrite(&sample, 1u, sizeof(sample), fp);
        if (sizeof(sample) != size_written)
        {
            ERROR("Failed to write to WAV file");
            return -1;
        }
    }

    if (ret < 0)
    {
        _error = ptttl_sample_generator_error();
        return ret;
    }

    // Seek back to beginning and populate header
    ret = fseek(fp, 0u, SEEK_SET);
    if (0 != ret)
    {
        ERROR("Failed to seek within WAV file for writing");
        return -1;
    }

    int32_t framecount = ((int32_t) generator.current_sample) + 1u;
    _default_header.subchunk2_size = (framecount * BITS_PER_SAMPLE) / 8;
    _default_header.chunk_size = (4  + (8 + _default_header.subchunk1_size)) + (8 + _default_header.subchunk2_size);
    _default_header.sample_rate = generator.sample_rate;
    _default_header.byte_rate = (generator.sample_rate * BITS_PER_SAMPLE) / 8;

    // Write header
    size_t size_written = fwrite(&_default_header, 1u, sizeof(_default_header), fp);
    if (sizeof(_default_header) != size_written)
    {
        ERROR("Failed to write to WAV file");
        return -1;
    }

    fclose(fp);

    return 0;
}
