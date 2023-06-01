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
} ptttl_sample_generator_t;


const char *ptttl_to_wav_error(void);

int ptttl_sample_generator_create(ptttl_output_t *parsed_ptttl, ptttl_sample_generator_t *generator);

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
                                    int32_t *sample);

int ptttl_source_to_wav(ptttl_output_t *parsed_ptttl, const char *wav_filename);

#endif // PTTTL_TO_WAV_H
