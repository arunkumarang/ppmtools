/*
 * Ppmtools to convert the color space, bayer format, bit-depth, scale the 
 * image, and find the difference between images.
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ppm.h"
#include "pgm.h"
#include "version.h"

/* ---------- macro definition ---------- */

#define FILTER_BICUBIC_ENABLE   1

#define CLIP(X)  ((X) > 255 ? 255 : (X) < 0 ? 0 : X)
#define SIGN(X)  (X < 0 ? -X : X)

// RGB -> YCbCr
#define CRGB2Y(R, G, B) CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16)
#define CRGB2Cb(R, G, B) CLIP((36962 * (B - CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16) ) >> 16) + 128)
#define CRGB2Cr(R, G, B) CLIP((46727 * (R - CLIP((19595 * R + 38470 * G + 7471 * B ) >> 16) ) >> 16) + 128)

// YCbCr -> RGB
#define CYCbCr2R(Y, Cb, Cr) CLIP( Y + ( 91881 * Cr >> 16 ) - 179 )
#define CYCbCr2G(Y, Cb, Cr) CLIP( Y - (( 22544 * Cb + 46793 * Cr ) >> 16) + 135)
#define CYCbCr2B(Y, Cb, Cr) CLIP( Y + (116129 * Cb >> 16 ) - 226 )

void usage(void)
{
    fprintf (stdout, "usage: ppmtools option [arguments]");
    fprintf (stdout, "\n  -d  file1.ppm  file2.ppm  diff_file.ppm                                \
                      \n  -s  in_file.ppm  out_file.ppm  bit_depth (8 - 16)                              \
                      \n  -z  in_file.ppm  out_file.ppm  scale_factor (0.1 - 8.0)                      \
                      \n  -c  in_file.ppm  out_file.ppm  convert_opt (0: yuv to rgb, 1: rgb to yuv)      \
                      \n  -b  in_file.ppm  out_file.ppm  convert_opt (0: ppm to bayer, 1: bayer to ppm)  \
                      \n  -v  version number                                                             \
                      \n  -h  help                                                                       \
                      \n");
    exit(1);
}

void version_num(void)
{
    fprintf (stdout, "%s\n", version);
}

static void die(const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fputc('\n', stderr);
    exit(1);
}

int sat(int X, int L0, int L1) { return (X < L0 ? L0 : (X > L1 ? L1 : X)); }

float bicubic_filter(float A, float B, float C, float D, float t)
{
    float a = -A / 2.0f + (3.0f*B) / 2.0f - (3.0f*C) / 2.0f + D / 2.0f;
    float b = A - (5.0f*B) / 2.0f + 2.0f*C - D / 2.0f;
    float c = -A / 2.0f + C / 2.0f;
    float d = B;

    return a*t*t*t + b*t*t + c*t + d;
}

u_short get_pixel(u_short *src, int x, int y, int w, int h)
{
    x = sat(x, 0, w-1);
    y = sat(y, 0, h-1);

    return src[y * w + x];
}

u_short bicubic(u_short *src, float u, float v, int sw, int sh, int mv)
{
    u_short p00, p10, p20, p30;
    u_short p01, p11, p21, p31;
    u_short p02, p12, p22, p32;
    u_short p03, p13, p23, p33;

    float col0, col1, col2, col3, value;
    float x, y, xfract, yfract;
    int xint, yint;

    x = (float)u - 0.5f;
    y = (float)v - 0.5f;

    xint = (int)x;
    yint = (int)y;

    xfract = x - floorf(x);
    yfract = y - floorf(y);

    /* 1st row */
    p00 = get_pixel(src, xint - 1, yint - 1, sw, sh);
    p10 = get_pixel(src, xint + 0, yint - 1, sw, sh);
    p20 = get_pixel(src, xint + 1, yint - 1, sw, sh);
    p30 = get_pixel(src, xint + 2, yint - 1, sw, sh);

    /* 2nd row */
    p01 = get_pixel(src, xint - 1, yint + 0, sw, sh);
    p11 = get_pixel(src, xint + 0, yint + 0, sw, sh);
    p21 = get_pixel(src, xint + 1, yint + 0, sw, sh);
    p31 = get_pixel(src, xint + 2, yint + 0, sw, sh);

    /* 3rd row */
    p02 = get_pixel(src, xint - 1, yint + 1, sw, sh);
    p12 = get_pixel(src, xint + 0, yint + 1, sw, sh);
    p22 = get_pixel(src, xint + 1, yint + 1, sw, sh);
    p32 = get_pixel(src, xint + 2, yint + 1, sw, sh);

    /* 4th row */
    p03 = get_pixel(src, xint - 1, yint + 2, sw, sh);
    p13 = get_pixel(src, xint + 0, yint + 2, sw, sh);
    p23 = get_pixel(src, xint + 1, yint + 2, sw, sh);
    p33 = get_pixel(src, xint + 2, yint + 2, sw, sh);

    col0 = bicubic_filter(p00, p10, p20, p30, xfract);
    col1 = bicubic_filter(p01, p11, p21, p31, xfract);
    col2 = bicubic_filter(p02, p12, p22, p32, xfract);
    col3 = bicubic_filter(p03, p13, p23, p33, xfract);

    value = bicubic_filter(col0, col1, col2, col3, yfract);

    return (u_short) sat((int)value, 0, mv);
}

