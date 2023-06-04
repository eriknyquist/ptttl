// Sample file using ptttl_to_wav.c

#include <stdio.h>
#include <stdlib.h>
#include "ptttl_parser.h"
#include "ptttl_to_wav.h"

static FILE *fp = NULL;

static int _ptttl_readchar(char *nextchar)
{
    size_t ret = fread(nextchar, 1, 1, fp);
    return (1u == ret) ? 0 : 1;
}

int main(int argc, char *argv[])
{
    if (2 != argc)
    {
        printf("Usage: %s <ptttl or rtttl source file>\n", argv[0]);
        return -1;
    }

    fp = fopen(argv[1], "rb");
    if (NULL == fp)
    {
        printf("Unable to open file %s\n", argv[1]);
        return -1;
    }

    ptttl_output_t output;

    int ret = ptttl_parse(_ptttl_readchar, &output);
    if (ret < 0)
    {
        ptttl_parser_error_t err = ptttl_parser_error();
        printf("Error in %s (line %d, column %d): %s\n", argv[1], err.line, err.column, err.error_message);
        return -1;
    }

    fclose(fp);

    ret = ptttl_to_wav(&output, "test_file.wav");
    if (ret < 0)
    {
        printf("Error generating WAV file: %s\n", ptttl_to_wav_error());
        return ret;
    }

    printf("Done\n");
}
