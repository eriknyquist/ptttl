/* ptttl_to_wav.h
 *
 * Converts the output of ptttl_parse() into a WAV file.
 * No dynamic memory allocation, and no loading the entire WAV file in memory.
 *
 * Requires ptttl_parser.c and ptttl_sample_generator.c
 *
 * Requires stdint.h, and fopen/fseek/fwrite from stdio.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2025
 */

#ifndef PTTTL_TO_WAV_H
#define PTTTL_TO_WAV_H


#include "ptttl_parser.h"


#ifdef __cplusplus
    extern "C" {
#endif


/**
 * Return error info after ptttl_to_wav has returned -1
 *
 * @param   Pointer to initialized parser object
 *
 * @return  Object describing the error that occurred. error_message field will be NULL
 *          if no error has occurred.
 */
ptttl_parser_error_t ptttl_to_wav_error(ptttl_parser_t *parser);

/**
 * Generate samples for some parsed PTTTL data and write them directly to a .wav file.
 * No dynamic memory allocation. Does not require holding the entire .wav file in memory
 * at once.
 *
 * @param parser         Pointer to initialized parser object
 * @param wav_filename   Pointer to name of .wav file to create. Must be writeable.
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_to_wav_error for
 *         an error description if -1 is returned.
 */
int ptttl_to_wav(ptttl_parser_t *parser, const char *wav_filename);


#ifdef __cplusplus
    }
#endif

#endif // PTTTL_TO_WAV_H
