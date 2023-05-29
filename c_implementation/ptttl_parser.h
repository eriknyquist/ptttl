/* pttl_parser.c
 *
 * PTTTL & RTTTL parser implemented in C. No dynamic memory allocation, and minimal
 * dependencies (requires strtoul() from stdlib.h, and memset() from string.h).
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#ifndef PTTTL_PARSER_H
#define PTTTL_PARSER_H

#define PTTTL_MAX_CHANNELS_PER_FILE (8u)   // Max. channels allowed in a single PTTTL file
#define PTTTL_MAX_NOTES_PER_CHANNEL (512)  // Max. notes allowed in a single PTTTL channel
#define PTTTL_MAX_NAME_LEN          (256u)  // Max. characters allowed in the "name" field of a PTTTL file

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
 * Represents a single note, pitch & duration
 */
typedef struct
{
    float pitch_hz;        ///< Note pitch in Hz (0.0f for pause)
    float duration_secs;   ///< Note duration in seconds
    float vibrato_freq_hz; ///< Vibrato frequency in Hz (0.0f for no vibrato)
    float vibrato_var_hz;  ///< Vibrato +/-variance from main pitch, in Hz
} note_t;

/**
 * Represents a single channel (sequence of notes)
 */
typedef struct
{
    unsigned int note_count;                    ///< Number of notes populated
    note_t notes[PTTTL_MAX_NOTES_PER_CHANNEL];  ///< Array of notes for this channel
} channel_t;

/**
 * Holds processed PTTTL data as a sequence of channel_t objects
 */
typedef struct
{
    char name[PTTTL_MAX_NAME_LEN];                    ///< Name field of PTTTL file
    unsigned int channel_count;                       ///< Number of channels populated
    channel_t channels[PTTTL_MAX_CHANNELS_PER_FILE];  ///< Array of channels
} ptttl_output_t;

/**
 * Holds information about a failure to parse input PTTTL text
 */
typedef struct
{
    const char *error_message;  ///< Human-readable error message
    int line;                   ///< Line number within input text
    int column;                 ///< Column number within input text
} ptttl_error_t;

/**
 * Get pointer to error message after ptttl_parse has returned -1
 *
 * @return  Object describing the error that occurred
 */
ptttl_error_t ptttl_error(void);

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