void apply_bicubic(ppm_t *src, ppm_t *dst, int dst_width, int dst_height)
{
    int u, v;
    for (v = 0; v < dst_height; v++) {
        for (u = 0; u < dst_width; u++) {
            dst->ch1[v * dst_width + u] = bicubic(src->ch1, (float)u, (float)v, src->width, src->height, src->maxval);
            dst->ch2[v * dst_width + u] = bicubic(src->ch2, (float)u, (float)v, src->width, src->height, src->maxval);
            dst->ch3[v * dst_width + u] = bicubic(src->ch3, (float)u, (float)v, src->width, src->height, src->maxval);
        }
    }
}

int bilinear(float p11, float p12, float p21, float p22, float x, float y)
{
    int xx, yy;
    float u_ratio, v_ratio, u_opposite, v_opposite, result;

    x = x - 0.5f;
    y = y - 0.5f;
    xx = (int)floorf(x);
    yy = (int)floorf(y);

    u_ratio = x - xx;
    v_ratio = y - yy;
    u_opposite = 1.f - u_ratio;
    v_opposite = 1.f - v_ratio;

    result = (p11 * u_opposite  + p12 * u_ratio) * v_opposite +
             (p21 * u_opposite  + p22 * u_ratio) * v_ratio;

    return (int) (result);
}

void apply_bilinear(ppm_t *src, ppm_t *dst, int dst_width, int dst_height)
{
    int x = 0, y = 0;
    dst->width = dst_width;
    dst->height = dst_height;
    dst->maxval = src->maxval;

    for (y = 0; y < dst_height; y++) {
        for (x = 0; x < dst_width; x++) {
            dst->ch1[y * dst_width + x] = bilinear((float)src->ch1[y * src->width + x],
                                                  (float)src->ch1[y * src->width + (x + 1)],
                                                  (float)src->ch1[(y+1) * src->width + x],
                                                  (float)src->ch1[(y+1) * src->width + (x + 1)],
                                                  (float)x, (float)y);

            dst->ch2[y * dst_width + x] = bilinear((float)src->ch2[y * src->width + x],
                                                  (float)src->ch2[y * src->width + (x + 1)],
                                                  (float)src->ch2[(y+1) * src->width + x],
                                                  (float)src->ch2[(y+1) * src->width + (x + 1)],
                                                  (float)x, (float)y);

            dst->ch3[y * dst_width + x] = bilinear((float)src->ch3[y * src->width + x],
                                                  (float)src->ch3[y * src->width + (x + 1)],
                                                  (float)src->ch3[(y+1) * src->width + x],
                                                  (float)src->ch3[(y+1) * src->width + (x + 1)],
                                                  (float)x, (float)y);
        }
    }
}

