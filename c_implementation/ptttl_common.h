/* ptttl_common.h
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
#ifndef PTTTL_COMMON_H
#define PTTTL_COMMON_H


#ifdef __cplusplus
    extern "C" {
#endif


// Max. value allowed for note octave
#define NOTE_OCTAVE_MAX (8u)

// Number of valid note duration values
#define NOTE_DURATION_COUNT (6u)


/**
 * Enumerates all possible musical notes in a single octave
 */
typedef enum
{
    NOTE_C = 0,        // C
    NOTE_CS,           // C sharp
    NOTE_DB = NOTE_CS, // D flat
    NOTE_D,            // D
    NOTE_DS,           // D sharp
    NOTE_EB = NOTE_DS, // E flat
    NOTE_E,            // E
    NOTE_ES,           // E sharp
    NOTE_F = NOTE_ES,  // F
    NOTE_FS,           // F sharp
    NOTE_GB = NOTE_FS, // G flat
    NOTE_G,            // G
    NOTE_GS,           // G sharp
    NOTE_AB = NOTE_GS, // A flat
    NOTE_A,            // A
    NOTE_AS,           // A sharp
    NOTE_BB = NOTE_AS, // B flat
    NOTE_B,            // B
    NOTE_INVALID, // Unecognized/invalid note
    NOTE_PITCH_COUNT = NOTE_INVALID
} note_pitch_e;


/* Key number (0-based) of the first key (C) for each octave. Index 0 holds
 * the key number of the first key for octave 0, index 1 holds the key number
 * of the first key for octave 1, and so on. */
static const uint32_t _octave_starts[NOTE_OCTAVE_MAX + 1u] = {0u, 3u, 15u, 27u, 39u, 51u, 63u, 75u, 87u};


#ifdef __cplusplus
    }
#endif


#endif // PTTTL_COMMON_H

