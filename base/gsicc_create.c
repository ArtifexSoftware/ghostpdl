/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* This is the code that is used to convert the various PDF and PS CIE
   based color spaces to ICC profiles.  This enables the use of an
   external CMS that is ICC centric to be used for ALL color management.

   The following spaces are handled:

   From PDF

   % Input Spaces

   CalRGB      -->  ICC 1-D LUTS and Matrix
   CalGray     -->  ICC 1-D LUT
   LAB         -->  ICC MLUT with a 2x2 sized table

   From PS

   %% Input Spaces

   CIEBasedABC  --> ICC 1-D LUTs and Matrix
   CIEBasedA    --> ICC 1-D LUT
   CIEBasedDEF  --> 3-D MLUT plus 1-D LUTs
   CIEBasedDEFG --> 4-D MLUT pluse 1-D LUTs

   %% Output Spaces

   Type1 CRD -->  ICC will have MLUT if render table present.

   A few notes:

   Required Tags for ALL profiles include:

       profileDescriptionTag
       copyrightTag
       mediaWhatePointTag
       chromaticAdaptationTag (V4 -  when measurement data is for other than D50)

   For color input profiles:

       Required if N-component LUT-based:

          AToB0Tag   (NOTE ONE WAY! BtoA0Tag is optional. Not true for
                          display profiles.)

       Required if 3 component matrix based:

           redMatrixColumnTag
           greenMatrixColumnTag
           blueMatrixColumnTag
           redTRCTag
           greenTRCTag
           blueTRCTag

       Notes:

       3-component can include AToB0Tag.
       Only CIEXYZ encoding can be used with matrix/TRC models.
       If CIELAB encoding is to be used, we must use LUT-based.

    For Monochrome input:

       Required:
           grayTRCTag

       Optional
           AToB0Tag

    For Color Display Profiles:

        Required if N-Component LUT-Based

            AtoB0Tag
            BToA0Tag   (Note inverse required here).

        Required if 3 component matrix based display profiles

            redMatrixColumnTag
            greenMatrixColumnTag
            blueMatrixColumnTag
            redTRCTag
            greenTRCTag
            blueTRCTag

        Optional

            AtoB0Tag
            BToA0Tag   (Note inverse required here).

    For Monochrome Display Profiles

        Required

            grayTRCTag

        Optional

            AtoB0Tag
            BtoA0Tag

Note: All profile data must be encoded as big-endian

   */

#include "icc34.h"   /* Note this header is needed even if lcms is not
                            compiled as default CMS */
#include "string_.h"
#include "gsmemory.h"
#include "gx.h"
#include <gp.h>

#include "gxgstate.h"
#include "gstypes.h"
#include "gscspace.h"
#include "gscie.h"
#include "gsicc_create.h"
#include "gxarith.h"
#include "gsicc_manage.h"
#include "gsicc_cache.h"
#include "math_.h"
#include "gscolor2.h"
#include "gxcie.h"

static void
add_xyzdata(unsigned char *input_ptr, icS15Fixed16Number temp_XYZ[]);

#define SAVEICCPROFILE 0
#define HEADER_SIZE 128
#define TAG_SIZE 12
#define XYZPT_SIZE 12
#define DATATYPE_SIZE 8
#define CURVE_SIZE 512
#define IDENT_CURVE_SIZE 0
#define NUMBER_COMMON_TAGS 2
#define icMultiUnicodeText 0x6d6c7563           /* 'mluc' v4 text type */
#define icMultiFunctionAtoBType 0x6d414220      /* 'mAB ' v4 lutAtoBtype type */
#define D50_X 0.9642f
#define D50_Y 1.0f
#define D50_Z 0.8249f
#define DEFAULT_TABLE_NSIZE 9
#define FORWARD_V2_TABLE_SIZE 9
#define BACKWARD_V2_TABLE_SIZE 33
#define DEFAULT_TABLE_GRAYSIZE 128
#define V2_COMMON_TAGS NUMBER_COMMON_TAGS + 1

typedef unsigned short u1Fixed15Number;
#if SAVEICCPROFILE
unsigned int icc_debug_index = 0;
#endif

typedef struct cielab_s {
    float lstar;
    float astar;
    float bstar;
} cielab_t;

static const char desc_name[] = "Ghostscript Internal Profile";
static const char copy_right[] = "Copyright Artifex Software 2009-2023";

typedef struct {
    icTagSignature      sig;            /* The tag signature */
    icUInt32Number      offset;         /* Start of tag relative to
                                         * start of header, Spec
                                         * Clause 5 */
    icUInt32Number      size;           /* Size in bytes */
    unsigned char       byte_padding;
} gsicc_tag;
/* In generating 2x2x2 approximations as well as cases
   where we will need to squash components together we
   will go to float and then to 16 bit tables, hence the
   float pointer.  Otherwise we will keep the data
   in the existing byte form that it is in the CIEDEF(G)
   tables of postscript */
typedef struct {
    unsigned short *data_short;
    unsigned char *data_byte;  /* Used for cases where we can
                                   use the table as is */
    int     clut_dims[4];
    int     clut_num_input;
    int     clut_num_output;
    int     clut_num_entries;   /* Number of entries */
    int     clut_word_width;    /* Word width of table, 1 or 2 */
} gsicc_clut;

typedef struct {
    float   *a_curves;
    gsicc_clut *clut;
    float   *m_curves;
    gs_matrix3 *matrix;
    float   *b_curves;
    int num_in;
    int num_out;
    gs_vector3 *white_point;
    gs_vector3 *black_point;
    float *cam;
} gsicc_lutatob;

static int
get_padding(int x)
{
    return (4 -x%4)%4;
}

/* For some weird reason I cant link to the one in gscie.c */
static void
gsicc_matrix_init(register gs_matrix3 * mat)
{
    mat->is_identity =
        mat->cu.u == 1.0 && is_fzero2(mat->cu.v, mat->cu.w) &&
        mat->cv.v == 1.0 && is_fzero2(mat->cv.u, mat->cv.w) &&
        mat->cw.w == 1.0 && is_fzero2(mat->cw.u, mat->cw.v);
}

static void
gsicc_make_diag_matrix(gs_matrix3 *matrix, gs_vector3 * vec)
{
    matrix->cu.u = vec->u;
    matrix->cv.v = vec->v;
    matrix->cw.w = vec->w;
    matrix->cu.v = 0;
    matrix->cu.w = 0;
    matrix->cw.u = 0;
    matrix->cw.v = 0;
    matrix->cv.u = 0;
    matrix->cv.w = 0;
    matrix->is_identity = (vec->u == 1.0)&&(vec->v == 1.0)&&(vec->w == 1.0);
}

/* This function maps a gs matrix type to an ICC CLUT. This is required due to the
   multiple matrix and 1-D LUT forms for postscript management, which the ICC does not
   support (at least the older versions).  clut is allocated externally */
static void
gsicc_matrix3_to_mlut(gs_matrix3 *mat, unsigned short *clut)
{
    /* Step through the grid values */
    float grid_points[8][3]={{0,0,0},
                             {0,0,1},
                             {0,1,0},
                             {0,1,1},
                             {1,0,0},
                             {1,0,1},
                             {1,1,0},
                             {1,1,1}};
    int k;
    gs_vector3 input,output;
    unsigned short *curr_ptr = clut, value;
    float valueflt;

    for (k = 0; k < 8; k++) {
        input.u = grid_points[k][0];
        input.v = grid_points[k][1];
        input.w = grid_points[k][2];
        cie_mult3(&input, mat, &output);
        valueflt = output.u;
        if (valueflt < 0) valueflt = 0;
        if (valueflt > 1) valueflt = 1;
        value = (unsigned short) (valueflt*65535.0);
        *curr_ptr ++= value;
        valueflt = output.v;
        if (valueflt < 0) valueflt = 0;
        if (valueflt > 1) valueflt = 1;
        value = (unsigned short) (valueflt*65535.0);
        *curr_ptr ++= value;
        valueflt = output.w;
        if (valueflt < 0) valueflt = 0;
        if (valueflt > 1) valueflt = 1;
        value = (unsigned short) (valueflt*65535.0);
        *curr_ptr ++= value;
    }
}

static void
apply_adaption(float matrix[], float in[], float out[])
{
    out[0] = matrix[0] * in[0] + matrix[1] * in[1] + matrix[2] * in[2];
    out[1] = matrix[3] * in[0] + matrix[4] * in[1] + matrix[5] * in[2];
    out[2] = matrix[6] * in[0] + matrix[7] * in[1] + matrix[8] * in[2];
}

/* This function mashes all the elements together into a single CLUT
   for the ICC profile.  This is an approach of last resort, but
   guaranteed to work. */
static int
gsicc_create_clut(const gs_color_space *pcs, gsicc_clut *clut, gs_range *ranges,
                  gs_vector3 *white_point, bool range_adjust, float cam[],
                  gs_memory_t *memory)
{
    gs_gstate *pgs;
    int code;
    int num_points = clut->clut_num_entries;
    int table_size = clut->clut_dims[0]; /* Same resolution in each direction*/
    int num_components = clut->clut_num_input;
    int j,i,index;
    float *input_samples[4], *fltptr;
    gs_range *curr_range;
    unsigned short *ptr_short;
    gs_client_color cc;
    frac xyz[3];
    float xyz_float[3];
    float temp;
    gs_color_space_index cs_index;

    /* This completes the joint cache inefficiently so that
       we can sample through it and get our table entries */
    code = gx_cie_to_xyz_alloc(&pgs, pcs, memory);
    if (code < 0)
        return gs_rethrow(code, "Allocation of cie to xyz transform failed");
    cs_index = gs_color_space_get_index(pcs);

    /* Create the sample indices across the input ranges
       for each color component.  When the concretization/remap occurs
       to be fed into this icc profile, we may will need to apply a linear
       map to the input if the range is something other than 0 to 1 */
    for (i = 0; i < num_components; i++) {
        input_samples[i] = (float*) gs_alloc_bytes(memory,
                                sizeof(float)*table_size,"gsicc_create_clut");
        if (input_samples[i] == NULL) {
            for (j = 0; j < i; j++) {
                gs_free_object(memory, input_samples[j], "gsicc_create_clut");
            }
            return gs_throw(gs_error_VMerror, "Allocation of input_sample arrays failed");
        }
        fltptr = input_samples[i];
        curr_range = &(ranges[i]);
        for (j = 0; j < table_size; j++ ) {
            *fltptr ++= ((float) j/ (float) (table_size-1)) *
                (curr_range->rmax - curr_range->rmin) + curr_range->rmin;
        }
    }
    /* Go through all the entries.
       Uniformly from min range to max range */
    ptr_short = clut->data_short;
    for (i = 0; i < num_points; i++) {
        switch (num_components) {
        case 1:
            /* Get the input vector value */
            fltptr = input_samples[0];
            index = i%table_size;
            cc.paint.values[0] = fltptr[index];
            break;
        case 3:
            /* The first channel varies least rapidly in the ICC table */
            fltptr = input_samples[2];
            index = i%table_size;
            cc.paint.values[2] = fltptr[index];
            fltptr = input_samples[1];
            index = (unsigned int) floor((float) i/(float) table_size)%table_size;
            cc.paint.values[1] = fltptr[index];
            fltptr = input_samples[0];
            index = (unsigned int) floor((float) i/(float) (table_size*
                                                        table_size))%table_size;
            cc.paint.values[0] = fltptr[index];
            break;
        case 4:
            /* The first channel varies least rapidly in the ICC table */
            fltptr = input_samples[3];
            index = i%table_size;
            cc.paint.values[3] = fltptr[index];
            fltptr = input_samples[2];
            index = (unsigned int) floor((float) i/(float) table_size)%table_size;
            cc.paint.values[2] = fltptr[index];
            fltptr = input_samples[1];
            index = (unsigned int) floor((float) i/(float) (table_size*
                                                        table_size))%table_size;
            cc.paint.values[1] = fltptr[index];
            fltptr = input_samples[0];
            index = (unsigned int) floor((float) i/(float) (table_size*
                                        table_size*table_size))%table_size;
            cc.paint.values[0] = fltptr[index];
            break;
        default:
            return_error(gs_error_rangecheck); /* Should never happen */
        }
        /* These special concretizations functions do not go through
           the ICC mapping like the procs associated with the color space */
        switch (cs_index) {
            case gs_color_space_index_CIEA:
                gx_psconcretize_CIEA(&cc, pcs, xyz, xyz_float, pgs);
                /* AR forces this case to always be achromatic.  We will
                   do the same even though it does not match the PS
                   specification */
                /* Use the resulting Y value to scale the D50 Illumination.
                   note that we scale to the whitepoint here.  Matrix out
                   handles mapping to CIE D50 */
                xyz_float[0] = white_point->u * xyz_float[1];
                xyz_float[2] = white_point->w * xyz_float[1];
                break;
            case gs_color_space_index_CIEABC:
                gx_psconcretize_CIEABC(&cc, pcs, xyz, xyz_float, pgs);
                break;
            case gs_color_space_index_CIEDEF:
                gx_psconcretize_CIEDEF(&cc, pcs, xyz, xyz_float, pgs);
                break;
            case gs_color_space_index_CIEDEFG:
               gx_psconcretize_CIEDEFG(&cc, pcs, xyz, xyz_float, pgs);
               break;
            default:
                return gs_throw(-1, "Invalid gs_color_space_index when creating ICC profile");
        }
        /* We need to map these values to D50 illuminant so that things work
           correctly with ICC profile */
        //apply_adaption(cam, xyz_float, xyz_adapt);

        /* Correct for range of ICC CIEXYZ table data */
        for (j = 0; j < 3; j++) {
            temp = xyz_float[j]/(1 + 32767.0/32768);
            if (temp < 0) temp = 0;
            if (temp > 1) temp = 1;
           *ptr_short ++= (unsigned int)(temp * 65535);
        }
    }
    gx_cie_to_xyz_free(pgs); /* Free the joint cache we created */
    for (i = 0; i < num_components; i++) {
        gs_free_object(memory, input_samples[i], "gsicc_create_clut");
    }
    return 0;
}

/* This function maps a gs vector type to an ICC CLUT.
   This is used in the CIEA type.  clut is allocated
   externally. We may need to replace this with a range value.
   For now we are mapping to an output between 0 and the vector */
static void
gsicc_vec_to_mlut(gs_vector3 *vec, unsigned short *clut)
{
    unsigned short *curr_ptr = clut;
    int temp;

    *curr_ptr ++= 0;
    *curr_ptr ++= 0;
    *curr_ptr ++= 0;
    temp = (int)(vec->u * 65535);
    if (temp > 65535) temp = 65535;
    if (temp < 0) temp = 0;
    *curr_ptr ++= temp;
    temp = (int)(vec->v * 65535);
    if (temp > 65535) temp = 65535;
    if (temp < 0) temp = 0;
    *curr_ptr ++= temp;
    temp = (int)(vec->w * 65535);
    if (temp > 65535) temp = 65535;
    if (temp < 0) temp = 0;
    *curr_ptr ++= temp;
}

#if SAVEICCPROFILE
/* Debug dump of internally created ICC profile for testing */
static void
save_profile(const gs_memory_t *mem, unsigned char *buffer, char filename[], int buffer_size)
{
    char full_file_name[50];
    gp_file *fid;

    gs_snprintf(full_file_name,sizeof(full_file_name),"%d)Profile_%s.icc",icc_debug_index,filename);
    fid = gp_fopen(mem, full_file_name,"wb");
    gp_fwrite(buffer,sizeof(unsigned char),buffer_size,fid);
    gp_fclose(fid);
    icc_debug_index++;
}
#endif

