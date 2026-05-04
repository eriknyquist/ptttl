/** 
 * @file ptttl_sample_generator.h
 *
 * @brief Converts the output of ptttl_parse_next() into a stream of signed 16-bit audio samples
 * suitable for a WAV file. Samples can be obtained one at a time, at your leisure.
 *
 * Requires ptttl_parser.c
 *
 * Requires stdint.h and memset() from string.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * @author Erik Nyquist
 */

#ifndef PTTTL_SAMPLE_GENERATOR_H
#define PTTTL_SAMPLE_GENERATOR_H


#include <stdint.h>
#include "ptttl_parser.h"


#ifdef __cplusplus
    extern "C" {
#endif


/**
 * Number of harmonics to use when generating the square, sawtooth & triangle waveforms.
 * Higher numbers generate higher quality waves, at the cost of increased sample
 * generation time.
 */
#ifndef PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS
#define PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS 16
#endif // PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS

/**
 * ptttl_sample_generator_config_t object initialization with sane defaults
 */
#define PTTTL_SAMPLE_GENERATOR_CONFIG_DEFAULT {.sample_rate=44100u, .attack_samples=100u, \
                                               .decay_samples=500u, .amplitude=0.8f}


/**
 * Enumerates all available built-in waveform types
 */
typedef enum
{
    WAVEFORM_TYPE_SINE = 0,  ///< Generates a sine wave
    WAVEFORM_TYPE_TRIANGLE,  ///< Generates a triangle wave
    WAVEFORM_TYPE_SAWTOOTH,  ///< Generates a sawtooth wave
    WAVEFORM_TYPE_SQUARE,    ///< Generates a square wave
    WAVEFORM_TYPE_NOKIA,     ///< Mimics the sound of a Nokia 3310 phone
    WAVEFORM_TYPE_COUNT
} ptttl_waveform_type_e;


/**
 * Callback function for a waveform generator
 *
 * @param x        Phase; current position on the waveform, in turns (0.0 through 1.0)
 * @param p        Wave frequency in Hz
 * @param s        Sampling rate in Hz
 * @param wgendata Pointer to waveform-specific data set up by ptttl_waveform_note_setup_t,
 *                 may be NULL if no setup function is provided
 *
 * @return Value for the given position, in the range -1.0 through 1.0
 */
typedef float (*ptttl_waveform_generator_t)(float x, float p, unsigned int s, void *wgendata);

/**
 * Optional per-note setup callback for a waveform generator.
 *
 * Called once each time a new note starts. The implementation should write any
 * note-specific data it needs (e.g. precomputed harmonic coefficients) into
 * the wgendata buffer pointed to by the wgendata parameter, which is backed by
 * ptttl_note_stream_t.wgendata_storage. The same pointer is passed as wgendata
 * to the ptttl_waveform_generator_t on every subsequent sample for this note.
 *
 * May be NULL if the waveform generator requires no per-note setup.
 *
 * @param channel_idx  Channel number the note belongs to. Channel numbers are in
 *                     the same order that the channel occurs in the PTTTL/RTTTL
 *                     source text, starting from 0.
 * @param note_number  Piano key number of the new note (1-88), or 0 for a rest
 * @param sample_rate  Sample rate in Hz
 * @param wgendata     Pointer to ptttl_note_stream_t.wgendata_storage; write
 *                     note-specific data here
 */
typedef void (*ptttl_waveform_note_setup_t)(uint32_t channel_idx,
                                            uint8_t note_number,
                                            unsigned int sample_rate,
                                            void *wgendata);

/**
 * Pairs a waveform generator function with an optional per-note setup callback.
 * Used with ptttl_sample_generator_set_custom_waveform to define a fully custom
 * waveform including any note-specific precomputation.
 */
typedef struct
{
    ptttl_waveform_generator_t  wgen;        ///< Per-sample waveform function (must not be NULL)
    ptttl_waveform_note_setup_t note_setup;  ///< Per-note setup callback, may be NULL
    /**
     * Pointer to waveform-specific data, will be passed to the note_setup and wgen
     * callbacks, may be NULL
     */
    void *wgendata;
} ptttl_waveform_t;

/**
 * Represents the current note that samples are being generated for on any one channel
 */
