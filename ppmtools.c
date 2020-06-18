//
//ppmtools 1.0, Copyright 2018-20 by Arun
//

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include "ppm.h"
#include "pgm.h"

//------------------ macro definition ------------------

#define FILTER_BICUBIC_ENABLE	1

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


void usage(const char *av0)
{
    fprintf (stdout, "usage: %s [options]", av0);
    fprintf (stdout, "\n -d reffile1 reffile2 difffile\
                      \n -s infile outfile bitdepth(8-16)\
                      \n -z infile outfile zoomfactor(0.1-8.0x)\
                      \n -c infile outfile convertopt(0:YUV, 1:RGB)\
                      \n -b infile outfile convertopt(0:ppm to bayer, 1:bayer to ppm)\
                      \n");
    exit(1);
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

u_short getPixel(u_short *src, int x, int y, int w, int h)
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

    // 1st row
    p00 = getPixel(src, xint - 1, yint - 1, sw, sh);   
    p10 = getPixel(src, xint + 0, yint - 1, sw, sh);
    p20 = getPixel(src, xint + 1, yint - 1, sw, sh);
    p30 = getPixel(src, xint + 2, yint - 1, sw, sh);

    // 2nd row
    p01 = getPixel(src, xint - 1, yint + 0, sw, sh);
    p11 = getPixel(src, xint + 0, yint + 0, sw, sh);
    p21 = getPixel(src, xint + 1, yint + 0, sw, sh);
    p31 = getPixel(src, xint + 2, yint + 0, sw, sh);

    // 3rd row
    p02 = getPixel(src, xint - 1, yint + 1, sw, sh);
    p12 = getPixel(src, xint + 0, yint + 1, sw, sh);
    p22 = getPixel(src, xint + 1, yint + 1, sw, sh);
    p32 = getPixel(src, xint + 2, yint + 1, sw, sh);

    // 4th row
    p03 = getPixel(src, xint - 1, yint + 2, sw, sh);
    p13 = getPixel(src, xint + 0, yint + 2, sw, sh);
    p23 = getPixel(src, xint + 1, yint + 2, sw, sh);
    p33 = getPixel(src, xint + 2, yint + 2, sw, sh);

    col0 = bicubic_filter(p00, p10, p20, p30, xfract);
    col1 = bicubic_filter(p01, p11, p21, p31, xfract);
    col2 = bicubic_filter(p02, p12, p22, p32, xfract);
    col3 = bicubic_filter(p03, p13, p23, p33, xfract);

    value = bicubic_filter(col0, col1, col2, col3, yfract);

    return (u_short) sat((int)value, 0, mv);
}

