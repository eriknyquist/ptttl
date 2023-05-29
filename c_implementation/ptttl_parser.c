/* pttl_parser.c
 *
 * PTTTL & RTTTL parser implemented in C. No dynamic memory allocation, and minimal
 * dependencies (requires strtoul() from stdlib.h, and memset() from string.h).
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ptttl_parser.h"

#define MAX_ERRMSG_SIZE (256u)

static ptttl_error_t _error;

#define ERROR(s, l, c)              \
{                                   \
    _error.error_message = s;       \
    _error.line = l;                \
    _error.column = c;              \
}

/**
 * Holds the string name + pitch (Hz) of a musical note
 */
typedef struct
{
    const char *string;
    float pitch;
} note_info_t;

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

/**
 * Enumerates all possible musical notes in a single octave
 */
typedef enum
{
    NOTE_C = 0,
    NOTE_CS,
    NOTE_DB,
    NOTE_D,
    NOTE_DS,
    NOTE_EB,
    NOTE_E,
    NOTE_ES,
    NOTE_F,
    NOTE_FS,
    NOTE_GB,
    NOTE_G,
    NOTE_GS,
    NOTE_AB,
    NOTE_A,
    NOTE_AS,
    NOTE_BB,
    NOTE_B,
    NOTE_INVALID,
    NOTE_PITCH_COUNT = NOTE_INVALID
} note_pitch_e;


#define NOTE_DURATION_COUNT (6u)
// Valid values for note duration
static unsigned int _valid_note_durations[NOTE_DURATION_COUNT] = {1u, 2u, 4u, 8u, 16u, 32u};

// Max. value allowed for note octave
#define NOTE_OCTAVE_MAX (8u)

// Helper macro, checks if a character is whitespace
#define IS_WHITESPACE(c) (((c) == '\t') || ((c) == ' ') || ((c) == '\v') || ((c) == '\n'))

// Helper macro, checks if a character is a digit
#define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

// Helper macro, set error and return if max. input size is reached
#define INPUT_SIZE_CHECK(i)                                       \
{                                                                 \
    if (i->pos >= i->input_text_size)                             \
    {                                                             \
        ERROR("Unexpected EOF encountered", i->line, i->column);  \
        return -1;                                                \
    }                                                             \
}

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

// Maps note_pitch_e enum values to the corresponding pitch in Hz
static note_info_t _note_info[NOTE_PITCH_COUNT] =
{
    {"c", 261.625565301},    // NOTE_C
    {"c#", 277.182630977},   // NOTE_CS
    {"db", 277.182630977},   // NOTE_DB
    {"d", 293.664767918},    // NOTE_D
    {"d#", 311.126983723},   // NOTE_DS
    {"eb", 311.126983723},   // NOTE_EB
    {"e", 329.627556913},    // NOTE_E
    {"e#", 349.228231433},   // NOTE_ES
    {"f", 349.228231433},    // NOTE_F
    {"f#", 369.994422712},   // NOTE_FS
    {"gb", 369.994422712},   // NOTE_GB
    {"g", 391.995435982},    // NOTE_G
    {"g#", 415.30469758},    // NOTE_GS
    {"ab", 415.30469758},    // NOTE_AB
    {"a", 440.0},            // NOTE_A
    {"a#", 466.163761518},   // NOTE_AS
    {"bb", 466.163761518},   // NOTE_BB
    {"b", 493.883301256}     // NOTE_B
};

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
    if (size > 2)
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

/**
 * Look for the next non-whitespace character, starting from the current input
 * position, and return it. Input position, line + column count will be incremented
 * accordingly. After this function runs successfully, the input position will be
 * at the character *after* the first non-whitespace character that was found.
 *
 * @param input    Pointer to PTTTL input data
 * @param output   Pointer to location to store first non-whitespace char
 *
 * @return 0 if successful, -1 if EOF was encountered before non-whitespace char was found
 */
static int _get_next_visible_char(ptttl_input_t *input, char *output)
{
    while (input->pos < input->input_text_size)
    {
        if (IS_WHITESPACE(input->input_text[input->pos]))
        {
            if ('\n' == input->input_text[input->pos])
            {
                input->line += 1;
                input->column = 1;
            }
            else
            {
                input->column += 1;
            }

            input->pos += 1;
        }
        else
        {
            *output = input->input_text[input->pos];
            input->pos += 1;
            input->column += 1;
            return 0;
        }
    }

    return -1;
}

