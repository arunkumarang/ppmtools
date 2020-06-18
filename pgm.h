#ifndef PGM_H
#define PGM_H

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  u_char;
typedef unsigned short u_short;

typedef struct ImagePGM
{
    int width;
    int height;
    int maxval;
    u_short *ch;
} ImagePGM;

ImagePGM *ImagePGMCreate(int width, int height, int maxval);
void   ImagePGMRelease(ImagePGM *image);
ImagePGM *ImagePGMRead(char *filename);
void   ImagePGMWrite(ImagePGM *image, char *filename);

int    ImagePGMWidth(ImagePGM *image);
int    ImagePGMHeight(ImagePGM *image);

void   ImagePGMClear(ImagePGM *image, u_short grey);

void   ImagePGMSetPixel(ImagePGM *image, int x, int y, u_short val);
u_short ImagePGMGetPixel(ImagePGM *image, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* PGM_H */