void ApplyBiCubic(Image *src, Image *dst, int dstWidth, int dstHeight)
{
    int u, v;
    for (v = 0; v < dstHeight; v++) {
        for (u = 0; u < dstWidth; u++) {
            dst->ch1[v * dstWidth + u] = bicubic(src->ch1, (float)u, (float)v, src->width, src->height, src->maxval);
            dst->ch2[v * dstWidth + u] = bicubic(src->ch2, (float)u, (float)v, src->width, src->height, src->maxval);
            dst->ch3[v * dstWidth + u] = bicubic(src->ch3, (float)u, (float)v, src->width, src->height, src->maxval);
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

void ApplyBilinear(Image *src, Image *dst, int dstWidth, int dstHeight)
{
    int x = 0, y = 0;
    dst->width = dstWidth;
    dst->height = dstHeight;
    dst->maxval = src->maxval;

    for (y = 0; y < dstHeight; y++) {
        for (x = 0; x < dstWidth; x++) {
            dst->ch1[y * dstWidth + x] = bilinear((float)src->ch1[y * src->width + x], (float)src->ch1[y * src->width + (x + 1)], (float)src->ch1[(y+1) * src->width + x], (float)src->ch1[(y+1) * src->width + (x + 1)], (float)x, (float)y);
            dst->ch2[y * dstWidth + x] = bilinear((float)src->ch2[y * src->width + x], (float)src->ch2[y * src->width + (x + 1)], (float)src->ch2[(y+1) * src->width + x], (float)src->ch2[(y+1) * src->width + (x + 1)], (float)x, (float)y);
            dst->ch3[y * dstWidth + x] = bilinear((float)src->ch3[y * src->width + x], (float)src->ch3[y * src->width + (x + 1)], (float)src->ch3[(y+1) * src->width + x], (float)src->ch3[(y+1) * src->width + (x + 1)], (float)x, (float)y);
        }	
    }
}

void pgm_diff(char *diffName, char *srcName, char *dstName)
{
    int x = 0, y = 0;
    Image *src = ImageRead(srcName);
    Image *dst = ImageRead(dstName);
    Image *diff = 0;

    if (NULL == (diff = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
    }

    if ((src->width != dst->width) || (src->height != dst->height)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            float val1 = ((float)src->ch1[y * src->width + x] / (float)src->maxval) - ((float)dst->ch1[y * dst->width + x] / (float)dst->maxval);
            float val2 = ((float)src->ch2[y * src->width + x] / (float)src->maxval) - ((float)dst->ch2[y * dst->width + x] / (float)dst->maxval);
            float val3 = ((float)src->ch3[y * src->width + x] / (float)src->maxval) - ((float)dst->ch3[y * dst->width + x] / (float)dst->maxval);

            diff->ch1[y * diff->width + x] = (int)(SIGN(val1) * (float)diff->maxval);
            diff->ch2[y * diff->width + x] = (int)(SIGN(val2) * (float)diff->maxval);
            diff->ch3[y * diff->width + x] = (int)(SIGN(val3) * (float)diff->maxval);
        }
    }

    printf("diff image '%s'", diffName);
    ImageWrite(diff, diffName);

    ImageRelease(src);
    ImageRelease(dst);
    ImageRelease(diff);
}

void pgm_scale(char *srcName, char *dstName, int bitdepth)
{
    int x = 0, y = 0;
    Image *src = ImageRead(srcName);
    Image *dst = NULL;
    int dstMaxvalue = (1 << bitdepth) - 1;

    if (NULL == (dst = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y++) {
        for (x = 0; x < src->width; x++) {
            float val1 = (float)src->ch1[y * src->width + x] / (float)src->maxval;
            float val2 = (float)src->ch2[y * src->width + x] / (float)src->maxval;
            float val3 = (float)src->ch3[y * src->width + x] / (float)src->maxval;

            dst->ch1[y * dst->width + x] = (u_short)(val1 * (float)dstMaxvalue);
            dst->ch2[y * dst->width + x] = (u_short)(val2 * (float)dstMaxvalue);
            dst->ch3[y * dst->width + x] = (u_short)(val3 * (float)dstMaxvalue);
        }
    }
    dst->maxval = dstMaxvalue;

    printf("rescaled image '%s'", dstName);
    ImageWrite(dst, dstName);

    ImageRelease(src);
    ImageRelease(dst);
}

void ppm_to_bayer(char *srcName, char *dstName)
{
    int bayerMaxvalue = 0, x = 0, y = 0;
    Image *src = ImageRead(srcName);
    ImagePGM *dst = NULL;

    bayerMaxvalue = (1 << 16) - 1;

    if (NULL == (dst = ImagePGMCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
    }

    if ((src->width <= 0) || (src->height <= 0)) {
        return ;
    }

    for (y = 0; y < src->height; y+=2) {
        for (x = 0; x < src->width; x+=2) {
            int R1 = (src->ch1[(y+0) * src->width + (x+0)] + src->ch1[(y+0) * src->width + (x+1)] + src->ch1[(y+1) * src->width + (x+0)] + src->ch1[(y+1) * src->width + (x+1)]) >> 2;
            int G2 = (src->ch2[(y+0) * src->width + (x+0)] + src->ch2[(y+0) * src->width + (x+1)]) >> 1;
            int G3 = (src->ch2[(y+1) * src->width + (x+0)] + src->ch2[(y+1) * src->width + (x+1)]) >> 1;
            int B4 = (src->ch3[(y+0) * src->width + (x+0)] + src->ch3[(y+0) * src->width + (x+1)] + src->ch3[(y+1) * src->width + (x+0)] + src->ch3[(y+1) * src->width + (x+1)]) >> 2;

            dst->ch[(y+0) * dst->width + (x+0)] = (u_short)(((float)R1 / (float)src->maxval) * (float)bayerMaxvalue);
            dst->ch[(y+0) * dst->width + (x+1)] = (u_short)(((float)G2 / (float)src->maxval) * (float)bayerMaxvalue);
            dst->ch[(y+1) * dst->width + (x+0)] = (u_short)(((float)G3 / (float)src->maxval) * (float)bayerMaxvalue);
            dst->ch[(y+1) * dst->width + (x+1)] = (u_short)(((float)B4 / (float)src->maxval) * (float)bayerMaxvalue);
        }
    }
    dst->maxval = bayerMaxvalue;

    printf("bayer image '%s'", dstName);
    ImagePGMWrite(dst, dstName);

    ImageRelease(src);
    ImagePGMRelease(dst);
}

void bayer_to_ppm(char *srcName, char *dstName)
{
    int x = 0, y = 0;
    ImagePGM *src = ImagePGMRead(srcName);
    Image *bilinear = NULL;
    Image *dst = NULL;

    if (NULL == (bilinear = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
    }

    if (NULL == (dst = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
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
        ApplyBiCubic(dst, bilinear, dst->width, dst->height);
    } else {
        ApplyBilinear(dst, bilinear, dst->width, dst->height);
    }

    printf("ppm image '%s'", dstName);
    ImageWrite(bilinear, dstName);

    ImagePGMRelease(src);
    ImageRelease(dst);
}

void ppm_rgb_to_yuv(char *srcName, char *dstName)
{
    int x = 0, y = 0;
    Image *src = ImageRead(srcName);
    Image *dst = NULL;

    if (NULL == (dst = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
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

    printf("ppm yuv image '%s'", dstName);
    ImageWrite(dst, dstName);

    ImageRelease(src);
    ImageRelease(dst);
}

void ppm_yuv_to_rgb(char *srcName, char *dstName)
{
    int x = 0, y = 0;
    Image *src = ImageRead(srcName);
    Image *dst = NULL;

    if (NULL == (dst = ImageCreate(src->width, src->height, src->maxval))) {
        die("error: %s", "memory allocation");
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

    printf("ppm yuv image '%s'", dstName);
    ImageWrite(dst, dstName);

    ImageRelease(src);
    ImageRelease(dst);
}

void ppm_zoom(char *srcName, char *dstName, float scale) {

    int y, x;
    Image *src = ImageRead(srcName);
    Image *dst = NULL;
    int dstWidth, dstHeight;

    dstWidth  = (long)((float)src->width  * scale);
    dstHeight = (long)((float)src->height * scale);

    if (NULL == (dst = ImageCreate(dstWidth, dstHeight, src->maxval))) {
        die("error: %s", "memory allocation");
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

    printf("ppm zoom image '%s'", dstName);
    ImageWrite(dst, dstName);

    ImageRelease(src);
    ImageRelease(dst);
}

int main (int argc, char *argv[])
{
    char *arg = NULL;

    if (5 != argc) { usage(argv[0]); }

    while ((arg = argv[1]) != NULL) {
        if (*arg != '-')
            break;
        for (;;) {
            switch (*++arg) {
            case 0:
                break;
            case 'b':
                {
                    char *srcName = NULL, *dstName = NULL;
                    int convopt = 0;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        fprintf(stderr, "usage: ppmdiff.exe -b infile outfile convertopt(0:ppm to bayer, 1:bayer to ppm)\n");
                        die("error: %s ", "incorrect args");
                    }

                    srcName = argv[2];
                    dstName = argv[3];
                    convopt = atoi(argv[4]);
                    
                    if (0 != convopt && 1 != convopt) {
                        usage(argv[0]);
                        die("error: %s ", "incorrect args");
                    }

                    if (0 == convopt) {
                        ppm_to_bayer(srcName, dstName);		//PPM to Bayer
                    } else {
                        bayer_to_ppm(srcName, dstName);		//Bayer to PPM
                    }
                    continue;
                }
            case 's':
                {
                    char *srcName = NULL, *dstName = NULL;
                    int bitdepth = 8;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        fprintf(stderr, "usage: ppmdiff.exe -s infile outfile bitdepth(8-16)\n");
                        die("error: %s ", "incorrect args");
                    }

                    srcName = argv[2];
                    dstName = argv[3];
                    bitdepth = atoi(argv[4]);
                    
                    if (!(bitdepth >= 8 && bitdepth <= 16)) {
                        usage(argv[0]);
                        die("error: %s ", "incorrect args");
                    }

                    pgm_scale(srcName, dstName, bitdepth);
                    continue;
                }
            case 'd':
                {
                    char *srcName = NULL, *dstName = NULL, *diffName = NULL;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        fprintf(stderr, "usage: ppmdiff.exe -d reffile1 reffile2 difffile\n");
                        die("error: %s ", "incorrect args");
                    }

                    srcName = argv[2];
                    dstName = argv[3];
                    diffName = argv[4];
                    
                    pgm_diff(diffName, srcName, dstName);
                    continue;
                }
            case 'c':
                {
                    char *srcName = NULL, *dstName = NULL;
                    int convopt = 0;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        fprintf(stderr, "usage: ppmdiff.exe -c infile outfile convertopt(0:YUV, 1:RGB)\n");
                        die("error: %s ", "incorrect args");
                    }

                    srcName = argv[2];
                    dstName = argv[3];
                    convopt = atoi(argv[4]);

                    if (0 != convopt && 1 != convopt) {
                        usage(argv[0]);
                        die("error: %s ", "incorrect args");
                    }

                    if (0 == convopt) {
                        ppm_rgb_to_yuv(srcName, dstName);
                    } else {
                        ppm_yuv_to_rgb(srcName, dstName);
                    }
                    continue;
                }
            case 'z':
                {
                    char *srcName = NULL, *dstName = NULL;
                    float zoomfact = 1.f;

                    if (NULL == argv[2] || NULL == argv[3] || NULL == argv[4]) {
                        fprintf(stderr, "usage: ppmdiff.exe -z infile outfile zoomfactor(0.1-8.0x)\n");
                        die("error: %s ", "incorrect args");
                    }

                    srcName = argv[2];
                    dstName = argv[3];
                    zoomfact = (float)atof(argv[4]);
                    
                    if (!(zoomfact > 0.f && zoomfact <= 8.f)) {
                        usage(argv[0]);
                        die("error: %s ", "incorrect args");
                    }

                    ppm_zoom(srcName, dstName, zoomfact);
                    continue;
                }
            case 'h':
                usage(argv[0]);
                continue;
            default:
                die("Unknown flag '%s'", arg);
            }
            break;
        }
        argv++;
    }

    return 0;
}