/**
 * Parse an unsigned integer from the current input position
 *
 * @param input    Pointer to PTTTL input data
 * @param output   Pointer to location to store parsed unsigned integer
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_uint_from_input(ptttl_input_t *input, unsigned int *output)
{
    char buf[32u];
    int pos = 0;

    while (input->pos < input->input_text_size)
    {
        char c = input->input_text[input->pos];
        if (IS_DIGIT(c))
        {
            if (pos == (sizeof(buf) - 1))
            {
                ERROR("Integer is too long", input->line, input->column);
                return -1;
            }

            buf[pos] = c;
            input->column += 1;
            input->pos += 1;
            pos += 1;
        }
        else
        {
            buf[pos] = '\0';
            break;
        }
    }

    if (0 == pos)
    {
        ERROR("Expected an integer", input->line, input->column);
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
 * @param input     Pointer to PTTTL input data
 * @param settings  Pointer to location to store parsed setting data
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_option(char opt, ptttl_input_t *input, settings_t *settings)
{
    if (':' == opt)
    {
        // End of settings section
        return 0;
    }

    char equals;
    int result = _get_next_visible_char(input, &equals);
    if (result < 0)
    {
        ERROR("Reached EOF before end of settings section", input->line, input->column);
        return -1;
    }

    if ('=' != equals)
    {
        ERROR("Invalid option setting", input->line, input->column);
        return -1;
    }

    int ret = 0;
    switch (opt)
    {
        case 'b':
            ret = _parse_uint_from_input(input, &settings->bpm);
            break;
        case 'd':
        {
            ret = _parse_uint_from_input(input, &settings->default_duration);

            if (0 == ret)
            {
                if (!_valid_note_duration(settings->default_duration))
                {
                    ERROR("Invalid note duration (must be 1, 2, 4, 8, 16 or 32)", input->line, input->column);
                    return -1;
                }
            }
            break;
        }
        case 'o':
            ret = _parse_uint_from_input(input, &settings->default_octave);
            if (0 == ret)
            {
                if (NOTE_OCTAVE_MAX < settings->default_octave)
                {
                    ERROR("Invalid octave (must be 0 through 8)", input->line, input->column);
                    return -1;
                }
            }
            break;
        case 'f':
            ret = _parse_uint_from_input(input, &settings->default_vibrato_freq);
            break;
        case 'v':
            ret = _parse_uint_from_input(input, &settings->default_vibrato_var);
            break;
        default:
            ERROR("Unrecognized option key", input->line, input->column);
            return -1;
            break;
    }

    return ret;
}

/**
 * Parse the 'settings' section from the current input position, and populate
 * a settings_t object
 *
 * @param input     Pointer to PTTTL input data
 * @param settings  Pointer to location to store parsed settings
 *
 * @return 0 if successful, 1 otherwise
 */
static int _parse_settings(ptttl_input_t *input, settings_t *settings)
{
    char c = '\0';

    settings->bpm = 0u;
    settings->default_duration = 8u;
    settings->default_octave = 4u;
    settings->default_vibrato_freq = 0u;
    settings->default_vibrato_var = 0u;

    while(input->pos < input->input_text_size)
    {
        int result = _get_next_visible_char(input, &c);
        if (result < 0)
        {
            ERROR("Reached EOF before end of settings section", input->line, input->column);
            return -1;
        }

        result = _parse_option(c, input, settings);
        if (result < 0)
        {
            return result;
        }

        // After parsing option, next visible char should be comma or colon, otherwise error
        result = _get_next_visible_char(input, &c);
        if (result < 0)
        {
            ERROR("Reached EOF before end of settings section", input->line, input->column);
            return -1;
        }

        if (':' == c)
        {
            // End of settings section
            break;
        }
        else if (',' != c)
        {
            ERROR("Invalid settings section", input->line, input->column);
            return -1;
        }

    }

    printf("b=%u, d=%u, o=%u, f=%u, v=%u\n", settings->bpm, settings->default_duration,
           settings->default_octave, settings->default_vibrato_freq, settings->default_vibrato_var);

    return 0;
}

