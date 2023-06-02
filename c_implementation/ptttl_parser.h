/* ptttl_parser.h
 *
 * Parser for RTTTL (Ring Tone Text Transfer Language) and PTTTL (Polyphonic Tone
 * Text Transfer Language)
 *
 * Converts a PTTTL or RTTTL source file into a ptttl_output_t object, which is
 * an intermediate representation that can be processed by ptttl_sample_generator.c
 * to obtain the audio samples directly, or ptttl_to_wav.c to create a .wav file.
 *
 * Requires stdint.h, strtoul() from stdlib.h, and memset() from string.h.
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#ifndef PTTTL_PARSER_H
#define PTTTL_PARSER_H

#include <stdint.h>


#define PTTTL_MAX_CHANNELS_PER_FILE (8u)    // Max. channels allowed in a single PTTTL file
#define PTTTL_MAX_NOTES_PER_CHANNEL (128u)  // Max. notes allowed in a single PTTTL channel
#define PTTTL_MAX_NAME_LEN          (256u)  // Max. characters allowed in the "name" field of a PTTTL file


#ifndef PTTTL_VIBRATO_ENABLED
#define PTTTL_VIBRATO_ENABLED (0u)          // If 1, vibrato settings for each note will be included in ptttl_output_t
#endif // PTTTL_VIBRATO_ENABLED


/**
 * Represents PTTTL source loaded fully into memory from a single file
 */
typedef struct
{
    char *input_text;        ///< Pointer to PTTTL text
    size_t input_text_size;  ///< Size of PTTTL text (excluding NULL terminator, if any)
    int line;                ///< Current line count
    int column;              ///< Current column count
    int pos;                 ///< Current position in input text
} ptttl_input_t;

/**
 * Represents a single "compiled" note: pitch, duration & vibrato data
 */
typedef struct
{
    float pitch_hz;             ///< Note pitch in Hz (0.0f for pause)
    float duration_secs;        ///< Note duration in seconds
#if PTTTL_VIBRATO_ENABLED
    uint32_t vibrato_settings;  ///< Bits 0-15 is vibrato frequency, and 16-31 is variance, both in Hz
#endif // PTTTL_VIBRATO_ENABLED
} ptttl_output_note_t;

/**
 * Represents a single channel (sequence of notes)
 */
typedef struct
{
    unsigned int note_count;                    ///< Number of notes populated
    ptttl_output_note_t notes[PTTTL_MAX_NOTES_PER_CHANNEL];  ///< Array of notes for this channel
} ptttl_output_channel_t;

/**
 * Holds processed PTTTL data as a sequence of channel_t objects
 */
typedef struct
{
    char name[PTTTL_MAX_NAME_LEN];                    ///< Name field of PTTTL file
    unsigned int channel_count;                       ///< Number of channels populated
    ptttl_output_channel_t channels[PTTTL_MAX_CHANNELS_PER_FILE];  ///< Array of channels
} ptttl_output_t;

/**
 * Holds information about a failure to parse input PTTTL text
 */
typedef struct
{
    const char *error_message;  ///< Human-readable error message
    int line;                   ///< Line number within input text
    int column;                 ///< Column number within input text
} ptttl_parser_error_t;

/**
 * Get pointer to error message after ptttl_parse has returned -1
 *
 * @return  Object describing the error that occurred
 */
ptttl_parser_error_t ptttl_parser_error(void);

/**
 * Convert a ptttl_input_t struct to a ptttl_output_t struct
 *
 * @param input   Pointer to input data
 * @param output  Pointer to location to store output data
 *
 * @return  0 if successful, -1 otherwise. If -1, use #ptttl_error_message
 *          to get a more detailed error message.
 */
int ptttl_parse(ptttl_input_t *input, ptttl_output_t *output);

#endif // PTTTL_PARSER_H
