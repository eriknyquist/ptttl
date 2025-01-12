/* ptttl_parser.c
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

#include <stdlib.h>
#include <string.h>
#include "ptttl_parser.h"
#include "ptttl_common.h"


// Helper macro, stores information about an error, which can be retrieved by ptttl_parser_error()
#define ERROR(_parser, _msg)                                \
{                                                           \
    _parser->error.error_message = _msg;                    \
    _parser->error.line = _parser->active_stream->line;     \
    _parser->error.column = _parser->active_stream->column; \
}

// Helper macro, checks if a character is whitespace
#define IS_WHITESPACE(c) (((c) == '\t') || ((c) == ' ') || ((c) == '\v') ||  \
                          ((c) == '\n') || ((c) == '\r') || ((c) == '\f'))

// Helper macro, checks if a character is a digit
#define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

#define CHECK_SHARPONLY(string, size, notechar, val, sharpval) \
{                                                              \
    if (notechar == string[0])                                 \
    {                                                          \
        if (size > 1)                                          \
        {                                                      \
            if ('#' == string[1])                              \
            {                                                  \
                return sharpval;                               \
            }                                                  \
            else                                               \
            {                                                  \
                return NOTE_INVALID;                           \
            }                                                  \
        }                                                      \
        else                                                   \
        {                                                      \
            return val;                                        \
        }                                                      \
    }                                                          \
}

#define CHECK_FLATONLY(string, size, notechar, val, flatval)  \
{                                                             \
    if (notechar == string[0])                                \
    {                                                         \
        if (size > 1)                                         \
        {                                                     \
            if ('b' == string[1])                             \
            {                                                 \
                return flatval;                               \
            }                                                 \
            else                                              \
            {                                                 \
                return NOTE_INVALID;                          \
            }                                                 \
        }                                                     \
        else                                                  \
        {                                                     \
            return val;                                       \
        }                                                     \
    }                                                         \
}

#define CHECK_SHARPFLAT(string, size, notechar, val, sharpval, flatval) \
{                                                                       \
    if (notechar == string[0])                                          \
    {                                                                   \
        if (size > 1)                                                   \
        {                                                               \
            if ('#' == string[1])                                       \
            {                                                           \
                return sharpval;                                        \
            }                                                           \
            else if ('b' == string[1])                                  \
            {                                                           \
                return flatval;                                         \
            }                                                           \
            else                                                        \
            {                                                           \
                return NOTE_INVALID;                                    \
            }                                                           \
        }                                                               \
        else                                                            \
        {                                                               \
            return val;                                                 \
        }                                                               \
    }                                                                   \
}

// Check iface function return value, EOF indicates error
#define CHECK_IFACE_RET(_parser, _retval)                \
{                                                        \
    if (1 == _retval)                                    \
    {                                                    \
        ERROR(_parser, "Unexpected EOF encountered");    \
        return -1;                                       \
    }                                                    \
    else if (0 > _retval)                                \
    {                                                    \
        ERROR(_parser, "interface callback returned -1");\
        return -1;                                       \
    }                                                    \
}

// Check iface function return value, EOF is allowed
#define CHECK_IFACE_RET_EOF(_parser, _retval)            \
{                                                        \
    if (1 == _retval)                                    \
    {                                                    \
        return 1;                                        \
    }                                                    \
    else if (0 > _retval)                                \
    {                                                    \
        ERROR(_parser, "interface callback returned -1");\
        return -1;                                       \
    }                                                    \
}

#define ADVANCE_LINE_COLUMN(_parser, _char)  \
{                                            \
    if ('\n' == _char)                       \
    {                                        \
        _parser->active_stream->line += 1;   \
        _parser->active_stream->column = 1;  \
    }                                        \
    else                                     \
    {                                        \
        _parser->active_stream->column += 1; \
    }                                        \
}

#define SAVE_CHAR(_parser, _nextchar)               \
{                                                   \
    _parser->active_stream->saved_char = _nextchar; \
    _parser->active_stream->have_saved_char = 1u;   \
}


// Set vibrato settings for a ptttl_output_note_t instance
#define SET_VIBRATO(note, freq, var) ((note)->vibrato_settings = ((freq) & 0xffffu) | (((var) & 0xffffu) << 16u))

// Set note settings for a ptttl_output_note_t instance
#define SET_NOTE(note, value, duration) ((note)->note_settings = ((value) & 0x7fu) | (((duration) & 0xffffu) << 7u))


// Valid values for note duration
static const unsigned int _valid_note_durations[NOTE_DURATION_COUNT] = {1u, 2u, 4u, 8u, 16u, 32u};


/**
 * Convert note_pitch_e and octave number to a piano key number from 1 through 88.
 *
 * @param parser   Pointer to parser object
 * @param note     Note pitch enum (assumed to already be validated)
 * @param octave   Octave number (assumed to already be validated)
 * @param output   Pointer to location to store output
 *
 * @return 0 if successful, -1 if an error occurred
 */
