/* pttl_to_wav.h
 *
 * Converts the output of ptttl_parse() into samples suitable for a WAV file.
 * No dynamic memory allocation, and no need to have enough memory to hold the
 * entire WAV file in memory.
 *
 * Requires stdint.h, fopen/fseek/fwrite from stdio.h, memset() from string.h, and sinf from math.h.
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#ifndef PTTTL_TO_WAV_H
#define PTTTL_TO_WAV_H

#include <stdint.h>
#include "ptttl_parser.h"

/**
 * Represents the current note that samples are being generated for on any one channel
 */
typedef struct
{
    unsigned int sine_index;    // Monotonically increasing index for sinf() function
    unsigned int start_sample;  // The sample index on which this note started
    unsigned int num_samples;   // Number of samples this note runs for
    unsigned int note_index;    // Index of this note within the ptttl_channel_t->notes array
} ptttl_note_stream_t;

/**
 * Represents a sample generator instance created for a specific ptttl_output_t instance
 */
typedef struct
{
    unsigned int current_sample;
    ptttl_note_stream_t note_streams[PTTTL_MAX_CHANNELS_PER_FILE];
    uint8_t channel_finished[PTTTL_MAX_CHANNELS_PER_FILE];
    int32_t sample_rate;
} ptttl_sample_generator_t;


/**
 * Return pointer to a string describing the last error that occurred
 *
 * @return Pointer to error string, or NULL if no error occurred
 */
const char *ptttl_to_wav_error(void);

/**
 * Initialize a sample generator instance for a specific ptttl_output_t object
 *
 * @param parsed_ptttl   Pointer to parsed PTTTL data
 * @param generator      Pointer to generator instance to initialize
 * @param sample_rate    Sample rate, samples per second
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_to_wav_error for
 *         an error description if -1 is returned.
 */
int ptttl_sample_generator_create(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                  int32_t sample_rate);

/**
 * Generate the next audio sample for some parsed PTTTL data
 *
 * @param parsed_ptttl   Pointer to parsed PTTTL data
 * @param generator      Pointer to initialized generator object
 * @param sample         Pointer to location to store sample value
 *
 * @return 0 if successful, 1, if all samples have been generated, and -1 if an error occurred.
           Call #ptttl_to_wav_error for an error description if -1 is returned.
 */
int ptttl_sample_generator_generate(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator,
                                    int16_t *sample);

/**
 * Generate samples for some parsed PTTTL data and write them directly to a .wav file.
 * No dynamic memory allocation. Does not require holding the entire .wav file in memory
 * at once.
 *
 * @param parsed_ptttl   Pointer to parsed PTTTL data
 * @param wav_filename   Pointer to name of .wav file to create. Must be writeable.
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_to_wav_error for
 *         an error description if -1 is returned.
 */
int ptttl_to_wav(ptttl_output_t *parsed_ptttl, const char *wav_filename);

#endif // PTTTL_TO_WAV_H
