/* ptttl_sample_generator.c
 *
 * Converts the output of ptttl_parse_next() into a stream of signed 16-bit audio samples
 * suitable for a WAV file. Samples can be obtained one at a time, at your leisure.
 *
 * Requires ptttl_parser.c
 *
 * Requires stdint.h and memset() from string.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#include <string.h>

#include "ptttl_sample_generator.h"
#include "ptttl_common.h"


// Max positive value of a signed 16-bit sample
#define MAX_SAMPLE_VALUE   (0x7FFF)

// Default waveform type for all channels
#define DEFAULT_WAVEFORM_TYPE WAVEFORM_TYPE_SINE

#define PI 3.14159265358979323846f

// Store an error message for reporting by ptttl_parser_error()
#define ERROR(_parser, _msg)                                        \
{                                                                   \
    _parser->error.error_message = _msg;                            \
    _parser->error.line = _parser->active_stream->line;             \
    _parser->error.column = _parser->active_stream->column;         \
}

/**
 * Holds per-note data for the Nokia waveform generator.
 * Used as the wgendata layout when WAVEFORM_TYPE_NOKIA is active.
 */
typedef struct
{
    int count;                                            ///< Number of valid harmonic coefficients
    float coeffs[PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS];   ///< Linear-amplitude Fourier coefficients
} ptttl_nokia_wgendata_t;

/**
 * Holds per-note data for the Nokia waveform generator for all channels.
 */
static ptttl_nokia_wgendata_t _nokia_wgendata[PTTTL_MAX_CHANNELS_PER_FILE];


// maps piano key number (0-87) to pitch in Hz
static const float _note_pitches[88] =
{
    // Octave 0
    27.50000f, 29.13524f, 30.86771f,
    // Octave 1
    32.70320f, 34.64783f, 36.70810f, 38.89087f, 41.20344f, 43.65353f, 46.24930f,
    48.99943f, 51.91309f, 55.00000f, 58.27047f, 61.73541f,
    // Octave 2
    65.40639f, 69.29566f, 73.41619f, 77.78175f, 82.40689f, 87.30706f, 92.49861f,
    97.99886f, 103.8262f, 110.0000f, 116.5409f, 123.4708f,
    // Octave 3
    130.8128f, 138.5913f, 146.8324f, 155.5635f, 164.8138f, 174.6141f, 184.9972f,
    195.9977f, 207.6523f, 220.0000f, 233.0819f, 246.9417f,
    // Octave 4
    261.6256f, 277.1826f, 293.6648f, 311.1270f, 329.6276f, 349.2282f, 369.9944f,
    391.9954f, 415.3047f, 440.0000f, 466.1638f, 493.8833f,
    // Octave 5
    523.2511f, 554.3653f, 587.3295f, 622.2540f, 659.2551f, 698.4565f, 739.9888f,
    783.9909f, 830.6094f, 880.0000f, 932.3275f, 987.7666f,
    // Octave 6
    1046.502f, 1108.731f, 1174.659f, 1244.508f, 1318.510f, 1396.913f, 1479.978f,
    1567.982f, 1661.219f, 1760.000f, 1864.655f, 1975.533f,
    // Octave 7
    2093.005f, 2217.461f, 2349.318f, 2489.016f, 2637.020f, 2793.826f, 2959.955f,
    3135.963f, 3322.438f, 3520.000f, 3729.310f, 3951.066f,
    // Octave 8
    4186.009f
};

/* Nokia waveform harmonic coefficients, pre-converted from dB to linear amplitude.
 * Measured from a real Nokia 3310, covering C4-B6 (piano keys 39-74, table indices 0-35).
 * Notes outside this range are octave-folded to the nearest available entry.
 * Source: https://github.com/edwardball/monophonic-ringtone-synthesizer */
