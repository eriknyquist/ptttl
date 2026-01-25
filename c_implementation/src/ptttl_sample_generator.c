/* ptttl_sample_generator.c
 *
 * Converts the output of ptttl_parse_next() into a stream of signed 16-bit audio samples
 * suitable for a WAV file. Samples can be obtained one at a time, at your leisure.
 *
 * Requires ptttl_parser.c
 *
 * Requires stdint.h and memset() from string.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#include <string.h>

#include "ptttl_sample_generator.h"
#include "ptttl_common.h"


// Max positive value of a signed 16-bit sample
#define MAX_SAMPLE_VALUE   (0x7FFF)

// Default waveform type for all channels
#define DEFAULT_WAVEFORM_TYPE WAVEFORM_TYPE_SINE

#define PI 3.14159265358979323846f

// Store an error message for reporting by ptttl_parser_error()
#define ERROR(_parser, _msg)                                        \
{                                                                   \
    _parser->error.error_message = _msg;                            \
    _parser->error.line = _parser->active_stream->line;             \
    _parser->error.column = _parser->active_stream->column;         \
}


// Forward declaration of built-in waveform generators
static float _sine_generator(float x, float p, unsigned int s);
static float _triangle_generator(float x, float p, unsigned int s);
static float _sawtooth_generator(float x, float p, unsigned int s);
static float _square_generator(float x, float p, unsigned int s);

// Mapping of waveform type enums to waveform generator functions
static ptttl_waveform_generator_t _waveform_generators[WAVEFORM_TYPE_COUNT] =
{
    _sine_generator,           // WAVEFORM_TYPE_SINE
    _triangle_generator,       // WAVEFORM_TYPE_TRIANGLE
    _sawtooth_generator,       // WAVEFORM_TYPE_SAWTOOTH
    _square_generator          // WAVEFORM_TYPE_SQUARE
};


/**
 * Calculate the power of 2 for a given exponent
 *
 * @param exp  Exponent
 *
 * @return Result
 */
static uint32_t _raise_powerof2(uint32_t exp)
{
    uint32_t ret = 1u;
    for (uint32_t i = 0u; i < exp; i++)
    {
        ret *= 2u;
    }

    return ret;
}

/**
 * Square wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _square_generator(float x, float p, unsigned int s)
{
    x = x - (int)x;
    if (x < 0.0f) x += 1.0f;

    // Calculate max. number of harmonics given the waveform frequency
    int maxh = (int) ((((float) s) * 0.5f) / p);
    if (maxh < 1) maxh = 1;               // At least the fundamental harmonic
    int hcount = (maxh < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS) ?
                  maxh : PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS;

    // Generate square point by summing points of sine waves of the harmonics
    float sum = 0.0f;
    // square only has odd harmonics
    for (int n = 1; n <= hcount; n += 2)
    {
        sum += (4.0f / (PI * n)) * _sine_generator(n * x, 0.0f, 0u);
    }

    return sum;
}

/**
 * Sawtooth wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _sawtooth_generator(float x, float p, unsigned int s)
{
    x = x - (int)x;
    if (x < 0.0f) x += 1.0f;

    // Calculate max. number of harmonics given the waveform frequency
    int maxh = (int) ((((float) s) * 0.5f) / p);
    if (maxh < 1) maxh = 1;               // At least the fundamental harmonic
    int hcount = (maxh < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS) ?
                  maxh : PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS;

    // Generate sawtooth point by summing points of sine waves of the harmonics
    float sum = 0.0f;
    for (int n = 1; n <= hcount; ++n)
    {
        float sign = (n & 1) ? 1.0f : -1.0f;
        sum += sign * (2.0f / (PI * n)) * _sine_generator(n * x, 0.0f, 0u);
    }
    return sum;
}

/**
 * Triangle wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _triangle_generator(float x, float p, unsigned int s)
{
    (void) p; // Unused
    (void) s; // Unused

    float value = 0.0f;

    for (int k = 0; k < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS; k++)
    {
        int n = 2 * k + 1;
        float sign = (k & 1) ? -1.0f : 1.0f;
        value += sign * _sine_generator(n * x, p, s) / (n * n);
    }

    value *= 8.0f / (float) (PI * PI);
    return value;
}

/**
 * Sine wave generator
 *
 * @see ptttl_waveform_generator_t
 *
 * Fast sine approximation copied from:
 * https://github.com/skeeto/scratch/blob/master/misc/rtttl.c
 *
 */
