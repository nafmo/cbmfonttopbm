/*
 * font2pbm
 * Convert a Commodore 64 font to a Portable Bitmap (PBM) file.
 * Copyright 2003-2025 Peter Krefting <peter@softwolves.pp.se>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX:MIT-0
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Holder structure for a portable bitmap */
struct pbm
{
    int x, y;
    uint8_t *data;
};

uint8_t *readfile(FILE *, int, int);
struct pbm createpbm(int, int, const uint8_t *, int chars);
void printpbm(struct pbm);

int main(int argc, char *argv[])
{
    int xsize, ysize, chars, skip;
    FILE *file;
    uint8_t *data;
    struct pbm pbm;

    /* Check for -r flag */
    if (argc >= 2 && strcmp(argv[1], "-r") == 0)
    {
        skip = 0;
        -- argc;
        ++ argv;
    }
    else
    {
        /* Skip load address */
        skip = 2;
    }

    /* Help screen */
    if (argc < 3 || argc > 5)
    {
        printf("Usage: %s [-r] size num [filename]\n\n"
               "  -r:        ROM image (no load address)\n"
               "  size:      1x1, 1x2, 2x1 or 2x2\n"
               "  num:       Number of characters in font\n"
               "  filename:  Name of file to read\n",
               argv[0]);
        return 0;
    }

    /* Check parameters */
    if (sscanf(argv[1], "%dx%d", &xsize, &ysize) != 2 ||
        xsize < 1 || xsize > 2 ||
        ysize < 1 || ysize > 2)
    {
        fprintf(stderr, "%s: Illegal size specification \"%s\"\n",
                argv[0], argv[1]);
        return 1;
    }

    if (sscanf(argv[2], "%d", &chars) != 1)
    {
        fprintf(stderr, "%s: Illegal number of chars \"%s\"\n",
                argv[0], argv[2]);
        return 1;
    }

    if (4 == argc)
    {
        file = fopen(argv[3], "rb");
        if (!file)
        {
            fprintf(stderr, "%s: Can't open \"%s\": %s\n",
                    argv[0], argv[3], strerror(errno));
            return 1;
        }
    }
    else
    {
        file = stdin;
    }

    /* Read data */
    data = readfile(file, chars * xsize * ysize * 8, skip);
    if (4 == argc)
    {
        fclose(file);
    }

    if (!data)
    {
        fprintf(stderr, "%s: Invalid input from \"%s\"\n",
                argv[0], argv[3]);
        return 1;
    }

    /* Convert to PBM */
    pbm = createpbm(xsize, ysize, data, chars);
    if (!pbm.data)
    {
        fprintf(stderr, "%s: Out of memroy\n", argv[0]);
        return 1;
    }

    /* Output the image */
    printpbm(pbm);

    /* Clean up */
    free(data);
    free(pbm.data);
    return 0;
}

uint8_t *readfile(FILE *file, int bytes, int skip)
{
    uint8_t *buffer;
    size_t length;
    int i;

    /* Skip load address */
    for (i = 0; i < skip; ++ i)
    {
        fgetc(file);
    }

    /* Slurp everything */
    buffer = (uint8_t *) malloc(bytes);
    if (buffer)
    {
        length = fread(buffer, 1, bytes, file);
        if (length != bytes)
        {
            free(buffer);
            buffer = NULL;
        }
    }

    return buffer;
}

struct pbm createpbm(int x, int y, const uint8_t *data, int numchars)
{
    int charsperline, i;
    struct pbm output;

    /*
     * Output characters, one by one. With 256 pixels width, we can output
     * 32 1�1 or 1�2 characters in a line, or 16 2�1 or 2�2 characters.
     * The created image is 64 pixels high, which is 8 characters in 1�1 or
     * 2�1, or 4 characters in 1�2 or 2�2.
     */
    output.x = 256;
    charsperline = 32 / x;

    /*
     * The height of the output image depends on the number of characters
     * in the font.
     */
    output.y = numchars / charsperline * 8 * y;

    /* Allocate the data for the bitmap */
    output.data = malloc(256 / 8 * output.y);
    if (!output.data) return output;

    /* Convert characters */
    for (i = 0; i < numchars; ++ i)
    {
        int xchar, ychar;

        /*
         * Calculate (x,y) coordinate in bitmap where this character is to
         * be written.
         */

        for (xchar = 0; xchar < x; ++ xchar)
        {
            int xpos;
            xpos = (i % charsperline) * 8 * x + xchar * 8;
            for (ychar = 0; ychar < y; ++ ychar)
            {
                int fontofs, pbmofs, line, ypos;

                ypos = (i / charsperline) * 8 * y + 8 * ychar;
                /* Calculate index in font data for this character */
                fontofs = (i + xchar * numchars + ychar * numchars * x) * 8;

                /*
                 * Calculate first byte offset in bitmap for this part of the
                 * character.
                 */
                pbmofs = ypos * 256 / 8 + xpos / 8;
                for (line = 0; line < 8; ++ line)
                {
                    /* Font data has eight consecutive scan lines */
                    output.data[pbmofs + line * 256 / 8] = data[fontofs + line];
                }
            }
        }
    }

    return output;
}

void printpbm(struct pbm pbm)
{
    /* PBM header */
    printf("P4\n"
           "# Commodore 64 font converted by font2pbm\n"
           "%d %d\n", pbm.x, pbm.y);

    /* Image data */
    fwrite(pbm.data, 1, 256 / 8 * pbm.y, stdout);
}
