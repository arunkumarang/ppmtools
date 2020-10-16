#ifndef PGM_H
#define PGM_H

#include <sys/types.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char  u_char;
typedef unsigned short u_short;

typedef struct pgm
{
    int width;
    int height;
    int maxval;
    u_short *ch;
} pgm_t;

pgm_t* alloc_pgm_buffer(int width, int height, int maxval);
void   free_pgm_buffer(pgm_t *image);
void   clear_pgm_image(pgm_t *image, u_short grey);

pgm_t* read_pgm_image(char *filename);
void   write_pgm_image(pgm_t *image, char *filename);

#ifdef __cplusplus
}
#endif

#endif /* PGM_H */