static void
write_bigendian_4bytes(unsigned char *curr_ptr,ulong input)
{
   *curr_ptr++ = (0xff & (input >> 24));
   *curr_ptr++ = (0xff & (input >> 16));
   *curr_ptr++ = (0xff & (input >> 8));
   *curr_ptr++ = (0xff & input);
}

static void
write_bigendian_2bytes(unsigned char *curr_ptr,ushort input)
{
   *curr_ptr++ = (0xff & (input >> 8));
   *curr_ptr++ = (0xff & input);
}

static void
setdatetime(icDateTimeNumber *datetime)
{
    datetime->day = 0;
    datetime->hours = 0;
    datetime->minutes = 0;
    datetime->month = 0;
    datetime->seconds = 0;
    datetime->year = 0;
}

static icS15Fixed16Number
double2XYZtype(float number_in)
{
    short s;
    unsigned short m;

    if (number_in < 0) {
        number_in = 0;
#ifdef DEBUG
        gs_warn("Negative CIEXYZ in created ICC Profile");
#endif
    }

    s = (short) number_in;
    m = (unsigned short) ((number_in - s) * 65536.0);
    return (icS15Fixed16Number) ((s << 16) | m);
}

static icS15Fixed16Number
double2icS15Fixed16Number(float number_in)
{
    short s;
    unsigned short m;
    icS15Fixed16Number temp;
    float number;

    if (number_in < 0) {
        number = -number_in;
        s = (short) number;
        m = (unsigned short) ((number - s) * 65536.0);
        temp = (icS15Fixed16Number) ((s << 16) | m);
        temp = -temp;
        return temp;
    } else {
        s = (short) number_in;
        m = (unsigned short) ((number_in - s) * 65536.0);
        return (icS15Fixed16Number) ((s << 16) | m);
    }
}

static unsigned short
float2u8Fixed8(float number_in)
{
    return (unsigned short) (number_in * 256);
}

static
void init_common_tags(gsicc_tag tag_list[],int num_tags, int *last_tag)
{
 /*    profileDescriptionTag
       copyrightTag  */

    int curr_tag, temp_size;

    if (*last_tag < 0)
        curr_tag = 0;
    else
        curr_tag = (*last_tag)+1;

    tag_list[curr_tag].offset = HEADER_SIZE+num_tags*TAG_SIZE + 4;
    tag_list[curr_tag].sig = icSigProfileDescriptionTag;
    /* temp_size = DATATYPE_SIZE + 4 + strlen(desc_name) + 1 + 4 + 4 + 3 + 67; */
    temp_size = 2*strlen(desc_name) + 28;
    /* +1 for NULL + 4 + 4 for unicode + 3 + 67 script code */
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;

    curr_tag++;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset +
                                                    tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = icSigCopyrightTag;
    /* temp_size = DATATYPE_SIZE + strlen(copy_right) + 1; */
    temp_size = 2*strlen(copy_right) + 28;
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;
    *last_tag = curr_tag;
}

/* Code to write out v4 text type which is a table of unicode text
   for different regions */
static void
add_v4_text_tag(unsigned char *buffer,const char text[], gsicc_tag tag_list[],
                int curr_tag)
{
    unsigned char *curr_ptr;
    int k;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr,icMultiUnicodeText);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,1); /* Number of names */
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,12); /* Record size */
    curr_ptr += 4;
    write_bigendian_2bytes(curr_ptr,0x656e); /* ISO 639-1, en */
    curr_ptr += 2;
    write_bigendian_2bytes(curr_ptr,0x5553); /* ISO 3166, US */
    curr_ptr += 2;
    write_bigendian_4bytes(curr_ptr,2*strlen(text)); /* String length */
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,28); /* Offset to string */
    curr_ptr += 4;
    /* String written as UTF-16BE. No NULL */
    for (k = 0; k < strlen(text); k++) {
        *curr_ptr ++= 0;
        *curr_ptr ++= text[k];
    }
    memset(curr_ptr,0,tag_list[curr_tag].byte_padding);  /* padding */
}

static void
add_desc_tag(unsigned char *buffer, const char text[], gsicc_tag tag_list[],
                int curr_tag)
{
    unsigned char *curr_ptr;
    int len = strlen(text) + 1;
    int k;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr, icSigTextDescriptionType);
    curr_ptr += 4;
    memset(curr_ptr, 0, 4);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr, len);
    curr_ptr += 4;
    for (k = 0; k < strlen(text); k++) {
        *curr_ptr++ = text[k];
    }
    memset(curr_ptr, 0, 12 + 67 + 1);
    memset(curr_ptr, 0, tag_list[curr_tag].byte_padding);  /* padding */
}

static void
add_text_tag(unsigned char *buffer, const char text[], gsicc_tag tag_list[],
            int curr_tag)
{
    unsigned char *curr_ptr;
    int k;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr, icSigTextType);
    curr_ptr += 4;
    memset(curr_ptr, 0, 4);
    curr_ptr += 4;
    for (k = 0; k < strlen(text); k++) {
        *curr_ptr++ = text[k];
    }
    memset(curr_ptr, 0, 1);
    memset(curr_ptr, 0, tag_list[curr_tag].byte_padding);  /* padding */
}

static void
add_common_tag_data(unsigned char *buffer,gsicc_tag tag_list[], int vers)
{
    unsigned char *curr_ptr;
    curr_ptr = buffer;

    if (vers == 4) {
        add_v4_text_tag(curr_ptr, desc_name, tag_list, 0);
        curr_ptr += tag_list[0].size;
        add_v4_text_tag(curr_ptr, copy_right, tag_list, 1);
    } else {
        add_desc_tag(curr_ptr, desc_name, tag_list, 0);
        curr_ptr += tag_list[0].size;
        add_text_tag(curr_ptr, copy_right, tag_list, 1);
    }
}

static
void  init_tag(gsicc_tag tag_list[], int *last_tag, icTagSignature tagsig,
               int datasize)
{
    /* This should never be called first. Common tags should be taken care of */

    int curr_tag = (*last_tag)+1;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset +
                                                    tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = tagsig;
    tag_list[curr_tag].byte_padding = get_padding(DATATYPE_SIZE + datasize);
    tag_list[curr_tag].size = DATATYPE_SIZE + datasize +
                                            tag_list[curr_tag].byte_padding;
    *last_tag = curr_tag;
}

static void
setheader_common(icHeader *header, int vers)
{
    /* This needs to all be predefined for a simple copy. MJV todo */
    header->cmmId = 0;
    if (vers == 4)
        header->version = 0x04200000;
    else
        header->version = 0x02200000;
    setdatetime(&(header->date));
    header->magic = icMagicNumber;
    header->platform = icSigMacintosh;
    header->flags = 0;
    header->manufacturer = 0;
    header->model = 0;
    header->attributes[0] = 0;
    header->attributes[1] = 0;
    header->renderingIntent = 3;
    header->illuminant.X = double2XYZtype((float) 0.9642);
    header->illuminant.Y = double2XYZtype((float) 1.0);
    header->illuminant.Z = double2XYZtype((float) 0.8249);
    header->creator = 0;
    /* Version 4 includes a profile id, field which is an md5 sum */
    memset(header->reserved,0,44);
}

static void
copy_header(unsigned char *buffer,icHeader *header)
{
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr,header->size);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->version);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->deviceClass);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->colorSpace);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->pcs);
    curr_ptr += 4;

    /* Date and time */
    memset(curr_ptr,0,12);
    curr_ptr += 12;
    write_bigendian_4bytes(curr_ptr,header->magic);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->platform);
    curr_ptr += 4;
    memset(curr_ptr,0,24);
    curr_ptr += 24;
    write_bigendian_4bytes(curr_ptr,header->illuminant.X);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->illuminant.Y);
    curr_ptr += 4;
    write_bigendian_4bytes(curr_ptr,header->illuminant.Z);
    curr_ptr += 4;
    memset(curr_ptr,0,48);
}

static void
copy_tagtable(unsigned char *buffer,gsicc_tag *tag_list, ulong num_tags)
{
    unsigned int k;
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr,num_tags);
    curr_ptr += 4;
    for (k = 0; k < num_tags; k++) {
        write_bigendian_4bytes(curr_ptr,tag_list[k].sig);
        curr_ptr += 4;
        write_bigendian_4bytes(curr_ptr,tag_list[k].offset);
        curr_ptr += 4;
        write_bigendian_4bytes(curr_ptr,tag_list[k].size);
        curr_ptr += 4;
    }
}

static void
get_D50(icS15Fixed16Number XYZ[])
{
    XYZ[0] = double2XYZtype(D50_X);
    XYZ[1] = double2XYZtype(D50_Y);
    XYZ[2] = double2XYZtype(D50_Z);
}

static void
get_XYZ(icS15Fixed16Number XYZ[], gs_vector3 *vector)
{
    XYZ[0] = double2XYZtype(vector->u);
    XYZ[1] = double2XYZtype(vector->v);
    XYZ[2] = double2XYZtype(vector->w);
}

static void
get_XYZ_doubletr(icS15Fixed16Number XYZ[], float *vector)
{
    XYZ[0] = double2XYZtype(vector[0]);
    XYZ[1] = double2XYZtype(vector[1]);
    XYZ[2] = double2XYZtype(vector[2]);
}

static void
scale_matrix(float *matrix_input,float scale_factor)
{
    int k;

    for (k = 0; k < 9; k++) {
        matrix_input[k] = matrix_input[k]/2.0;
    }
}

static void
add_gammadata(unsigned char *input_ptr, unsigned short gamma,
              icTagTypeSignature curveType)
{
    unsigned char *curr_ptr;

    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,curveType);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;

    /* one entry for gamma */
    write_bigendian_4bytes(curr_ptr, 1);
    curr_ptr += 4;

    /* The encode (8frac8) gamma, with padding */
    write_bigendian_2bytes(curr_ptr, gamma);
    curr_ptr += 2;

    /* pad two bytes */
    memset(curr_ptr,0,2);
}

static void
add_xyzdata(unsigned char *input_ptr, icS15Fixed16Number temp_XYZ[])
{
    int j;
    unsigned char *curr_ptr;

    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigXYZType);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    for (j = 0; j < 3; j++) {
        write_bigendian_4bytes(curr_ptr, temp_XYZ[j]);
        curr_ptr += 4;
    }
}

/* If abc matrix is identity the abc and lmn curves can be mashed together  */
static void
merge_abc_lmn_curves(gx_cie_vector_cache *DecodeABC_caches,
                     gx_cie_scalar_cache *DecodeLMN)
{

}

static void
add_matrixwithbias(unsigned char *input_ptr, float *float_ptr_in, bool has_bias)
{
    unsigned char *curr_ptr;
    float *float_ptr = float_ptr_in;
    int k;

    /* GS Matrix is coming in with data arranged in row ordered form */
    curr_ptr = input_ptr;
    for (k = 0; k < 9; k++ ){
        write_bigendian_4bytes(curr_ptr, double2icS15Fixed16Number(*float_ptr));
        curr_ptr += 4;
        float_ptr++;
    }
    if (has_bias){
        memset(curr_ptr,0,4*3);
    }
}

static void
matrixmult(float leftmatrix[], int nlrow, int nlcol,
           float rightmatrix[], int nrrow, int nrcol, float result[])
{
    float *curr_row;
    int k,l,j,ncols,nrows;
    float sum;

    nrows = nlrow;
    ncols = nrcol;
    if (nlcol == nrrow) {
        for (k = 0; k < nrows; k++) {
            curr_row = &(leftmatrix[k*nlcol]);
            for (l = 0; l < ncols; l++) {
                sum = 0.0;
                for (j = 0; j < nlcol; j++) {
                    sum = sum + curr_row[j] * rightmatrix[j*nrcol+l];
                }
                result[k*ncols+l] = sum;
            }
        }
    }
}

static void
gsicc_create_copy_matrix3(float *src, float *des)
{
    memcpy(des,src,9*sizeof(float));
}

static void
gsicc_create_compute_cam( gs_vector3 *white_src, gs_vector3 *white_des,
                                float *cam)
{
    float cat02matrix[] = {0.7328f, 0.4296f, -0.1624f,
                            -0.7036f, 1.6975f, 0.0061f,
                             0.003f, 0.0136f, 0.9834f};
    float cat02matrixinv[] = {1.0961f, -0.2789f, 0.1827f,
                              0.4544f, 0.4735f, 0.0721f,
                             -0.0096f, -0.0057f, 1.0153f};
    float vonkries_diag[9];
    float temp_matrix[9];
    float lms_wp_src[3], lms_wp_des[3];
    int k;

    matrixmult(cat02matrix,3,3,&(white_src->u),3,1,&(lms_wp_src[0]));
    matrixmult(cat02matrix,3,3,&(white_des->u),3,1,&(lms_wp_des[0]));
    memset(&(vonkries_diag[0]),0,sizeof(float)*9);

    for (k = 0; k < 3; k++) {
        if (lms_wp_src[k] > 0 ) {
            vonkries_diag[k*3+k] = lms_wp_des[k]/lms_wp_src[k];
        } else {
            vonkries_diag[k*3+k] = 1;
        }
    }
    matrixmult(&(vonkries_diag[0]), 3, 3, &(cat02matrix[0]), 3, 3,
                &(temp_matrix[0]));
    matrixmult(&(cat02matrixinv[0]), 3, 3, &(temp_matrix[0]), 3, 3, &(cam[0]));
}

static int
gsicc_compute_cam(gsicc_lutatob *icc_luta2bparts, gs_memory_t *memory)
{
    gs_vector3 d50;

    d50.u = D50_X;
    d50.v = D50_Y;
    d50.w = D50_Z;

    /* Calculate the chromatic adaptation matrix */
    icc_luta2bparts->cam = (float*) gs_alloc_bytes(memory,
                                        9 * sizeof(float), "gsicc_compute_cam");
    if (icc_luta2bparts->cam == NULL) {
        return gs_throw(gs_error_VMerror, "Allocation of ICC cam failed");
    }
    gsicc_create_compute_cam(icc_luta2bparts->white_point, &(d50), icc_luta2bparts->cam);
    return 0;
}

/* Compute the CAT02 transformation to get us from the Cal White
   point to the D50 white point.  We could pack this in a chad tag
   and let the CMM worry about applying but it is safer if we just
   take care of it ourselves by mapping the primaries.  This is what is
   also done for the table based data */
static float*
gsicc_get_cat02_cam(float *curr_wp, gs_memory_t *memory)
{
    gs_vector3 d50;
    gs_vector3 wp;
    float *cam;

    wp.u = curr_wp[0];
    wp.v = curr_wp[1];
    wp.w = curr_wp[2];

    d50.u = D50_X;
    d50.v = D50_Y;
    d50.w = D50_Z;

    cam = (float*)gs_alloc_bytes(memory, 9 * sizeof(float), "gsicc_get_cat02_cam");
    if (cam == NULL) {
        gs_throw(gs_error_VMerror, "Allocation of cat02 matrix failed");
        return NULL;
    }
    gsicc_create_compute_cam(&wp, &(d50), cam);

    return cam;
}

