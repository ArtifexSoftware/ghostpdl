#ifdef __cplusplus
extern "C" {
#endif

/* Definitions for source pixel format.
 * Reasonable settings are as follows:
 * 8-bit: unsigned char ETS_SrcPixel; ETS_SRC_MAX = 255
 * 12-bit 0..4095: unsigned short (or int) ETS_SrcPixel; ETS_SRC_MAX = 4095
 * 12-bit 0..4096: unsigned short (or int) ETS_SrcPixel; ETS_SRC_MAX = 4096
 * 16-bit 0..65535: unsigned short (or int) ETS_SrcPixel; ETS_SRC_MAX = 65535
 * 16-bit 0..65536: unsigned int ETS_SrcPixel; ETS_SRC_MAX = 65536
 */

/* A quick and dirty switch for 8 or 16 bit data */
#define CHAR_SOURCE

#ifdef CHAR_SOURCE
    typedef unsigned char ETS_SrcPixel;
    #define ETS_SRC_MAX 255
#else
    typedef unsigned short ETS_SrcPixel;
    #define ETS_SRC_MAX 65535
#endif

/* Photoshop (and possibly other image formats define white in a CMYK image with a 
   value of 255 (65535).  This is opposite of the PAM files that Robin has created.
   The ETS code expects white to be at 0.   Adjustments to the values in gray level
   will be baked into the LUT if needed */
typedef enum {
    ETS_BLACK_IS_ZERO = 0,
    ETS_BLACK_IS_ONE
} ETS_POLARITY;

typedef unsigned char uchar;

/* To use the file dump capability:

   Open a file as with: fopen ("dumpfile", "wb");
   Put the resulting FILE * pointer in params->dump_file.
   Set params->dump_level to the desired level. ET_DUMP_ALL dumps all
   inputs and outputs. Other values will lead to much smaller files,
   but may not be as insightful.

   If no dump file is desired, set params->dump_file to NULL.
*/

typedef enum {
    ETS_DUMP_MINIMAL,
    ETS_DUMP_PARAMS,
    ETS_DUMP_LUTS,
    ETS_DUMP_INPUT,
    ETS_DUMP_ALL
} ETS_DumpLevel;

typedef struct {
    int width;
    int n_planes;
    int levels; /* Number of levels on output, <= 256 */
    int **luts;
    double distscale; /* 0 to autoselect based on aspect */
    int aspect_x;
    int aspect_y;
    int *strengths;
    int rand_scale; /* 0 is default. Larger means more random. */
    int *c1_scale; /* now an array, one per channel; 0 is default */
    int ets_style;
    int r_style;
    FILE *dump_file;
    ETS_DumpLevel dump_level;
    int **rand_scale_luts;
    ETS_POLARITY polarity;
} ETS_Params;

typedef struct _ETS_Ctx ETS_Ctx;

ETS_Ctx *
ets_new(const ETS_Params *params);

void
ets_line(ETS_Ctx *ctx, uchar **dest, const ETS_SrcPixel *const *src);

void
ets_free(ETS_Ctx *ctx);

void *
ets_malloc_aligned(int size, int align);

void
ets_free_aligned(void *p);

#ifdef __cplusplus
}
#endif
