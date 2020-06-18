#ifndef PPM_H
#define PPM_H

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  u_char;
typedef unsigned short u_short;

typedef struct Image
{
    int width;
    int height;
    int maxval;
    u_short *ch1;
    u_short *ch2;
    u_short *ch3;
} Image;

Image *ImageCreate(int width, int height, int maxval);
void   ImageRelease(Image *image);
Image *ImageRead(char *filename);
void   ImageWrite(Image *image, char *filename);

int    ImageWidth(Image *image);
int    ImageHeight(Image *image);

void   ImageClear(Image *image, u_short red, u_short green, u_short blue);

void   ImageSetPixel(Image *image, int x, int y, int chan, u_short val);
u_short ImageGetPixel(Image *image, int x, int y, int chan);

#ifdef __cplusplus
}
#endif

#endif /* PPM_H */

