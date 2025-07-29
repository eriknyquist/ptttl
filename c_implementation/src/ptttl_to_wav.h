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

#ifndef PTTTL_WAVFILE_GENERATION_STRATEGY
/**
 * Defines the strategy used by #ptttl_to_wav to generate .wav files. The
 * available options make various trade-offs between dynamic memory usage,
 * performance, and composability:
 *
 * - 0 (default): <b>Seeking not allowed, dynamic memory allocation not allowed.</b>
 *   This is the most portable and composable strategy. No dynamic/heap memory
 *   will be used, and no seek operations will be performed on the output file
 *   stream (allowing 'stdout' to be used as the output stream, for example).
 *   Due to the fact that we need to know how many sample points / frames there
 *   will be in order to generate the first few .wav bytes containing the header,
 *   and no seeking is allowed, this strategy comes with a performance cost,
 *   since the parser & sample generator must make two full passes of the input
 *   text; once to determine the total number of sample points / frames the
 *   generated .wav file will contain, and again to obtain the sample point /
 *   frame values for writing to the output stream.
 *
 * - 1: <b>Seeking not allowed, dynamic memory allocation allowed</b>.
 *      This option is more performant than 0, at the cost of dynamic memory
 *      allocation being required. Seek operations will not be performed on the
 *      output stream, but dynamic memory allocations will be performed.
 *
 * - 2: <b>Seeking allowed, dynamic memory allocation not allowed.</b>
 *      This strategy is the most performant, at the cost of composability.
 *      Seek operations will be performed on the output file stream, so only one
 *      pass of the parser & sample generator are required. No dynamic memory
 *      allocation will be performed.
 */
#define PTTTL_WAVFILE_GENERATION_STRATEGY 0
#endif

/**
 * Convert PTTTL/RTTTL source to .wav file
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
