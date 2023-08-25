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
#include <stdio.h>

#include "ptttl_sample_generator.h"
#include "ptttl_common.h"


// Max positive value of a signed 16-bit sample
#define MAX_SAMPLE_VALUE   (0x7FFF)


// Store an error message for reporting by ptttl_sample_generator_error()
#define ERROR(error_msg) (_error = error_msg)


// Size of table of pre-computed sine values
#define SINE_TABLE_SIZE (4096u)

// PI * 2 as a constant, for convenience
#define TWO_PI (6.28318530718f)

/* Table of pre-computed sine values, allows to implement a slightly faster but
 * also slightly less accurate sinf() function */
static float _sine_table[SINE_TABLE_SIZE];
static int _sine_table_initialized = 0;


// Static storage for description of last error
static const char *_error = NULL;


/**
 * Calculate the power of 2 for a given exponent
 *
 * @param exp  Exponent
 *
 * @return Result
 */
static inline unsigned int _raise_powerof2(unsigned int exp)
{
    unsigned int ret = 1u;
    for (unsigned int i = 0u; i < exp; i++)
    {
        ret *= 2u;
    }

    return ret;
}

/**
 * Faster but less accurate implementation of sinf function. Uses values
 * pre-computed one time by the standard sinf() function.
 *
 * @param x  Input value
 *
 * @return Computed sine value
 */
float fast_sinf(float x)
{
    float quotient = x / TWO_PI;
    x = (x - TWO_PI * (int) quotient) + (TWO_PI * ((int) (x < 0)));

    float index = x * (SINE_TABLE_SIZE / TWO_PI);
    int index_low = (int)index;
    int index_high = (index_low + 1) % SINE_TABLE_SIZE;
    float frac = index - index_low;

    float y = _sine_table[index_low] + frac * (_sine_table[index_high] - _sine_table[index_low]);

    return y;
}

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
    return fast_sinf(2.0f * M_PI * freq * (((float) sine_index) / (float) sample_rate));
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
    return (int32_t) (sample_norm * (float) MAX_SAMPLE_VALUE);
}

/**
 * Convert a piano key note number (1 thorugh 88) to the corresponding pitch
 * in Hz.
 *
 * @param note_number Piano key note number from 1 through 88, where 1 is the lowest note
 *                    and 88 is the highest note.
 * @param pitch_hz    Pointer to location to store corresponding pitch in Hz
 */
static void _note_number_to_pitch(uint32_t note_number, float *pitch_hz)
{
    // Maps note_pitch_e enum values to the corresponding pitch in Hz
    static const float note_pitches[NOTE_PITCH_COUNT] =
    {
        261.625565301f,   // NOTE_C
        277.182630977f,   // NOTE_CS & NOTE_DB
        293.664767918f,   // NOTE_D
        311.126983723f,   // NOTE_DS & NOTE_EB
        329.627556913f,   // NOTE_E
        349.228231433f,   // NOTE_ES & NOTE_F
        369.994422712f,   // NOTE_FS & NOTE_GB
        391.995435982f,   // NOTE_G
        415.30469758f,    // NOTE_GS & NOTE_AB
        440.0f,           // NOTE_A
        466.163761518f,   // NOTE_AS & NOTE_BB
        493.883301256f    // NOTE_B
    };

    float result = 0.0f;
    int octave = (int) NOTE_OCTAVE_MAX;

    // Some nasty arithmetic to do a branchless conversion of note number to octave number
    octave = ((int) (((note_number - 3u) / 12u) + 1u)) * (int) !(note_number < 3);

    if (0 == octave)
    {
        result = note_pitches[NOTE_A + (note_number - 1u)];
    }
    else
    {
        unsigned int note_pitch_index = note_number - _octave_starts[octave];
        result = note_pitches[note_pitch_index];
    }

    // Set true pitch based on octave, if octave is not 4
    if (octave < 4)
    {
        result = result / (float) _raise_powerof2((unsigned int) (4 - octave));
    }
    else if (octave > 4)
    {
        result = result * (float) _raise_powerof2((unsigned int) (octave - 4));
    }

    *pitch_hz = result;
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
    uint32_t time_ms = PTTTL_NOTE_DURATION(&channel->notes[note_index]);
    float num_samples = ((float) time_ms) * (((float) generator->config.sample_rate) / 1000.0f);
    note_stream->num_samples = (unsigned int) num_samples;

    // Handle case where attack + delay is longer than note length
    unsigned int attack = generator->config.attack_samples;
    unsigned int decay = generator->config.decay_samples;
    if ((attack + decay) > note_stream->num_samples)
    {
        unsigned int diff = (attack + decay) - note_stream->num_samples;
        if (attack > decay)
        {
            attack = (attack > diff) ? attack - diff : 0u;
        }
        else
        {
            decay = (decay > diff) ? decay - diff : 0u;
        }
    }

    note_stream->attack = attack;
    note_stream->decay = decay;

    // Calculate note pitch from piano key number
    note_stream->note_number = PTTTL_NOTE_VALUE(&channel->notes[note_index]);

    if (0u != note_stream->note_number)
    {
        _note_number_to_pitch(note_stream->note_number, &note_stream->pitch_hz);
    }
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
    generator->parsed_ptttl = parsed_ptttl;

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

    // Initialize sine table, if not done yet
    if (!_sine_table_initialized)
    {
        for (int i = 0; i < SINE_TABLE_SIZE; ++i)
        {
            _sine_table[i] = sinf(TWO_PI * i / SINE_TABLE_SIZE);
        }

        _sine_table_initialized = 1;
    }

    return 0;
}

