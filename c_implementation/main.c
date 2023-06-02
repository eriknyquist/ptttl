#include <stdio.h>
#include <stdlib.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"


int main(int argc, char *argv[])
{
    char buf[2048];

    FILE *f = fopen(argv[1], "rb");
    if (NULL == f)
    {
        printf("Unable to open file %s\n", argv[1]);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    fread(buf, fsize, 1, f);
    buf[fsize] = '\0';
    fclose(f);

    ptttl_output_t output;
    ptttl_input_t input;

    input.input_text = buf;
    input.input_text_size = fsize;
    int ret = ptttl_parse(&input, &output);
    if (ret < 0)
    {
        ptttl_error_t err = ptttl_error();
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        return -1;
    }

    printf("ptttl_output_t size : %u\n\n", sizeof(ptttl_output_t));
    printf("Song name     : %s\n", output.name);
    printf("Channel count : %u\n", output.channel_count);

    for (unsigned int i = 0u; i < output.channel_count; i++)
    {
        printf("Channel %u: %u notes\n", i, output.channels[i].note_count);
    }

    ret = ptttl_to_wav(&output, "test_file.wav");
    if (ret < 0)
    {
        printf("Error generating WAV file: %s\n", ptttl_to_wav_error());
        return ret;
    }

    printf("Done\n");
}