static int _note_name_to_number(ptttl_parser_t *parser, note_pitch_e note, unsigned int octave, uint32_t *output)
{
    uint32_t result = 0u;

    if (octave == 0u)
    {
        if (note < NOTE_A)
        {
            ERROR(parser, "Invalid musical note for octave 0");
            return -1;
        }

        result = (uint32_t) (note - NOTE_A);
    }
    else
    {
        result = (uint32_t) (_octave_starts[octave] + (uint32_t) note);
    }

    *output = result + 1u;
    return 0;
}

/**
 * Find the corresponding note_pitch_e value corresponding to a musical note string
 *
 * @param string    Pointer to musical note string
 * @param size      Size of musical note string
 *
 * @return corresponding note_pitch_e value
 */
static note_pitch_e _note_string_to_enum(char *string, int size)
{
    if ((size > 2) || (size < 1))
    {
        return NOTE_INVALID;
    }

    CHECK_SHARPONLY(string, size, 'c', NOTE_C, NOTE_CS);
    CHECK_SHARPFLAT(string, size, 'd', NOTE_D, NOTE_DS, NOTE_DB);
    CHECK_SHARPFLAT(string, size, 'e', NOTE_E, NOTE_ES, NOTE_EB);
    CHECK_SHARPONLY(string, size, 'f', NOTE_F, NOTE_FS);
    CHECK_SHARPFLAT(string, size, 'g', NOTE_G, NOTE_GS, NOTE_GB);
    CHECK_SHARPFLAT(string, size, 'a', NOTE_A, NOTE_AS, NOTE_AB);
    CHECK_FLATONLY(string, size, 'b', NOTE_B, NOTE_BB);

    return NOTE_INVALID;
}

static int _readchar_wrapper(ptttl_parser_t *parser, char *nextchar)
{
    int ret = 0;
    if (1u == parser->active_stream->have_saved_char)
    {
        parser->active_stream->have_saved_char = 0u;
        *nextchar = parser->active_stream->saved_char;
    }
    else
    {
        ret = parser->iface.read(nextchar);
        parser->active_stream->position += 1u;
    }

    return ret;
}

static int _seek_wrapper(ptttl_parser_t *parser, uint32_t position)
{
    int ret = parser->iface.seek(position);
    if (0 == ret)
    {
        parser->active_stream->position = position;
    }

    return ret;
}


/**
 * Starting from the current input position, consume all characters that are not
 * PTTTL source (comments and whitespace), incrementing line and column counters as needed,
 * until a visible PTTTL character or EOF is seen. Input position will be left at the first
 * visible PTTTL source character that is seen.
 *
 * @param parser  Pointer to parser object
 *
 * @return 0 if successful, -1 if an error occurred, and 1 if EOF was seen
 */
static int _eat_all_nonvisible_chars(ptttl_parser_t *parser)
{
    char nextchar = '\0';
    int readchar_ret = 0;
    uint8_t in_comment = 0u;

    while ((readchar_ret = _readchar_wrapper(parser, &nextchar)) == 0)
    {
        if (1u == in_comment)
        {
            if ('\n' == nextchar)
            {
                in_comment = 0u;
            }
            else
            {
                parser->active_stream->column += 1u;
                continue;
            }
        }

        if (IS_WHITESPACE(nextchar))
        {
            ADVANCE_LINE_COLUMN(parser, nextchar);
        }
        else
        {
            if ('#' == nextchar)
            {
                in_comment = 1u;
                parser->active_stream->column += 1u;
            }
            else
            {
                SAVE_CHAR(parser, nextchar);
                return 0;
            }
        }
    }

    return readchar_ret;
}

/**
 * Look for the next non-whitespace character, starting from the current input
 * position, and return it. Input position, line + column count will be incremented
 * accordingly. After this function runs successfully, the input position will be
 * at the character *after* the first non-whitespace character that was found.
 *
 * @param parser  Pointer to parser object
 * @param output  Pointer to location to store next visible character
 *
 * @return 0 if successful, -1 if an error occurred, and 1 if EOF was encountered before non-whitespace char was found
 */