/**
 * Parse musical note character(s) at the current input position, and provide the
 * corresponding pitch value in Hz
 *
 * @param input       Pointer to PTTTL input data
 * @param note_pitch  Pointer to location to store note pitch in Hz
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_musical_note(ptttl_input_t *input, float *note_pitch)
{
    // Read musical note name, convert to lowercase if needed
    char notebuf[3];
    unsigned int notepos = 0u;
    while ((notepos < 2u) && (input->pos < input->input_text_size))
    {
        char c = input->input_text[input->pos];
        if ((c >= 'A') && (c <= 'G'))
        {
            notebuf[notepos] = c + ' ';
        }
        else if (((c >= 'a') && (c <= 'z')) || (c == '#'))
        {
            notebuf[notepos] = c;
        }
        else
        {
            break;
        }

        notepos += 1u;
        input->pos += 1u;
        input->column += 1u;
    }

    if (notepos == 0u)
    {
        ERROR("Expecting a musical note name", input->line, input->column);
        return -1;
    }

    note_pitch_e pitch = _note_string_to_enum(notebuf, notepos);
    if (NOTE_INVALID == pitch)
    {
        ERROR("Invalid musical note name", input->line, input->column);
        return -1;
    }

    *note_pitch = _note_info[pitch].pitch;
    return 0;
}

static unsigned int _raise_powerof2(unsigned int num)
{
    unsigned int ret = 1u;
    for (unsigned int i = 0u; i < num; i++)
    {
        ret *= 2u;
    }

    return ret;
}

/**
 * Parse vibrato settings at the end of a PTTTL note (if any) from the current
 * input position. If there are no vibrato settings at the current input position,
 * return success.
 *
 * @param input     Pointer to input PTTTL data
 * @param freq_hz   Pointer to location to store parsed vibrato frequency Hz
 * @param freq_var  Pointer to location to store parsed vibrato variance Hz
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_note_vibrato(ptttl_input_t *input, float *freq_hz, float *var_hz)
{
    if ('v' != input->input_text[input->pos])
    {
        return 0;
    }

    input->pos += 1;
    input->column += 1;

    INPUT_SIZE_CHECK(input);

    // Parse vibrato frequency, if any
    if (IS_DIGIT(input->input_text[input->pos]))
    {
        unsigned int freq = 0u;
        int ret = _parse_uint_from_input(input, &freq);
        if (ret < 0)
        {
            return ret;
        }

        *freq_hz = (float) freq;
    }
    else
    {
        return 0;
    }

    if ('-' == input->input_text[input->pos])
    {
        input->pos += 1;
        input->column += 1;

        INPUT_SIZE_CHECK(input);

        unsigned int var = 0u;
        int ret = _parse_uint_from_input(input, &var);
        if (ret < 0)
        {
            return ret;
        }

        *var_hz = (float) var;
    }

    return 0;
}

/**
 * Parse a single PTTTL note (duration, note letter, octave, and vibrato settings)
 * from the current input position, and populate a note_t object.
 *
 * @param input     Pointer to input PTTTL data
 * @param settings  Pointer to PTTTL settings parsed from settings section
 * @param output    Pointer to location to store output note_t data
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_ptttl_note(ptttl_input_t *input, settings_t *settings, note_t *output)
{
    unsigned int dot_seen = 0u;

    // Read note duration, if it exists
    unsigned int duration = settings->default_duration;
    if (IS_DIGIT(input->input_text[input->pos]))
    {
        int ret = _parse_uint_from_input(input, &duration);
        if (ret < 0)
        {
            return ret;
        }

        if (!_valid_note_duration(duration))
        {
            ERROR("Invalid note duration (must be 1, 2, 4, 8, 16 or 32)", input->line, input->column);
            return -1;
        }
    }

    int ret = _parse_musical_note(input, &output->pitch_hz);
    if (ret < 0)
    {
        return ret;
    }

    INPUT_SIZE_CHECK(input);

    // Check for dot after note letter
    if ('.' == input->input_text[input->pos])
    {
        dot_seen = 1u;
        input->pos += 1u;
        input->column += 1u;
        INPUT_SIZE_CHECK(input);
    }

    // Read octave, if it exists
    unsigned int octave = settings->default_octave;
    if (IS_DIGIT(input->input_text[input->pos]))
    {
        octave = ((unsigned int) input->input_text[input->pos]) - 48u;
        if (NOTE_OCTAVE_MAX < octave)
        {
            ERROR("Invalid octave (must be 0 through 8)", input->line, input->column);
            return -1;
        }

        input->pos += 1;
        input->column += 1;
    }

    INPUT_SIZE_CHECK(input);

    // Check for dot again after octave
    if ('.' == input->input_text[input->pos])
    {
        dot_seen = 1u;
        input->pos += 1u;
        input->column += 1u;
        INPUT_SIZE_CHECK(input);
    }

    // Set true pitch based on octave, if octave is not 4
    if (octave < 4u)
    {
        output->pitch_hz = output->pitch_hz / (float) _raise_powerof2(4u - octave);
    }
    else if (octave > 4u)
    {
        output->pitch_hz = output->pitch_hz * (float) _raise_powerof2(octave - 4u);
    }

    // Set note time in seconds based on note duration + BPM
    float whole_time = (60.0f / (float) settings->bpm) * 4.0f;
    output->duration_secs = whole_time / (float) duration;

    // Handle dotted rhythm
    if (dot_seen == 1u)
    {
        output->duration_secs += (output->duration_secs / 2.0f);
    }

    output->vibrato_freq_hz = settings->default_vibrato_freq;
    output->vibrato_var_hz = settings->default_vibrato_var;

    return _parse_note_vibrato(input, &output->vibrato_freq_hz, &output->vibrato_var_hz);
}

/**
 * Parse the entire "data" section and populate the output struct
 *
 * @param input     Pointer to PTTTL input data
 * @param output    Pointer to location to store parsed output
 * @param settings  Pointer to PTTTL settings parsed from settings section
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_note_data(ptttl_input_t *input, ptttl_output_t *output, settings_t *settings)
{
    (void) memset(output, 0, sizeof(ptttl_output_t));
    unsigned int current_channel_idx = 0u;

    while (input->pos < input->input_text_size)
    {
        channel_t *current_channel = &output->channels[current_channel_idx];
        note_t *current_note = &current_channel->notes[current_channel->note_count];

        int ret = _parse_ptttl_note(input, settings, current_note);
        if (ret < 0)
        {
            return ret;
        }

        // Increment note count for this channel
        current_channel->note_count += 1u;

        char next_char;
        int result = _get_next_visible_char(input, &next_char);
        if (result < 0)
        {
            // EOF was reached
            break;
        }

        if ('|' == next_char)
        {
            // Channel increment
            current_channel_idx += 1u;
            if (PTTTL_MAX_CHANNELS_PER_FILE == current_channel_idx)
            {
                ERROR("Maximum channel count exceeded", input->line, input->column);
                return -1;
            }
        }
        else if (';' == next_char)
        {
            // block end; make sure all blocks have the same number of channels
            if (0u == output->channel_count)
            {
                output->channel_count = current_channel_idx + 1u;
            }
            else
            {
                if ((current_channel_idx + 1u) != output->channel_count)
                {
                    ERROR("All blocks must have the same number of channels", input->line, input->column);
                    return -1;
                }
            }

            current_channel_idx = 0u;
        }
        else
        {
            if (',' != next_char)
            {
                ERROR("Unexpected character, expecting one of: , | ;", input->line, input->column);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * @see ptttl_parser.h
 */