static void
add_ident_curves(unsigned char *input_ptr,int number_of_curves)
{
    unsigned char *curr_ptr;
    int k;

    curr_ptr = input_ptr;
    for (k = 0; k < number_of_curves; k++) {
       /* Signature */
        write_bigendian_4bytes(curr_ptr,icSigCurveType);
        curr_ptr += 4;
        /* Reserved */
        memset(curr_ptr,0,4);
        curr_ptr += 4;
        /* Count */
        write_bigendian_4bytes(curr_ptr, 0);
        curr_ptr += 4;
    }
}

static void
add_clutAtoB(unsigned char *input_ptr, gsicc_clut *clut)
{
    unsigned char *curr_ptr = input_ptr;
    int k;
    int num_channels_in = clut->clut_num_input;
    int number_samples = clut->clut_num_entries;

    /* First write out the dimensions for each channel */
    for (k = 0; k < num_channels_in; k++) {
        memset(curr_ptr, clut->clut_dims[k], 1);
        curr_ptr++;
    }
    /* Set the remainder of the dimenensions */
    memset(curr_ptr, 0, 16-num_channels_in);
    curr_ptr += (16-num_channels_in);
    /* word size */
    memset(curr_ptr, clut->clut_word_width, 1);
    curr_ptr++;
    /* padding */
    memset(curr_ptr, 0, 3);
    curr_ptr += 3;
    if (clut->data_byte != NULL) {
        /* A byte table */
        memcpy(curr_ptr,clut->data_byte,number_samples*3);
    } else {
        /* A float table */
        for ( k = 0; k < number_samples*3; k++ ) {
            write_bigendian_2bytes(curr_ptr,clut->data_short[k]);
            curr_ptr += 2;
        }
    }
}

static void
add_curve(unsigned char *input_ptr, float *curve_data, int num_samples)
{
    unsigned char *curr_ptr;
    unsigned short value;
    int k;

   /* Signature */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigCurveType);
    curr_ptr += 4;
    /* Reserved */
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    /* Count */
    write_bigendian_4bytes(curr_ptr, num_samples);
    curr_ptr += 4;
    /* Now the data uInt16 Number 0 to 65535.  For now assume input is 0 to 1.
            Need to fix this.  MJV */
    for (k = 0; k < num_samples; k++) {
        if (curve_data[k] < 0) curve_data[k] = 0;
        if (curve_data[k] > 1) curve_data[k] = 1;
        value = (unsigned int) (curve_data[k]*65535.0);
        write_bigendian_2bytes(curr_ptr,value);
        curr_ptr+=2;
    }
}

/* See comments before add_lutAtoBtype about allowable forms, which will
    explain much of these size calculations */
static int
getsize_lutAtoBtype(gsicc_lutatob *lutatobparts)
{
    int data_offset, mlut_size;
    int numout = lutatobparts->num_out;
    int numin = lutatobparts->num_in;
    int pad_bytes;

    data_offset = 32;
    /* B curves always present */
    if (lutatobparts->b_curves != NULL) {
        data_offset += (numout*(CURVE_SIZE*2+12));
    } else {
        data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
    }
    /* M curves present if Matrix is present */
    if (lutatobparts->matrix != NULL ) {
        data_offset += (12*4);
        /* M curves */
        if (lutatobparts->m_curves != NULL) {
            data_offset += (numout*(CURVE_SIZE*2+12));
        } else {
            data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
        }
    }
    /* A curves present if clut is present */
    if (lutatobparts->clut != NULL) {
        /* We may need to pad the clut to make sure we are on a 4 byte boundary */
        mlut_size = lutatobparts->clut->clut_num_entries *
                            lutatobparts->clut->clut_word_width * 3;
        pad_bytes = (4 - mlut_size%4)%4;
        data_offset += (mlut_size + pad_bytes + 20);
        if (lutatobparts->a_curves != NULL) {
            data_offset += (numin*(CURVE_SIZE*2+12));
        } else {
            data_offset += (numin*(IDENT_CURVE_SIZE*2+12));
        }
    }
    return data_offset;
}

/* Note:  ICC V4 fomat allows ONLY these forms
B
M - Matrix - B
A - CLUT - B
A - CLUT - M - Matrix - B
Other forms are created by making some of these items identity.  In other words
the B curves must always be included.  If CLUT is present, A curves must be present.
Also, if Matrix is present M curves must be present.  A curves cannot be
present if CLUT is not present. */
static void
add_lutAtoBtype(unsigned char *input_ptr, gsicc_lutatob *lutatobparts)
{
/* We need to figure out all the offsets to the various objects based upon
    which ones are actually present */
    unsigned char *curr_ptr;
    long mlut_size = 0;			/* silence compiler warning */
    int data_offset;
    int k;
    int numout = lutatobparts->num_out;
    int numin = lutatobparts->num_in;
    int pad_bytes = 0;

    /* Signature */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icMultiFunctionAtoBType);
    curr_ptr += 4;
    /* Reserved */
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    /* Padded sizes */
    *curr_ptr++ = numin;
    *curr_ptr++ = numout;
    memset(curr_ptr,0,2);
    curr_ptr += 2;
    /* Note if data offset is zero, element is not present */
    /* offset to B curves (last curves) */
    data_offset = 32;
    if (lutatobparts->b_curves == NULL) {
        /* identity curve must be present */
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (numout*(CURVE_SIZE*2+12));
    }
    curr_ptr += 4;
    /* offset to matrix and M curves */
    if (lutatobparts->matrix == NULL) {
        memset(curr_ptr,0,4);  /* Matrix */
        curr_ptr += 4;
        memset(curr_ptr,0,4);  /* M curves */
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (12*4);
        curr_ptr += 4;
        /* offset to M curves (Matrix curves -- only come with matrix) */
        if (lutatobparts->m_curves == NULL) {
            /* identity curve must be present */
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
        } else {
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numout*(CURVE_SIZE*2+12));
        }
    }
    curr_ptr += 4;
    /* offset to CLUT and A curves */
    if (lutatobparts->clut == NULL) {
        memset(curr_ptr,0,4); /* CLUT */
        curr_ptr += 4;
        memset(curr_ptr,0,4); /* A curves */
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        mlut_size = (long)lutatobparts->clut->clut_num_entries *
                          lutatobparts->clut->clut_word_width * 3;
        pad_bytes = (4 - mlut_size%4)%4;
        data_offset += (mlut_size + pad_bytes + 20);
        curr_ptr += 4;
        /* offset to A curves (first curves) */
        if (lutatobparts->a_curves == NULL || lutatobparts->clut == NULL) {
            /* identity curve must be present */
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numin*(IDENT_CURVE_SIZE*2+12));
        } else {
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numin*(CURVE_SIZE*2+12));
        }
    }
    curr_ptr += 4;
    /* Header is completed */
    /* Now write out the various parts (i.e. curves, matrix and clut) */
    /* First the B curves */
    if (lutatobparts->b_curves != NULL) {
        for (k = 0; k < numout; k++) {
            add_curve(curr_ptr, (lutatobparts->b_curves)+k*CURVE_SIZE, CURVE_SIZE);
            curr_ptr += (12 + CURVE_SIZE*2);
        }
    } else {
        add_ident_curves(curr_ptr,numout);
        curr_ptr += numout*(12 + IDENT_CURVE_SIZE*2);
    }
    /* Then the matrix */
    if (lutatobparts->matrix != NULL) {
        add_matrixwithbias(curr_ptr,(float*) lutatobparts->matrix,true);
        curr_ptr += (12*4);
        /* M curves */
        if (lutatobparts->m_curves != NULL) {
            for (k = 0; k < numout; k++) {
                add_curve(curr_ptr, (lutatobparts->m_curves)+k*CURVE_SIZE, CURVE_SIZE);
                curr_ptr += (12 + CURVE_SIZE*2);
            }
        } else {
            add_ident_curves(curr_ptr,numout);
            curr_ptr += numout*(12 + IDENT_CURVE_SIZE*2);
        }
    }
    /* Then the clut */
    if (lutatobparts->clut != NULL) {
        add_clutAtoB(curr_ptr, lutatobparts->clut);
        curr_ptr += (20 + mlut_size);
        memset(curr_ptr,0,pad_bytes); /* 4 byte boundary */
        curr_ptr += pad_bytes;
        /* The A curves */
        if (lutatobparts->a_curves != NULL) {
            for (k = 0; k < numin; k++) {
                add_curve(curr_ptr, (lutatobparts->a_curves)+k*CURVE_SIZE,
                            CURVE_SIZE);
                curr_ptr += (12 + CURVE_SIZE*2);
            }
        } else {
            add_ident_curves(curr_ptr,numin);
            curr_ptr += numin*(12 + IDENT_CURVE_SIZE*2);
        }

    }
}

/* This creates an ICC profile from the PDF calGray and calRGB definitions */
cmm_profile_t*
gsicc_create_from_cal(float *white, float *black, float *gamma, float *matrix,
                      gs_memory_t *memory, int num_colors)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int profile_size,k;
    int num_tags;
    gsicc_tag *tag_list;
    unsigned short encode_gamma;
    unsigned char *curr_ptr;
    int last_tag;
    icS15Fixed16Number temp_XYZ[3];
    int tag_location;
    icTagSignature TRC_Tags[3] = {icSigRedTRCTag, icSigGreenTRCTag,
                                  icSigBlueTRCTag};
    int trc_tag_size;
    unsigned char *buffer;
    cmm_profile_t *result;
    float *cat02;
    float black_adapt[3];

    /* Fill in the common stuff */
    setheader_common(header, 4);
    header->pcs = icSigXYZData;
    profile_size = HEADER_SIZE;
    header->deviceClass = icSigInputClass;
    if (num_colors == 3) {
        header->colorSpace = icSigRgbData;
        num_tags = 10;  /* common (2) + rXYZ,gXYZ,bXYZ,rTRC,gTRC,bTRC,bkpt,wtpt */
    } else if (num_colors == 1) {
        header->colorSpace = icSigGrayData;
        num_tags = 5;  /* common (2) + GrayTRC,bkpt,wtpt */
        TRC_Tags[0] = icSigGrayTRCTag;
    } else {
        return NULL;
    }
    tag_list = (gsicc_tag*) gs_alloc_bytes(memory,
                    sizeof(gsicc_tag)*num_tags,"gsicc_create_from_cal");
    if (tag_list == NULL)
        return NULL;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE*num_tags;
    profile_size += 4; /* number of tags.... */
    last_tag = -1;
    init_common_tags(tag_list, num_tags, &last_tag);
    if (num_colors == 3) {
        init_tag(tag_list, &last_tag, icSigRedColorantTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigGreenColorantTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigBlueColorantTag, XYZPT_SIZE);
    }
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);
    /* 4 for count, 2 for gamma, Extra 2 bytes for 4 byte alignment requirement */
    trc_tag_size = 8;
    for (k = 0; k < num_colors; k++) {
        init_tag(tag_list, &last_tag, TRC_Tags[k], trc_tag_size);
    }
    for(k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }
    /* Now we can go ahead and fill our buffer with the data.  Profile
       buffer data is in non-gc memory */
    buffer = gs_alloc_bytes(memory->non_gc_memory,
                            profile_size, "gsicc_create_from_cal");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_from_cal");
        return NULL;
    }
    curr_ptr = buffer;
    /* The header */
    header->size = profile_size;
    copy_header(curr_ptr,header);
    curr_ptr += HEADER_SIZE;
    /* Tag table */
    copy_tagtable(curr_ptr,tag_list,num_tags);
    curr_ptr += TAG_SIZE*num_tags;
    curr_ptr += 4;
    /* Now the data.  Must be in same order as we created the tag table */
    /* First the common tags */
    add_common_tag_data(curr_ptr, tag_list, 4);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }
    tag_location = NUMBER_COMMON_TAGS;

    /* Get the cat02 matrix */
    cat02 = gsicc_get_cat02_cam(white, memory);
    if (cat02 == NULL)
    {
        gs_rethrow(gs_error_VMerror, "Creation of cat02 matrix / ICC profile failed");
        return NULL;
    }

    /* The matrix */
    if (num_colors == 3) {
        for ( k = 0; k < 3; k++ ) {
            float primary[3];
            /* Apply the cat02 matrix to the primaries */
            apply_adaption(cat02, &(matrix[k * 3]), &(primary[0]));
            get_XYZ_doubletr(temp_XYZ, &(primary[0]));
            add_xyzdata(curr_ptr, temp_XYZ);
            curr_ptr += tag_list[tag_location].size;
            tag_location++;
        }
    }
    /* White and black points.  WP is D50 */
    get_D50(temp_XYZ);
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    /* Black point.  Apply cat02*/
    apply_adaption(cat02, black, &(black_adapt[0]));
    get_XYZ_doubletr(temp_XYZ, &(black_adapt[0]));
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    /* Now the gamma values */
    for (k = 0; k < num_colors; k++) {
        encode_gamma = float2u8Fixed8(gamma[k]);
        add_gammadata(curr_ptr, encode_gamma, icSigCurveType);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;
    }
    result = gsicc_profile_new(NULL, memory, NULL, 0);
    if (result == NULL)
    {
        gs_throw(gs_error_VMerror, "Creation of ICC profile failed");
        return NULL;
    }
    result->buffer = buffer;
    result->buffer_size = profile_size;
    result->num_comps = num_colors;
    if (num_colors == 3) {
        result->data_cs = gsRGB;
        result->default_match = CAL_RGB;
    } else {
        result->data_cs = gsGRAY;
        result->default_match = CAL_GRAY;
    }
    /* Set the hash code  */
    gsicc_get_icc_buff_hash(buffer, &(result->hashcode), result->buffer_size);
    result->hash_is_valid = true;
    /* Free up the tag list */
    gs_free_object(memory, tag_list, "gsicc_create_from_cal");
    gs_free_object(memory, cat02, "gsicc_create_from_cal");

#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if (num_colors == 3)
        save_profile(memory,buffer,"from_calRGB",profile_size);
    else
        save_profile(memory,buffer,"from_calGray",profile_size);
#endif
    return result;
}

static void
gsicc_create_free_luta2bpart(gs_memory_t *memory, gsicc_lutatob *icc_luta2bparts)
{
    /* Note that white_point, black_point and matrix are not allocated but
       are on the local stack */
    gs_free_object(memory, icc_luta2bparts->a_curves,
                    "gsicc_create_free_luta2bpart");
    gs_free_object(memory, icc_luta2bparts->b_curves,
                    "gsicc_create_free_luta2bpart");
    gs_free_object(memory, icc_luta2bparts->m_curves,
                    "gsicc_create_free_luta2bpart");
    gs_free_object(memory, icc_luta2bparts->cam,
                    "gsicc_create_free_luta2bpart");
    if (icc_luta2bparts->clut) {
        /* Note, data_byte is handled externally.  We do not free that member here */
        gs_free_object(memory, icc_luta2bparts->clut->data_short,
                        "gsicc_create_free_luta2bpart");
        gs_free_object(memory, icc_luta2bparts->clut,
                        "gsicc_create_free_luta2bpart");
    }
}

static void
gsicc_create_init_luta2bpart(gsicc_lutatob *icc_luta2bparts)
{
    icc_luta2bparts->a_curves = NULL;
    icc_luta2bparts->b_curves = NULL;
    icc_luta2bparts->clut = NULL;
    icc_luta2bparts->m_curves = NULL;
    icc_luta2bparts->cam = NULL;
    icc_luta2bparts->matrix = NULL;
    icc_luta2bparts->white_point = NULL;
    icc_luta2bparts->black_point = NULL;
    icc_luta2bparts->num_in = 0;
    icc_luta2bparts->num_out = 0;
}