/**
 * Generate the next sample for the given note stream on the given channel
 *
 * @param generator      Pointer to initialized sample generator
 * @param note           Pointer to the ptttl_output_note_t instance to generate a sample for
 * @param stream         Pointer to ptttl_note_stream_t instance to generate a sample for
 * @param channel_idx    Channel index of channel the generated sample is for
 * @param sample         Pointer to location to store generated sample
 *
 * @return 0 if there are more samples remaining for the provided note stream, 1 otherwise
 */
static int _generate_channel_sample(ptttl_sample_generator_t *generator, ptttl_output_note_t *note,
                                    ptttl_note_stream_t *stream, unsigned int channel_idx,
                                    float *sample)
{
    int ret = 0;

    // Generate next sample value for this channel
    if (0u == stream->note_number) // Note number 0 indicates pause/rest
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
            float note_pitch_hz = stream->pitch_hz + pitch_change_hz;

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

        raw_sample = _generate_sine_sample(generator->config.sample_rate, stream->pitch_hz, stream->sine_index);
#if PTTTL_VIBRATO_ENABLED
        }
#endif // PTTTL_VIBRATO_ENABLED

        stream->sine_index += 1u;

        // Handle attack & decay
        unsigned int samples_elapsed = generator->current_sample - stream->start_sample;
        unsigned int samples_remaining = stream->num_samples - samples_elapsed;

        // Modify channel sample amplitude based on attack/decay settings
        if (samples_elapsed < stream->attack)
        {
            raw_sample *= ((float) samples_elapsed) / ((float) stream->attack);
        }
        else if (samples_remaining < stream->decay)
        {
            raw_sample *= ((float) samples_remaining) / ((float) stream->decay);
        }

        // Set final desired amplitude for channel sample
        *sample = ((float) raw_sample) * generator->config.amplitude;
    }

    // Check if last sample for this note stream
    if ((generator->current_sample - stream->start_sample) >= stream->num_samples)
    {
        if (stream->note_index >= generator->parsed_ptttl->channels[channel_idx].note_count)
        {
            // All notes for this channel finished
            ret = 1u;
        }
        else
        {
            // Load the next note for this channel
            stream->note_index += 1u;
            _load_note_stream(generator, &generator->parsed_ptttl->channels[channel_idx],
                              stream->note_index, &generator->note_streams[channel_idx]);
        }
    }

    return ret;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_generate(ptttl_sample_generator_t *generator, uint32_t *num_samples,
                                    int16_t *samples)
{
    if ((NULL == generator) || (NULL == num_samples) || (NULL == samples))
    {
        ERROR("NULL pointer passed to function");
        return -1;
    }

    uint32_t samples_to_generate = *num_samples;
    *num_samples = 0u;

    for (unsigned int samplenum = 0u; samplenum < samples_to_generate; samplenum++)
    {
        float summed_sample = 0.0f;
        unsigned int num_channels_provided = 0u;

        // Sum the current state of all channels to generate the next sample
        for (unsigned int chan = 0u; chan < generator->parsed_ptttl->channel_count; chan++)
        {
            if (1u == generator->channel_finished[chan])
            {
                // No more samples to generate for this channel
                continue;
            }

            num_channels_provided += 1u;
            ptttl_note_stream_t *stream = &generator->note_streams[chan];
            ptttl_output_note_t *note = &generator->parsed_ptttl->channels[chan].notes[stream->note_index];

            float chan_sample = 0.0f;
            generator->channel_finished[chan] = _generate_channel_sample(generator, note, stream,
                                                                         chan, &chan_sample);

            summed_sample += chan_sample;
        }

        if (num_channels_provided == 0u)
        {
            // Finished-- no samples left on any channel
            return 1;
        }

        generator->current_sample += 1u;
        samples[samplenum] = (int16_t) (summed_sample / (float) generator->parsed_ptttl->channel_count);
        *num_samples += 1u;
    }

    return 0;
}