ptttl_error_t ptttl_error(void)
{
    return _error;
}

/**
 * @see ptttl_parser.h
 */
int ptttl_parse(ptttl_input_t *input, ptttl_output_t *output)
{
    if ((NULL == input) || (NULL == output))
    {
        ERROR("NULL pointer provided", 0, 0);
        return -1;
    }

    if (NULL == input->input_text)
    {
        ERROR("NULL pointer provided", 0, 0);
        return -1;
    }

    if (0u == input->input_text_size)
    {
        ERROR("Input text size is 0", 0, 0);
        return -1;
    }

    input->line = 1;
    input->column = 1;
    input->pos = 0;

    // Read name (first field)
    int ret = _get_next_visible_char(input, &output->name[0]);
    if (ret < 0)
    {
        ERROR("Unexpected EOF encountered", input->line, input->column);
        return -1;
    }

    int namepos = 1;
    while (input->pos < input->input_text_size)
    {
        char namechar = input->input_text[input->pos];
        if (':' == namechar)
        {
            input->pos += 1u;
            input->column += 1u;
            output->name[namepos] = '\0';
            break;
        }
        else
        {
            if ('\n' == namechar)
            {
                input->column = 1;
                input->line += 1;
            }
            else
            {
                input->column += 1;
            }

            output->name[namepos] = namechar;
            namepos += 1;
        }

        input->pos += 1u;
    }

    INPUT_SIZE_CHECK(input);

    settings_t settings;
    ret = _parse_settings(input, &settings);
    if (ret < 0)
    {
        return -1;
    }

    return _parse_note_data(input, output, &settings);
}