typedef struct
{
    ptttl_waveform_t waveform;       ///< Waveform generator functions
    uint32_t vibrato_frequency;      ///< Vibrato frequency, in HZ
    uint32_t vibrato_variance;       ///< Vibrato variance, in HZ
    unsigned int sine_index;         ///< Monotonically increasing index for sinf() function, note pitch
    unsigned int start_sample;       ///< The sample index on which this note started
    unsigned int num_samples;        ///< Number of samples this note runs for
    unsigned int attack;             ///< Note attack length, in samples
    unsigned int decay;              ///< Note decay length, in samples
    unsigned int note_number;        ///< Piano key number for this note, 1-88
    float pitch_hz;                  ///< Note pitch in Hz
    float phasor_state;              ///< Phasor state for vibrato (frequency modulation)
    float sample_error;              ///< Accumulated sample count error for this note
} ptttl_note_stream_t;

/**
 * Holds configurable parameters for sample generation
 */
typedef struct
{
    unsigned int sample_rate;     ///< Sampling rate in samples per second (Hz)
    unsigned int attack_samples;  ///< no. of samples to ramp from 0 to full volume, at note start
    unsigned int decay_samples;   ///< no. of samples to ramp from full volume to 0, at note end
    float amplitude;              ///< Amplitude of generated samples between 0.0-1.0, with 1.0 being full volume
} ptttl_sample_generator_config_t;

/**
 * Represents a sample generator instance created for a specific PTTTL/RTTTL source text
 */
typedef struct
{
    unsigned int current_sample;
    ptttl_note_stream_t note_streams[PTTTL_MAX_CHANNELS_PER_FILE];
    uint8_t channel_finished[PTTTL_MAX_CHANNELS_PER_FILE];
    ptttl_sample_generator_config_t config;
    ptttl_parser_t *parser;
} ptttl_sample_generator_t;


/**
 * Initialize a sample generator instance for a specific PTTTL/RTTTL source text
 *
 * @param parser         Pointer to initialized PTTTL parser object
 * @param generator      Pointer to generator instance to initialize
 * @param config         Pointer to sample generator configuration data
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_parser_error
 *         for an error description if -1 is returned.
 */
int ptttl_sample_generator_create(ptttl_parser_t *parser, ptttl_sample_generator_t *generator,
                                  ptttl_sample_generator_config_t *config);

/**
 * Set the built-in waveform type for a specific channel of the sample generator
 *
 * @param generator    Pointer to initialized generator object
 * @param channel      Channel index to set the waveform type for. Channel index
 *                     is 0-based, and channels are indexed in the order in which they appear
 *                     in the PTTTL source text. For example, channel 0 is the first channel
 *                     that appears in the source text, channel 1 is the second channel, and so on.
 * @param type         Built-in waveform type to generate for the specified channel.
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_parser_error
 *         for an error description if -1 is returned.
 */
int ptttl_sample_generator_set_waveform(ptttl_sample_generator_t *generator,
                                        uint32_t channel, ptttl_waveform_type_e type);

/**
 * Set a fully custom waveform for a specific channel, including an optional per-note
 * setup callback. The waveform struct pairs a generator function with an optional
 * note_setup callback for precomputing per-note data.
 *
 * @param generator    Pointer to initialized generator object
 * @param channel      Channel index (0-based)
 * @param waveform     Pointer to waveform definition struct
 *
 * @return 0 if successful, -1 if an error occurred.
 */
int ptttl_sample_generator_set_custom_waveform(ptttl_sample_generator_t *generator,
                                               uint32_t channel,
                                               const ptttl_waveform_t *waveform);

/**
 * Generate the next audio sample(s) for an initialized generator object
 *
 * @param generator        Pointer to initialized generator object
 * @param num_samples      Pointer to number of samples to generate. If successful,
 *                         then this pointer is re-used to write out the actual number
 *                         of samples generated.
 * @param samples          Pointer to location to store sample values. The caller is
 *                         expected to provide at least (sizeof(int16_t) * num_samples)
 *                         bytes of storage for the generated samples.
 *
 * @return 0 if successful, 1 if all samples have been generated, and -1 if an error occurred.
 *         Call #ptttl_parser_error for an error description if -1 is returned.
 */
int ptttl_sample_generator_generate(ptttl_sample_generator_t *generator,
                                    uint32_t *num_samples, int16_t *samples);


#ifdef __cplusplus
    }
#endif

#endif // PTTTL_SAMPLE_GENERATOR_H
