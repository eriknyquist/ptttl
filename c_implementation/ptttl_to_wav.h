/* ptttl_to_wav.c
 *
 * Converts the output of ptttl_parse() into a WAV file.
 * No dynamic memory allocation, and no need to have enough memory to hold the
 * entire WAV file in memory.
 *
 * Requires ptttl_parser.c and ptttl_sample_generator.c
 *
 * Requires stdint.h, and fopen/fseek/fwrite from stdio.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#ifndef PTTTL_TO_WAV_H
#define PTTTL_TO_WAV_H

#include "ptttl_parser.h"

/**
 * Return pointer to a string describing the last error that occurred
 *
 * @return Pointer to error string, or NULL if no error occurred
 */
const char *ptttl_to_wav_error(void);

/**
 * Generate samples for some parsed PTTTL data and write them directly to a .wav file.
 * No dynamic memory allocation. Does not require holding the entire .wav file in memory
 * at once.
 *
 * @param parsed_ptttl   Pointer to parsed PTTTL data
 * @param wav_filename   Pointer to name of .wav file to create. Must be writeable.
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_to_wav_error for
 *         an error description if -1 is returned.
 */
int ptttl_to_wav(ptttl_output_t *parsed_ptttl, const char *wav_filename);

#endif // PTTTL_TO_WAV_H
