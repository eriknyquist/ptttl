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
#define ERROR(s)                    \
{                                   \
    _error.error_message = s;       \
    _error.line = _line;            \
    _error.column = _column;        \
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

#define CHECK_READCHAR_RET(_retval)                  \
{                                                    \
    if (1 == _retval)                                \
    {                                                \
        ERROR("Unexpected EOF encountered");         \
        return -1;                                   \
    }                                                \
    else if (0 > _retval)                            \
    {                                                \
        ERROR("readchar callback returned -1");      \
        return -1;                                   \
    }                                                \
}

#define CHECK_READCHAR_RET_EOF(_retval)              \
{                                                    \
    if (1 == _retval)                                \
    {                                                \
        return 1;                                    \
    }                                                \
    else if (0 > _retval)                            \
    {                                                \
        ERROR("readchar callback returned -1");      \
        return -1;                                   \
    }                                                \
}

#ifdef PTTTL_VIBRATO_ENABLED
// Set vibrato settings for a ptttl_output_note_t instance
#define SET_VIBRATO(note, freq, var) ((note)->vibrato_settings = ((freq) & 0xffffu) | (((var) & 0xffffu) << 16u))
#endif // PTTTL_VIBRATO_ENABLED

// Set note settings for a ptttl_output_note_t instance
#define SET_NOTE(note, value, duration) ((note)->note_settings = ((value) & 0x7fu) | (((duration) & 0xffffu) << 7u))



static unsigned int _line = 0u;    // Current line number in PTTTL source file
static unsigned int _column = 0u;  // Current column number in PTTTL source file

/**
 * Holds all values that can be gleaned from the PTTTL 'settings' section
 */
typedef struct
{
    unsigned int bpm;
    unsigned int default_duration;
    unsigned int default_octave;
    unsigned int default_vibrato_freq;
    unsigned int default_vibrato_var;
} settings_t;


// Valid values for note duration
static const unsigned int _valid_note_durations[NOTE_DURATION_COUNT] = {1u, 2u, 4u, 8u, 16u, 32u};

// Holds information about last error encountered
static ptttl_parser_error_t _error;

static uint8_t _have_saved_char = 0u;
static char _saved_char = '\0';


/**
 * Convert note_pitch_e and octave number to a piano key number from 1 through 88.
 *
 * @param note     Note pitch enum (assumed to already be validated)
 * @param octave   Octave number (assumed to already be validated)
 * @param output   Pointer to location to store output
 *
 * @return 0 if successful, -1 if an error occurred
 */
static int _note_name_to_number(note_pitch_e note, unsigned int octave, uint32_t *output)
{
    uint32_t result = 0u;

    if (octave == 0u)
    {
        if (note < NOTE_A)
        {
            ERROR("Invalid musical note for octave 0");
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

static int _readchar_wrapper(ptttl_parser_readchar_t readchar, char *nextchar)
{
    if (1u == _have_saved_char)
    {
        _have_saved_char = 0u;
        *nextchar = _saved_char;
        return 0;
    }

    return readchar(nextchar);
}

/**
 * Starting from the current input position, consume all characters that are not
 * PTTTL source (comments and whitespace), incrementing line and column counters as needed,
 * until a visible PTTTL character or EOF is seen. Input position will be left at the first
 * visible PTTTL source character that is seen.
 *
 * @param readchar  Callback function to read next PTTTL source character
 *
 * @return 0 if successful, -1 if an error occurred, and 1 if EOF was seen
 */
static int _eat_all_nonvisible_chars(ptttl_parser_readchar_t readchar)
{
    char nextchar = '\0';
    int readchar_ret = 0;
    uint8_t in_comment = 0u;

    while ((readchar_ret = _readchar_wrapper(readchar, &nextchar)) == 0)
    {
        if (1u == in_comment)
        {
            if ('\n' == nextchar)
            {
                in_comment = 0u;
            }
            else
            {
                _column += 1u;
                continue;
            }
        }

        if (IS_WHITESPACE(nextchar))
        {
            if ('\n' == nextchar)
            {
                _line += 1;
                _column = 1;
            }
            else
            {
                _column += 1;
            }
        }
        else
        {
            if ('#' == nextchar)
            {
                in_comment = 1u;
                _column += 1u;
            }
            else
            {
                _saved_char = nextchar;
                _have_saved_char = 1u;
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
 * @param input     Pointer to PTTTL input data
 * @param readchar  Callback function to read next PTTTL source character
 *
 * @return 0 if successful, -1 if and error occurred, and 1 if EOF was encountered before non-whitespace char was found
 */
static int _get_next_visible_char(ptttl_parser_readchar_t readchar, char *output)
{
    int ret = _eat_all_nonvisible_chars(readchar);
    if (ret != 0)
    {
        return ret;
    }

    return _readchar_wrapper(readchar, output);
}

/**
 * Parse an unsigned integer from the current input position
 *
 * @param input        Pointer to PTTTL input data
 * @param readchar     Callback function to read next PTTTL source character
 * @param eof_allowed  if 1, error will not be reported on EOF
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_uint_from_input(ptttl_parser_readchar_t readchar, unsigned int *output,
                                  uint8_t eof_allowed)
{
    char buf[32u];
    int pos = 0;
    int readchar_ret = 0;
    char nextchar = '\0';

    while ((readchar_ret = _readchar_wrapper(readchar, &nextchar)) == 0)
    {
        if (IS_DIGIT(nextchar))
        {
            if (pos == (sizeof(buf) - 1))
            {
                ERROR("Integer is too long");
                return -1;
            }

            buf[pos] = nextchar;
            _column += 1;
            pos += 1;
        }
        else
        {
            _saved_char = nextchar;
            _have_saved_char = 1u;
            buf[pos] = '\0';
            break;
        }
    }

    if (eof_allowed)
    {
        CHECK_READCHAR_RET_EOF(readchar_ret);
    }
    else
    {
        CHECK_READCHAR_RET(readchar_ret);
    }

    if (0 == pos)
    {
        ERROR("Expected an integer");
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
 * @param readchar  Callback function to read next PTTTL source character
 * @param settings  Pointer to location to store parsed setting data
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_option(char opt, ptttl_parser_readchar_t readchar, settings_t *settings)
{
    if (':' == opt)
    {
        // End of settings section
        return 0;
    }

    char equals;
    int result = _get_next_visible_char(readchar, &equals);
    CHECK_READCHAR_RET(result);

    if ('=' != equals)
    {
        ERROR("Invalid option setting");
        return -1;
    }

    int ret = 0;
    switch (opt)
    {
        case 'b':
            ret = _parse_uint_from_input(readchar, &settings->bpm, 0u);
            break;
        case 'd':
        {
            ret = _parse_uint_from_input(readchar, &settings->default_duration, 0u);

            if (0 == ret)
            {
                if (!_valid_note_duration(settings->default_duration))
                {
                    ERROR("Invalid note duration (must be 1, 2, 4, 8, 16 or 32)");
                    return -1;
                }
            }
            break;
        }
        case 'o':
            ret = _parse_uint_from_input(readchar, &settings->default_octave, 0u);
            if (0 == ret)
            {
                if (NOTE_OCTAVE_MAX < settings->default_octave)
                {
                    ERROR("Invalid octave (must be 0 through 8)");
                    return -1;
                }
            }
            break;
        case 'f':
            ret = _parse_uint_from_input(readchar, &settings->default_vibrato_freq, 0u);
            break;
        case 'v':
            ret = _parse_uint_from_input(readchar, &settings->default_vibrato_var, 0u);
            break;
        default:
            ERROR("Unrecognized option key");
            return -1;
            break;
    }

    return ret;
}

/**
 * Parse the 'settings' section from the current input position, and populate
 * a settings_t object
 *
 * @param readchar  Callback function to read next PTTTL source character
 * @param settings  Pointer to location to store parsed settings
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_settings(ptttl_parser_readchar_t readchar, settings_t *settings)
{
    char c = '\0';

    settings->bpm = 0u;
    settings->default_duration = 8u;
    settings->default_octave = 4u;
    settings->default_vibrato_freq = 7u;
    settings->default_vibrato_var = 10u;

    while (':' != c)
    {
        int result = _get_next_visible_char(readchar, &c);
        CHECK_READCHAR_RET(result);

        result = _parse_option(c, readchar, settings);
        if (result != 0)
        {
            return result;
        }

        // After parsing option, next visible char should be comma or colon, otherwise error
        result = _get_next_visible_char(readchar, &c);
        CHECK_READCHAR_RET(result);

        if ((',' != c) && (':' != c))
        {
            ERROR("Invalid settings section");
            return -1;
        }
    }

    return 0;
}

/**
 * Parse musical note character(s) at the current input position, and provide the
 * corresponding note_pitch_e enum
 *
 * @param readchar    Callback function to read next PTTTL source character
 * @param note_pitch  Pointer to location to store note pitch enum
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_musical_note(ptttl_parser_readchar_t readchar, note_pitch_e *note_pitch)
{
    // Read musical note name, convert to lowercase if needed
    char notebuf[3];
    int notepos = 0;
    int readchar_ret = 0;
    char nextchar = '\0';

    while (notepos < 2)
    {
        if ((readchar_ret = _readchar_wrapper(readchar, &nextchar)) != 0)
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
            _saved_char = nextchar;
            _have_saved_char = 1u;
            break;
        }

        notepos += 1;
        _column += 1u;
    }

    CHECK_READCHAR_RET_EOF(readchar_ret);

    if (notepos == 0)
    {
        ERROR("Expecting a musical note name");
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
            ERROR("Invalid musical note name");
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
 * @param readchar  Callback function to read next PTTTL source character
 * @param settings  Pointer to PTTTL settings parsed from settings section (contains default vibrato settings)
 * @param output    Pointer to current output note object (contains final vibrato settings)
 *
 * @return 0 if successful, 1 if EOF was encountered, and -1 if an error occurred
 */
static int _parse_note_vibrato(ptttl_parser_readchar_t readchar, settings_t *settings, ptttl_output_note_t *output)
{
    int readchar_ret = 0;
    char nextchar = '\0';

    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    if ('v' != nextchar)
    {
        _saved_char = nextchar;
        _have_saved_char = 1u;
        return 0;
    }

    _column += 1;

#if PTTTL_VIBRATO_ENABLED
    SET_VIBRATO(output, settings->default_vibrato_freq, settings->default_vibrato_var);
#endif // PTTTL_VIBRATO_ENABLED

    uint32_t freq_hz = 0u;
    uint32_t var_hz = 0u;

    // Parse vibrato frequency, if any
    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    _saved_char = nextchar;
    _have_saved_char = 1u;
    if (IS_DIGIT(nextchar))
    {
        unsigned int freq = 0u;
        int ret = _parse_uint_from_input(readchar, &freq, 1u);
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

    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    if ('-' == nextchar)
    {
        _column += 1;

        unsigned int var = 0u;
        int ret = _parse_uint_from_input(readchar, &var, 1u);
        if (ret != 0)
        {
            return ret;
        }

        var_hz = (uint32_t) var;
    }
    else
    {
        _saved_char = nextchar;
        _have_saved_char = 1u;
    }

#if PTTTL_VIBRATO_ENABLED
    SET_VIBRATO(output, freq_hz, var_hz);
#else
    (void) freq_hz;
    (void) var_hz;
#endif // PTTTL_VIBRATO_ENABLED

    return 0;
}

/**
 * Parse a single PTTTL note (duration, musical note, octave, and vibrato settings)
 * from the current input position, and populate a ptttl_output_note_t object.
 *
 * @param readchar  Callback function to read next PTTTL source character
 * @param settings  Pointer to PTTTL settings parsed from settings section
 * @param output    Pointer to location to store output ptttl_output_note_t data
 *
 * @return 0 if successful, 1 if EOF was encountered, and -1 if an error occurred
 */
static int _parse_ptttl_note(ptttl_parser_readchar_t readchar, settings_t *settings, ptttl_output_note_t *output)
{
    unsigned int dot_seen = 0u;

    // Read note duration, if it exists
    unsigned int duration = settings->default_duration;

    char nextchar = '\0';
    int readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    _saved_char = nextchar;
    _have_saved_char = 1u;

    if (IS_DIGIT(nextchar))
    {
        int ret = _parse_uint_from_input(readchar, &duration, 0u);
        if (ret != 0)
        {
            return ret;
        }

        if (!_valid_note_duration(duration))
        {
            ERROR("Invalid note duration (must be 1, 2, 4, 8, 16 or 32)");
            return -1;
        }
    }

    note_pitch_e note_pitch = NOTE_C;
    int ret = _parse_musical_note(readchar, &note_pitch);
    if (ret != 0)
    {
        return ret;
    }

    // Check for dot after note letter
    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    if ('.' == nextchar)
    {
        dot_seen = 1u;
        _column += 1u;
    }
    else
    {
        _saved_char = nextchar;
        _have_saved_char = 1u;
    }

    // Read octave, if it exists
    unsigned int octave = settings->default_octave;
    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    if (IS_DIGIT(nextchar))
    {
        octave = ((unsigned int) nextchar) - 48u;
        if (NOTE_OCTAVE_MAX < octave)
        {
            ERROR("Invalid octave (must be 0 through 8)");
            return -1;
        }

        _column += 1;
    }
    else
    {
        _saved_char = nextchar;
        _have_saved_char = 1u;
    }

    // Check for dot again after octave
    readchar_ret = _readchar_wrapper(readchar, &nextchar);
    CHECK_READCHAR_RET_EOF(readchar_ret);
    if ('.' == nextchar)
    {
        dot_seen = 1u;
        _column += 1u;
    }
    else
    {
        _saved_char = nextchar;
        _have_saved_char = 1u;
    }

    uint32_t note_number = 0u;
    if (NOTE_INVALID != note_pitch)
    {
        ret = _note_name_to_number(note_pitch, octave, &note_number);
        if (ret != 0)
        {
            return ret;
        }
    }

    // Set note time in seconds based on note duration + BPM
    float whole_time = (60.0f / (float) settings->bpm) * 4.0f;
    float duration_secs = whole_time / (float) duration;

    // Handle dotted rhythm
    if (dot_seen == 1u)
    {
        duration_secs += (duration_secs / 2.0f);
    }

    SET_NOTE(output, note_number, (uint32_t) (duration_secs * 1000.0f));

    return _parse_note_vibrato(readchar, settings, output);
}

/**
 * Parse the entire "data" section from the current input position, and populate
 * the note data in the output struct for the intermediate representation
 *
 * @param readchar  Callback function to read next PTTTL source character
 * @param output    Pointer to location to store intermediate representation
 * @param settings  Pointer to PTTTL settings parsed from settings section
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_note_data(ptttl_parser_readchar_t readchar, ptttl_output_t *output, settings_t *settings)
{
    int result = 0;
    unsigned int current_channel_idx = 0u;
    unsigned int first_block = 1u;

    while (0 == result)
    {
        ptttl_output_channel_t *current_channel = &output->channels[current_channel_idx];

        if (PTTTL_MAX_NOTES_PER_CHANNEL <= current_channel->note_count)
        {
            ERROR("Maximum note count exceeded for channel");
            return -1;
        }

        ptttl_output_note_t *current_note = &current_channel->notes[current_channel->note_count];

        int ret = _parse_ptttl_note(readchar, settings, current_note);
        if (ret != 0)
        {
            return ret;
        }

        // Increment note count for this channel
        current_channel->note_count += 1u;

        char next_char;
        int result = _get_next_visible_char(readchar, &next_char);
        CHECK_READCHAR_RET_EOF(result);

        if ('|' == next_char)
        {
            // Channel increment
            current_channel_idx += 1u;

            if (first_block == 1u)
            {
                // Channel count is only updated during the first block
                output->channel_count += 1u;
            }

            if (PTTTL_MAX_CHANNELS_PER_FILE == current_channel_idx)
            {
                ERROR("Maximum channel count exceeded");
                return -1;
            }
        }
        else if (';' == next_char)
        {
            if (first_block == 1u)
            {
                first_block = 0u;
            }
            else
            {
                if ((current_channel_idx + 1u) != output->channel_count)
                {
                    ERROR("All blocks must have the same number of channels");
                    return -1;
                }
            }

            current_channel_idx = 0u;
        }
        else
        {
            if (',' != next_char)
            {
                ERROR("Unexpected character, expecting one of: , | ;");
                return -1;
            }
        }

        result = _eat_all_nonvisible_chars(readchar);
        if (0 != result)
        {
            break;
        }
    }

    return result;
}

/**
 * @see ptttl_parser.h
 */
ptttl_parser_error_t ptttl_parser_error(void)
{
    return _error;
}

/**
 * @see ptttl_parser.h
 */
int ptttl_parse(ptttl_parser_readchar_t readchar, ptttl_output_t *output)
{
    if ((NULL == readchar) || (NULL == output))
    {
        ERROR("NULL pointer provided");
        return -1;
    }

    _line = 1u;
    _column = 1u;
    _have_saved_char = 0u;

    (void) memset(output, 0, sizeof(ptttl_output_t));
    output->channel_count = 1u;

    // Read name (first field)
    int ret = _get_next_visible_char(readchar, &output->name[0]);
    CHECK_READCHAR_RET(ret);

    unsigned int namepos = 1u;
    int readchar_ret = 0;
    char namechar = '\0';
    while ((readchar_ret = _readchar_wrapper(readchar, &namechar)) == 0)
    {
        if (':' == namechar)
        {
            _column += 1u;
            output->name[namepos] = '\0';
            break;
        }
        else
        {
            if ((PTTTL_MAX_NAME_LEN - 1u) == namepos)
            {
                ERROR("Name too long, see PTTTL_MAX_NAME_LEN in ptttl_parser.h");
                return -1;
            }

            if ('\n' == namechar)
            {
                _column = 1;
                _line += 1;
            }
            else
            {
                _column += 1;
            }

            output->name[namepos] = namechar;
            namepos += 1u;
        }
    }

    CHECK_READCHAR_RET(readchar_ret);

    // Read PTTTL settings, next section after the name
    settings_t settings;
    ret = _parse_settings(readchar, &settings);
    if (ret != 0)
    {
        return -1;
    }

    ret = _eat_all_nonvisible_chars(readchar);
    CHECK_READCHAR_RET(ret);

    // Read & process all note data
    ret = _parse_note_data(readchar, output, &settings);
    return (ret >= 0) ? 0 : -1;
}
