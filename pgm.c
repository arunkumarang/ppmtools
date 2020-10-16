/*
 * pgm.c: read and write pgm image.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "pgm.h"

static void die(char *message)
{
    fprintf(stderr, "ppm: %s\n", message);
    exit(1);
}

static void check_dimension(int dim)
{
    if (dim < 1 || dim > SHRT_MAX ) {
        die("file contained unreasonable width or height");
    }
}

int get_pgm_width(pgm_t *image)
{
    return image->width;
}

int get_pgm_height(pgm_t *image)
{
    return image->height;
}

void set_pgm_pixel(pgm_t *image, int x, int y, u_short val)
{
    int offset = (y * image->width + x);

    u_short *data = image->ch;
    data[offset] = val;
}

u_short get_pgm_pixel(pgm_t *image, int x, int y)
{
    int offset = (y * image->width + x);

    u_short *data = image->ch;

    return (u_short) data[offset];
}

static void read_pgm_header(FILE *fp, int *width, int *height, int *maxval)
{
    char ch;
    if (fscanf(fp, "P%c\n", &ch) != 1 || ch != '5') {
        die("file is not in ppm raw format; cannot read");
    }

    ch = getc(fp);
    while (ch == '#') {
        do {
            ch = getc(fp);
        } while (ch != '\n');
        ch = getc(fp);
    }

    if (!isdigit(ch)) {
        die("cannot read header information from ppm file");
    }

    ungetc(ch, fp);

    /* read the width, height, and maximum value for a pixel */
    fscanf(fp, "%d%d%d\n", width, height, maxval);

    check_dimension(*width);
    check_dimension(*height);
}

pgm_t* alloc_pgm_buffer(int width, int height, int maxval)
{
    pgm_t *image = (pgm_t *) malloc(sizeof(pgm_t));

    int byte = sizeof(u_short);

    if (!image) { die("cannot allocate memory for new image"); }

    image->width  = width;
    image->height = height;
    image->maxval = maxval;

    if (NULL == (image->ch = (u_short *) malloc(width * height * byte))) { return NULL; }

    if (!image->ch) { die("cannot allocate memory for new image"); }

    return image;
}

void free_pgm_buffer(pgm_t *image)
{
    if (!image) { die("cannot release memory for image"); }

    if (image->ch) { free(image->ch); image->ch = NULL; }

    free(image);

    image = NULL;
}

void clear_pgm_image(pgm_t *image, u_short grey)
{
    int i;
    int pix = image->width * image->height;

    u_short *ch = image->ch;

    for (i = 0; i < pix; i++) {
        *ch++ = grey;
    }
}

pgm_t* read_pgm_image(char *filename)
{
    int width, height, maxval, num, size, byte, chsize, x, y, channel;
    u_short *data, *ch = NULL;
    unsigned char *data0 = 0;

    pgm_t *image = (pgm_t*) malloc(sizeof(pgm_t));
    FILE  *fp    = fopen(filename, "rb");

    if (!image) { die("cannot allocate memory for new image"); }
    if (!fp) { die("cannot open file for reading"); }

    read_pgm_header(fp, &width, &height, &maxval);

    channel = 1;
    chsize  = width * height * sizeof(u_short);  //for 2 byte pixel
    byte    = maxval > 255 ? sizeof(u_short) : sizeof(u_char);
    size    = width * height * byte * channel;  //r,g,b channel
    data    = (u_short *) malloc(chsize * channel);
    ch      = (u_short *) malloc(chsize);

    if (!data) { die("cannot allocate memory for new image"); }

    num = fread((void *) data, 1, (size_t) size, fp);
    if (num != size) { die("cannot read image data from file"); }

    fclose(fp);

    data0 = (unsigned char*) data;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width;  x++) {
            if (maxval > 255) {
                ch[y * width + x] = (((data[y * (width * channel) + (x * channel) + 0] & 0x00ff) << 8) |
                                     ((data[y * (width * channel) + (x * channel) + 0] & 0xff00) >> 8));
            } else {
                ch[y * width + x] = data0[y * (width * channel) + (x * channel) + 0];
            }
        }
    }

    image->width  = width;
    image->height = height;
    image->maxval = maxval;
    image->ch     = ch;

    if (data) { free(data); data = 0; }

    return image;
}

void write_pgm_image(pgm_t *image, char *filename)
{
    int num, x, y;
    int channel = 1;
    int byte = image->maxval > 255 ? sizeof(u_short) : sizeof(u_char);
    int pitch = image->width * byte;
    int chsize  = pitch * image->height;
    FILE *fp = 0;

    u_char *data0 = 0;
    u_short *data     = (u_short *) malloc(chsize * channel);
    if (!data) { die("cannot allocate memory for new image"); }

    data0 = (u_char*) data;
    for (y = 0; y < image->height; y++) {
        for (x = 0; x < image->width;  x++) {
            if (image->maxval > 255) {
                data[(y * image->width * channel) + (x * channel) + 0] = (((image->ch[y * image->width + x] & 0x00ff) << 8) |
                                                                          ((image->ch[y * image->width + x] & 0xff00) >>8 ));
            } else {
                data0[(y * image->width * channel) + (x * channel) + 0] = (u_char) image->ch[y * image->width + x];
            }
        }
    }

   fp = fopen(filename, "wb");
   if (!fp) { die("cannot open file for writing"); }

   fprintf(fp, "P5\n%d %d\n%d\n", image->width, image->height, image->maxval);
   num = fwrite((void *) data, 1, (size_t) (chsize * channel), fp);
   if (num != (chsize * channel)) { die("cannot write image data to file"); }

   fclose(fp);

   if (data) { free(data); data = 0; }
}
