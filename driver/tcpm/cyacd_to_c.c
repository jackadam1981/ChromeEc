/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Utility to convert from CYACD file to byte array for Cypress EZ-PD CCG6DF, CCG6SF bootloader */

#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

static void output_line(FILE *dst, const char *line)
{
    if (fwrite(line, 1, strlen(line), dst) != strlen(line))
    {
        err(1, "error writing to output file");
    }
}

int main(int argc, char *argv[])
{
    char line[256];
    char rline[300];
    int row_n;
    int row_idx;
    int i;
    int metafound; 
    FILE *src;
    FILE *dst;
    int j;
   

    /* check args, open files */
    if (4 != argc)
        err(1, "  Usage is: acd_to_c <ACD file> -|<output file> <image number>\n");

    src = fopen(argv[1], "r");
    if (src == 0)
        err(1, "can't open '%s'\n", argv[1]);

    if (!strcmp(argv[2], "-"))
    {
        dst = stdout;
    }
    else
    {
        if ((dst = fopen(argv[2], "w+")) == 0)
            err(1, "can't open '%s'\n", argv[2]);
    }

    sprintf(line, "#include <stdint.h>\n");
    output_line(dst, line);
    sprintf(line, "const uint8_t image%s[] = {\n", argv[3]);
    output_line(dst, line);

    // skip first line with attributes
    fgets(rline, 300, src);

    row_n = 0; 
    metafound = 0;
    do
    {
        row_idx = 11;
        fgets(rline, 300, src);

        // check if metadata row
        if ((rline[4]=='1') && (rline[5]=='F') && ((rline[6]=='E') || (rline[6]=='F')))
            {
                sprintf(line, "};\n\nconst uint8_t image%s_metadata[] = {\n", argv[3]);
                output_line(dst, line);
                metafound = 1;
            }    

        sprintf(line, "// row %d\n", row_n++);
        output_line(dst, line);
        for(i=0; i<8; i++)
        {

            for (j=0; j<16; j++)
            {
                sprintf(line, "0x%c%c,", rline[row_idx], rline[row_idx+1]);
                output_line(dst, line);
                row_idx+=2;
            }
            sprintf(line, "\n");
            output_line(dst, line);
        }
        if (metafound) break;
    } while (!feof(src));

    sprintf(line, "};\n\nconst int image%s_rows = %d;\n\n", argv[3], row_n-1);
    output_line(dst, line);

    fclose(src);
    fclose(dst);

    return 0;
}