static int _get_next_visible_char(ptttl_parser_t *parser, char *output)
{
    int ret = _eat_all_nonvisible_chars(parser);
    if (ret != 0)
    {
        return ret;
    }

    return _readchar_wrapper(parser, output);
}

/**
 * Parse an unsigned integer from the current input position
 *
 * @param parser       Pointer to parser object
 * @param output       Pointer to location to store parsed integer value
 * @param eof_allowed  if 1, error will not be reported on EOF
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_uint_from_input(ptttl_parser_t *parser, unsigned int *output,
                                  uint8_t eof_allowed)
{
    char buf[32u];
    int pos = 0;
    int readchar_ret = 0;
    char nextchar = '\0';

    while ((readchar_ret = _readchar_wrapper(parser, &nextchar)) == 0)
    {
        if (IS_DIGIT(nextchar))
        {
            if (pos == (sizeof(buf) - 1))
            {
                ERROR(parser, "Integer is too long");
                return -1;
            }

            buf[pos] = nextchar;
            parser->active_stream->column += 1;
            pos += 1;
        }
        else
        {
            SAVE_CHAR(parser, nextchar);
            buf[pos] = '\0';
            break;
        }
    }

    if (eof_allowed)
    {
        CHECK_IFACE_RET_EOF(parser, readchar_ret);
    }
    else
    {
        CHECK_IFACE_RET(parser, readchar_ret);
    }

    if (0 == pos)
    {
        ERROR(parser, "Expected an integer");
        return -1;
    }

    *output = (unsigned int) strtoul(buf, NULL, 0);
    return 0;
}

/**
 * Check if number is a valid note duration
 *
 * @param duration   Note duration value to check
 *
 * @return 1 if note duration is value, 0 otherwise
 */
static unsigned int _valid_note_duration(unsigned int duration)
{
    unsigned int valid = 0u;
    for (unsigned int i = 0u; i < NOTE_DURATION_COUNT; i++)
    {
        if (_valid_note_durations[i] == duration)
        {
            valid = 1u;
            break;
        }
    }

    return valid;
}