static float _sine_generator(float x, float p, unsigned int s)
{
    (void) p; // Unused
    (void) s; // Unused

    x  = x < 0 ? 0.5f - x : x;
    x -= 0.500f + (float)(int)x;
    x *= 16.00f * ((x < 0 ? -x : x) - 0.50f);
    x += 0.225f * ((x < 0 ? -x : x) - 1.00f) * x;
    return x;
}


/**
 * Generate a single point on a waveform, between -1.0-1.0, for a given sample rate and frequency
 *
 * @param wgen         Waveform generator function
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of sine wave
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static float _generate_waveform_point(ptttl_waveform_generator_t wgen, unsigned int sample_rate,
                                      float freq, unsigned int sine_index)
{
    float sine_state = freq * (((float) sine_index) / (float) sample_rate);
    return wgen(sine_state, freq, sample_rate);
}


/**
 * Generate a single waveform sample between 0-65535 for a given sample rate and frequency
 *
 * @param wgen         Waveform generator function
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of the waveform
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static int32_t _generate_waveform_sample(ptttl_waveform_generator_t wgen, unsigned int sample_rate,
                                         float freq, unsigned int sine_index)
{
    // Calculate sample value between 0.0 - 1.0
    float sample_norm = _generate_waveform_point(wgen, sample_rate, freq, sine_index);

    // Convert to 0x0-0x7fff range
    return (int32_t) (sample_norm * (float) MAX_SAMPLE_VALUE);
}

/**
 * Convert a piano key note number (1 through 88) to the corresponding pitch
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

    uint32_t octave = ((note_number + 20u) / 12u) - 1u;
    uint32_t note_pitch_index = (note_number + 8u) % NOTE_PITCH_COUNT;
    float result = note_pitches[note_pitch_index];

    // Set true pitch based on octave, if octave is not 4
    if (octave < 4u)
    {
        result = result / (float) _raise_powerof2(4u - octave);
    }
    else if (octave > 4u)
    {
        result = result * (float) _raise_powerof2(octave - 4u);
    }

    *pitch_hz = result;
}


/**
 * Load a single PTTTL note from a specific channel into a note_stream_t object
 *
 * @param generator    Pointer to initialized sample generator
 * @param note         Pointer to parsed note object
 * @param note_stream  Pointer to note stream object to populate
 */
static void _load_note_stream(ptttl_sample_generator_t *generator, ptttl_output_note_t *note,
                              ptttl_note_stream_t *note_stream)
{
    note_stream->sine_index = 0u;
    note_stream->start_sample = generator->current_sample;

    note_stream->phasor_state = 0.0f;

    // Calculate note time in samples
    uint32_t time_ms = PTTTL_NOTE_DURATION(note);
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
    note_stream->vibrato_frequency = PTTTL_NOTE_VIBRATO_FREQ(note);
    note_stream->vibrato_variance = PTTTL_NOTE_VIBRATO_VAR(note);

    // Calculate note pitch from piano key number
    note_stream->note_number = PTTTL_NOTE_VALUE(note);

    if (0u != note_stream->note_number)
    {
        _note_number_to_pitch(note_stream->note_number, &note_stream->pitch_hz);
    }
}


/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_create(ptttl_parser_t *parser, ptttl_sample_generator_t *generator,
                                  ptttl_sample_generator_config_t *config)
{
    ASSERT(NULL != parser);
    ASSERT(NULL != generator);
    ASSERT(NULL != config);

    if (0u == parser->channel_count)
    {
        ERROR(parser, "PTTTL parser object has a channel count of 0");
        return -1;
    }

    if ((config->amplitude > 1.0f) || (config->amplitude < 0.0f))
    {
        ERROR(parser, "Sample generator amplitude must be between 0.0 - 1.0");
        return -1;
    }

    // Copy config data into generator object
    generator->config = *config;
    generator->parser = parser;

    generator->current_sample = 0u;

    memset(generator->channel_finished, 0, sizeof(generator->channel_finished));

    // Populate note streams for initial note on all channels
    for (uint32_t chan = 0u; chan < parser->channel_count; chan++)
    {
        ptttl_output_note_t note;
        int ret = ptttl_parse_next(generator->parser, chan, &note);
        if (ret != 0)
        {
            return ret;
        }

        _load_note_stream(generator, &note, &generator->note_streams[chan]);

        // Set default waveform generator
        generator->note_streams[chan].wgen = _waveform_generators[DEFAULT_WAVEFORM_TYPE];
    }

    return 0;
}