void diff_image(char *diff_name, char *src_name, char *dst_name)
{
    int x = 0, y = 0;
    ppm_t *src = read_ppm_image(src_name);
    ppm_t *dst = read_ppm_image(dst_name);
    ppm_t *diff = 0;

    if (NULL == (diff = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width != dst->width) || (src->height != dst->height)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            float val1 = ((float)src->ch1[y * src->width + x] / (float)src->maxval) -
                         ((float)dst->ch1[y * dst->width + x] / (float)dst->maxval);
            float val2 = ((float)src->ch2[y * src->width + x] / (float)src->maxval) -
                         ((float)dst->ch2[y * dst->width + x] / (float)dst->maxval);
            float val3 = ((float)src->ch3[y * src->width + x] / (float)src->maxval) -
                         ((float)dst->ch3[y * dst->width + x] / (float)dst->maxval);

            diff->ch1[y * diff->width + x] = (int)(SIGN(val1) * (float)diff->maxval);
            diff->ch2[y * diff->width + x] = (int)(SIGN(val2) * (float)diff->maxval);
            diff->ch3[y * diff->width + x] = (int)(SIGN(val3) * (float)diff->maxval);
        }
    }

    printf("diff image '%s'", diff_name);
    write_ppm_image(diff, diff_name);

    free_ppm_buffer(src);
    free_ppm_buffer(dst);
    free_ppm_buffer(diff);
}

void conv_bitdepth(char *src_name, char *dst_name, int bit_depth)
{
    int x = 0, y = 0;
    ppm_t *src = read_ppm_image(src_name);
    ppm_t *dst = NULL;
    int dst_maxval = (1 << bit_depth) - 1;

    if (NULL == (dst = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            float val1 = (float)src->ch1[y * src->width + x] / (float)src->maxval;
            float val2 = (float)src->ch2[y * src->width + x] / (float)src->maxval;
            float val3 = (float)src->ch3[y * src->width + x] / (float)src->maxval;

            dst->ch1[y * dst->width + x] = (u_short)(val1 * (float)dst_maxval);
            dst->ch2[y * dst->width + x] = (u_short)(val2 * (float)dst_maxval);
            dst->ch3[y * dst->width + x] = (u_short)(val3 * (float)dst_maxval);
        }
    }
    dst->maxval = dst_maxval;

    printf("rescaled image '%s'", dst_name);
    write_ppm_image(dst, dst_name);

    free_ppm_buffer(src);
    free_ppm_buffer(dst);
}