static void
gsicc_create_initialize_clut(gsicc_clut *clut)
{
    int k;

    clut->clut_num_entries = clut->clut_dims[0];
    for (k = 1; k < clut->clut_num_input; k++) {
        clut->clut_num_entries *= clut->clut_dims[k];
    }
    clut->data_byte =  NULL;
    clut->data_short = NULL;
}

/* A common form used for most of the PS CIE color spaces */
static int
create_lutAtoBprofile(unsigned char **pp_buffer_in, icHeader *header,
                      gsicc_lutatob *lutatobparts, bool yonly, bool mashedLUT,
                      gs_memory_t *memory)
{
    int num_tags = 5;  /* common (2), AToB0Tag,bkpt, wtpt */
    int k;
    gsicc_tag *tag_list;
    int profile_size, last_tag, tag_location, tag_size;
    unsigned char *buffer,*curr_ptr;
    icS15Fixed16Number temp_XYZ[3];
    gs_vector3 d50;
    float *cam;
    gs_matrix3 temp_matrix;
    float lmn_vector[3],d50_cieA[3];

    profile_size = HEADER_SIZE;
    tag_list = (gsicc_tag*) gs_alloc_bytes(memory, sizeof(gsicc_tag)*num_tags,
                                            "create_lutAtoBprofile");
    if (tag_list == NULL)
        return gs_throw(gs_error_VMerror, "Allocation of ICC tag list failed");

    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE*num_tags;
    profile_size += 4; /* number of tags.... */
    last_tag = -1;
    init_common_tags(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);

    /* Get the tag size of the A2B0 with the lutAtoBType */
    /* Compensate for init_tag() adding DATATYPE_SIZE */
    tag_size = getsize_lutAtoBtype(lutatobparts) - DATATYPE_SIZE;
    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);
    /* Add all the tag sizes to get the new profile size */
    for(k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }
    /* End of tag table information */
    /* Now we can go ahead and fill our buffer with the data.  Profile
       is in non-gc memory */
    buffer = gs_alloc_bytes(memory->non_gc_memory, profile_size,
                            "create_lutAtoBprofile");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "create_lutAtoBprofile");
        return gs_throw(gs_error_VMerror, "Allocation of ICC buffer failed");
    }
    curr_ptr = buffer;
    /* The header */
    header->size = profile_size;
    copy_header(curr_ptr,header);
    curr_ptr += HEADER_SIZE;
    /* Tag table */
    copy_tagtable(curr_ptr, tag_list, num_tags);
    curr_ptr += TAG_SIZE * num_tags;
    curr_ptr += 4;
    /* Now the data.  Must be in same order as we created the tag table */
    /* First the common tags */
    add_common_tag_data(curr_ptr, tag_list, 4);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }
    tag_location = NUMBER_COMMON_TAGS;
    /* Here we take care of chromatic adapatation.  Compute the
       matrix.  We will need to hit the data with the matrix and
       store it in the profile. */
    d50.u = D50_X;
    d50.v = D50_Y;
    d50.w = D50_Z;
    cam = (float*) gs_alloc_bytes(memory, 9 * sizeof(float), "create_lutAtoBprofile");
    if (cam == NULL) {
        gs_free_object(memory, tag_list, "create_lutAtoBprofile");
        gs_free_object(memory->non_gc_memory, buffer, "create_lutAtoBprofile");
        return gs_throw(gs_error_VMerror, "Allocation of ICC cam failed");
    }
    gsicc_create_compute_cam(lutatobparts->white_point, &(d50), cam);
    gs_free_object(memory, lutatobparts->cam, "create_lutAtoBprofile");
    lutatobparts->cam = cam;
    get_D50(temp_XYZ); /* See Appendix D6 in spec */
    add_xyzdata(curr_ptr, temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    get_XYZ(temp_XYZ, lutatobparts->black_point);
    add_xyzdata(curr_ptr, temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    /* Multiply the matrix in the AtoB object by the cam so that the data
       is in D50 */
    if (lutatobparts->matrix == NULL) {
        gsicc_create_copy_matrix3(cam, (float*) &temp_matrix);
        lutatobparts->matrix = &temp_matrix;
    } else {
        if (yonly) {
            /* Used for CIEBaseA case.  Studies of CIEBasedA spaces
               and AR rendering of these reveals that they only look
               at the product sum of the MatrixA and the 2nd column of
               the LM Matrix (if there is one).  This is used as a Y
               decode value from which to map between the black point
               and the white point.  The black point is actually ignored
               and a black point of 0 is used. Essentialy we have
               weighted versions of D50 in each column of the matrix
               which ensures we stay on the achromatic axis */
            lmn_vector[0] = lutatobparts->matrix->cv.u;
            lmn_vector[1] = lutatobparts->matrix->cv.v;
            lmn_vector[2] = lutatobparts->matrix->cv.w;
            if (mashedLUT) {
                /* Table data already scaled */
                d50_cieA[0] = D50_X;
                d50_cieA[1] = D50_Y;
                d50_cieA[2] = D50_Z;
            } else {
                /* Need to do final scaling to ICC CIEXYZ range */
                d50_cieA[0] = (float)(D50_X / (1.0 + (32767.0/32768.0)));
                d50_cieA[1] = (float)(D50_Y / (1.0 + (32767.0/32768.0)));
                d50_cieA[2] = (float)(D50_Z / (1.0 + (32767.0/32768.0)));
            }
            matrixmult(&(d50_cieA[0]), 3, 1, &(lmn_vector[0]), 1, 3,
                        &(lutatobparts->matrix->cu.u));
        } else {
            matrixmult(cam, 3, 3, &(lutatobparts->matrix->cu.u), 3, 3,
                    &(temp_matrix.cu.u));
            lutatobparts->matrix = &temp_matrix;
        }
    }
    /* Now the AToB0Tag Data. Here this will include the M curves, the matrix
       and the B curves. We may need to do some adustements with respect
       to encode and decode.  For now assume all is between 0 and 1. */
    add_lutAtoBtype(curr_ptr, lutatobparts);
    *pp_buffer_in = buffer;
    gs_free_object(memory, tag_list, "create_lutAtoBprofile");
    return 0;
}

/* Shared code between all the PS types whereby we mash together all the
   components into a single CLUT.  Not preferable in general but necessary
   when the PS components do not map easily into the ICC forms */
static int
gsicc_create_mashed_clut(gsicc_lutatob *icc_luta2bparts,
                         icHeader *header, gx_color_lookup_table *Table,
                         const gs_color_space *pcs, gs_range *ranges,
                         unsigned char **pp_buffer_in, int *profile_size_out,
                         bool range_adjust, gs_memory_t* memory)
{
    int k;
    int code;
    gsicc_clut *clut;
    gs_matrix3 ident_matrix;
    gs_vector3 ones_vec;

   /* A table is going to be mashed form of all the transform */
    /* Allocate space for the clut */
    clut = (gsicc_clut*) gs_alloc_bytes(memory, sizeof(gsicc_clut),
                                "gsicc_create_mashed_clut");
    if (clut == NULL)
        return gs_throw(gs_error_VMerror, "Allocation of ICC clut failed");
    icc_luta2bparts->clut = clut;
    if ( icc_luta2bparts->num_in == 1 ) {
        /* Use a larger sample for 1-D input */
        clut->clut_dims[0] = DEFAULT_TABLE_GRAYSIZE;
    } else {
        for (k = 0; k < icc_luta2bparts->num_in; k++) {
            if (Table != NULL && Table->dims[k] > DEFAULT_TABLE_NSIZE ) {
                /* If it has a table use the existing table size if
                   it is larger than our default size */
                clut->clut_dims[k] = Table->dims[k];
            } else {
                /* If not, then use a default size */
                clut->clut_dims[k] = DEFAULT_TABLE_NSIZE;
            }
        }
    }
    clut->clut_num_input = icc_luta2bparts->num_in;
    clut->clut_num_output = 3;  /* CIEXYZ */
    clut->clut_word_width = 2;  /* 16 bit */
    gsicc_create_initialize_clut(clut);
    /* Allocate space for the table data */
    clut->data_short = (unsigned short*) gs_alloc_bytes(memory,
        clut->clut_num_entries*3*sizeof(unsigned short),"gsicc_create_mashed_clut");
    if (clut->data_short == NULL) {
        gs_free_object(memory, clut, "gsicc_create_mashed_clut");
        return gs_throw(gs_error_VMerror, "Allocation of ICC clut short data failed");
    }
    /* Create the table */
    code = gsicc_create_clut(pcs, clut, ranges, icc_luta2bparts->white_point,
                             range_adjust, icc_luta2bparts->cam, memory);
    if (code < 0) {
        gs_free_object(memory, clut, "gsicc_create_mashed_clut");
        return gs_rethrow(code, "Creation of ICC clut failed");
    }
    /* Initialize other parts. Also make sure acurves are reset since
       they have been mashed into the table. */
    gs_free_object(memory, icc_luta2bparts->a_curves, "gsicc_create_mashed_clut");
    icc_luta2bparts->a_curves = NULL;
    icc_luta2bparts->b_curves = NULL;
    icc_luta2bparts->m_curves = NULL;
    ones_vec.u = 1;
    ones_vec.v = 1;
    ones_vec.w = 1;
    gsicc_make_diag_matrix(&ident_matrix,&ones_vec);
    icc_luta2bparts->matrix = &ident_matrix;
    /* Now create the profile */
    if (icc_luta2bparts->num_in == 1 ) {
        code = create_lutAtoBprofile(pp_buffer_in, header, icc_luta2bparts, true,
                                     true, memory);
    } else {
        code = create_lutAtoBprofile(pp_buffer_in, header, icc_luta2bparts, false,
                                     true, memory);
    }
    return code;
}

/* Shared code by ABC, DEF and DEFG compaction of ABC/LMN parts.  This is used
   when either MatrixABC is identity, LMN Decode is identity or MatrixLMN
   is identity.  This allows us to map into the ICC form and not have to mash
   into a full CLUT */
static int
gsicc_create_abc_merge(gsicc_lutatob *atob_parts, gs_matrix3 *matrixLMN,
                       gs_matrix3 *matrixABC, bool has_abc_procs,
                       bool has_lmn_procs, gx_cie_vector_cache *abc_caches,
                       gx_cie_scalar_cache *lmn_caches, gs_memory_t *memory)
{
    gs_matrix3 temp_matrix;
    gs_matrix3 *matrix_ptr;
    float *curr_pos;

    /* Determine the matrix that we will be using */
    if (!(matrixLMN->is_identity) && !(matrixABC->is_identity)){
        /* Use the product of the ABC and LMN matrices, since lmn_procs identity.
           Product must be LMN_Matrix*ABC_Matrix */
        cie_matrix_mult3(matrixLMN, matrixABC, &temp_matrix);
        cie_matrix_transpose3(&temp_matrix, atob_parts->matrix);
    } else {
        /* Either ABC matrix or LMN matrix is identity */
        if (matrixABC->is_identity) {
            matrix_ptr = matrixLMN;
        } else {
            matrix_ptr = matrixABC;
        }
        cie_matrix_transpose3(matrix_ptr, atob_parts->matrix);
    }
    /* Merge the curves */
    if (has_abc_procs && has_lmn_procs && matrixABC->is_identity) {
        /* Merge the curves into the abc curves. no b curves */
        merge_abc_lmn_curves(abc_caches, lmn_caches);
        has_lmn_procs = false;
    }
    /* Figure out what curves get mapped to where.  The only time we will use the b
       curves is if matrixABC is not the identity and we have lmn procs */
    if ( !(matrixABC->is_identity) && has_lmn_procs) {
        /* A matrix followed by a curve */
        atob_parts->b_curves = (float*) gs_alloc_bytes(memory,
                            3*CURVE_SIZE*sizeof(float),"gsicc_create_abc_merge");
        if (atob_parts->b_curves == NULL)
            return gs_throw(gs_error_VMerror, "Allocation of ICC b curves failed");
        curr_pos = atob_parts->b_curves;
        memcpy(curr_pos,&(lmn_caches[0].floats.values[0]),CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&(lmn_caches[1].floats.values[0]),CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&(lmn_caches[2].floats.values[0]),CURVE_SIZE*sizeof(float));
        if (has_abc_procs) {
            /* Also a curve before the matrix */
            atob_parts->m_curves = (float*) gs_alloc_bytes(memory,
                            3*CURVE_SIZE*sizeof(float),"gsicc_create_abc_merge");
            if (atob_parts->m_curves == NULL) {
                gs_free_object(memory, atob_parts->b_curves, "gsicc_create_abc_merge");
                return gs_throw(gs_error_VMerror, "Allocation of ICC m curves failed");
            }
            curr_pos = atob_parts->m_curves;
            memcpy(curr_pos,&(abc_caches[0].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(abc_caches[1].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(abc_caches[2].floats.values[0]),CURVE_SIZE*sizeof(float));
        }
    } else {
        /* Only one set of curves before a matrix.  Need to check this to make sure
           there is not an issue here and we have has_abc_procs true and
           has_lmn_procs true */
        if (has_abc_procs) {
            atob_parts->m_curves = (float*) gs_alloc_bytes(memory,
                            3*CURVE_SIZE*sizeof(float),"gsicc_create_abc_merge");
            if (atob_parts->m_curves == NULL)
                return gs_throw(gs_error_VMerror, "Allocation of ICC m curves failed");
            curr_pos = atob_parts->m_curves;
            memcpy(curr_pos,&(abc_caches[0].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(abc_caches[1].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(abc_caches[2].floats.values[0]),CURVE_SIZE*sizeof(float));
        }
        if (has_lmn_procs) {
            atob_parts->m_curves = (float*) gs_alloc_bytes(memory,
                                3*CURVE_SIZE*sizeof(float),"gsicc_create_abc_merge");
            if (atob_parts->m_curves == NULL)
                return gs_throw(gs_error_VMerror, "Allocation of ICC m curves failed");
            curr_pos = atob_parts->m_curves;
            memcpy(curr_pos,&(lmn_caches[0].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(lmn_caches[1].floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&(lmn_caches[2].floats.values[0]),CURVE_SIZE*sizeof(float));
        }
    }
    /* Note that if the b_curves are null and we have a matrix we need to scale
       the matrix values by 2. Otherwise an input value of 50% gray, which is
       32767 would get mapped to 32767 by the matrix.  This will be interpreted
       as a max XYZ value (s15.16) when it is eventually mapped to u16.16 due
       to the mapping of X=Y by the identity table.  If there are b_curves
       these have an output that is 16 bit. */
    if (atob_parts->b_curves == NULL) {
        scale_matrix((float*) atob_parts->matrix, 2.0);
    }
    return 0;
}

/* The ABC color space is modeled using the V4 lutAtoBType which has the
   flexibility to model  the various parameters.  Simplified versions are used
   it possible when certain parameters in the ABC color space definition are
   the identity. */
int
gsicc_create_fromabc(const gs_color_space *pcs, unsigned char **pp_buffer_in,
                     int *profile_size_out, gs_memory_t *memory,
                     gx_cie_vector_cache *abc_caches,
                     gx_cie_scalar_cache *lmn_caches, bool *islab)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
#if SAVEICCPROFILE
    int debug_catch = 1;
#endif
    int k;
    gs_matrix3 matrix_input_trans;
    gsicc_lutatob icc_luta2bparts;
    float *curr_pos;
    bool has_abc_procs = !((abc_caches->floats.params.is_identity &&
                         (abc_caches)[1].floats.params.is_identity &&
                         (abc_caches)[2].floats.params.is_identity));
    bool has_lmn_procs = !((lmn_caches->floats.params.is_identity &&
                         (lmn_caches)[1].floats.params.is_identity &&
                         (lmn_caches)[2].floats.params.is_identity));
    gs_cie_abc *pcie = pcs->params.abc;
    bool input_range_ok;
    int code;

    gsicc_create_init_luta2bpart(&icc_luta2bparts);
    gsicc_matrix_init(&(pcie->common.MatrixLMN));  /* Need this set now */
    gsicc_matrix_init(&(pcie->MatrixABC));          /* Need this set now */
    /* Fill in the common stuff */
    setheader_common(header, 4);

    /* We will use an input type class which keeps us from having to
       create an inverse.  We will keep the data a generic 3 color.
       Since we are doing PS color management the PCS is XYZ */
    header->colorSpace = icSigRgbData;
    header->deviceClass = icSigInputClass;
    header->pcs = icSigXYZData;
    icc_luta2bparts.num_in = 3;
    icc_luta2bparts.num_out = 3;
    icc_luta2bparts.white_point = &(pcie->common.points.WhitePoint);
    icc_luta2bparts.black_point = &(pcie->common.points.BlackPoint);

    /* Calculate the chromatic adaptation matrix */
    code = gsicc_compute_cam(&icc_luta2bparts, memory);
    if (code < 0) {
        return gs_rethrow(code, "Create ICC from CIEABC failed");
    }

    /* Detect if the space is CIELAB. We don't have access to pgs here though */
    /* *islab = cie_is_lab(pcie); This is not working yet */
    *islab = false;

    /* Check what combination we have with respect to the various
       LMN and ABC parameters. Depending upon the situation we
       may be able to use a standard 3 channel input profile type. If we
       do not have the LMN decode we can mash together the ABC and LMN
       matrix. Also, if ABC is identity we can mash the ABC and LMN
       decode procs.  If we have an ABC matrix, LMN procs and an LMN
       matrix we will need to create a small (2x2x2) CLUT for the ICC format. */
    input_range_ok = check_range(&(pcie->RangeABC.ranges[0]), 3);
    if (!input_range_ok) {
        /* We have a range problem at input */
        code = gsicc_create_mashed_clut(&icc_luta2bparts, header, NULL, pcs,
                                 &(pcie->RangeABC.ranges[0]), pp_buffer_in,
                                 profile_size_out, true, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed in ICC creation from ABC mashed. CLUT");
    } else {
        if (pcie->MatrixABC.is_identity || !has_lmn_procs ||
                            pcie->common.MatrixLMN.is_identity) {
            /* The merging of these parts into the curves/matrix/curves of the
               lutAtoBtype portion can be used by abc, def and defg */
            icc_luta2bparts.matrix = &matrix_input_trans;
            code = gsicc_create_abc_merge(&(icc_luta2bparts), &(pcie->common.MatrixLMN),
                                    &(pcie->MatrixABC), has_abc_procs,
                                    has_lmn_procs, pcie->caches.DecodeABC.caches,
                                    pcie->common.caches.DecodeLMN, memory);
            if (code < 0)
                return gs_rethrow(code, "Failed in ICC creation from ABC. Merge");
            icc_luta2bparts.clut =  NULL;
            /* Create the profile.  This is for the common generic form we will use
               for almost everything. */
            code = create_lutAtoBprofile(pp_buffer_in, header, &icc_luta2bparts, false,
                                         false, memory);
            if (code < 0)
                return gs_rethrow(code, "Failed in ICC creation from ABC. Profile");
        } else {
            /* This will be a bit more complex as we have an ABC matrix, LMN decode
               and an LMN matrix.  We will need to create an MLUT to handle this properly.
               Any ABC decode will be handled as the A curves.  ABC matrix will be the
               MLUT, LMN decode will be the M curves.  LMN matrix will be the Matrix
               and b curves will be identity. */
            if (has_abc_procs) {
                icc_luta2bparts.a_curves = (float*) gs_alloc_bytes(memory,
                                3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
                if (icc_luta2bparts.a_curves == NULL)
                    return gs_throw(gs_error_VMerror, "Allocation of ICC a curves failed");

                curr_pos = icc_luta2bparts.a_curves;
                memcpy(curr_pos,&(pcie->caches.DecodeABC.caches->floats.values[0]),
                                CURVE_SIZE*sizeof(float));
                curr_pos += CURVE_SIZE;
                memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[1]).floats.values[0]),
                                CURVE_SIZE*sizeof(float));
                curr_pos += CURVE_SIZE;
                memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[2]).floats.values[0]),
                                CURVE_SIZE*sizeof(float));
            }
            if (has_lmn_procs) {
                icc_luta2bparts.m_curves = (float*) gs_alloc_bytes(memory,
                                3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
                if (icc_luta2bparts.m_curves == NULL) {
                    gs_free_object(memory, icc_luta2bparts.a_curves,
                                   "gsicc_create_fromabc");
                    return gs_throw(gs_error_VMerror, "Allocation of ICC m curves failed");
                }
                curr_pos = icc_luta2bparts.m_curves;
                memcpy(curr_pos,&(pcie->common.caches.DecodeLMN->floats.values[0]),
                                CURVE_SIZE*sizeof(float));
                curr_pos += CURVE_SIZE;
                memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[1]).floats.values[0]),
                                CURVE_SIZE*sizeof(float));
                curr_pos += CURVE_SIZE;
                memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[2]).floats.values[0]),
                                CURVE_SIZE*sizeof(float));
            }
            /* Convert ABC matrix to 2x2x2 MLUT type */
            icc_luta2bparts.clut = (gsicc_clut*) gs_alloc_bytes(memory,
                                        sizeof(gsicc_clut),"gsicc_create_fromabc");
            if (icc_luta2bparts.m_curves == NULL) {
                gs_free_object(memory, icc_luta2bparts.a_curves,
                               "gsicc_create_fromabc");
                gs_free_object(memory, icc_luta2bparts.m_curves,
                               "gsicc_create_fromabc");
                return gs_throw(gs_error_VMerror, "Allocation of ICC clut failed");
            }
            for (k = 0; k < 3; k++) {
                icc_luta2bparts.clut->clut_dims[k] = 2;
            }
            icc_luta2bparts.clut->clut_num_input = 3;
            icc_luta2bparts.clut->clut_num_output = 3;
            icc_luta2bparts.clut->clut_word_width = 2;
            gsicc_create_initialize_clut(icc_luta2bparts.clut);
            /* 8 grid points, 3 outputs */
            icc_luta2bparts.clut->data_short =
                            (unsigned short*) gs_alloc_bytes(memory,
                            8*3*sizeof(short),"gsicc_create_fromabc");
            if (icc_luta2bparts.clut->data_short == NULL) {
                gs_free_object(memory, icc_luta2bparts.a_curves,
                               "gsicc_create_fromabc");
                gs_free_object(memory, icc_luta2bparts.m_curves,
                               "gsicc_create_fromabc");
                gs_free_object(memory, icc_luta2bparts.clut,
                               "gsicc_create_fromabc");
                return gs_throw(gs_error_VMerror, "Allocation of ICC clut data failed");
            }
            gsicc_matrix3_to_mlut(&(pcie->MatrixABC), icc_luta2bparts.clut->data_short);
            /* LMN Matrix */
            cie_matrix_transpose3(&(pcie->common.MatrixLMN), &matrix_input_trans);
            icc_luta2bparts.matrix = &matrix_input_trans;
            /* Create the profile */
            code = create_lutAtoBprofile(pp_buffer_in, header, &icc_luta2bparts,
                                         false, false, memory);
            if (code < 0)
                return code;
        }
    }
    gsicc_create_free_luta2bpart(memory, &icc_luta2bparts);
    *profile_size_out = header->size;
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if(debug_catch)
        save_profile(memory,*pp_buffer_in,"fromabc",header->size);