/**
 * Generate the next sample for the given note stream on the given channel
 *
 * @param generator      Pointer to initialized sample generator
 * @param stream         Pointer to ptttl_note_stream_t instance to generate a sample for
 * @param channel_idx    Channel index of channel the generated sample is for
 * @param sample         Pointer to location to store generated sample
 *
 * @return 0 if there are more samples remaining for the provided note stream,
             1 if no more samples, -1 if an error occurred
 */
static int _generate_channel_sample(ptttl_sample_generator_t *generator, ptttl_note_stream_t *stream,
                                    unsigned int channel_idx, float *sample)
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

        if ((0u != stream->vibrato_frequency) || (0u != stream->vibrato_variance))
        {
            float vsine = _generate_waveform_point(_sine_generator, generator->config.sample_rate,
                                                   stream->vibrato_frequency, stream->sine_index);
            float pitch_change_hz = ((float) stream->vibrato_variance) * vsine;
            float note_pitch_hz = stream->pitch_hz + pitch_change_hz;

            float vsample = stream->wgen(stream->phasor_state, note_pitch_hz, generator->config.sample_rate);

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
            raw_sample = _generate_waveform_sample(stream->wgen, generator->config.sample_rate,
                                                   stream->pitch_hz, stream->sine_index);
        }

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
        // Load the next note for this channel
        ptttl_output_note_t note;
        ret = ptttl_parse_next(generator->parser, channel_idx, &note);
        if (ret == 0)
        {
            _load_note_stream(generator, &note, &generator->note_streams[channel_idx]);
        }
    }

    return ret;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_set_waveform(ptttl_sample_generator_t *generator,
                                        uint32_t channel, ptttl_waveform_type_e type)
{
    ASSERT(NULL != generator);

    if (channel >= generator->parser->channel_count)
    {
        ERROR(generator->parser, "Invalid channel index");
        return -1;
    }

    if ((type < 0) || (type >= WAVEFORM_TYPE_COUNT))
    {
        ERROR(generator->parser, "Invalid waveform type");
        return -1;
    }

    generator->note_streams[channel].wgen = _waveform_generators[type];
    return 0;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_set_custom_waveform(ptttl_sample_generator_t *generator,
                                               uint32_t channel, ptttl_waveform_generator_t wgen)
{
    ASSERT(NULL != generator);
    ASSERT(NULL != wgen);

    if (channel >= generator->parser->channel_count)
    {
        ERROR(generator->parser, "Invalid channel index");
        return -1;
    }

    generator->note_streams[channel].wgen = wgen;
    return 0;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_generate(ptttl_sample_generator_t *generator, uint32_t *num_samples,
                                    int16_t *samples)
{
    ASSERT(NULL != generator);
    ASSERT(NULL != num_samples);
    ASSERT(NULL != samples);

    uint32_t samples_to_generate = *num_samples;
    *num_samples = 0u;

    for (unsigned int samplenum = 0u; samplenum < samples_to_generate; samplenum++)
    {
        float summed_sample = 0.0f;
        unsigned int num_channels_provided = 0u;

        // Sum the current state of all channels to generate the next sample
        for (unsigned int chan = 0u; chan < generator->parser->channel_count; chan++)
        {
            if (1u == generator->channel_finished[chan])
            {
                // No more samples to generate for this channel
                continue;
            }

            num_channels_provided += 1u;
            ptttl_note_stream_t *stream = &generator->note_streams[chan];

            float chan_sample = 0.0f;
            int ret = _generate_channel_sample(generator, stream, chan, &chan_sample);
            if (ret < 0)
            {
                return ret;
            }

            generator->channel_finished[chan] = ret;

            summed_sample += chan_sample;
        }

        if (num_channels_provided == 0u)
        {
            // Finished-- no samples left on any channel
            return 1;
        }

        generator->current_sample += 1u;
        samples[samplenum] = (int16_t) (summed_sample / (float) generator->parser->channel_count);
        *num_samples += 1u;
    }

    return 0;
}