void ppm_to_bayer(char *src_name, char *dst_name)
{
    int bayer_maxval = 0, x = 0, y = 0;
    ppm_t *src = read_ppm_image(src_name);
    pgm_t *dst = NULL;

    bayer_maxval = (1 << 16) - 1;

    if (NULL == (dst = alloc_pgm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y+=2) {
        for (x = 0; x < src->width; x+=2) {
            int R1 = (src->ch1[(y+0) * src->width + (x+0)] +
                      src->ch1[(y+0) * src->width + (x+1)] +
                      src->ch1[(y+1) * src->width + (x+0)] +
                      src->ch1[(y+1) * src->width + (x+1)]) >> 2;

            int G2 = (src->ch2[(y+0) * src->width + (x+0)] +
                      src->ch2[(y+0) * src->width + (x+1)]) >> 1;

            int G3 = (src->ch2[(y+1) * src->width + (x+0)] +
                      src->ch2[(y+1) * src->width + (x+1)]) >> 1;

            int B4 = (src->ch3[(y+0) * src->width + (x+0)] +
                      src->ch3[(y+0) * src->width + (x+1)] +
                      src->ch3[(y+1) * src->width + (x+0)] +
                      src->ch3[(y+1) * src->width + (x+1)]) >> 2;

            dst->ch[(y+0) * dst->width + (x+0)] = (u_short)(((float)R1 / (float)src->maxval) * (float)bayer_maxval);
            dst->ch[(y+0) * dst->width + (x+1)] = (u_short)(((float)G2 / (float)src->maxval) * (float)bayer_maxval);
            dst->ch[(y+1) * dst->width + (x+0)] = (u_short)(((float)G3 / (float)src->maxval) * (float)bayer_maxval);
            dst->ch[(y+1) * dst->width + (x+1)] = (u_short)(((float)B4 / (float)src->maxval) * (float)bayer_maxval);
        }
    }
    dst->maxval = bayer_maxval;

    printf("bayer image '%s'", dst_name);
    write_pgm_image(dst, dst_name);

    free_ppm_buffer(src);
    free_pgm_buffer(dst);
}

void bayer_to_ppm(char *src_name, char *dst_name)
{
    int x = 0, y = 0;
    pgm_t *src = read_pgm_image(src_name);
    ppm_t *bilinear = NULL;
    ppm_t *dst = NULL;

    if (NULL == (bilinear = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if (NULL == (dst = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y+=2) {
        for (x = 0; x < src->width; x+=2) {
            u_short R1 = src->ch[(y+0) * src->width + (x+0)];
            u_short G2 = src->ch[(y+0) * src->width + (x+1)];
            u_short G3 = src->ch[(y+1) * src->width + (x+0)];
            u_short B4 = src->ch[(y+1) * src->width + (x+1)];

            dst->ch1[(y+0) * dst->width + (x+0)] = R1;
            dst->ch1[(y+0) * dst->width + (x+1)] = R1;
            dst->ch1[(y+1) * dst->width + (x+0)] = R1;
            dst->ch1[(y+1) * dst->width + (x+1)] = R1;

            dst->ch2[(y+0) * dst->width + (x+0)] = G2;
            dst->ch2[(y+0) * dst->width + (x+1)] = G3;
            dst->ch2[(y+1) * dst->width + (x+0)] = G2;
            dst->ch2[(y+1) * dst->width + (x+1)] = G3;

            dst->ch3[(y+0) * dst->width + (x+0)] = B4;
            dst->ch3[(y+0) * dst->width + (x+1)] = B4;
            dst->ch3[(y+1) * dst->width + (x+0)] = B4;
            dst->ch3[(y+1) * dst->width + (x+1)] = B4;
        }
    }
    dst->maxval = src->maxval;

    if (1 == FILTER_BICUBIC_ENABLE) {
        apply_bicubic(dst, bilinear, dst->width, dst->height);
    } else {
        apply_bilinear(dst, bilinear, dst->width, dst->height);
    }

    printf("ppm image '%s'", dst_name);
    write_ppm_image(bilinear, dst_name);

    free_pgm_buffer(src);
    free_ppm_buffer(dst);
}

void rgb_to_yuv(char *src_name, char *dst_name)
{
    int x = 0, y = 0;
    ppm_t *src = read_ppm_image(src_name);
    ppm_t *dst = NULL;

    if (NULL == (dst = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            int Y  = CRGB2Y(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);
            int Cb = CRGB2Cb(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);
            int Cr = CRGB2Cr(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);

            dst->ch1[y * dst->width + x] = Y;
            dst->ch2[y * dst->width + x] = Cb;
            dst->ch3[y * dst->width + x] = Cr;
        }
    }

    printf("ppm yuv image '%s'", dst_name);
    write_ppm_image(dst, dst_name);

    free_ppm_buffer(src);
    free_ppm_buffer(dst);
}

void yuv_to_rgb(char *src_name, char *dst_name)
{
    int x = 0, y = 0;
    ppm_t *src = read_ppm_image(src_name);
    ppm_t *dst = NULL;

    if (NULL == (dst = alloc_ppm_buffer(src->width, src->height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            int R = CYCbCr2R(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);
            int G = CYCbCr2G(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);
            int B = CYCbCr2B(src->ch1[y * src->width + x], src->ch2[y * src->width + x], src->ch3[y * src->width + x]);

            dst->ch1[y * dst->width + x] = R;
            dst->ch2[y * dst->width + x] = G;
            dst->ch3[y * dst->width + x] = B;
        }
    }

    printf("ppm yuv image '%s'", dst_name);
    write_ppm_image(dst, dst_name);

    free_ppm_buffer(src);
    free_ppm_buffer(dst);
}

void scale_image(char *src_name, char *dst_name, float scale) {

    int y, x;
    ppm_t *src = read_ppm_image(src_name);
    ppm_t *dst = NULL;
    int dst_width, dst_height;

    dst_width  = (long)((float)src->width  * scale);
    dst_height = (long)((float)src->height * scale);

    if (NULL == (dst = alloc_ppm_buffer(dst_width, dst_height, src->maxval))) {
        die("error: %s", "insufficient memory available");
    }

    if ((dst->width <= 0) || (dst->height <= 0)) {
        return ;
    }

    for (y = 0; y < dst->height; y++) {
        float v = (float)y / (float)scale;

        for (x = 0; x < dst->width; ++x) {
            float u = (float)x / (float)scale;
            dst->ch1[y * dst->width + x] = bicubic(src->ch1, u, v, src->width, src->height, src->maxval);
            dst->ch2[y * dst->width + x] = bicubic(src->ch2, u, v, src->width, src->height, src->maxval);
            dst->ch3[y * dst->width + x] = bicubic(src->ch3, u, v, src->width, src->height, src->maxval);
        }
    }

    printf("ppm zoom image '%s'", dst_name);
    write_ppm_image(dst, dst_name);

    free_ppm_buffer(src);
    free_ppm_buffer(dst);
}

int main(int argc, char *argv[])
{
    char *arg = NULL;

    if (5 != argc && 2 != argc) { usage(); }

    while ((arg = argv[1]) != NULL) {
        if (*arg != '-')
            break;

        for (;;) {
            switch (*++arg) {
            case 0:
                break;
            case 'b':
                {
                    char *src_name = NULL, *dst_name = NULL;
                    int conv_opt = 0;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        die("error: %s ", "incorrect argument");
                    }

                    src_name = argv[2];
                    dst_name = argv[3];
                    conv_opt = atoi(argv[4]);

                    if (0 != conv_opt && 1 != conv_opt) {
                        usage();
                        die("error: %s ", "incorrect argument");
                    }

                    if (0 == conv_opt) {
                        ppm_to_bayer(src_name, dst_name);       //PPM to Bayer
                    } else if (1 == conv_opt) {
                        bayer_to_ppm(src_name, dst_name);       //Bayer to PPM
                    } else {
                        die("error: %s ", "incorrect argument");
                    }
                    continue;
                }
            case 's':
                {
                    char *src_name = NULL, *dst_name = NULL;
                    int bit_depth = 8;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        die("error: %s ", "incorrect argument");
                    }

                    src_name = argv[2];
                    dst_name = argv[3];
                    bit_depth = atoi(argv[4]);

                    if (!(bit_depth >= 8 && bit_depth <= 16)) {
                        die("error: %s ", "incorrect argument");
                    }

                    conv_bitdepth(src_name, dst_name, bit_depth);
                    continue;
                }
            case 'd':
                {
                    char *src_name = NULL, *dst_name = NULL, *diff_name = NULL;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        die("error: %s ", "incorrect argument");
                    }

                    src_name = argv[2];
                    dst_name = argv[3];
                    diff_name = argv[4];

                    diff_image(diff_name, src_name, dst_name);
                    continue;
                }
            case 'c':
                {
                    char *src_name = NULL, *dst_name = NULL;
                    int conv_opt = 0;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        die("error: %s ", "incorrect argument");
                    }

                    src_name = argv[2];
                    dst_name = argv[3];
                    conv_opt = atoi(argv[4]);

                    if (0 != conv_opt && 1 != conv_opt) {
                        die("error: %s ", "incorrect argument");
                    }

                    if (0 == conv_opt) {
                        rgb_to_yuv(src_name, dst_name);
                    } else if (1 == conv_opt) {
                        yuv_to_rgb(src_name, dst_name);
                    } else {
                        die("error: %s ", "incorrect argument");
                    }
                    continue;
                }
            case 'z':
                {
                    char *src_name = NULL, *dst_name = NULL;
                    float scale_fact = 1.f;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        die("error: %s ", "incorrect argument");
                    }

                    src_name = argv[2];
                    dst_name = argv[3];
                    scale_fact = (float)atof(argv[4]);

                    if (!(scale_fact > 0.f && scale_fact <= 8.f)) {
                        die("error: %s ", "incorrect argument");
                    }

                    scale_image(src_name, dst_name, scale_fact);
                    continue;
                }
            case 'h':
                {
                usage();
                continue;
                }
            case 'v':
                {
                version_num();
                continue;
                }
            default:
                die("unknown option '-%s'", arg);
            }
            break;
        }

        argv++;
    }

    return 0;
}