#endif
    return 0;
}

int
gsicc_create_froma(const gs_color_space *pcs, unsigned char **pp_buffer_in,
                   int *profile_size_out, gs_memory_t *memory,
                   gx_cie_vector_cache *a_cache, gx_cie_scalar_cache *lmn_caches)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
#if SAVEICCPROFILE
    int debug_catch = 1;
#endif
    gs_matrix3 matrix_input;
    float *curr_pos;
    bool has_a_proc = !(a_cache->floats.params.is_identity);
    bool has_lmn_procs = !(lmn_caches->floats.params.is_identity &&
                         (lmn_caches)[1].floats.params.is_identity &&
                         (lmn_caches)[2].floats.params.is_identity);
    gsicc_lutatob icc_luta2bparts;
    bool common_range_ok;
    gs_cie_a *pcie = pcs->params.a;
    bool input_range_ok;
    int code;

    gsicc_create_init_luta2bpart(&icc_luta2bparts);
    /* Fill in the common stuff */
    setheader_common(header, 4);
    /* We will use an input type class which keeps us from having to
       create an inverse.  We will keep the data a generic 3 color.
       Since we are doing PS color management the PCS is XYZ */
    header->colorSpace = icSigGrayData;
    header->deviceClass = icSigInputClass;
    header->pcs = icSigXYZData;
    icc_luta2bparts.num_out = 3;
    icc_luta2bparts.num_in = 1;
    icc_luta2bparts.white_point = &(pcie->common.points.WhitePoint);
    icc_luta2bparts.black_point = &(pcie->common.points.BlackPoint);

    code = gsicc_compute_cam(&icc_luta2bparts, memory);
    if (code < 0) {
        return gs_rethrow(code, "Create from CIEA failed");
    }

    /* Check the range values.  If the internal ranges are outside of
       0 to 1 then we will need to sample as a full CLUT.  The input
       range can be different, but we we will correct for this.  Finally
       we need to worry about enforcing the achromatic constraint for the
       CLUT if we are creating the entire thing. */
    common_range_ok = check_range(&(pcie->common.RangeLMN.ranges[0]),3);
    if (!common_range_ok) {
        input_range_ok = check_range(&(pcie->RangeA),1);
        code = gsicc_create_mashed_clut(&icc_luta2bparts, header, NULL, pcs,
                                 &(pcie->RangeA), pp_buffer_in, profile_size_out,
                                 !input_range_ok, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed to create ICC mashed CLUT");
    } else {
        /* We do not need to create a massive CLUT.  Try to maintain
           the objects as best we can */
        /* Since we are going from 1 gray input to 3 XYZ values, we will need
           to include the MLUT for the 1 to 3 conversion applied by the matrix A.
           Depending upon the other parameters we may have simpiler forms, but this
           is required even when Matrix A is the identity. */
        if (has_a_proc) {
            icc_luta2bparts.a_curves = (float*) gs_alloc_bytes(memory,
                CURVE_SIZE*sizeof(float),"gsicc_create_froma");
            if (icc_luta2bparts.a_curves == NULL)
                return gs_throw(gs_error_VMerror, "Allocation of ICC a curves failed");
            memcpy(icc_luta2bparts.a_curves,
                    &(pcie->caches.DecodeA.floats.values[0]),
                    CURVE_SIZE*sizeof(float));
        }
        if (has_lmn_procs) {
            icc_luta2bparts.m_curves = (float*) gs_alloc_bytes(memory,
                3*CURVE_SIZE*sizeof(float),"gsicc_create_froma");
            if (icc_luta2bparts.m_curves == NULL) {
                gs_free_object(memory, icc_luta2bparts.a_curves, "gsicc_create_froma");
                return gs_throw(gs_error_VMerror, "Allocation of ICC m curves failed");
            }
            curr_pos = icc_luta2bparts.m_curves;
            memcpy(curr_pos,&(pcie->common.caches.DecodeLMN->floats.values[0]),
                        CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[1]).floats.values[0]),
                        CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[2]).floats.values[0]),
                        CURVE_SIZE*sizeof(float));
        }
        /* Convert diagonal A matrix to 2x1 MLUT type */
        icc_luta2bparts.clut = (gsicc_clut*) gs_alloc_bytes(memory,
            sizeof(gsicc_clut),"gsicc_create_froma"); /* 2 grid points 3 outputs */
        if (icc_luta2bparts.clut == NULL) {
            gs_free_object(memory, icc_luta2bparts.a_curves, "gsicc_create_froma");
            gs_free_object(memory, icc_luta2bparts.m_curves, "gsicc_create_froma");
            return gs_throw(gs_error_VMerror, "Allocation of ICC clut failed");
        }
        icc_luta2bparts.clut->clut_dims[0] = 2;
        icc_luta2bparts.clut->clut_num_input = 1;
        icc_luta2bparts.clut->clut_num_output = 3;
        icc_luta2bparts.clut->clut_word_width = 2;
        gsicc_create_initialize_clut(icc_luta2bparts.clut);
        /* 2 grid points 3 outputs */
        icc_luta2bparts.clut->data_short = (unsigned short*)
                    gs_alloc_bytes(memory, 2 * 3 * sizeof(short),
                   "gsicc_create_froma");
        if (icc_luta2bparts.clut == NULL) {
            gs_free_object(memory, icc_luta2bparts.a_curves, "gsicc_create_froma");
            gs_free_object(memory, icc_luta2bparts.m_curves, "gsicc_create_froma");
            gs_free_object(memory, icc_luta2bparts.clut, "gsicc_create_froma");
            return gs_throw(gs_error_VMerror, "Allocation of ICC clut data failed");
        }
        /*  Studies of CIEBasedA spaces
            and AR rendering of these reveals that they only look
            at the product sum of the MatrixA and the 2nd column of
            the LM Matrix (if there is one).  This is used as a Y
            decode value from which to map between the black point
            and the white point.  The black point is actually ignored
            and a black point of 0 is used. */
        gsicc_vec_to_mlut(&(pcie->MatrixA), icc_luta2bparts.clut->data_short);
        cie_matrix_transpose3(&(pcie->common.MatrixLMN), &matrix_input);
        /* Encoding to ICC range happens in create_lutAtoBprofile */
        icc_luta2bparts.matrix = &matrix_input;
        icc_luta2bparts.num_in = 1;
        icc_luta2bparts.num_out = 3;
        /* Create the profile */
        /* Note Adobe only looks at the Y value for CIEBasedA spaces.
           we will do the same */
        code = create_lutAtoBprofile(pp_buffer_in, header, &icc_luta2bparts, true,
                                     false, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed to create ICC AtoB Profile");
    }
    *profile_size_out = header->size;
    gsicc_create_free_luta2bpart(memory, &icc_luta2bparts);
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if(debug_catch)
        save_profile(memory,*pp_buffer_in,"froma",header->size);
#endif
    return 0;
}

