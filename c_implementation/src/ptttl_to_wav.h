/**
 * @file ptttl_to_wav.h
 *
 * @brief Converts the output of ptttl_parse() into a WAV file.
 * No dynamic memory allocation, and no loading the entire WAV file in memory.
 *
 * Requires ptttl_parser.c and ptttl_sample_generator.c
 *
 * Requires stdint.h, and fopen/fseek/fwrite from stdio.h
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * @author Erik Nyquist
 */

#ifndef PTTTL_TO_WAV_H
#define PTTTL_TO_WAV_H


#include "ptttl_parser.h"
#include "ptttl_sample_generator.h"


#ifdef __cplusplus
    extern "C" {
#endif


/**
 * Generate samples for some parsed PTTTL data and write them directly to a .wav file.
 * No dynamic memory allocation. Does not require holding the entire .wav file in memory
 * at once.
 *
 * @param parser         Pointer to initialized parser object
 * @param fp             File stream that has already been opened for writing. Generated
 *                       contents of .wav file will be written here.
 * @param config         Pointer to configuration for sample generator. May be NULL.
 *                       If NULL, a default configuration will be used.
 * @param wave_type      Waveform type to use for all channels
 *
 * @return 0 if successful, -1 if an error occurred. Call #ptttl_parser_error for
 *         an error description if -1 is returned.
 */
int ptttl_to_wav(ptttl_parser_t *parser, FILE *fp, ptttl_sample_generator_config_t *config,
                 ptttl_waveform_type_e wave_type);


#ifdef __cplusplus
    }
#endif

#endif // PTTTL_TO_WAV_H
