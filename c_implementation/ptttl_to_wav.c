/* pttl_to_wav.c
 *
 * Converts the output of ptttl_parse() into samples suitable for a WAV file.
 * No dynamic memory allocation, and no need to have enough memory to hold the
 * entire WAV file in memory.
 *
 * Requires fopen/fseek/fwrite from stdio.h, and memset() from string.h, and sinf from math.h.
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ptttl_to_wav.h"

#define BITS_PER_SAMPLE (16)
#define SAMPLE_RATE (44100)
#define MAX_SAMPLE_VALUE (0x7FFF)
#define AMPLITUDE (0.8f)

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
 * WAV header data with all fixed/known values populated (chunk_size and
 * subchunk2_size can only be calculated when we know how many samples are in
 * the WAV file)
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
	.sample_rate = SAMPLE_RATE,
    .byte_rate = (SAMPLE_RATE * BITS_PER_SAMPLE) / 8,
    .block_align = BITS_PER_SAMPLE / 8,
    .bits_per_sample = BITS_PER_SAMPLE,

    .subchunk2_id = {'d', 'a', 't', 'a'},
    .subchunk2_size = 0
};


#define ERROR(error_msg) (_error = error_msg)


static const char *_error = NULL;

/**
 * Generate a single sine wave sample for an active note_stream_t object
 *
 * @param freq        Frequency of sine wave
 * @param sine_index  Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static int32_t _generate_sine_sample(float freq, unsigned int sine_index)
{
    // Calculate sample value between 0.0 - 1.0
    float sin_input = 2.0f * M_PI * freq * (((float) sine_index) / (float) SAMPLE_RATE);
    float sample_norm = sinf(sin_input);

    // Convert to 0x0-0x7fff range
    int32_t sample = (int32_t) (sample_norm * (float) MAX_SAMPLE_VALUE);
    if (sample < -(MAX_SAMPLE_VALUE))
    {
        sample = -MAX_SAMPLE_VALUE;
    }
    else if (sample > MAX_SAMPLE_VALUE)
    {
        sample = MAX_SAMPLE_VALUE;
    }

    return sample;
}

/**
 * Load a single PTTTL note from a specific channel into a note_stream_t object
 * 
 * @param generator    Pointer to initialized sample generator
 * @param channel      Pointer to channel containing PTTTL note to load
 * @param note_index   Index of note within channel
 * @param note_stream  Pointer to note stream object to populate
 */
static void _load_note_stream(ptttl_sample_generator_t *generator, ptttl_output_channel_t *channel,
                             unsigned int note_index, ptttl_note_stream_t *note_stream)
{
    note_stream->sine_index = 0u;
    note_stream->note_index = note_index;
    note_stream->start_sample = generator->current_sample;

    // Calculate note time in samples
    float num_samples = channel->notes[note_index].duration_secs  * (float) SAMPLE_RATE;
    note_stream->num_samples = (unsigned int) num_samples;
}

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
int ptttl_sample_generator_create(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator)
{
    if ((NULL == parsed_ptttl) || (NULL == generator))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if (0u == parsed_ptttl->channel_count)
    {
        ERROR("Parsed PTTTL object has a channel count of 0");
        return -1;
    }

    // Zero out 'finished' flag for channels

    // Populate note streams for initial note on all channels
    generator->current_sample = 0u;

    for (unsigned int i = 0u; i < parsed_ptttl->channel_count; i++)
    {
        // Verify all channels have at least 1 note, while we're at it
        if (0u == parsed_ptttl->channels[i].note_count)
        {
            ERROR("Channel has a note count of 0");
            return -1;
        }

        _load_note_stream(generator, &parsed_ptttl->channels[i], 0u, &generator->note_streams[i]);
    }

    return 0;
}

/**
 * @see ptttl_to_wav.h
 */
int ptttl_sample_generator_generate(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                    int32_t *sample)
{
    if ((NULL == parsed_ptttl) || (NULL == generator))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    float summed_sample = 0.0f;
    unsigned int num_channels_provided = 0u;

    for (unsigned int i = 0u; i < parsed_ptttl->channel_count; i++)
    {
        if (1u == generator->channel_finished[i])
        {
            continue;
        }

        ptttl_note_stream_t *stream = &generator->note_streams[i];
        ptttl_output_note_t *note = &parsed_ptttl->channels[i].notes[stream->note_index];

        // Generate next sample value for this channel, add it to the sum
        if (0.0f < note->pitch_hz) // Pitch of 0 indicates pause/rest
        {
            int32_t raw_sample = _generate_sine_sample(note->pitch_hz, stream->sine_index);
            stream->sine_index += 1u;

            // Turn down volume to account for other channels
            summed_sample += ((float) raw_sample) * AMPLITUDE;
        }

        num_channels_provided += 1u;

        // Check if last sample for this note stream
        if ((generator->current_sample - stream->start_sample) >= stream->num_samples)
        {
            if (stream->note_index >= parsed_ptttl->channels[i].note_count)
            {
                // All notes for this channel finished
                generator->channel_finished[i] = 1u;
                continue;
            }
            else
            {
                // Load the next note for this channel
                stream->note_index += 1u;
                _load_note_stream(generator, &parsed_ptttl->channels[i], stream->note_index,
                                  &generator->note_streams[i]);
            }
        }
    }

    if (num_channels_provided == 0u)
    {
        return 1;
    }

    generator->current_sample += 1u;

    if (NULL != sample)
    {
        *sample = (int32_t) (summed_sample / (float) parsed_ptttl->channel_count);
    }

    return 0;
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

    ptttl_sample_generator_t generator;
    int ret = ptttl_sample_generator_create(parsed_ptttl, &generator);
    if (ret < 0)
    {
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
    int32_t sample = 0;
    while ((ret = ptttl_sample_generator_generate(parsed_ptttl, &generator, &sample)) == 0)
    {
        int16_t i16_sample = (int16_t) sample;
        size_t size_written = fwrite(&i16_sample, 1u, sizeof(i16_sample), fp);
        if (sizeof(i16_sample) != size_written)
        {
            ERROR("Failed to write to WAV file");
            return -1;
        }
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
