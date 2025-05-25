/* ptttl_common.h
 *
 * Data/types common across several ptttl source files
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */
#ifndef PTTTL_COMMON_H
#define PTTTL_COMMON_H


#ifdef __cplusplus
    extern "C" {
#endif


#include <stdint.h>


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


#define ASSERT(cond) { if (!(cond)) __builtin_trap(); }

#ifdef __cplusplus
    }
#endif


#endif // PTTTL_COMMON_H

