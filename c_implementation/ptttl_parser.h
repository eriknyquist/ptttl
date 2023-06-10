/* ptttl_parser.h
 *
 * Parser for RTTTL (Ring Tone Text Transfer Language) and PTTTL (Polyphonic Tone
 * Text Transfer Language, superset of RTTTL which supports polyphony)
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

#include "ptttl_config.h"


#ifdef __cplusplus
    extern "C" {
#endif


#if PTTTL_VIBRATO_ENABLED
// Read vibrato frequency from vibrato settings
#define PTTTL_NOTE_VIBRATO_FREQ(note) (((note)->vibrato_settings) & 0xffffu)

// Read vibrato variance from vibrato settings
#define PTTTL_NOTE_VIBRATO_VAR(note)  ((((note)->vibrato_settings) >> 16u) & 0xffffu)
#endif // PTTTL_VIBRATO_ENABLED


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
 * Callback function to fetch the next character of PTTTL source
 *
 * @param input_char   Pointer to location to store fetched PTTTL source character
 *
 * @return 0 if successful, 1 if no more characters remain, and -1 if an error
 *         occurred (causes parsing to halt early)
 */
typedef int (*ptttl_parser_readchar_t)(char *input_char);


/**
 * Get pointer to error message after ptttl_parse has returned -1
 *
 * @return  Object describing the error that occurred
 */
ptttl_parser_error_t ptttl_parser_error(void);


/**
 * Convert a ptttl_input_t struct to a ptttl_output_t struct
 *
 * @param input              Pointer to input data
 * @param readchar           Callback function to read the next PTTTL source character
 *
 * @return  0 if successful, -1 otherwise. If -1, use #ptttl_error_message
 *          to get a more detailed error message.
 */
int ptttl_parse(ptttl_parser_readchar_t readchar, ptttl_output_t *output);


#ifdef __cplusplus
    }
#endif

#endif // PTTTL_PARSER_H