/* Common code shared by def and defg generation */
static int
gsicc_create_defg_common(gs_cie_abc *pcie, gsicc_lutatob *icc_luta2bparts,
                         bool has_lmn_procs, bool has_abc_procs,
                         icHeader *header, gx_color_lookup_table *Table,
                         const gs_color_space *pcs, gs_range *ranges,
                         unsigned char **pp_buffer_in, int *profile_size_out,
                         gs_memory_t* memory)
{
    gs_matrix3 matrix_input_trans;
    int k;
    bool input_range_ok;
    int code;

    gsicc_matrix_init(&(pcie->common.MatrixLMN));  /* Need this set now */
    gsicc_matrix_init(&(pcie->MatrixABC));          /* Need this set now */
    setheader_common(header, 4);

    /* We will use an input type class which keeps us from having to
       create an inverse.  We will keep the data a generic 3 color.
       Since we are doing PS color management the PCS is XYZ */
    header->deviceClass = icSigInputClass;
    header->pcs = icSigXYZData;
    icc_luta2bparts->num_out = 3;
    icc_luta2bparts->white_point = &(pcie->common.points.WhitePoint);
    icc_luta2bparts->black_point = &(pcie->common.points.BlackPoint);

    /* Calculate the chromatic adaptation matrix */
    code = gsicc_compute_cam(icc_luta2bparts, memory);
    if (code < 0) {
        return gs_rethrow(code, "Create ICC from CIEABC failed");
    }

    /* question now is, can we keep the table as it is, or do we need to merge
     some of the def(g) parts.  Some merging or operators into the table must occur
     if we have MatrixABC, LMN Decode and Matrix LMN, otherwise we can encode
     the table directly and squash the rest into the curves matrix curve portion
     of the ICC form */
    if ( (!(pcie->MatrixABC.is_identity) && has_lmn_procs &&
                   !(pcie->common.MatrixLMN.is_identity)) || 1 ) {
        /* Table must take over some of the other elements. We are going to
           go to a 16 bit table in this case.  For now, we are going to
           mash all the elements in the table.  We may want to revisit this later. */
        /* We must complete the defg or def decode function such that it is within
           the HIJ(K) range AND is scaled to index into the CLUT properly */
        if (gs_color_space_get_index(pcs) == gs_color_space_index_CIEDEF) {
            input_range_ok = check_range(&(pcs->params.def->RangeDEF.ranges[0]),3);
        } else {
            input_range_ok = check_range(&(pcs->params.defg->RangeDEFG.ranges[0]),4);
        }
        code = gsicc_create_mashed_clut(icc_luta2bparts, header, Table,
                            pcs, ranges, pp_buffer_in, profile_size_out,
                            !input_range_ok, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed to create ICC clut");
    } else {
        /* Table can stay as is. Handle the ABC/LMN portions via the curves
           matrix curves operation */
        icc_luta2bparts->matrix = &matrix_input_trans;
        code = gsicc_create_abc_merge(icc_luta2bparts, &(pcie->common.MatrixLMN),
                                &(pcie->MatrixABC), has_abc_procs,
                                has_lmn_procs, pcie->caches.DecodeABC.caches,
                                pcie->common.caches.DecodeLMN, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed to create ICC abc merge");

        /* Get the table data */
        icc_luta2bparts->clut = (gsicc_clut*) gs_alloc_bytes(memory,
                            sizeof(gsicc_clut),"gsicc_create_defg_common");
        if (icc_luta2bparts->clut == NULL)
            return gs_throw(gs_error_VMerror, "Allocation of ICC clut failed");

        for (k = 0; k < icc_luta2bparts->num_in; k++) {
            icc_luta2bparts->clut->clut_dims[k] = Table->dims[k];
        }
        icc_luta2bparts->clut->clut_num_input = icc_luta2bparts->num_in;
        icc_luta2bparts->clut->clut_num_output = 3;
        icc_luta2bparts->clut->clut_word_width = 1;
        gsicc_create_initialize_clut(icc_luta2bparts->clut);
        /* Get the PS table data directly */
        icc_luta2bparts->clut->data_byte = (byte*) Table->table->data;
        /* Create the profile. */
        code = create_lutAtoBprofile(pp_buffer_in, header, icc_luta2bparts, false,
                                     false, memory);
        if (code < 0)
            return gs_rethrow(code, "Failed to create ICC lutAtoB");
    }
    gsicc_create_free_luta2bpart(memory, icc_luta2bparts);
    *profile_size_out = header->size;
    return 0;
}

/* If we have an ABC matrix, a DecodeLMN and an LMN matrix we have to mash
   together the table, Decode ABC (if present) and ABC matrix. */
int
gsicc_create_fromdefg(const gs_color_space *pcs, unsigned char **pp_buffer_in,
                      int *profile_size_out, gs_memory_t *memory,
                      gx_cie_vector_cache *abc_caches,
                      gx_cie_scalar_cache *lmn_caches,
                      gx_cie_scalar_cache *defg_caches)
{
    gs_cie_defg *pcie = pcs->params.defg;
    gsicc_lutatob icc_luta2bparts;
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
#if SAVEICCPROFILE
    int debug_catch = 1;
#endif
    float *curr_pos;
    bool has_abc_procs = !((abc_caches->floats.params.is_identity &&
                         (abc_caches)[1].floats.params.is_identity &&
                         (abc_caches)[2].floats.params.is_identity));
    bool has_lmn_procs = !((lmn_caches->floats.params.is_identity &&
                         (lmn_caches)[1].floats.params.is_identity &&
                         (lmn_caches)[2].floats.params.is_identity));
    bool has_defg_procs = !((defg_caches->floats.params.is_identity &&
                         (defg_caches)[1].floats.params.is_identity &&
                         (defg_caches)[2].floats.params.is_identity &&
                         (defg_caches)[3].floats.params.is_identity));
    int code;

    /* Fill in the uncommon stuff */
    gsicc_create_init_luta2bpart(&icc_luta2bparts);
    header->colorSpace = icSigCmykData;
    icc_luta2bparts.num_in = 4;

    /* The a curves stored as def procs */
    if (has_defg_procs) {
        icc_luta2bparts.a_curves = (float*) gs_alloc_bytes(memory,
            4*CURVE_SIZE*sizeof(float),"gsicc_create_fromdefg");
        if (icc_luta2bparts.a_curves == NULL)
            return gs_throw(gs_error_VMerror, "Allocation of ICC a curves failed");
        curr_pos = icc_luta2bparts.a_curves;
        memcpy(curr_pos,&(pcie->caches_defg.DecodeDEFG->floats.values[0]),
                CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&((pcie->caches_defg.DecodeDEFG[1]).floats.values[0]),
                CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&((pcie->caches_defg.DecodeDEFG[2]).floats.values[0]),
                CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&((pcie->caches_defg.DecodeDEFG[3]).floats.values[0]),
                CURVE_SIZE*sizeof(float));
    }
    /* Note the recast.  Should be OK since we only access common stuff in there */
    code = gsicc_create_defg_common((gs_cie_abc*) pcie, &icc_luta2bparts,
                                    has_lmn_procs, has_abc_procs,
                                    header, &(pcie->Table), pcs,
                                    &(pcie->RangeDEFG.ranges[0]),
                                    pp_buffer_in, profile_size_out, memory);
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if(debug_catch)
        save_profile(memory,*pp_buffer_in,"fromdefg",header->size);
#endif
    return code;
}

int
gsicc_create_fromdef(const gs_color_space *pcs, unsigned char **pp_buffer_in,
                     int *profile_size_out, gs_memory_t *memory,
                     gx_cie_vector_cache *abc_caches,
                     gx_cie_scalar_cache *lmn_caches,
                     gx_cie_scalar_cache *def_caches)
{
    gs_cie_def *pcie = pcs->params.def;
    gsicc_lutatob icc_luta2bparts;
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
#if SAVEICCPROFILE
    int debug_catch = 1;
#endif
    float *curr_pos;
    bool has_abc_procs = !((abc_caches->floats.params.is_identity &&
                         (abc_caches)[1].floats.params.is_identity &&
                         (abc_caches)[2].floats.params.is_identity));
    bool has_lmn_procs = !((lmn_caches->floats.params.is_identity &&
                         (lmn_caches)[1].floats.params.is_identity &&
                         (lmn_caches)[2].floats.params.is_identity));
    bool has_def_procs = !((def_caches->floats.params.is_identity &&
                         (def_caches)[1].floats.params.is_identity &&
                         (def_caches)[2].floats.params.is_identity));
    int code;

    gsicc_create_init_luta2bpart(&icc_luta2bparts);

    header->colorSpace = icSigRgbData;
    icc_luta2bparts.num_in = 3;

    /* The a curves stored as def procs */
    if (has_def_procs) {
        icc_luta2bparts.a_curves = (float*) gs_alloc_bytes(memory,
                        3*CURVE_SIZE*sizeof(float),"gsicc_create_fromdef");
        if (icc_luta2bparts.a_curves == NULL)
            return gs_throw(gs_error_VMerror, "Allocation of ICC a curves failed");
        curr_pos = icc_luta2bparts.a_curves;
        memcpy(curr_pos,&(pcie->caches_def.DecodeDEF->floats.values[0]),
                CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&((pcie->caches_def.DecodeDEF[1]).floats.values[0]),
                CURVE_SIZE*sizeof(float));
        curr_pos += CURVE_SIZE;
        memcpy(curr_pos,&((pcie->caches_def.DecodeDEF[2]).floats.values[0]),
                CURVE_SIZE*sizeof(float));
    }
    code = gsicc_create_defg_common((gs_cie_abc*) pcie, &icc_luta2bparts,
                                    has_lmn_procs, has_abc_procs, header,
                                    &(pcie->Table), pcs, &(pcie->RangeDEF.ranges[0]),
                                    pp_buffer_in, profile_size_out, memory);
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if(debug_catch)
        save_profile(memory,*pp_buffer_in,"fromdef",header->size);
#endif
    return code;
}

void
gsicc_create_fromcrd(unsigned char *buffer, gs_memory_t *memory)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header, 4);
}

/* V2 creation from current profile */

#define TRC_V2_SIZE 256

static void
init_common_tagsv2(gsicc_tag tag_list[], int num_tags, int *last_tag)
{
    /*    profileDescriptionTag copyrightTag  */
    int curr_tag, temp_size;

    if (*last_tag < 0)
        curr_tag = 0;
    else
        curr_tag = (*last_tag) + 1;

    tag_list[curr_tag].offset = HEADER_SIZE + num_tags * TAG_SIZE + 4;
    tag_list[curr_tag].sig = icSigProfileDescriptionTag;
    temp_size = DATATYPE_SIZE + 4 + strlen(desc_name) + 1 + 12 + 67;
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;

    curr_tag++;

    tag_list[curr_tag].offset = tag_list[curr_tag - 1].offset +
        tag_list[curr_tag - 1].size;
    tag_list[curr_tag].sig = icSigCopyrightTag;
    temp_size = DATATYPE_SIZE + strlen(copy_right) + 1;
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;
    *last_tag = curr_tag;
}

static int
getsize_lut16Type(int tablesize, int num_inputs, int num_outputs)
{
    int clutsize;

    /* Header (-8 as we already include the type later)
       plus linear curves (2 points each of 2 bytes) */
    int size = 52 - 8 + 4 * num_inputs + 4 * num_outputs;
    clutsize = (int) pow(tablesize, num_inputs) * num_outputs * 2;
    return size + clutsize;
}

static int
getsize_lut8Type(int tablesize, int num_inputs, int num_outputs)
{
    int clutsize;

    /* Header (-8 as we already include the type later)
       plus linear curves (2 points each of 2 bytes) */
    int size = 48 - 8 + 256 * num_inputs + 256 * num_outputs;
    clutsize = (int)pow(tablesize, num_inputs) * num_outputs;
    return size + clutsize;
}


static byte*
write_v2_common_data(byte *buffer, int profile_size, icHeader *header,
    gsicc_tag *tag_list, int num_tags, byte *mediawhitept)
{
    byte *curr_ptr = buffer;
    int k;

    /* The header */
    header->size = profile_size;
    copy_header(curr_ptr, header);
    curr_ptr += HEADER_SIZE;

    /* Tag table */
    copy_tagtable(curr_ptr, tag_list, num_tags);
    curr_ptr += TAG_SIZE*num_tags;
    curr_ptr += 4;

    /* Common tags */
    add_common_tag_data(curr_ptr, tag_list, 2);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }

    /* Media white point. Get from current profile */
    write_bigendian_4bytes(curr_ptr, icSigXYZType);
    curr_ptr += 4;
    memset(curr_ptr, 0, 4);
    curr_ptr += 4;
    memcpy(curr_ptr, mediawhitept, 12);
    curr_ptr += 12;

    return curr_ptr;
}

static gsicc_link_t*
get_link(const gs_gstate *pgs, cmm_profile_t *src_profile,
    cmm_profile_t *des_profile, gsicc_rendering_intents_t intent)
{
    gsicc_rendering_param_t rendering_params;

    /* Now the main colorants. Get them and the TRC data from using the link
    between the source profile and the CIEXYZ profile */
    rendering_params.black_point_comp = gsBLACKPTCOMP_OFF;
    rendering_params.override_icc = false;
    rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
    rendering_params.rendering_intent = intent;
    rendering_params.cmm = gsCMM_DEFAULT;
    return gsicc_get_link_profile(pgs, NULL, src_profile, des_profile,
        &rendering_params, pgs->memory, false);
}

static void
get_colorant(int index, gsicc_link_t *link, icS15Fixed16Number XYZ_Data[])
{
    unsigned short des[3], src[3];
    int k;

    src[0] = 0;
    src[1] = 0;
    src[2] = 0;
    src[index] = 65535;
    (link->procs.map_color)(NULL, link, &src, &des, 2);
    for (k = 0; k < 3; k++) {
        XYZ_Data[k] = double2XYZtype((float)des[k] / 65535.0);
    }
}

static void
get_trc(int index, gsicc_link_t *link, float **htrc, int trc_size)
{
    unsigned short des[3], src[3];
    float max;
    float *ptrc = *htrc;
    int k;

    src[0] = 0;
    src[1] = 0;
    src[2] = 0;

    /* First get the max value for Y on the range */
    src[index] = 65535;
    (link->procs.map_color)(NULL, link, &src, &des, 2);
    max = des[1];

    for (k = 0; k < trc_size; k++) {
        src[index] = (unsigned short)((double)k * (double)65535 / (double)(trc_size - 1));
        (link->procs.map_color)(NULL, link, &src, &des, 2);
        /* Use Y */
        ptrc[k] = (float)des[1] / max;
    }
}

static void
clean_lut(gsicc_clut *clut, gs_memory_t *memory)
{
    if (clut->clut_word_width == 2)
        gs_free_object(memory, clut->data_short, "clean_lut");
    else
        gs_free_object(memory, clut->data_byte, "clean_lut");
}

