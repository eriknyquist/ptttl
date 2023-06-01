#include <string.h>
#include <math.h>

#include "ptttl_to_wav.h"

#define SAMPLE_RATE (44100u)
#define MAX_SAMPLE_VALUE (0x7FFFu)
#define AMPLITUDE (0.8f)


#define ERROR(error_msg) (_error = error_msg)


static const char *_error = NULL;

const char *ptttl_to_wav_error(void)
{
    return _error;
}

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
            summed_sample += ((float) raw_sample) * (float) parsed_ptttl->channel_count;
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

int ptttl_source_to_wav(ptttl_output_t *parsed_ptttl, const char *wav_filename)
{
    return 0;
}
