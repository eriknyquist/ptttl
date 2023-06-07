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

#define MAX_SAMPLE_VALUE   (0x7FFF)   ///< Max. value of a signed 16-bit sample
#define VIBRATO_CHUNK_SIZE (200u)     ///< Number of samples between updating vibrato frequency

#define ERROR(error_msg) (_error = error_msg)


static const char *_error = NULL;


/**
 * Generate a single point on a sine wave, between 0.0-1.0, for a given sample rate and frequency
 *
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of sine wave
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static float _generate_sine_point(unsigned int sample_rate, float freq, unsigned int sine_index)
{
    return sinf(2.0f * M_PI * freq * (((float) sine_index) / (float) sample_rate));
}

/**
 * Generate a single sine wave sample between 0-65535 for a given sample rate and frequency
 *
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of sine wave
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static int32_t _generate_sine_sample(unsigned int sample_rate, float freq, unsigned int sine_index)
{
    // Calculate sample value between 0.0 - 1.0
    float sample_norm = _generate_sine_point(sample_rate, freq, sine_index);

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
#ifdef PTTTL_VIBRATO_ENABLED
    note_stream->phasor_state = 0.0f;
#endif // PTTTL_VIBRATO_ENABLED

    // Calculate note time in samples
    float num_samples = channel->notes[note_index].duration_secs  * (float) generator->config.sample_rate;
    note_stream->num_samples = (unsigned int) num_samples;
}

/**
 * @see ptttl_sample_generator.h
 */
const char *ptttl_sample_generator_error(void)
{
    return _error;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_create(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                  ptttl_sample_generator_config_t *config)
{
    if ((NULL == parsed_ptttl) || (NULL == generator) || (NULL == config))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    if (0u == parsed_ptttl->channel_count)
    {
        ERROR("Parsed PTTTL object has a channel count of 0");
        return -1;
    }

    if ((config->amplitude > 1.0f) || (config->amplitude < 0.0f))
    {
        ERROR("Sample generator amplitude must be between 0.0 - 1.0");
        return -1;
    }

    // Copy config data into generator object
    generator->config = *config;

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
 * Generate the next sample for the given note stream on the given channel
 *
 * @param parsed_ptttl   Pointer to parsed PTTTL data
 * @param generator      Pointer to initialized sample generator
 * @param note           Pointer to the ptttl_output_note_t instance to generate a sample for
 * @param stream         Pointer to ptttl_note_stream_t instance to generate a sample for
 * @param channel_idx    Channel index of channel the generated sample is for
 * @param sample         Pointer to location to store generated sample
 *
 * @return 0 if there are more samples remaining for the provided note stream, 1 otherwise
 */
static int _generate_channel_sample(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                    ptttl_output_note_t *note, ptttl_note_stream_t *stream,
                                    unsigned int channel_idx, float *sample)
{
    int ret = 0;
    unsigned int samples_elapsed = generator->current_sample - stream->start_sample;
    unsigned int samples_remaining = stream->num_samples - samples_elapsed;

    unsigned int attack = generator->config.attack_samples;
    unsigned int decay = generator->config.decay_samples;

    // Handle case where attack + delay is longer than note length
    if ((attack + decay) > stream->num_samples)
    {
        unsigned int diff = (attack + decay) - stream->num_samples;
        if (attack > decay)
        {
            attack = (attack > diff) ? attack - diff : 0u;
        }
        else
        {
            decay = (decay > diff) ? decay - diff : 0u;
        }
    }

    // Generate next sample value for this channel
    if (0.0f == note->pitch_hz) // Pitch of 0 indicates pause/rest
    {
        *sample = 0.0f;
    }
    else
    {
        int32_t raw_sample = 0;

#if PTTTL_VIBRATO_ENABLED
        uint32_t vfreq = PTTTL_NOTE_VIBRATO_FREQ(note);
        uint32_t vvar = PTTTL_NOTE_VIBRATO_VAR(note);

        if ((0u != vfreq) || (0u != vvar))
        {
            float vsine = _generate_sine_point(generator->config.sample_rate, vfreq, stream->sine_index);
            float pitch_change_hz = ((float) vvar) * vsine;
            float note_pitch_hz = note->pitch_hz + pitch_change_hz;

            float vsample = sinf(2.0f * M_PI * stream->phasor_state);
            float phasor_inc = note_pitch_hz / generator->config.sample_rate;
            stream->phasor_state += phasor_inc;
            if (stream->phasor_state >= 1.0f)
            {
                stream->phasor_state -= 1.0f;
            }

            raw_sample = (int32_t) (vsample * (float) MAX_SAMPLE_VALUE);
        }
        else
        {
#endif // PTTTL_VIBRATO_ENABLED

        raw_sample = _generate_sine_sample(generator->config.sample_rate, note->pitch_hz, stream->sine_index);
#if PTTTL_VIBRATO_ENABLED
        }
#endif // PTTTL_VIBRATO_ENABLED

        stream->sine_index += 1u;

        // Modify channel sample amplitude based on attack/decay settings
        if (samples_elapsed < attack)
        {
            raw_sample *= ((float) samples_elapsed) / ((float) attack);
        }
        else if (samples_remaining < decay)
        {
            raw_sample *= ((float) samples_remaining) / ((float) decay);
        }

        // Set final desired amplitude for channel sample
        *sample = ((float) raw_sample) * generator->config.amplitude;
    }

    // Check if last sample for this note stream
    if ((generator->current_sample - stream->start_sample) >= stream->num_samples)
    {
        if (stream->note_index >= parsed_ptttl->channels[channel_idx].note_count)
        {
            // All notes for this channel finished
            ret = 1u;
        }
        else
        {
            // Load the next note for this channel
            stream->note_index += 1u;
            _load_note_stream(generator, &parsed_ptttl->channels[channel_idx], stream->note_index,
                              &generator->note_streams[channel_idx]);
        }
    }

    return ret;
}

/**
 * @see ptttl_sample_generator.h
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

    // Sum the current state of all channels to generate the next sample
    for (unsigned int i = 0u; i < parsed_ptttl->channel_count; i++)
    {
        if (1u == generator->channel_finished[i])
        {
            // No more samples to generate for this channel
            continue;
        }

        num_channels_provided += 1u;
        ptttl_note_stream_t *stream = &generator->note_streams[i];
        ptttl_output_note_t *note = &parsed_ptttl->channels[i].notes[stream->note_index];

        float chan_sample = 0.0f;
        int channel_finished = _generate_channel_sample(parsed_ptttl, generator, note, stream,
                                                        i, &chan_sample);

        summed_sample += chan_sample;

        if (1 == channel_finished)
        {
            // All notes for this channel finished
            generator->channel_finished[i] = 1u;
        }
    }

    if (num_channels_provided == 0u)
    {
        // Finished-- no samples left on any channel
        return 1;
    }

    generator->current_sample += 1u;

    if (NULL != sample)
    {
        *sample = (int16_t) (summed_sample / (float) parsed_ptttl->channel_count);
    }

    return 0;
}
