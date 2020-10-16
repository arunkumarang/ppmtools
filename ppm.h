#ifndef PPM_H
#define PPM_H

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  u_char;
typedef unsigned short u_short;

typedef struct ppm
{
    int width;
    int height;
    int maxval;
    u_short *ch1;
    u_short *ch2;
    u_short *ch3;
} ppm_t;

ppm_t* alloc_ppm_buffer(int width, int height, int maxval);
void   free_ppm_buffer(ppm_t *image);
void   clear_ppm_buffer(ppm_t *image, u_short red, u_short green, u_short blue);

ppm_t* read_ppm_image(char *filename);
void   write_ppm_image(ppm_t *image, char *filename);

#ifdef __cplusplus
}
#endif

#endif /* PPM_H */

