/* pttl_parser.c
 *
 * PTTTL & RTTTL parser implemented in C. No dynamic memory allocation.
 *
 * Requires stdint.h, strtoul() from stdlib.h and memset() from string.h.
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#include <stdlib.h>
#include <string.h>
#include "ptttl_parser.h"


// Helper macro, stores information about an error, which can be retrieved by ptttl_error()
#define ERROR(s, l, c)              \
{                                   \
    _error.error_message = s;       \
    _error.line = l;                \
    _error.column = c;              \
}

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

// Max. value allowed for note octave
#define NOTE_OCTAVE_MAX (8u)

// Number of valid note duration values
#define NOTE_DURATION_COUNT (6u)


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
    NOTE_C = 0,   // C
    NOTE_CS,      // C sharp
    NOTE_DB,      // D flat
    NOTE_D,       // D
    NOTE_DS,      // D sharp
    NOTE_EB,      // E flat
    NOTE_E,       // E
    NOTE_ES,      // E sharp
    NOTE_F,       // F
    NOTE_FS,      // F sharp
    NOTE_GB,      // G flat
    NOTE_G,       // G
    NOTE_GS,      // G sharp
    NOTE_AB,      // A flat
    NOTE_A,       // A
    NOTE_AS,      // A sharp
    NOTE_BB,      // B flat
    NOTE_B,       // B
    NOTE_INVALID, // Unecognized/invalid note
    NOTE_PITCH_COUNT = NOTE_INVALID
} note_pitch_e;


// Maps note_pitch_e enum values to the corresponding pitch in Hz
static float _note_pitches[NOTE_PITCH_COUNT] =
{
    261.625565301f,   // NOTE_C
    277.182630977f,   // NOTE_CS
    277.182630977f,   // NOTE_DB
    293.664767918f,   // NOTE_D
    311.126983723f,   // NOTE_DS
    311.126983723f,   // NOTE_EB
    329.627556913f,   // NOTE_E
    349.228231433f,   // NOTE_ES
    349.228231433f,   // NOTE_F
    369.994422712f,   // NOTE_FS
    369.994422712f,   // NOTE_GB
    391.995435982f,   // NOTE_G
    415.30469758f,    // NOTE_GS
    415.30469758f,    // NOTE_AB
    440.0f,           // NOTE_A
    466.163761518f,   // NOTE_AS
    466.163761518f,   // NOTE_BB
    493.883301256f    // NOTE_B
};

// Valid values for note duration
static unsigned int _valid_note_durations[NOTE_DURATION_COUNT] = {1u, 2u, 4u, 8u, 16u, 32u};

// Holds information about lastg error encountered
static ptttl_error_t _error;


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
 * Starting from the current input position, consume all characters that are not
 * PTTTL source (comments and whitespace), incrementing line and column counters as needed,
 * until a visible PTTTL character or EOF is seen. Input position will be left at the first
 * visible PTTTL source character that is seen.
 *
 * @param input   Pointer to PTTTL input data
 *
 * @return 0 if successful, -1 if EOF was seen
 */
static int _eat_all_nonvisible_chars(ptttl_input_t *input)
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
            return 0;
        }
    }

    return -1;
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
    int ret = _eat_all_nonvisible_chars(input);
    if (ret < 0)
    {
        return -1;
    }

    *output = input->input_text[input->pos];
    input->pos += 1;
    input->column += 1;
    return 0;
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
        if ((c >= 'A') && (c <= 'Z'))
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

    if ((notepos == 1u) && (notebuf[0] == 'p'))
    {
        *note_pitch = 0.0f;
    }
    else
    {
        note_pitch_e enum_val = _note_string_to_enum(notebuf, notepos);
        if (NOTE_INVALID == enum_val)
        {
            ERROR("Invalid musical note name", input->line, input->column);
            return -1;
        }

        *note_pitch = _note_pitches[enum_val];
    }

    return 0;
}

/**
 * Calculate the power of 2 for a given exponent
 *
 * @param exp  Exponent
 *
 * @return Result
 */
static unsigned int _raise_powerof2(unsigned int exp)
{
    unsigned int ret = 1u;
    for (unsigned int i = 0u; i < exp; i++)
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
static int _parse_note_vibrato(ptttl_input_t *input, settings_t *settings, ptttl_output_note_t *output)
{
    if ('v' != input->input_text[input->pos])
    {
        return 0;
    }

    input->pos += 1;
    input->column += 1;

    INPUT_SIZE_CHECK(input);

    uint32_t freq_hz = settings->default_vibrato_freq;
    uint32_t var_hz = settings->default_vibrato_var;

    // Parse vibrato frequency, if any
    if (IS_DIGIT(input->input_text[input->pos]))
    {
        unsigned int freq = 0u;
        int ret = _parse_uint_from_input(input, &freq);
        if (ret < 0)
        {
            return ret;
        }

        freq_hz = (uint32_t) freq;
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

        var_hz = (uint32_t) var;
    }

    output->vibrato_settings = freq_hz & 0xffffu;
    output->vibrato_settings |= ((var_hz & 0xffffu) << 16u);

    return 0;
}

/**
 * Parse a single PTTTL note (duration, musical note, octave, and vibrato settings)
 * from the current input position, and populate a ptttl_output_note_t object.
 *
 * @param input     Pointer to input PTTTL data
 * @param settings  Pointer to PTTTL settings parsed from settings section
 * @param output    Pointer to location to store output ptttl_output_note_t data
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_ptttl_note(ptttl_input_t *input, settings_t *settings, ptttl_output_note_t *output)
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

    return _parse_note_vibrato(input, settings, output);
}

/**
 * Parse the entire "data" section from the current input position, and populate
 * the output struct
 *
 * @param input     Pointer to PTTTL input data
 * @param output    Pointer to location to store parsed output
 * @param settings  Pointer to PTTTL settings parsed from settings section
 *
 * @return 0 if successful, -1 otherwise
 */
static int _parse_note_data(ptttl_input_t *input, ptttl_output_t *output, settings_t *settings)
{
    unsigned int current_channel_idx = 0u;

    while (input->pos < input->input_text_size)
    {
        ptttl_output_channel_t *current_channel = &output->channels[current_channel_idx];
        ptttl_output_note_t *current_note = &current_channel->notes[current_channel->note_count];

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

        ret = _eat_all_nonvisible_chars(input);
        if (ret < 0)
        {
            return 0;
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

    (void) memset(output, 0, sizeof(ptttl_output_t));

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

    // Read PTTTL settings, next section after the name
    settings_t settings;
    ret = _parse_settings(input, &settings);
    if (ret < 0)
    {
        return -1;
    }

    ret = _eat_all_nonvisible_chars(input);
    if (ret < 0)
    {
        ERROR("Unexpected EOF encountered", input->line, input->column);
        return -1;
    }

    // Read & process all note data
    return _parse_note_data(input, output, &settings);
}
