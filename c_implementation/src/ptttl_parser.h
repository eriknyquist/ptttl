/*! \mainpage
 *
 * See \link ptttl_parser.h API documentation for ptttl_parser.h \endlink
 *
 * See \link ptttl_sample_generator.h API documentation for ptttl_sample_generator.h \endlink
 *
 * See \link ptttl_to_wav.h API documentation for ptttl_to_wav.h \endlink
 */

/**
 * @file ptttl_parser.h
 *
 * @brief Parser for RTTTL (Ring Tone Text Transfer Language) and PTTTL (Polyphonic Tone
 * Text Transfer Language, superset of RTTTL which supports polyphony)
 *
 * Converts a PTTTL or RTTTL source file into a stream of ptttl_output_note_t objects,
 * which is an intermediate representation that can be processed by ptttl_sample_generator.c
 * to obtain PCM audio samples.
 *
 * Requires stdint.h, strtoul() from stdlib.h, and memset() from string.h.
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * @author Erik K. Nyquist
 */

#ifndef PTTTL_PARSER_H
#define PTTTL_PARSER_H


#include <stdint.h>


#ifdef __cplusplus
    extern "C" {
#endif

#define PTTTL_VERSION "v0.2.0"

/**
 * Maximum number of channels (note lanes) allowed in a single PTTTL file. This
 * setting will significantly affect the size of the ptttl_parser_t and ptttl_sample_generator_t
 * structs-- more channels makes them larger.
 */
#ifndef PTTTL_MAX_CHANNELS_PER_FILE
#define PTTTL_MAX_CHANNELS_PER_FILE  (16u)
#endif // PTTTL_MAX_CHANNELS_PER_FILE


/**
 * Maximum size of the name (first colon-separated field in a PTTTL or RTTTL file).
 * This name is stored directly in the ptttl_parser_t struct, so changing this size
 * directly affects the size of the ptttl_parser_t struct.
 */
#ifndef PTTTL_MAX_NAME_LEN
#define PTTTL_MAX_NAME_LEN           (256u)
#endif // PTTTL_MAX_NAME_LEN


// Read vibrato frequency from vibrato settings
#define PTTTL_NOTE_VIBRATO_FREQ(note) (((note)->vibrato_settings) & 0xffffu)

// Read vibrato variance from vibrato settings
#define PTTTL_NOTE_VIBRATO_VAR(note)  ((((note)->vibrato_settings) >> 16u) & 0xffffu)

// Read the musical note value from note settings
#define PTTTL_NOTE_VALUE(note) (((note)->note_settings) & 0x7fu)

// Read the note duration from note settings
#define PTTTL_NOTE_DURATION(note) ((((note)->note_settings) >> 7u) & 0xffffu)


/**
 * Represents a single "compiled" note: pitch, duration & vibrato data
 */
typedef struct
{
    /**
     * Bits 0-6   : Note value. Valid values are 0 through 88, with 1 through 88 representing
     *              the 88 keys on a piano keyboard (1 is the lowest note and 88 is the highest
     *              note), and 0 representing a pause/rest.
     *
     * Bits 7-22  : Note duration in milliseconds. Valid values are 0x0 through 0xffff.
     *
     * Bits 23-31 : Unused
     */
    uint32_t note_settings;

    /**
     * Bits 0-15  : Vibrato frequency in Hz
     *
     * Bits 16-31 : Vibrato maximum +/- variance from the main pitch, in Hz
     */
    uint32_t vibrato_settings;
} ptttl_output_note_t;


/**
 * Tracks current position in input text for a single PTTTL channel
 */
typedef struct
{
    uint32_t position;       ///< Current position in input text stream
    uint32_t line;           ///< Current line number in input text
    uint32_t column;         ///< Current column number in input text
    uint32_t block;          ///< Current block number, starting from 0
    uint8_t have_saved_char; ///< 1 if a character has been read but not yet used
    char saved_char;         ///< Unused character
} ptttl_parser_input_stream_t;


/**
 * Holds function pointers that make up an interface for reading PTTTL source
 * from various locations (e.g. from memory, or from a file)
 */
typedef struct
{
    /**
     * Callback function to fetch the next character of PTTTL/RTTTL source
     *
     * @param input_char   Pointer to location to store fetched PTTTL/RTTTL source character
     *
     * @return 0 if successful, 1 if no more characters remain, and -1 if an error
     *         occurred (causes parsing to halt early)
     */
    int (*read)(char *input_char);

    /**
     * Callback function to seek to an absolute position within the input text
     *
     * @param position   0-based position to seek to. For example, if the position is 0,
     *                   then the next 'read' call should return the first character of the
     *                   input text, and if the position is 23, then the next 'read' call
     *                   should return the 24th character of the input text, and so on.
     *
     * @return 0 if successful, 1 if an invalid position was provided, and -1 if an error
     *         occurred (causes parsing to halt early)
     */
    int (*seek)(uint32_t position);

} ptttl_parser_input_iface_t;


/**
 * Holds information about a failure to parse input PTTTL/RTTTL text
 */
typedef struct
{
    const char *error_message;  ///< Human-readable error message
    int line;                   ///< Line number within input text
    int column;                 ///< Column number within input text
} ptttl_parser_error_t;


/**
 * Tracks current position in input text for all channels
 */
typedef struct
{
    char name[PTTTL_MAX_NAME_LEN];              ///< Name from the "settings" section
    unsigned int bpm;                           ///< BPM from the "settings" section
    unsigned int default_duration;              ///< Default note duration from the "settings" section
    unsigned int default_octave;                ///< Default octave from the "settings" section
    unsigned int default_vibrato_freq;          ///< Default vibrato frequency from the "settings" section
    unsigned int default_vibrato_var;           ///< Default vibrato variance from the "settings" section
    ptttl_parser_error_t error;                 ///< Last parsing error that occurred
    uint32_t channel_count;                     ///< Total number of channels present in input text
    ptttl_parser_input_stream_t *active_stream; ///< Input stream currently being parsed
    ptttl_parser_input_stream_t stream;         ///< Input stream used for 'settings' section
    ptttl_parser_input_stream_t channels[PTTTL_MAX_CHANNELS_PER_FILE];
    ptttl_parser_input_iface_t iface;           ///< Input interface for reading PTTTL source
} ptttl_parser_t;



/**
 * Return error info after #ptttl_parse_init or #ptttl_parse_next has returned -1
 *
 * @return  Object describing the error that occurred. error_message field will be NULL
 *          if no error has occurred.
 */
ptttl_parser_error_t ptttl_parser_error(ptttl_parser_t *state);


/**
 * Initializes the PTTTL parser for a particular PTTTL/RTTTL input text. Must be called
 * once before attempting to parse a PTTTL/RTTTL input text with #ptttl_parse_next.
 *
 * @param parser  Pointer to parser object to initialize
 * @param iface   Input interface for reading PTTTL/RTTTL source text
 *
 * @return  0 if successful, -1 otherwise. If -1, use #ptttl_parser_error
 *          to get detailed error information.
 */
int ptttl_parse_init(ptttl_parser_t *parser, ptttl_parser_input_iface_t iface);


/**
 * Read PTTTL/RTTTL source text for the next note of the specified channel, and produce
 * an intermediate representation of the note that can be used to generate audio data.
 *
 * @param parser       Pointer to initialized parser object
 * @param channel_idx  Channel number to get next note for. Channel numbers are in the same order
 *                     that the channel occurs in the PTTTL/RTTTL source text, starting from 0.
 * @param note         Pointer to location to store intermediate representation of PTTTL/RTTTL note
 *
 * @return  0 if successful, 1 if there are no more notes available for the given channel,
 *          and -1 if parsing error occurred. If -1, use #ptttl_parser_error to get detailed
 *          error information.
 */
int ptttl_parse_next(ptttl_parser_t *parser, uint32_t channel_idx, ptttl_output_note_t *note);

#ifdef __cplusplus
    }
#endif

#endif // PTTTL_PARSER_H
