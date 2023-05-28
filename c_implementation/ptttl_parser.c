#include <stdlib.h>
#include <stdio.h>
#include "ptttl_parser.h"

#define MAX_ERRMSG_SIZE (256u)

static ptttl_error_t _error;

#define ERROR(s, l, c)              \
{                                   \
    _error.error_message = s;       \
    _error.line = l;                \
    _error.column = c;              \
}


typedef struct
{
    const char *string;
    float pitch;
} note_info_t;

typedef struct
{
    unsigned int bpm;
    unsigned int default_duration;
    unsigned int default_octave;
    unsigned int default_vibrato_freq;
    unsigned int default_vibrato_var;
} settings_t;

#define NOTE_PITCH_COUNT (18u)

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
    NOTE_INVALID
} note_pitch_e;

#define IS_WHITESPACE(c) (((c) == '\t') || ((c) == ' ') || ((c) == '\v') || ((c) == '\n'))

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

static int _get_next_visible_char(ptttl_input_t *input, char *output)
{
    while (input->pos < input->input_text_size)
    {
        if (IS_WHITESPACE(input->input_text[input->pos]))
        {
            if ('\n' == input->input_text[input->pos])
            {
                input->line += 1;
                input->column = 0;
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
            ret = _parse_uint_from_input(input, &settings->default_duration);
            break;
        case 'o':
            ret = _parse_uint_from_input(input, &settings->default_octave);
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

static int _parse_settings(ptttl_input_t *input, settings_t *settings)
{
    char c = '\0';

    settings->bpm = 0u;
    settings->default_duration = 8u;
    settings->default_octave = 4u;
    settings->default_vibrato_freq = 0u;
    settings->default_vibrato_var = 0u;

    do
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

        if ((c != ',') && (c != ':'))
        {
            ERROR("Invalid settings section", input->line, input->column);
            return -1;
        }
    }
    while(':' != c);

    printf("b=%u, d=%u, o=%u, f=%u, v=%u\n", settings->bpm, settings->default_duration,
           settings->default_octave, settings->default_vibrato_freq, settings->default_vibrato_var);

    return 0;
}

static int _parse_note_data(ptttl_input_t *input, ptttl_output_t *output, settings_t *settings)
{

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
    input->column = 0;
    input->pos = 0;
    settings_t settings;
    int ret = _parse_settings(input, &settings);
    if (ret < 0)
    {
        return -1;
    }

    return _parse_note_data(input, output, &settings);
}
