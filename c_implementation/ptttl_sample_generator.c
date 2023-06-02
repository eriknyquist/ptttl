/* ptttl_sample_generator.c
 *
 * Converts the output of ptttl_parse() into a stream of signed 16-bit audio samples
 * suitable for a WAV file. Samples can be obtained one at a time, at your leisure.
 *
 * Requires ptttl_parser.c
 *
 * Requires stdint.h, memset() from string.h, and sinf from math.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <math.h>
#include <string.h>

#include "ptttl_sample_generator.h"

#define MAX_SAMPLE_VALUE (0x7FFF)
#define AMPLITUDE (0.8f)

#define ERROR(error_msg) (_error = error_msg)


static const char *_error = NULL;


/**
 * Generate a single sine wave sample for an active note_stream_t object
 *
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of sine wave
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static int32_t _generate_sine_sample(int32_t sample_rate, float freq, unsigned int sine_index)
{
    // Calculate sample value between 0.0 - 1.0
    float sin_input = 2.0f * M_PI * freq * (((float) sine_index) / (float) sample_rate);
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
    float num_samples = channel->notes[note_index].duration_secs  * (float) generator->sample_rate;
    note_stream->num_samples = (unsigned int) num_samples;
}

/**
 * @see ptttl_to_wav.h
 */
int ptttl_sample_generator_create(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                  int32_t sample_rate)
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

    generator->sample_rate = sample_rate;
    generator->current_sample = 0u;

    memset(generator->channel_finished, 0, sizeof(generator->channel_finished));

    // Populate note streams for initial note on all channels
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
                                    int16_t *sample)
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
            int32_t raw_sample = _generate_sine_sample(generator->sample_rate, note->pitch_hz, stream->sine_index);
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
        *sample = (int16_t) (summed_sample / (float) parsed_ptttl->channel_count);
    }

    return 0;
}