/**
 * Parse a single option in the "settings" section, and populate the corresponding
 * field in the provided settings_t object
 *
 * @param opt       Key for the option being set
 * @param parser    Pointer to parser object
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_option(char opt, ptttl_parser_t *parser)
{
    if (':' == opt)
    {
        // End of settings section
        return 0;
    }

    char equals;
    int result = _get_next_visible_char(parser, &equals);
    CHECK_IFACE_RET(parser, result);

    if ('=' != equals)
    {
        ERROR(parser, "Invalid option setting");
        return -1;
    }

    int ret = 0;
    switch (opt)
    {
        case 'b':
            ret = _parse_uint_from_input(parser, &parser->bpm, 0u);
            break;
        case 'd':
        {
            ret = _parse_uint_from_input(parser, &parser->default_duration, 0u);

            if (0 == ret)
            {
                if (!_valid_note_duration(parser->default_duration))
                {
                    ERROR(parser, "Invalid note duration (must be 1, 2, 4, 8, 16 or 32)");
                    return -1;
                }
            }
            break;
        }
        case 'o':
            ret = _parse_uint_from_input(parser, &parser->default_octave, 0u);
            if (0 == ret)
            {
                if (NOTE_OCTAVE_MAX < parser->default_octave)
                {
                    ERROR(parser, "Invalid octave (must be 0 through 8)");
                    return -1;
                }
            }
            break;
        case 'f':
            ret = _parse_uint_from_input(parser, &parser->default_vibrato_freq, 0u);
            break;
        case 'v':
            ret = _parse_uint_from_input(parser, &parser->default_vibrato_var, 0u);
            break;
        default:
            ERROR(parser, "Unrecognized option key");
            return -1;
            break;
    }

    return ret;
}

/**
 * Parse the 'settings' section from the current input position, and populate
 * a settings_t object
 *
 * @param parser  Pointer to parser object
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_settings(ptttl_parser_t *parser)
{
    char c = '\0';

    parser->bpm = 0u;
    parser->default_duration = 8u;
    parser->default_octave = 4u;
    parser->default_vibrato_freq = 7u;
    parser->default_vibrato_var = 10u;

    while (':' != c)
    {
        int result = _get_next_visible_char(parser, &c);
        CHECK_IFACE_RET(parser, result);

        result = _parse_option(c, parser);
        if (result != 0)
        {
            return result;
        }

        // After parsing option, next visible char should be comma or colon, otherwise error
        result = _get_next_visible_char(parser, &c);
        CHECK_IFACE_RET(parser, result);

        if ((',' != c) && (':' != c))
        {
            ERROR(parser, "Invalid settings section");
            return -1;
        }
    }

    return 0;
}

/**
 * Parse musical note character(s) at the current input position, and provide the
 * corresponding note_pitch_e enum
 *
 * @param parser      Pointer to parser object
 * @param note_pitch  Pointer to location to store note pitch enum
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_musical_note(ptttl_parser_t *parser, note_pitch_e *note_pitch)
{
    // Read musical note name, convert to lowercase if needed
    char notebuf[3];
    int notepos = 0;
    int readchar_ret = 0;
    char nextchar = '\0';

    while (notepos < 2)
    {
        if ((readchar_ret = _readchar_wrapper(parser, &nextchar)) != 0)
        {
            break;
        }

        if ((nextchar >= 'A') && (nextchar <= 'Z'))
        {
            notebuf[notepos] = nextchar + ' ';
        }
        else if (((nextchar >= 'a') && (nextchar <= 'z')) || (nextchar == '#'))
        {
            notebuf[notepos] = nextchar;
        }
        else
        {
            SAVE_CHAR(parser, nextchar);
            break;
        }

        notepos += 1;
        parser->active_stream->column += 1u;
    }

    CHECK_IFACE_RET_EOF(parser, readchar_ret);

    if (notepos == 0)
    {
        ERROR(parser, "Expecting a musical note name");
        return -1;
    }

    if ((notepos == 1) && (notebuf[0] == 'p'))
    {
        *note_pitch = NOTE_INVALID;
    }
    else
    {
        note_pitch_e enum_val = _note_string_to_enum(notebuf, notepos);
        if (NOTE_INVALID == enum_val)
        {
            ERROR(parser, "Invalid musical note name");
            return -1;
        }

        *note_pitch = enum_val;
    }

    return 0;
}

/**
 * Parse vibrato settings at the end of a PTTTL note (if any) from the current
 * input position. If there are no vibrato settings at the current input position,
 * return success.
 *
 * @param parser    Pointer to parser object
 * @param output    Pointer to current output note object (contains final vibrato settings)
 *
 * @return 0 if successful, 1 if EOF was encountered, and -1 if an error occurred
 */
static int _parse_note_vibrato(ptttl_parser_t *parser, ptttl_output_note_t *output)
{
    int readchar_ret = 0;
    char nextchar = '\0';

    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    if ('v' != nextchar)
    {
        SAVE_CHAR(parser, nextchar);
        SET_VIBRATO(output, 0, 0);
        return 0;
    }

    parser->active_stream->column += 1;

    SET_VIBRATO(output, parser->default_vibrato_freq, parser->default_vibrato_var);

    uint32_t freq_hz = 0u;
    uint32_t var_hz = 0u;

    // Parse vibrato frequency, if any
    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    SAVE_CHAR(parser, nextchar);
    if (IS_DIGIT(nextchar))
    {
        unsigned int freq = 0u;
        int ret = _parse_uint_from_input(parser, &freq, 1u);
        if (ret != 0)
        {
            return ret;
        }

        freq_hz = (uint32_t) freq;
    }
    else
    {
        return 0;
    }

    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    if ('-' == nextchar)
    {
        parser->active_stream->column += 1;

        unsigned int var = 0u;
        int ret = _parse_uint_from_input(parser, &var, 1u);
        if (ret != 0)
        {
            return ret;
        }

        var_hz = (uint32_t) var;
    }
    else
    {
        SAVE_CHAR(parser, nextchar);
    }

    SET_VIBRATO(output, freq_hz, var_hz);

    return 0;
}

/**
 * Parse a single PTTTL note (duration, musical note, octave, and vibrato settings)
 * from the current input position, and populate a ptttl_output_note_t object.
 *
 * @param parser    Pointer to parser object
 * @param output    Pointer to location to store output ptttl_output_note_t data
 *
 * @return 0 if successful, 1 if EOF was encountered, and -1 if an error occurred
 */