/* This is used for the A2B0 type table and B2A0. */
static int
create_clut_v2(gsicc_clut *clut, gsicc_link_t *link, int num_in,
        int num_out, int table_size, gs_memory_t *memory, int bitdepth)
{
    unsigned short *input_samples, *indexptr;
    unsigned short *ptr_short;
    byte *ptr_byte;
    int num_points, index;
    unsigned short input[4], output[4];
    int kk, j, i;

    clut->clut_num_input = num_in;
    clut->clut_num_output = num_out;
    clut->clut_word_width = bitdepth;
    for (kk = 0; kk < num_in; kk++)
        clut->clut_dims[kk] = table_size;
    clut->clut_num_entries = (int) pow(table_size, num_in);
    num_points = clut->clut_num_entries;
    if (bitdepth == 2) {
        clut->data_byte = NULL;
        clut->data_short = (unsigned short*)gs_alloc_bytes(memory,
                              (size_t)clut->clut_num_entries * num_out *
                                                 sizeof(unsigned short),
                              "create_clut_v2");
        if (clut->data_short == NULL)
            return -1;
    } else {
        clut->data_short = NULL;
        clut->data_byte = (byte*)gs_alloc_bytes(memory,
                               (size_t)clut->clut_num_entries * num_out,
                               "create_clut_v2");
        if (clut->data_byte == NULL)
            return -1;
    }

    /* Create the sample indices */
    input_samples = (unsigned short*) gs_alloc_bytes(memory,
        sizeof(unsigned short)*table_size, "create_clut_v2");
    if (input_samples == NULL) {
        return -1;
    }
    indexptr = input_samples;
    for (j = 0; j < table_size; j++)
        *indexptr++ = (unsigned short)(((double)j / (double)(table_size - 1)) * 65535.0);

    /* Now populate the table. Index 1 goes the slowest (e.g. R) */
    ptr_short = clut->data_short;
    ptr_byte = clut->data_byte;
    for (i = 0; i < num_points; i++) {
        if (num_in == 1) {
            index = i%table_size;
            input[0] = input_samples[index];
        }
        if (num_in == 3) {
            index = i%table_size;
            input[2] = input_samples[index];
            index = (unsigned int)floor((float)i / (float)table_size) % table_size;
            input[1] = input_samples[index];
            index = (unsigned int)floor((float)i / (float)(table_size*
                table_size)) % table_size;
            input[0] = input_samples[index];
        }
        if (num_in == 4) {
            index = i%table_size;
            input[3] = input_samples[index];
            index = (unsigned int)floor((float)i / (float)table_size) % table_size;
            input[2] = input_samples[index];
            index = (unsigned int)floor((float)i / (float)(table_size*
                table_size)) % table_size;
            input[1] = input_samples[index];
            index = (unsigned int)floor((float)i / (float)(table_size*
                table_size*table_size)) % table_size;
            input[0] = input_samples[index];
        }
        if (link == NULL) {
            /* gamut table case */
            for (j = 0; j < num_out; j++) {
                if (bitdepth == 2)
                    *ptr_short++ = 1;
                else
                    *ptr_byte++ = 1;
            }
        } else {
            double temp;
            (link->procs.map_color)(NULL, link, input, output, 2);

            /* Note.  We are using 16 bit for the forward table
               (colorant to lab) and 8 bit for the backward table
               (lab to colorant).  A larger table is used for the backward
               table to reduce quantization */

            if (bitdepth == 2) {
                /* Output is LAB 16 bit */
                /* Apply offset of 128 on a and b */
                output[1] = output[1] - 128;
                output[2] = output[2] - 128;
                /* Scale L to range 0 to 0xFF00 */
                temp = (double)output[0] / 65535.0;
                temp = temp * 65280.0;
                output[0] = (unsigned short)temp;
                for (j = 0; j < num_out; j++)
                    *ptr_short++ = output[j];
            } else {
                /* Output is colorant and 8 bit */
                for (j = 0; j < num_out; j++) {
                    double temp = (double)output[j] * 255.0 / 65535.0;
                    *ptr_byte++ = (byte) temp;
                }
            }
        }
    }
    gs_free_object(memory, input_samples, "create_clut_v2");
    return 0;
}


/* Here we write out the lut16Type or lut8Type data V2. Curves are always linear,
   matrix is the identity.  Table data is unique and could be a forward
   or inverse table */
static byte*
add_lutType(byte *input_ptr, gsicc_clut *lut)
{
    byte *curr_ptr;
    unsigned char numout = lut->clut_num_output;
    unsigned char numin = lut->clut_num_input;
    unsigned char tablesize = lut->clut_dims[0];
    float ident[9] = { 1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0 };
    int clut_size = lut->clut_num_entries * numout, k, j;

    /* Signature */
    curr_ptr = input_ptr;
    if (lut->clut_word_width == 2)
        write_bigendian_4bytes(curr_ptr, icSigLut16Type);
    else
        write_bigendian_4bytes(curr_ptr, icSigLut8Type);
    curr_ptr += 4;
    /* Reserved */
    memset(curr_ptr, 0, 4);
    curr_ptr += 4;
    /* Sizes padded */
    *curr_ptr++ = numin;
    *curr_ptr++ = numout;
    *curr_ptr++ = tablesize;
    *curr_ptr++ = 0;

    /* Now the identity matrix */
    add_matrixwithbias(curr_ptr, &(ident[0]), false);
    curr_ptr += (9 * 4);

    /* Input TRC are linear.  16 bit can have 2 points. 8 bit need 256 */
    if (lut->clut_word_width == 2) {
        /* Sizes */
        write_bigendian_2bytes(curr_ptr, 2);
        curr_ptr += 2;
        write_bigendian_2bytes(curr_ptr, 2);
        curr_ptr += 2;

        /* Input table data. Linear. */
        for (k = 0; k < numin; k++) {
            write_bigendian_2bytes(curr_ptr, 0);
            curr_ptr += 2;
            write_bigendian_2bytes(curr_ptr, 65535);
            curr_ptr += 2;
        }
    } else {
        /* Input table data. Linear. */
        for (k = 0; k < numin; k++)
            for (j = 0; j < 256; j++)
                *curr_ptr++ = j;
    }

    /* The CLUT. Write out each entry. */
    if (lut->clut_word_width == 2) {
        for (k = 0; k < clut_size; k++) {
            write_bigendian_2bytes(curr_ptr, lut->data_short[k]);
            curr_ptr += 2;
        }
    } else {
        for (k = 0; k < clut_size; k++)
            *curr_ptr++ = lut->data_byte[k];
    }

    /* Output table data. Linear. */
    if (lut->clut_word_width == 2) {
        for (k = 0; k < numout; k++) {
            write_bigendian_2bytes(curr_ptr, 0);
            curr_ptr += 2;
            write_bigendian_2bytes(curr_ptr, 65535);
            curr_ptr += 2;
        }
    } else {
        for (k = 0; k < numout; k++)
            for (j = 0; j < 256; j++)
                *curr_ptr++ = j;
    }
    return curr_ptr;
}

static int
create_write_table_intent(const gs_gstate *pgs, gsicc_rendering_intents_t intent,
        cmm_profile_t *src_profile, cmm_profile_t *des_profile, byte *curr_ptr,
        int table_size, int bit_depth, int padding)
{
    gsicc_link_t *link;
    int code;
    gsicc_clut clut;

    link = get_link(pgs, src_profile, des_profile, intent);
    if (link == NULL)
        return_error(gs_error_undefined);
    code = create_clut_v2(&clut, link, src_profile->num_comps,
        des_profile->num_comps, table_size, pgs->memory, bit_depth);
    if (code < 0)
        return code;
    curr_ptr = add_lutType(curr_ptr, &clut);
    memset(curr_ptr, 0, padding);
    clean_lut(&clut, pgs->memory);
    gsicc_release_link(link);
    return 0;
}

static void
gsicc_create_v2input(const gs_gstate *pgs, icHeader *header, cmm_profile_t *src_profile,
                byte *mediawhitept, cmm_profile_t *lab_profile)
{
    /* Need to create the forward table only (Gray, RGB, CMYK to LAB) */
    int num_tags = 4; /* 2 common + white + A2B0 */
    int profile_size = HEADER_SIZE;
    gsicc_tag *tag_list;
    gs_memory_t *memory = src_profile->memory;
    int last_tag = -1;
    byte *buffer, *curr_ptr;
    gsicc_link_t *link;
    int tag_size;
    gsicc_clut clut;
    int code, k;

    /* Profile description tag, copyright tag white point and grayTRC */
    tag_list = (gsicc_tag*)gs_alloc_bytes(memory,
        sizeof(gsicc_tag)*num_tags, "gsicc_create_v2input");
    if (tag_list == NULL)
        return;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE * num_tags;
    profile_size += 4; /* number of tags.... */

    /* Common tags */
    init_common_tagsv2(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);

    /* Get the tag size of the A2B0 with the lut16Type */
    tag_size = getsize_lut16Type(FORWARD_V2_TABLE_SIZE, src_profile->num_comps, 3);
    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);

    /* Now get the profile size */
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    /* Allocate buffer */
    buffer = gs_alloc_bytes(memory, profile_size, "gsicc_create_v2input");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_v2input");
        return;
    }

    /* Write out data */
    curr_ptr = write_v2_common_data(buffer, profile_size, header, tag_list,
        num_tags, mediawhitept);

    /* Now the A2B0 Tag */
    link = get_link(pgs, src_profile, lab_profile, gsPERCEPTUAL);
    if (link == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_v2input");
        gs_free_object(memory, buffer, "gsicc_create_v2input");
        return;
    }

    /* First create the data */
    code = create_clut_v2(&clut, link, src_profile->num_comps, 3,
        FORWARD_V2_TABLE_SIZE, pgs->memory, 2);
    if (code < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2input");
        gs_free_object(memory, buffer, "gsicc_create_v2input");
        return;
    }

    /* Now write it out */
    curr_ptr = add_lutType(curr_ptr, &clut);
    memset(curr_ptr, 0, tag_list[last_tag].byte_padding);  /* padding */

    /* Clean up */
    gsicc_release_link(link);
    clean_lut(&clut, pgs->memory);
    gs_free_object(memory, tag_list, "gsicc_create_v2input");
    /* Save the v2 data */
    src_profile->v2_data = buffer;
    src_profile->v2_size = profile_size;

#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    save_profile(memory,buffer, "V2InputType", profile_size);
#endif
}