#define NOKIA_TABLE_SIZE (36u)
#define NOKIA_MAX_NUM_HARMONICS (16u)
static const float _nokia_coeffs[NOKIA_TABLE_SIZE][NOKIA_MAX_NUM_HARMONICS] =
{
    /* [ 0] C4  (12) */
    {0.000014f, 0.001459f, 0.004722f, 0.032332f, 0.095089f, 0.090496f, 0.058699f, 0.042720f,
     0.119848f, 0.163542f, 0.157808f, 0.033934f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 1] C#4 (16) */
    {0.000014f, 0.003267f, 0.012754f, 0.075445f, 0.152451f, 0.131410f, 0.091439f, 0.118203f,
     0.259794f, 0.142603f, 0.031925f, 0.029149f, 0.024106f, 0.021509f, 0.027017f, 0.026923f},
    /* [ 2] D4  (11) */
    {0.000014f, 0.003764f, 0.016411f, 0.099570f, 0.100145f, 0.091123f, 0.076406f, 0.162978f,
     0.178291f, 0.048265f, 0.034844f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 3] Eb4 (13) */
    {0.000014f, 0.003959f, 0.018520f, 0.065182f, 0.141785f, 0.080934f, 0.089977f, 0.165055f,
     0.252133f, 0.052739f, 0.028915f, 0.025890f, 0.029897f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 4] E4  (11) */
    {0.000014f, 0.003996f, 0.025860f, 0.106446f, 0.094761f, 0.058902f, 0.137445f, 0.245262f,
     0.101890f, 0.037682f, 0.024301f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 5] F4  (13) */
    {0.000014f, 0.003113f, 0.046359f, 0.131713f, 0.081120f, 0.051361f, 0.099685f, 0.298284f,
     0.034129f, 0.012857f, 0.026280f, 0.026099f, 0.020377f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 6] F#4 (13) */
    {0.000014f, 0.002928f, 0.061324f, 0.098657f, 0.053349f, 0.094109f, 0.162043f, 0.103188f,
     0.033817f, 0.015315f, 0.028160f, 0.021093f, 0.015457f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 7] G4  (12) */
    {0.000014f, 0.007756f, 0.100260f, 0.179734f, 0.087525f, 0.155822f, 0.372932f, 0.048824f,
     0.037336f, 0.033507f, 0.034445f, 0.014293f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 8] G#4 (12) */
    {0.000014f, 0.013792f, 0.068019f, 0.148809f, 0.077025f, 0.162043f, 0.148467f, 0.042622f,
     0.052496f, 0.037725f, 0.021117f, 0.010260f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [ 9] A4  (11) */
    {0.001820f, 0.014492f, 0.080007f, 0.070734f, 0.074153f, 0.257413f, 0.062752f, 0.026371f,
     0.029897f, 0.021020f, 0.017343f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [10] Bb4 (11) */
    {0.000014f, 0.025301f, 0.166967f, 0.076936f, 0.139679f, 0.361518f, 0.038426f, 0.042426f,
     0.037250f, 0.016620f, 0.012783f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [11] B4  (11) */
    {0.000014f, 0.031019f, 0.121235f, 0.063552f, 0.171842f, 0.105470f, 0.022291f, 0.030383f,
     0.021337f, 0.014244f, 0.018350f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [12] C5  (11) */
    {0.002819f, 0.059106f, 0.178086f, 0.093353f, 0.326310f, 0.052617f, 0.035410f, 0.037293f,
     0.016544f, 0.020400f, 0.036613f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [13] C#5 (11) */
    {0.004922f, 0.087223f, 0.187340f, 0.155464f, 0.445789f, 0.036570f, 0.041508f, 0.021985f,
     0.010271f, 0.018886f, 0.045565f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [14] D5  (11) */
    {0.006960f, 0.127976f, 0.133699f, 0.201665f, 0.193700f, 0.046519f, 0.064214f, 0.027392f,
     0.030841f, 0.052015f, 0.035248f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [15] Eb5 (10) */
    {0.005353f, 0.074068f, 0.095308f, 0.204000f, 0.067551f, 0.054341f, 0.034287f, 0.012292f,
     0.025950f, 0.049389f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [16] E5  (10) */
    {0.004204f, 0.096968f, 0.060832f, 0.275823f, 0.034844f, 0.035329f, 0.014744f, 0.018820f,
     0.031596f, 0.025330f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [17] F5  (10) */
    {0.005112f, 0.184557f, 0.105228f, 0.400064f, 0.037769f, 0.045881f, 0.015726f, 0.021435f,
     0.040703f, 0.017564f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [18] F#5 (10) */
    {0.006275f, 0.168900f, 0.189946f, 0.130656f, 0.050365f, 0.026099f, 0.031198f, 0.049617f,
     0.030001f, 0.009400f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [19] G5  ( 9) */
    {0.012710f, 0.281598f, 0.224714f, 0.092497f, 0.041222f, 0.030209f, 0.027773f, 0.056706f,
     0.020097f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [20] G#5 ( 9) */
    {0.016639f, 0.183497f, 0.215095f, 0.039823f, 0.047822f, 0.014113f, 0.050076f, 0.031523f,
     0.006747f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [21] A5  ( 8) */
    {0.016037f, 0.098431f, 0.333526f, 0.038471f, 0.023476f, 0.025155f, 0.054154f, 0.016639f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [22] Bb5 ( 8) */
    {0.024925f, 0.091333f, 0.396396f, 0.053843f, 0.021337f, 0.022163f, 0.047767f, 0.010511f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [23] B5  ( 7) */
    {0.047767f, 0.120262f, 0.181397f, 0.068727f, 0.030699f, 0.059242f, 0.025069f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [24] C6  ( 7) */
    {0.143757f, 0.237208f, 0.156722f, 0.094543f, 0.059106f, 0.095089f, 0.023721f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [25] C#6 ( 6) */
    {0.164108f, 0.288157f, 0.074581f, 0.047767f, 0.049731f, 0.045355f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [26] D6  ( 6) */
    {0.159636f, 0.250397f, 0.067240f, 0.035329f, 0.069683f, 0.025535f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [27] Eb6 ( 6) */
    {0.074839f, 0.178291f, 0.061678f, 0.016871f, 0.060343f, 0.010003f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [28] E6  ( 5) */
    {0.115380f, 0.297941f, 0.044119f, 0.024753f, 0.034804f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [29] F6  ( 5) */
    {0.186264f, 0.443231f, 0.049846f, 0.025330f, 0.020074f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [30] F#6 ( 5) */
    {0.208992f, 0.169875f, 0.034406f, 0.064734f, 0.018995f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [31] G6  ( 5) */
    {0.189073f, 0.134626f, 0.053349f, 0.079183f, 0.004821f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [32] G#6 ( 5) */
    {0.228628f, 0.089666f, 0.024668f, 0.064288f, 0.005360f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [33] A6  ( 5) */
    {0.210441f, 0.079000f, 0.053596f, 0.026707f, 0.004033f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [34] Bb6 ( 4) */
    {0.208271f, 0.094434f, 0.066318f, 0.018308f, 0.000000f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
    /* [35] B6  ( 4) */
    {0.171842f, 0.108051f, 0.117525f, 0.006161f, 0.000000f, 0.000000f, 0.000000f, 0.000000f,
     0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f, 0.000000f},
};

// Stores the number of harmonics for each row in the _nokia_coeffs table
static const uint8_t _nokia_harmonic_count[NOKIA_TABLE_SIZE] =
{
    12, 16, 11, 13, 11, 13, 13, 12, 12, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10,  9,  9,  8,  8,  7,  7,  6,  6,  6,  5,  5,  5,  5,
     5,  5,  4,  4
};

/* Maps piano key 0-87 (note_number - 1) to a _nokia_coeffs row index.
 * Notes outside C4-B6 are octave-folded to the nearest available entry. */
static const uint8_t _nokia_key_to_table[88] =
{
     9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,
     9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,
     9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,
     9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,
     9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 24
};

// Forward declaration of built-in waveform generators and Nokia setup
static float _sine_generator(float x, float p, unsigned int s, void *wgendata);
static float _triangle_generator(float x, float p, unsigned int s, void *wgendata);
static float _sawtooth_generator(float x, float p, unsigned int s, void *wgendata);
static float _square_generator(float x, float p, unsigned int s, void *wgendata);
static float _nokia_generator(float x, float p, unsigned int s, void *wgendata);
static void  _nokia_note_setup(uint32_t channel_idx, uint8_t note_number,
                               unsigned int sample_rate, void *wgendata);

// Maps waveform type enums to built-in ptttl_waveform_t definitions.
// note_setup is NULL for all waveforms except Nokia.
static ptttl_waveform_t _builtin_waveforms[WAVEFORM_TYPE_COUNT] =
{
    { _sine_generator,     NULL, NULL},           // WAVEFORM_TYPE_SINE
    { _triangle_generator, NULL, NULL},           // WAVEFORM_TYPE_TRIANGLE
    { _sawtooth_generator, NULL, NULL},           // WAVEFORM_TYPE_SAWTOOTH
    { _square_generator,   NULL, NULL},           // WAVEFORM_TYPE_SQUARE
    { _nokia_generator, _nokia_note_setup, NULL}, // WAVEFORM_TYPE_NOKIA
};


/**
 * Square wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _square_generator(float x, float p, unsigned int s, void *wgendata)
{
    (void) wgendata;
    x = x - (int)x;
    if (x < 0.0f) x += 1.0f;

    // Calculate max. number of harmonics given the waveform frequency
    int maxh = (int) ((((float) s) * 0.5f) / p);
    if (maxh < 1) maxh = 1;               // At least the fundamental harmonic
    int hcount = (maxh < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS) ?
                  maxh : PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS;

    // Generate square point by summing points of sine waves of the harmonics
    float sum = 0.0f;
    // square only has odd harmonics
    for (int n = 1; n <= hcount; n += 2)
    {
        sum += (4.0f / (PI * n)) * _sine_generator(n * x, 0.0f, 0u, NULL);
    }

    return sum;
}

/**
 * Sawtooth wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _sawtooth_generator(float x, float p, unsigned int s, void *wgendata)
{
    (void) wgendata;
    x = x - (int)x;
    if (x < 0.0f) x += 1.0f;

    // Calculate max. number of harmonics given the waveform frequency
    int maxh = (int) ((((float) s) * 0.5f) / p);
    if (maxh < 1) maxh = 1;               // At least the fundamental harmonic
    int hcount = (maxh < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS) ?
                  maxh : PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS;

    // Generate sawtooth point by summing points of sine waves of the harmonics
    float sum = 0.0f;
    for (int n = 1; n <= hcount; ++n)
    {
        float sign = (n & 1) ? 1.0f : -1.0f;
        sum += sign * (2.0f / (PI * n)) * _sine_generator(n * x, 0.0f, 0u, NULL);
    }
    return sum;
}

/**
 * Triangle wave generator
 *
 * @see ptttl_waveform_generator_t
 */
static float _triangle_generator(float x, float p, unsigned int s, void *wgendata)
{
    (void) wgendata;
    (void) p; // Unused
    (void) s; // Unused

    float value = 0.0f;

    for (int k = 0; k < PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS; k++)
    {
        int n = 2 * k + 1;
        float sign = (k & 1) ? -1.0f : 1.0f;
        value += sign * _sine_generator(n * x, p, s, NULL) / (n * n);
    }

    value *= 8.0f / (float) (PI * PI);
    return value;
}

/**
 * Sine wave generator
 *
 * @see ptttl_waveform_generator_t
 *
 * Fast sine approximation copied from:
 * https://github.com/skeeto/scratch/blob/master/misc/rtttl.c
 *
 */
static float _sine_generator(float x, float p, unsigned int s, void *wgendata)
{
    (void) wgendata;
    (void) p; // Unused
    (void) s; // Unused

    x  = x < 0 ? 0.5f - x : x;
    x -= 0.500f + (float)(int)x;
    x *= 16.00f * ((x < 0 ? -x : x) - 0.50f);
    x += 0.225f * ((x < 0 ? -x : x) - 1.00f) * x;
    return x;
}

/**
 * Per-note setup callback for the Nokia waveform generator.
 * Populates wgendata (cast to ptttl_nokia_wgendata_t *) with harmonic coefficients
 * for the given note, looked up from the pre-measured coefficient table.
 */
static void _nokia_note_setup(uint32_t channel_idx, uint8_t note_number,
                              unsigned int sample_rate, void *wgendata)
{
    (void) sample_rate;
    (void) channel_idx;

    ptttl_nokia_wgendata_t *data = (ptttl_nokia_wgendata_t *) wgendata;

    if (0u == note_number)
    {
        data->count = 0;
        return;
    }

    uint8_t table_idx = _nokia_key_to_table[note_number - 1u];
    data->count = _nokia_harmonic_count[table_idx];

    for (int i = 0; i < data->count; i++)
    {
        data->coeffs[i] = _nokia_coeffs[table_idx][i];
    }
}

/**
 * Nokia 3310 waveform generator.
 * Sums sine harmonics weighted by coefficients pre-loaded into wgendata by
 * _nokia_note_setup. No table lookups on the per-sample hot path.
 *
 * @see ptttl_waveform_generator_t
 */
static float _nokia_generator(float x, float p, unsigned int s, void *wgendata)
{
    (void) p;
    (void) s;

    ptttl_nokia_wgendata_t *data = (ptttl_nokia_wgendata_t *) wgendata;

    if ((NULL == data) || (0 == data->count))
    {
        return 0.0f;
    }

    x = x - (int) x;
    if (x < 0.0f) x += 1.0f;

    float sum = 0.0f;
    int hcount = (PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS >= data->count) ?
                 data->count :
                 PTTTL_SAMPLE_GENERATOR_NUM_HARMONICS;

    const float *coeffs = data->coeffs;

    for (int n = 0; n < hcount; n++)
    {
        sum += coeffs[n] * _sine_generator((float)(n + 1) * x, 0.0f, 0u, NULL);
    }

    return sum;
}



/**
 * Generate a single point on a waveform, between -1.0-1.0, for a given sample rate and frequency
 *
 * @param waveform     Pointer to waveform generator struct
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of sine wave
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static float _generate_waveform_point(ptttl_waveform_t *waveform, unsigned int sample_rate,
                                      float freq, unsigned int sine_index)
{
    float sine_state = freq * (((float) sine_index) / (float) sample_rate);
    return waveform->wgen(sine_state, freq, sample_rate, waveform->wgendata);
}


/**
 * Generate a single waveform sample between 0-65535 for a given sample rate and frequency
 *
 * @param waveform     Pointer to waveform generator struct
 * @param sample_rate  Sampling rate
 * @param freq         Frequency of the waveform
 * @param sine_index   Index of sample within this note (0 is the first sample of the note)
 *
 * @return Sine wave sample
 */
static int32_t _generate_waveform_sample(ptttl_waveform_t *waveform, unsigned int sample_rate,
                                         float freq, unsigned int sine_index)
{
    // Calculate sample value between 0.0 - 1.0
    float sample_norm = _generate_waveform_point(waveform, sample_rate, freq, sine_index);

    // Convert to 0x0-0x7fff range
    return (int32_t) (sample_norm * (float) MAX_SAMPLE_VALUE);
}

/**
 * Load a single PTTTL note from a specific channel into a note_stream_t object
 *
 * @param generator    Pointer to initialized sample generator
 * @param note         Pointer to parsed note object
 * @param note_stream  Pointer to note stream object to populate
 * @param channel_idx  Channel number this note belongs to
 */
static void _load_note_stream(ptttl_sample_generator_t *generator, ptttl_output_note_t *note,
                              ptttl_note_stream_t *note_stream, uint32_t channel_idx)
{
    note_stream->sine_index = 0u;
    note_stream->start_sample = generator->current_sample;

    note_stream->phasor_state = 0.0f;

    /* Compute note duration directly from BPM and fraction index to avoid
     * any integer truncation in the time encoding. This gives exact sample
     * counts regardless of BPM and sample rate.
     *
     * Duration fractions: index 0=whole(1), 1=half(2), 2=quarter(4),
     *                     3=8th(8), 4=16th(16), 5=32nd(32) */
    static const unsigned int duration_divisors[6] = {1u, 2u, 4u, 8u, 16u, 32u};
    unsigned int dur_idx = PTTTL_NOTE_DURATION_IDX(note);
    unsigned int dot     = PTTTL_NOTE_DOT(note);
    unsigned int divisor = (dur_idx < 6u) ? duration_divisors[dur_idx] : 32u;

    // whole_note_samples = (60 / bpm) * 4 * sample_rate
    float whole_note_samples = (60.0f / (float) generator->parser->bpm)
                               * 4.0f
                               * (float) generator->config.sample_rate;
    float num_samples = whole_note_samples / (float) divisor;
    if (dot)
    {
        num_samples *= 1.5f;
    }

    // Carry forward the fractional sample remainder from previous notes on
    // this channel so truncation error never accumulates beyond one sample.
    num_samples += note_stream->sample_error;
    unsigned int num_samples_int = (unsigned int) num_samples;
    note_stream->sample_error = num_samples - (float) num_samples_int;
    note_stream->num_samples = num_samples_int;

    // Handle case where attack + delay is longer than note length
    unsigned int attack = generator->config.attack_samples;
    unsigned int decay = generator->config.decay_samples;
    if ((attack + decay) > note_stream->num_samples)
    {
        unsigned int diff = (attack + decay) - note_stream->num_samples;
        if (attack > decay)
        {
            attack = (attack > diff) ? attack - diff : 0u;
        }
        else
        {
            decay = (decay > diff) ? decay - diff : 0u;
        }
    }

    note_stream->attack = attack;
    note_stream->decay = decay;
    note_stream->vibrato_frequency = PTTTL_NOTE_VIBRATO_FREQ(note);
    note_stream->vibrato_variance = PTTTL_NOTE_VIBRATO_VAR(note);

    // Calculate note pitch from piano key number
    note_stream->note_number = PTTTL_NOTE_VALUE(note);

    if (0u != note_stream->note_number)
    {
        note_stream->pitch_hz = _note_pitches[note_stream->note_number - 1u];
    }

    // Call the waveform's per-note setup if provided
    if (NULL != note_stream->waveform.note_setup)
    {
        note_stream->waveform.note_setup(channel_idx, (uint8_t) note_stream->note_number,
                                         generator->config.sample_rate,
                                         note_stream->waveform.wgendata);
    }
}


/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_create(ptttl_parser_t *parser, ptttl_sample_generator_t *generator,
                                  ptttl_sample_generator_config_t *config)
{
    ASSERT(NULL != parser);
    ASSERT(NULL != generator);
    ASSERT(NULL != config);

    if (0u == parser->channel_count)
    {
        ERROR(parser, "PTTTL parser object has a channel count of 0");
        return -1;
    }

    if (config->sample_rate > 96000u)
    {
        ERROR(parser, "Invalid sample rate, greater than 96KHz not supported");
        return -1;
    }

    if ((config->amplitude > 1.0f) || (config->amplitude < 0.0f))
    {
        ERROR(parser, "Sample generator amplitude must be between 0.0 - 1.0");
        return -1;
    }

    // Copy config data into generator object
    generator->config = *config;
    generator->parser = parser;

    generator->current_sample = 0u;

    memset(generator->channel_finished, 0, sizeof(generator->channel_finished));
    memset(generator->note_streams, 0, sizeof(generator->note_streams));

    // Populate note streams for initial note on all channels
    for (uint32_t chan = 0u; chan < parser->channel_count; chan++)
    {
        ptttl_output_note_t note;
        int ret = ptttl_parse_next(generator->parser, chan, &note);

        // Skip any empty-track sentinels until we get a real note or EOF/error
        while ((ret == 0) && IS_EMPTY_TRACK_SENTINEL(&note))
        {
            ret = ptttl_parse_next(generator->parser, chan, &note);
        }

        if (ret != 0)
        {
            return ret;
        }

        _load_note_stream(generator, &note, &generator->note_streams[chan], chan);

        // Set default waveform generator
        generator->note_streams[chan].waveform = _builtin_waveforms[DEFAULT_WAVEFORM_TYPE];
    }

    return 0;
}

/**
 * Generate the next sample for the given note stream on the given channel
 *
 * @param generator      Pointer to initialized sample generator
 * @param stream         Pointer to ptttl_note_stream_t instance to generate a sample for
 * @param channel_idx    Channel index of channel the generated sample is for
 * @param sample         Pointer to location to store generated sample
 *
 * @return 0 if there are more samples remaining for the provided note stream,
             1 if no more samples, -1 if an error occurred
 */
static int _generate_channel_sample(ptttl_sample_generator_t *generator, ptttl_note_stream_t *stream,
                                    unsigned int channel_idx, float *sample)
{
    int ret = 0;

    // Generate next sample value for this channel
    if (0u == stream->note_number) // Note number 0 indicates pause/rest
    {
        *sample = 0.0f;
    }
    else
    {
        int32_t raw_sample = 0;

        if ((0u != stream->vibrato_frequency) || (0u != stream->vibrato_variance))
        {
            float vsine = _generate_waveform_point(&_builtin_waveforms[WAVEFORM_TYPE_SINE],
                                                   generator->config.sample_rate,
                                                   stream->vibrato_frequency,
                                                   stream->sine_index);
            float pitch_change_hz = ((float) stream->vibrato_variance) * vsine;
            float note_pitch_hz = stream->pitch_hz + pitch_change_hz;

            float vsample = stream->waveform.wgen(stream->phasor_state, note_pitch_hz,
                                                   generator->config.sample_rate,
                                                   stream->waveform.wgendata);

            float phasor_inc = note_pitch_hz / generator->config.sample_rate;
            stream->phasor_state += phasor_inc;
            if (stream->phasor_state >= 1.0f)
            {
                stream->phasor_state -= 1.0f;
            }

            raw_sample = (int32_t) (vsample * (float) MAX_SAMPLE_VALUE);
        }
        else
        {
            raw_sample = _generate_waveform_sample(&stream->waveform,
                                                   generator->config.sample_rate,
                                                   stream->pitch_hz, stream->sine_index);
        }

        stream->sine_index += 1u;

        // Handle attack & decay
        unsigned int samples_elapsed = generator->current_sample - stream->start_sample;
        unsigned int samples_remaining = stream->num_samples - samples_elapsed;

        // Modify channel sample amplitude based on attack/decay settings
        if (samples_elapsed < stream->attack)
        {
            raw_sample *= ((float) samples_elapsed) / ((float) stream->attack);
        }
        else if (samples_remaining < stream->decay)
        {
            raw_sample *= ((float) samples_remaining) / ((float) stream->decay);
        }

        // Set final desired amplitude for channel sample
        *sample = ((float) raw_sample) * generator->config.amplitude;
    }

    // Check if last sample for this note stream
    if ((generator->current_sample - stream->start_sample) >= stream->num_samples)
    {
        // Load the next note for this channel
        ptttl_output_note_t note;
        ret = ptttl_parse_next(generator->parser, channel_idx, &note);

        // Skip any empty-track sentinels — keep fetching until we get
        // a real note or EOF/error
        while ((ret == 0) && IS_EMPTY_TRACK_SENTINEL(&note))
        {
            ret = ptttl_parse_next(generator->parser, channel_idx, &note);
        }

        if (ret == 0)
        {
            _load_note_stream(generator, &note, &generator->note_streams[channel_idx],
                              channel_idx);
        }
    }

    return ret;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_set_waveform(ptttl_sample_generator_t *generator,
                                        uint32_t channel, ptttl_waveform_type_e type)
{
    ASSERT(NULL != generator);

    if (channel >= generator->parser->channel_count)
    {
        ERROR(generator->parser, "Invalid channel index");
        return -1;
    }

    if ((type < 0) || (type >= WAVEFORM_TYPE_COUNT))
    {
        ERROR(generator->parser, "Invalid waveform type");
        return -1;
    }

    ptttl_note_stream_t *stream = &generator->note_streams[channel];
    stream->waveform = _builtin_waveforms[type];

    if (WAVEFORM_TYPE_NOKIA == type)
    {
        stream->waveform.wgendata = &_nokia_wgendata[channel];
    }

    // Run note_setup immediately for the current note so the waveform takes
    // effect right away
    if (NULL != stream->waveform.note_setup)
    {
        stream->waveform.note_setup(channel, (uint8_t) stream->note_number,
                                    generator->config.sample_rate,
                                    stream->waveform.wgendata);
    }

    return 0;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_set_custom_waveform(ptttl_sample_generator_t *generator,
                                               uint32_t channel,
                                               const ptttl_waveform_t *waveform)
{
    ASSERT(NULL != generator);
    ASSERT(NULL != waveform);
    ASSERT(NULL != waveform->wgen);

    if (channel >= generator->parser->channel_count)
    {
        ERROR(generator->parser, "Invalid channel index");
        return -1;
    }

    ptttl_note_stream_t *stream = &generator->note_streams[channel];
    stream->waveform = *waveform;

    // Run note_setup immediately for the current note so the waveform takes
    // effect right away.
    if (NULL != stream->waveform.note_setup)
    {
        stream->waveform.note_setup(channel, (uint8_t) stream->note_number,
                                    generator->config.sample_rate,
                                    waveform->wgendata);
    }

    return 0;
}

/**
 * @see ptttl_sample_generator.h
 */
int ptttl_sample_generator_generate(ptttl_sample_generator_t *generator, uint32_t *num_samples,
                                    int16_t *samples)
{
    ASSERT(NULL != generator);
    ASSERT(NULL != num_samples);
    ASSERT(NULL != samples);

    uint32_t samples_to_generate = *num_samples;
    *num_samples = 0u;

    for (unsigned int samplenum = 0u; samplenum < samples_to_generate; samplenum++)
    {
        float summed_sample = 0.0f;
        unsigned int num_channels_provided = 0u;

        // Sum the current state of all channels to generate the next sample
        for (unsigned int chan = 0u; chan < generator->parser->channel_count; chan++)
        {
            if (1u == generator->channel_finished[chan])
            {
                // No more samples to generate for this channel
                continue;
            }

            num_channels_provided += 1u;
            ptttl_note_stream_t *stream = &generator->note_streams[chan];

            float chan_sample = 0.0f;
            int ret = _generate_channel_sample(generator, stream, chan, &chan_sample);
            if (ret < 0)
            {
                return ret;
            }

            generator->channel_finished[chan] = (uint8_t) ret;
            summed_sample += chan_sample;
        }

        if (num_channels_provided == 0u)
        {
            // Finished-- no samples left on any channel
            return 1;
        }

        generator->current_sample += 1u;
        samples[samplenum] = (int16_t) (summed_sample / (float) generator->parser->channel_count);
        *num_samples += 1u;
    }

    return 0;
}
