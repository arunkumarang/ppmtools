#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "ppm.h"

static void die(char *message)
{
    fprintf(stderr, "ppm: %s\n", message);
    exit(1);
}

static void checkDimension(int dim)
{
    if (dim < 1 || dim > SHRT_MAX ) {
        die("file contained unreasonable width or height");
    }
}

static void readPPMHeader(FILE *fp, int *width, int *height, int *maxval)
{
    char ch;
    
    if (fscanf(fp, "P%c\n", &ch) != 1 || ch != '6') {
        die("file is not in ppm raw format; cannot read");
    }

    /* skip comments */
    ch = getc(fp);
    while (ch == '#') {
        do {
            ch = getc(fp);
        } while (ch != '\n');	/* read to the end of the line */
        ch = getc(fp);            /* thanks, Elliot */
    }
    
    if (!isdigit(ch)) {
        die("cannot read header information from ppm file");
    }

    ungetc(ch, fp);		/* put that digit back */

    /* read the width, height, and maximum value for a pixel */
    fscanf(fp, "%d%d%d\n", width, height, maxval);

    checkDimension(*width);
    checkDimension(*height);
}

Image* ImageCreate(int width, int height, int maxval)
{
    Image *image = (Image *) malloc(sizeof(Image));
    
    int byte = sizeof(u_short);
    
    if (!image) { die("cannot allocate memory for new image"); }
  
    image->width  = width;
    image->height = height;
    image->maxval = maxval;
    
    if (NULL == (image->ch1 = (u_short *) malloc(width * height * byte))) { return NULL; }
    if (NULL == (image->ch2 = (u_short *) malloc(width * height * byte))) { return NULL; }
    if (NULL == (image->ch3 = (u_short *) malloc(width * height * byte))) { return NULL; }
    
    if (!image->ch1) { die("cannot allocate memory for new image"); }
    if (!image->ch2) { die("cannot allocate memory for new image"); }
    if (!image->ch3) { die("cannot allocate memory for new image"); }

    return image;
}

void ImageRelease(Image *image)
{
    if (!image) { die("cannot release memory for image"); }
    
    if (image->ch1) { free(image->ch1); image->ch1 = NULL; }
    if (image->ch2) { free(image->ch2); image->ch2 = NULL; }
    if (image->ch3) { free(image->ch3); image->ch3 = NULL; }

    free(image);
    
    image = NULL;
}

Image* ImageRead(char *filename)
{
    int width, height, maxval, num, size, byte, chsize, x, y, channel;
    u_short *data, *ch1, *ch2, *ch3;
	unsigned char *data0 = 0;
	
    
    Image *image = (Image *) malloc(sizeof(Image));
    FILE  *fp    = fopen(filename, "rb");
    
    if (!image) { die("cannot allocate memory for new image"); }
    if (!fp) { die("cannot open file for reading"); }
    
    readPPMHeader(fp, &width, &height, &maxval);
    
	channel       = 3;
    chsize        = width * height * sizeof(u_short);  //for 2 byte pixel
    byte          = maxval > 255 ? sizeof(u_short) : sizeof(u_char);
    size          = width * height * byte * channel;  //r,g,b channel
    data          = (u_short *) malloc(chsize * channel);
    
    ch1           = (u_short *) malloc(chsize);
    ch2           = (u_short *) malloc(chsize);
    ch3           = (u_short *) malloc(chsize);
    
    if (!data) { die("cannot allocate memory for new image"); }
    
    num = fread((void *) data, 1, (size_t) size, fp);
    if (num != size) { die("cannot read image data from file"); }
    
    fclose(fp);
    
	data0 = (unsigned char*) data;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width;  x++) {
			if (maxval > 255) {
				ch1[y * width + x] = (((data[y * (width * channel) + (x * channel) + 0] & 0x00ff) << 8) | ((data[y * (width * channel) + (x * channel) + 0] & 0xff00) >> 8));
				ch2[y * width + x] = (((data[y * (width * channel) + (x * channel) + 1] & 0x00ff) << 8) | ((data[y * (width * channel) + (x * channel) + 1] & 0xff00) >> 8));
				ch3[y * width + x] = (((data[y * (width * channel) + (x * channel) + 2] & 0x00ff) << 8) | ((data[y * (width * channel) + (x * channel) + 2] & 0xff00) >> 8));
			} else {
				ch1[y * width + x] = data0[y * (width * channel) + (x * channel) + 0];
				ch2[y * width + x] = data0[y * (width * channel) + (x * channel) + 1];
				ch3[y * width + x] = data0[y * (width * channel) + (x * channel) + 2];
			}
        }
    }
    
    image->width  = width;
    image->height = height;
    image->maxval = maxval;
    image->ch1    = ch1;
    image->ch2    = ch2;
    image->ch3    = ch3;
    
    if (data) { free(data); data = 0; }
    
    return image;
}

void ImageWrite(Image *image, char *filename)
{
    int num, x, y;
    int channel = 3;
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
				data[(y * image->width * channel) + (x * channel) + 0] = (((image->ch1[y * image->width + x] & 0x00ff) << 8) | ((image->ch1[y * image->width + x] & 0xff00) >> 8));
				data[(y * image->width * channel) + (x * channel) + 1] = (((image->ch2[y * image->width + x] & 0x00ff) << 8) | ((image->ch2[y * image->width + x] & 0xff00) >> 8));
				data[(y * image->width * channel) + (x * channel) + 2] = (((image->ch3[y * image->width + x] & 0x00ff) << 8) | ((image->ch3[y * image->width + x] & 0xff00) >> 8));
			} else {
				data0[(y * image->width * channel) + (x * channel) + 0] = (u_char)image->ch1[y * image->width + x];
				data0[(y * image->width * channel) + (x * channel) + 1] = (u_char)image->ch2[y * image->width + x];
				data0[(y * image->width * channel) + (x * channel) + 2] = (u_char)image->ch3[y * image->width + x];
			}
        }
    }
  
   fp = fopen(filename, "wb");
   if (!fp) { die("cannot open file for writing"); }
   
   fprintf(fp, "P6\n%d %d\n%d\n", image->width, image->height, image->maxval);
   num = fwrite((void *) data, 1, (size_t) (chsize * channel), fp);
   if (num != (chsize * channel)) { die("cannot write image data to file"); }
   
   fclose(fp);
   
   if (data) { free(data); data = 0; }
}  

int ImageWidth(Image *image)
{
    return image->width;
}

int ImageHeight(Image *image)
{
    return image->height;
}

void ImageClear(Image *image, u_short red, u_short green, u_short blue)
{
    int i;
    int pix = image->width * image->height;
    
    u_short *ch1 = image->ch1;
    u_short *ch2 = image->ch2;
    u_short *ch3 = image->ch3;

    for (i = 0; i < pix; i++) {
        *ch1++ = red;
        *ch2++ = green;
        *ch3++ = blue;
    }
}

void ImageSetPixel(Image *image, int x, int y, int chan, u_short val)
{
    int offset = (y * image->width + x);
    
    u_short *data = (chan == 0 ? image->ch1 : (chan == 1 ? image->ch2 : image->ch3));
    data[offset] = val;
}

u_short ImageGetPixel(Image *image, int x, int y, int chan)
{
    int offset = (y * image->width + x);
    
    u_short *data = (chan == 0 ? image->ch1 : (chan == 1 ? image->ch2 : image->ch3));
    
    return (u_short) data[offset];
}