static int _parse_ptttl_note(ptttl_parser_t *parser, ptttl_output_note_t *output)
{
    unsigned int dot_seen = 0u;

    // Read note duration, if it exists
    unsigned int duration = parser->default_duration;

    char nextchar = '\0';
    int readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    SAVE_CHAR(parser, nextchar);

    if (IS_DIGIT(nextchar))
    {
        int ret = _parse_uint_from_input(parser, &duration, 0u);
        if (ret != 0)
        {
            return ret;
        }

        if (!_valid_note_duration(duration))
        {
            ERROR(parser, "Invalid note duration (must be 1, 2, 4, 8, 16 or 32)");
            return -1;
        }
    }

    note_pitch_e note_pitch = NOTE_C;
    int ret = _parse_musical_note(parser, &note_pitch);
    if (ret != 0)
    {
        return ret;
    }

    // Check for dot after note letter
    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    if ('.' == nextchar)
    {
        dot_seen = 1u;
        parser->active_stream->column += 1u;
    }
    else
    {
        SAVE_CHAR(parser, nextchar);
    }

    // Read octave, if it exists
    unsigned int octave = parser->default_octave;
    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    if (IS_DIGIT(nextchar))
    {
        octave = ((unsigned int) nextchar) - 48u;
        if (NOTE_OCTAVE_MAX < octave)
        {
            ERROR(parser, "Invalid octave (must be 0 through 8)");
            return -1;
        }

        parser->active_stream->column += 1;
    }
    else
    {
        SAVE_CHAR(parser, nextchar);
    }

    // Check for dot again after octave
    readchar_ret = _readchar_wrapper(parser, &nextchar);
    CHECK_IFACE_RET_EOF(parser, readchar_ret);
    if ('.' == nextchar)
    {
        dot_seen = 1u;
        parser->active_stream->column += 1u;
    }
    else
    {
        SAVE_CHAR(parser, nextchar);
    }

    uint32_t note_number = 0u;
    if (NOTE_INVALID != note_pitch)
    {
        ret = _note_name_to_number(parser, note_pitch, octave, &note_number);
        if (ret != 0)
        {
            return ret;
        }
    }

    // Set note time in seconds based on note duration + BPM
    float whole_time = (60.0f / (float) parser->bpm) * 4.0f;
    float duration_secs = whole_time / (float) duration;

    // Handle dotted rhythm
    if (dot_seen == 1u)
    {
        duration_secs += (duration_secs / 2.0f);
    }

    SET_NOTE(output, note_number, (uint32_t) (duration_secs * 1000.0f));

    return _parse_note_vibrato(parser, output);
}


/**
 * @see ptttl_parser.h
 */
ptttl_parser_error_t ptttl_parser_error(ptttl_parser_t *parser)
{
    return parser->error;
}

/**
 * @see ptttl_parser.h
 */
int ptttl_parse_init(ptttl_parser_t *parser, ptttl_parser_input_iface_t iface)
{
    if (NULL == parser)
    {
        return -1;
    }

    parser->stream.line = 1u;
    parser->stream.column = 1u;
    parser->stream.position = 0u;
    parser->stream.have_saved_char = 0u;
    parser->channel_count = 0u;
    parser->error.line = 0;
    parser->error.column = 0;
    parser->error.error_message = NULL;

    if ((NULL == iface.read) || (NULL == iface.seek))
    {
        ERROR(parser, "NULL interface pointer provided");
        return -1;
    }

    parser->iface = iface;
    parser->active_stream = &parser->stream;

    // Read name (first field)
    int ret = _get_next_visible_char(parser, &parser->name[0]);
    CHECK_IFACE_RET(parser, ret);

    unsigned int namepos = 1u;
    int readchar_ret = 0;
    char namechar = '\0';
    while ((readchar_ret = _readchar_wrapper(parser, &namechar)) == 0)
    {
        if (':' == namechar)
        {
            parser->active_stream->column += 1u;
            parser->name[namepos] = '\0';
            break;
        }
        else
        {
            if ((PTTTL_MAX_NAME_LEN - 1u) == namepos)
            {
                ERROR(parser, "Name too long, see PTTTL_MAX_NAME_LEN in ptttl_parser.h");
                return -1;
            }

            ADVANCE_LINE_COLUMN(parser, namechar);

            parser->name[namepos] = namechar;
            namepos += 1u;
        }
    }

    CHECK_IFACE_RET(parser, readchar_ret);

    // Read PTTTL settings, next section after the name
    ret = _parse_settings(parser);
    if (ret != 0)
    {
        return -1;
    }

    // Figure out channel count and starting positions of each channel
    ret = 0;
    uint8_t first_block_finished = 0u;
    while ((0 == ret) && (first_block_finished == 0u))
    {
        ret = _eat_all_nonvisible_chars(parser);
        if (0 == ret)
        {
            ptttl_parser_channel_t *chan = &parser->channels[parser->channel_count];
            chan->stream.position = parser->active_stream->position;
            chan->stream.line = parser->active_stream->line;
            chan->stream.column = parser->active_stream->column;
            chan->stream.have_saved_char = parser->active_stream->have_saved_char;
            chan->stream.saved_char = parser->active_stream->saved_char;
            chan->channel_idx = parser->channel_count;

            parser->channel_count += 1u;

            char nextchar = '\0';
            while ((ret = _get_next_visible_char(parser, &nextchar)) == 0)
            {
                ADVANCE_LINE_COLUMN(parser, nextchar);

                if ('|' == nextchar)
                {
                    if (PTTTL_MAX_CHANNELS_PER_FILE == parser->channel_count)
                    {
                        ERROR(parser, "Exceeded maximum channel count");
                        return -1;
                    }
                    break;
                }
                else if (';' == nextchar)
                {
                    first_block_finished = 1u;
                    break;
                }
            }
        }
    }

    return 0;
}