static void
gsicc_create_v2output(const gs_gstate *pgs, icHeader *header, cmm_profile_t *src_profile,
                byte *mediawhitept, cmm_profile_t *lab_profile)
{
    /* Need to create forward and backward table (Gray, RGB, CMYK to LAB and back)
       and need to do this for all the intents */
    int num_tags = 10; /* 2 common + white + A2B0 + B2A0 + A2B1 + B2A1 + A2B2 + B2A2 + gamut */
    int profile_size = HEADER_SIZE;
    gsicc_tag *tag_list;
    gs_memory_t *memory = src_profile->memory;
    int last_tag = -1;
    byte *buffer, *curr_ptr;
    int tag_location;
    int tag_size;
    gsicc_clut gamutlut;
    int code, k;

    /* Profile description tag, copyright tag white point and grayTRC */
    tag_list = (gsicc_tag*)gs_alloc_bytes(memory,
        sizeof(gsicc_tag)*num_tags, "gsicc_create_v2output");
    if (tag_list == NULL)
        return;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE * num_tags;
    profile_size += 4; /* number of tags.... */

    /* Common tags */
    init_common_tagsv2(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);

    /* Get the tag size of the cluts with the lut16Type */
    /* Perceptual */
    tag_size = getsize_lut16Type(FORWARD_V2_TABLE_SIZE, src_profile->num_comps, 3);
    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);
    tag_size = getsize_lut8Type(BACKWARD_V2_TABLE_SIZE, 3, src_profile->num_comps);
    init_tag(tag_list, &last_tag, icSigBToA0Tag, tag_size);

    /* Relative Colorimetric */
    tag_size = getsize_lut16Type(FORWARD_V2_TABLE_SIZE, src_profile->num_comps, 3);
    init_tag(tag_list, &last_tag, icSigAToB1Tag, tag_size);
    tag_size = getsize_lut8Type(BACKWARD_V2_TABLE_SIZE, 3, src_profile->num_comps);
    init_tag(tag_list, &last_tag, icSigBToA1Tag, tag_size);

    /* Saturation */
    tag_size = getsize_lut16Type(FORWARD_V2_TABLE_SIZE, src_profile->num_comps, 3);
    init_tag(tag_list, &last_tag, icSigAToB2Tag, tag_size);
    tag_size = getsize_lut8Type(BACKWARD_V2_TABLE_SIZE, 3, src_profile->num_comps);
    init_tag(tag_list, &last_tag, icSigBToA2Tag, tag_size);

    /* And finally the Gamut Tag.  Since we can't determine gamut here this
       is essentially a required place holder. Make it small */
    tag_size = getsize_lut8Type(2, src_profile->num_comps, 1);
    init_tag(tag_list, &last_tag, icSigGamutTag, tag_size);

    /* Now get the profile size */
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    /* Allocate buffer */
    buffer = gs_alloc_bytes(memory, profile_size, "gsicc_create_v2output");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }

    /* Write out data */
    curr_ptr = write_v2_common_data(buffer, profile_size, header, tag_list,
        num_tags, mediawhitept);
    tag_location = V2_COMMON_TAGS;

    /* A2B0 */
    if (create_write_table_intent(pgs, gsPERCEPTUAL, src_profile, lab_profile,
        curr_ptr, FORWARD_V2_TABLE_SIZE, 2,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* B2A0 */
    if (create_write_table_intent(pgs, gsPERCEPTUAL, lab_profile, src_profile,
        curr_ptr, BACKWARD_V2_TABLE_SIZE, 1,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* A2B1 */
    if (create_write_table_intent(pgs, gsRELATIVECOLORIMETRIC, src_profile,
        lab_profile, curr_ptr, FORWARD_V2_TABLE_SIZE, 2,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* B2A1 */
    if (create_write_table_intent(pgs, gsRELATIVECOLORIMETRIC, lab_profile,
        src_profile, curr_ptr, BACKWARD_V2_TABLE_SIZE, 1,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* A2B2 */
    if (create_write_table_intent(pgs, gsSATURATION, src_profile, lab_profile,
        curr_ptr, FORWARD_V2_TABLE_SIZE, 2,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* B2A2 */
    if (create_write_table_intent(pgs, gsSATURATION, lab_profile, src_profile,
        curr_ptr, BACKWARD_V2_TABLE_SIZE, 1,
        tag_list[tag_location].byte_padding) < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* Gamut tag, which is bogus */
    code = create_clut_v2(&gamutlut, NULL, src_profile->num_comps, 1, 2, pgs->memory, 1);
    if (code < 0) {
        gs_free_object(memory, tag_list, "gsicc_create_v2output");
        return;
    }

    /* Now write it out */
    curr_ptr = add_lutType(curr_ptr, &gamutlut);
    memset(curr_ptr, 0, tag_list[tag_location].byte_padding);

    /* Done */
    gs_free_object(memory, tag_list, "gsicc_create_v2output");
    clean_lut(&gamutlut, pgs->memory);

    /* Save the v2 data */
    src_profile->v2_data = buffer;
    src_profile->v2_size = profile_size;

#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    save_profile(memory,buffer, "V2OutputType", profile_size);
#endif
}

static void
gsicc_create_v2displaygray(const gs_gstate *pgs, icHeader *header, cmm_profile_t *src_profile,
            byte *mediawhitept, cmm_profile_t *xyz_profile)
{
    int num_tags = 4;
    int profile_size = HEADER_SIZE;
    gsicc_tag *tag_list;
    gs_memory_t *memory = src_profile->memory;
    int last_tag = -1;
    /* 4 for name, 4 reserved, 4 for number entries, 2*num_entries */
    int trc_tag_size = 12 + 2 * TRC_V2_SIZE;
    byte *buffer, *curr_ptr;
    unsigned short des[3], src;
    float *trc;
    int tag_location;
    gsicc_link_t *link;
    float max;
    int k;

    /* Profile description tag, copyright tag white point and grayTRC */
    tag_list = (gsicc_tag*)gs_alloc_bytes(memory,
        sizeof(gsicc_tag)*num_tags, "gsicc_createv2display_gray");
    if (tag_list == NULL)
        return;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE * num_tags;
    profile_size += 4; /* number of tags.... */

    /* Common tags */
    init_common_tagsv2(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigGrayTRCTag, trc_tag_size);

    /* Now get the profile size */
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }
    /* Allocate buffer */
    buffer = gs_alloc_bytes(memory, profile_size, "gsicc_createv2display_gray");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "gsicc_createv2display_gray");
        return;
    }

    /* Start writing out data to buffer */
    curr_ptr = write_v2_common_data(buffer, profile_size, header, tag_list,
        num_tags, mediawhitept);
    tag_location = V2_COMMON_TAGS;

    /* Now the TRC. First collect the curve data and then write it out */
    /* Get the link between our gray profile and XYZ profile */
    link = get_link(pgs, src_profile, xyz_profile, gsPERCEPTUAL);
    if (link == NULL) {
        gs_free_object(memory, tag_list, "gsicc_createv2display_gray");
        gs_free_object(memory, buffer, "gsicc_createv2display_gray");
        return;
    }

    /* First get the max value for Y on the range */
    src = 65535;
    (link->procs.map_color)(NULL, link, &src, &(des[0]), 2);
    max = des[1];

    trc = (float*) gs_alloc_bytes(memory, TRC_V2_SIZE * sizeof(float), "gsicc_createv2display_gray");
    for (k = 0; k < TRC_V2_SIZE; k++) {
        src = (unsigned short)((double)k * (double)65535 / (double)(TRC_V2_SIZE - 1));
        (link->procs.map_color)(NULL, link, &src, &(des[0]), 2);
        trc[k] = (float)des[1] / max;
    }
    add_curve(curr_ptr, trc, TRC_V2_SIZE);
    curr_ptr += tag_list[tag_location].size;

    /* Clean up */
    gsicc_release_link(link);
    gs_free_object(memory, tag_list, "gsicc_createv2display_gray");
    gs_free_object(memory, trc, "gsicc_createv2display_gray");
    /* Save the v2 data */
    src_profile->v2_data = buffer;
    src_profile->v2_size = profile_size;

#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    save_profile(memory, buffer, "V2FromGray", profile_size);
#endif
}

static void
gsicc_create_v2displayrgb(const gs_gstate *pgs, icHeader *header, cmm_profile_t *src_profile,
        byte *mediawhitept, cmm_profile_t *xyz_profile)
{
    int num_tags = 9;
    int profile_size = HEADER_SIZE;
    gsicc_tag *tag_list;
    gs_memory_t *memory = src_profile->memory;
    int last_tag = -1;
    /* 4 for name, 4 reserved, 4 for number entries, 2*num_entries */
    int trc_tag_size = 12 + 2 * TRC_V2_SIZE;
    byte *buffer, *curr_ptr;
    float *trc;
    int tag_location;
    icS15Fixed16Number XYZ_Data[3];
    gsicc_link_t *link;
    int k;

    /* Profile description tag, copyright tag white point RGB colorants and
       RGB TRCs */
    tag_list = (gsicc_tag*)gs_alloc_bytes(memory,
        sizeof(gsicc_tag)*num_tags, "gsicc_create_v2displayrgb");
    if (tag_list == NULL)
        return;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE * num_tags;
    profile_size += 4; /* number of tags.... */

    /* Common tags + white point + RGB colorants + RGB TRCs */
    init_common_tagsv2(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigRedColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigGreenColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigBlueColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigRedTRCTag, trc_tag_size);
    init_tag(tag_list, &last_tag, icSigGreenTRCTag, trc_tag_size);
    init_tag(tag_list, &last_tag, icSigBlueTRCTag, trc_tag_size);

    /* Now get the profile size */
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    /* Allocate buffer */
    buffer = gs_alloc_bytes(memory, profile_size, "gsicc_create_v2displayrgb");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_v2displayrgb");
        return;
    }

    /* Start writing out data to buffer */
    curr_ptr = write_v2_common_data(buffer, profile_size, header, tag_list,
        num_tags, mediawhitept);
    tag_location = V2_COMMON_TAGS;

    /* Now the main colorants. Get them and the TRC data from using the link
        between the source profile and the CIEXYZ profile */
    link = get_link(pgs, src_profile, xyz_profile, gsPERCEPTUAL);
    if (link == NULL) {
        gs_free_object(memory, tag_list, "gsicc_create_v2displayrgb");
        gs_free_object(memory, buffer, "gsicc_create_v2displayrgb");
        return;
    }

    /* Get the Red, Green and Blue colorants */
    for (k = 0; k < 3; k++) {
        get_colorant(k, link, XYZ_Data);
        add_xyzdata(curr_ptr, XYZ_Data);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;
    }

    /* Now the TRCs */
    trc = (float*) gs_alloc_bytes(memory, TRC_V2_SIZE * sizeof(float), "gsicc_create_v2displayrgb");

    for (k = 0; k < 3; k++) {
        get_trc(k, link, &trc, TRC_V2_SIZE);
        add_curve(curr_ptr, trc, TRC_V2_SIZE);
        curr_ptr += tag_list[tag_location].size;
    }

    /* Clean up */
    gsicc_release_link(link);
    gs_free_object(memory, tag_list, "gsicc_create_v2displayrgb");
    gs_free_object(memory, trc, "gsicc_create_v2displayrgb");
    /* Save the v2 data */
    src_profile->v2_data = buffer;
    src_profile->v2_size = profile_size;

#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    save_profile(memory,buffer, "V2FromRGB", profile_size);
#endif
}

static void
gsicc_create_v2display(const gs_gstate *pgs, icHeader *header, cmm_profile_t *src_profile,
                    byte *mediawhitept, cmm_profile_t *xyz_profile)
{
    /* Need to create matrix with the TRCs.  Have to worry about gray
       and RGB cases. */
    if (header->colorSpace == icSigGrayData)
        gsicc_create_v2displaygray(pgs, header, src_profile, mediawhitept, xyz_profile);
    else
        gsicc_create_v2displayrgb(pgs, header, src_profile, mediawhitept, xyz_profile);
}

static int
readint32(byte *buff)
{
    int out = 0;
    byte *ptr = buff;
    int k;

    for (k = 0; k < 4; k++) {
        int temp = ptr[k];
        int shift = (3 - k) * 8;
        out += temp << shift;
    }
    return out;
}

/* Create special profile for going to/from CIEXYZ color space.  We will use
   this with lcms and the current profile to construct the structures in
   a new V2 profile */
static int
get_xyzprofile(cmm_profile_t *xyz_profile)
{
    icProfile iccprofile;
    icHeader *header = &(iccprofile.header);
    int num_tags = 9;  /* common (2) + rXYZ,gXYZ,bXYZ,rTRC,gTRC,bTRC,wtpt */
    int profile_size = HEADER_SIZE;
    gsicc_tag *tag_list;
    int last_tag = -1;
    /* 4 for name, 4 reserved, 4 for number entries. 0 entries implies linear */
    int trc_tag_size = 12;
    byte *buffer, *curr_ptr, *tempptr;
    int tag_location;
    gs_memory_t *memory = xyz_profile->memory;
    icS15Fixed16Number temp_XYZ[3];
    byte mediawhitept[12];
    icS15Fixed16Number one, zero;
    int k, j;
    int code;

    /* Fill in the common stuff */
    setheader_common(header, 2);
    /* If we have to create a table we will do it in XYZ.  If it is a matrix,
    it is still XYZ */
    header->pcs = icSigXYZData;
    header->colorSpace = icSigRgbData;
    header->deviceClass = icSigDisplayClass;

    /* Profile description tag, copyright tag white point and grayTRC */
    tag_list = (gsicc_tag*)gs_alloc_bytes(memory,
        sizeof(gsicc_tag) * num_tags, "get_xyzprofile");
    if (tag_list == NULL)
        return -1;
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE * num_tags;
    profile_size += 4; /* number of tags.... */

    /* Common tags + white point + RGB colorants + RGB TRCs */
    init_common_tagsv2(tag_list, num_tags, &last_tag);
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigRedColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigGreenColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigBlueColorantTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigRedTRCTag, trc_tag_size);
    init_tag(tag_list, &last_tag, icSigGreenTRCTag, trc_tag_size);
    init_tag(tag_list, &last_tag, icSigBlueTRCTag, trc_tag_size);

    /* Now get the profile size */
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    /* Allocate buffer */
    buffer = gs_alloc_bytes(memory, profile_size, "get_xyzprofile");
    if (buffer == NULL) {
        gs_free_object(memory, tag_list, "get_xyzprofile");
        return -1;
    }

    /* Media white point for this profile is D50 */
    get_D50(temp_XYZ); /* See Appendix D6 in spec */
    tempptr = mediawhitept;
    for (j = 0; j < 3; j++) {
        write_bigendian_4bytes(tempptr, temp_XYZ[j]);
        tempptr += 4;
    }

    /* Start writing out data to buffer */
    curr_ptr = write_v2_common_data(buffer, profile_size, header, tag_list,
        num_tags, mediawhitept);
    tag_location = V2_COMMON_TAGS;
    /* Now lets add the Red Green and Blue colorant information */
    one = double2XYZtype(1);
    zero = double2XYZtype(0);

    temp_XYZ[0] = one;
    temp_XYZ[1] = zero;
    temp_XYZ[2] = zero;
    add_xyzdata(curr_ptr, temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    temp_XYZ[0] = zero;
    temp_XYZ[1] = one;
    add_xyzdata(curr_ptr, temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    temp_XYZ[1] = zero;
    temp_XYZ[2] = one;
    add_xyzdata(curr_ptr, temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* And now the TRCs */
    add_curve(curr_ptr, NULL, 0);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    add_curve(curr_ptr, NULL, 0);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    add_curve(curr_ptr, NULL, 0);

    /* Done */
    gs_free_object(memory, tag_list, "get_xyzprofile");
    xyz_profile->buffer = buffer;
    xyz_profile->buffer_size = profile_size;
    code = gsicc_init_profile_info(xyz_profile);
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    save_profile(memory,buffer, "XYZProfile", profile_size);
#endif
    return code;
}

static bool
get_mediawp(cmm_profile_t *src_profile, byte *mediawhitept)
{
    byte *buffer = &(src_profile->buffer[128]);
    int num_tags = readint32(buffer);
    int tag_signature = -1;
    int offset;
    int k;
    int buffer_left = src_profile->buffer_size;

    if (buffer_left < 128)
        return false;
    buffer_left -= 128;

    if (buffer_left < 4)
        return false;

    buffer += 4;
    buffer_left -= 4;

    /* Get to the tag table */
    for (k = 0; k < num_tags; k++) {
        if (buffer_left < 12)
            return false;

        tag_signature = readint32(buffer);
        if (tag_signature == icSigMediaWhitePointTag)
            break;
        buffer += 12;
        buffer_left -= 12;
    }
    if (tag_signature != icSigMediaWhitePointTag)
        return false;

    if (buffer_left < 4)
        return false;

    buffer += 4;
    buffer_left -= 4;

    offset = readint32(buffer);

    if (buffer_left < offset + 8)
        return false;

    buffer = &(src_profile->buffer[offset + 8]);  /* Add offset of 8 for XYZ tag and padding */
    buffer_left = src_profile->buffer_size - (offset + 8);

    /* Data is already in the proper format. Just get the bytes */
    if (buffer_left < 12)
        return false;

    memcpy(mediawhitept, buffer, 12);
    return true;
}

static void
gsicc_create_v2(const gs_gstate *pgs, cmm_profile_t *src_profile)
{
    icProfile iccprofile;
    icHeader *header = &(iccprofile.header);
    byte mediawhitept[12];
    cmm_profile_t *xyz_profile;

    if (src_profile->v2_data != NULL)
        return;

    /* Fill in the common stuff */
    setheader_common(header, 2);

    /* Get the data_cs of current profile */
    switch (src_profile->data_cs) {
        case gsGRAY:
            header->colorSpace = icSigGrayData;
        break;
        case gsRGB:
            header->colorSpace = icSigRgbData;
            break;
        case gsCMYK:
            header->colorSpace = icSigCmykData;
            break;
        default:
#ifdef DEBUG
            gs_warn("Failed in creating V2 ICC profile");
#endif
            return;
            break;
    }

    /* Use the deviceClass from the source profile. */
    header->deviceClass = gsicc_get_device_class(src_profile);

    /* Unfortunately we have to get the media white point also. lcms wrapped
       up the method internally when it went to release 2 so we will do our
       own*/
    if (!get_mediawp(src_profile, &(mediawhitept[0]))) {
#ifdef DEBUG
        gs_warn("Failed in creating V2 ICC profile");
#endif
        return;
    }

    /* Also, we will want to create an XYZ ICC profile that we can use for
       creating our data with lcms.  If already created, this profile is
       stored in the manager */
    if (pgs->icc_manager->xyz_profile != NULL) {
        xyz_profile = pgs->icc_manager->xyz_profile;
    } else {
        xyz_profile = gsicc_profile_new(NULL, pgs->memory, NULL, 0);
        if (xyz_profile == NULL) {
#ifdef DEBUG
            gs_warn("Failed in creating V2 ICC profile");
#endif
            return;
        }
        if (get_xyzprofile(xyz_profile) != 0) {
#ifdef DEBUG
            gs_warn("Failed in creating V2 ICC profile");
#endif
            return;
        }
        pgs->icc_manager->xyz_profile = xyz_profile;
    }

    /* The type of stuff that we need to create */
    switch (header->deviceClass) {
        case icSigInputClass:
            header->pcs = icSigLabData;
            gsicc_create_v2input(pgs, header, src_profile, mediawhitept,
                pgs->icc_manager->lab_profile);
            break;
        case icSigDisplayClass:
            header->pcs = icSigXYZData;
            gsicc_create_v2display(pgs, header, src_profile, mediawhitept,
                xyz_profile);
            break;
        case icSigOutputClass:
            header->pcs = icSigLabData;
            gsicc_create_v2output(pgs, header, src_profile, mediawhitept,
                pgs->icc_manager->lab_profile);
            break;
        default:
#ifdef DEBUG
            gs_warn("Failed in creating V2 ICC profile");
#endif
            return;
            break;
    }
    return;
}

/* While someone could create something that was not valid for now we will
   just trust the version information in the header.  Allow anything with
   major version 2 */
static bool
gsicc_create_isv2(cmm_profile_t *profile)
{
    if (profile->vers == ICCVERS_UNKNOWN) {
        int majorvers = profile->buffer[8];

        if (majorvers == 2) {
            profile->vers = ICCVERS_2;
            return true;
        } else {
            profile->vers = ICCVERS_NOT2;
            return false;
        }
    }
    if (profile->vers == ICCVERS_2)
        return true;
    else
        return false;
}

byte*
gsicc_create_getv2buffer(const gs_gstate *pgs, cmm_profile_t *srcprofile,
                        int *size)
{
    if (gsicc_create_isv2(srcprofile)) {
        *size = srcprofile->buffer_size;
        return srcprofile->buffer;
    }

    if (srcprofile->profile_handle == NULL)
        srcprofile->profile_handle =
        gsicc_get_profile_handle_buffer(srcprofile->buffer,
        srcprofile->buffer_size, pgs->memory);

    /* Need to create v2 profile */
    gsicc_create_v2(pgs, srcprofile);

    *size = srcprofile->v2_size;
    return srcprofile->v2_data;
}
