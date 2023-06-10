/* ptttl_config.h
 *
 * Configurable build options for ptttl_parser.c / ptttl_sample_generator.c / ptttl_to_wav.c
 *
 * See https://github.com/eriknyquist/ptttl for more details about PTTTL.
 *
 * Erik Nyquist 2023
 */

#ifndef PTTTL_CONFIG_H
#define PTTTL_CONFIG_H


#ifdef __cplusplus
    extern "C" {
#endif


/**
 * Maximum number of channels (note lanes) allowed in a single PTTTL file. This
 * setting will significantly impact the size of the ptttl_output_t struct-- more channels
 * makes the struct larger.
 */
#ifndef PTTTL_MAX_CHANNELS_PER_FILE
#define PTTTL_MAX_CHANNELS_PER_FILE  (6u)
#endif // PTTTL_MAX_CHANNELS_PER_FILE

/**
 * Maximum number of notes allowed in a single channel (note lane). This setting will
 * significantly impact the size of the ptttl_output_t struct-- more notes makes the
 * struct larger.
 */
#ifndef PTTTL_MAX_NOTES_PER_CHANNEL
#define PTTTL_MAX_NOTES_PER_CHANNEL  (64u)
#endif // PTTTL_MAX_NOTES_PER_CHANNEL

/**
 * Maximum size of the name (first colon-separated field in a PTTTL or RTTTL file).
 * This name is stored directly in the ptttl_output_t struct, so changing this size
 * directly affects the size of the ptttl_output_t struct.
 */
#ifndef PTTTL_MAX_NAME_LEN
#define PTTTL_MAX_NAME_LEN           (256u)
#endif // PTTTL_MAX_NAME_LEN

/**
 * If 1, vibrato information for each note will be store in the ptttl_output_t
 * struct, and samples generated by ptttl_sample_generator will apply vibrato as
 * described. Disabling vibrato makes the ptttl_output_t struct smaller.
 */
#ifndef PTTTL_VIBRATO_ENABLED
#define PTTTL_VIBRATO_ENABLED        (1u)
#endif // PTTTL_VIBRATO_ENABLED


#ifdef __cplusplus
    }
#endif


#endif // PTTTL_CONFIG