/**
 * Eat input until we reach the first note of the given channel in the next block
 *
 * @param parser         Pointer to ptttl parser object
 * @param channel_idx    Index of channel to find
 * @param find_semicolon If true, start by eating input until a ';' (block end) is seen
 *
 * @return  0 if next block was found, 1 if end of input was reached, and -1 if an error occurred
 */
static int _jump_to_next_block(ptttl_parser_t *parser, uint32_t channel_idx, uint8_t find_semicolon)
{
    int ret = 0;
    char nextchar = '\0';

    if (1u == find_semicolon)
    {
        while ((ret = _get_next_visible_char(parser, &nextchar)) == 0)
        {
            CHECK_IFACE_RET_EOF(parser, ret);
            if (';' == nextchar)
            {
                break;
            }
        }
    }

    ret = _eat_all_nonvisible_chars(parser);
    CHECK_IFACE_RET_EOF(parser, ret);

    if (0u == channel_idx)
    {
        // If first channel, we're done
        return 0;
    }

    /* Skip 'channel_idx' pipe characters '|' to reach the first
     * note of this channel in the next block */
    for (uint32_t i = 0u; i < channel_idx; i++)
    {
        while ((ret = _get_next_visible_char(parser, &nextchar)) == 0)
        {
            CHECK_IFACE_RET_EOF(parser, ret);
            if ('|' == nextchar)
            {
                break;
            }
        }
    }

    ret = _eat_all_nonvisible_chars(parser);
    CHECK_IFACE_RET_EOF(parser, ret);

    return 0;
}


/**
 * @see ptttl_parser.h
 */
int ptttl_parse_next(ptttl_parser_t *parser, uint32_t channel, ptttl_output_note_t *note)
{
    if (NULL == parser)
    {
        return -1;
    }

    if (NULL == note)
    {
        ERROR(parser, "NULL output pointer provided");
        return -1;
    }

    if (channel >= parser->channel_count)
    {
        ERROR(parser, "Invalid channel requested");
        return -1;
    }

    ptttl_parser_channel_t *chan = &parser->channels[channel];
    parser->active_stream = &chan->stream;
    int ret = _seek_wrapper(parser, parser->active_stream->position);
    CHECK_IFACE_RET_EOF(parser, ret);

    ret = _parse_ptttl_note(parser, note);
    if (ret != 0)
    {
        return ret;
    }

    char next_char;
    ret = _get_next_visible_char(parser, &next_char);
    if (ret == 1)
    {
        return 0;
    }
    else if (ret == 0)
    {
        if ('|' == next_char)
        {
            ret = _jump_to_next_block(parser, chan->channel_idx, 1u);
            if (ret == 1)
            {
                return 0;
            }
        }
        else if (';' == next_char)
        {
            ret = _jump_to_next_block(parser, chan->channel_idx, 0u);
            if (ret == 1)
            {
                return 0;
            }
        }
        else if (',' == next_char)
        {
            ret = _eat_all_nonvisible_chars(parser);
            if (ret == 1)
            {
                return 0;
            }
        }
    }

    return ret;
}
