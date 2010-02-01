/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
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

          AToB0Tag   (NOTE ONE WAY! BtoA0Tag is optional. Not true for display profiles.)

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

#include "icc34.h"   /* Note this header is needed even if lcms is not compiled as default CMS */
#include "string_.h"
#include "gsmemory.h"
#include "gx.h"
#include "gxistate.h"
#include "gstypes.h"
#include "gscspace.h"
#include "gscie.h"
#include "gsicc_create.h"
#include "gxarith.h"
#include "gsiccmanage.h"
#include "gsicccache.h"
#include "math_.h"
#include "gscolor2.h"

static void
add_xyzdata(unsigned char *input_ptr, icS15Fixed16Number temp_XYZ[]);

#define SAVEICCPROFILE 0
#define USE_V4 1
#define HEADER_SIZE 128
#define TAG_SIZE 12
#define XYZPT_SIZE 12
#define DATATYPE_SIZE 8
#define CURVE_SIZE 512
#define IDENT_CURVE_SIZE 0
#define NUMBER_COMMON_TAGS 2 
#define icMultiUnicodeText 0x6d6c7563           /* 'mluc' v4 text type */
#define icMultiFunctionAtoBType 0x6d414220      /* 'mAB ' v4 lutAtoBtype type */
#define icSigChromaticAdaptationTag 0x63686164  /* 'chad' */

#if SAVEICCPROFILE
unsigned int icc_debug_index = 0;
#endif

typedef struct cielab_s {
    float lstar;
    float astar;
    float bstar;
} cielab_t;

static const char desc_name[] = "Ghostscript Internal Profile";
static const char copy_right[] = "Copyright Artifex Software 2009";

typedef struct {
    icTagSignature      sig;            /* The tag signature */
    icUInt32Number      offset;         /* Start of tag relative to 
                                         * start of header, Spec 
                                         * Clause 5 */
    icUInt32Number      size;           /* Size in bytes */
    unsigned char       byte_padding;
} gsicc_tag;

static int
get_padding(int x)
{
    return( (4 -x%4)%4 ); 
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

/* This function maps a gs matrix type to an ICC MLUT.
   This is required due to the multiple matrix and 1-D LUT
   forms for postscript management, which the ICC does not
   support (at least the older versions) */

static void 
gsicc_matrix_to_mlut(gs_matrix3 mat, icLut16Type *mlut)
{
    mlut->base.reserved[0] = 0;
    mlut->base.reserved[1] = 0;
    mlut->base.reserved[2] = 0;
    mlut->base.reserved[3] = 0;
    mlut->base.sig = icSigLut16Type;

    /* Much to do here */
}

#if SAVEICCPROFILE
/* Debug dump of internally created ICC profile for testing */
static void
save_profile(unsigned char *buffer, char filename[], int buffer_size)
{
    char full_file_name[50];
    FILE *fid;

    sprintf(full_file_name,"%d)Profile_%s.icc",icc_debug_index,filename);
    fid = fopen(full_file_name,"wb");
    fwrite(buffer,sizeof(unsigned char),buffer_size,fid);
    fclose(fid);
    icc_debug_index++;
}
#endif

static 
ulong swapbytes32(ulong input)
{
    ulong output = (((0x000000ff) & (input >> 24))
                    | ((0x0000ff00) & (input >> 8))
                    | ((0x00ff0000) & (input << 8))
                    | ((0xff000000) & (input << 24)));
    return output;
}
    
static void
write_bigendian_4bytes(unsigned char *curr_ptr,ulong input)
{
#if !arch_is_big_endian
   *curr_ptr ++= ((0x000000ff) & (input >> 24));
   *curr_ptr ++= ((0x000000ff) & (input >> 16));
   *curr_ptr ++= ((0x000000ff) & (input >> 8));
   *curr_ptr ++= ((0x000000ff) & (input));
#else   
   *curr_ptr ++= ((0x000000ff) & (input));
   *curr_ptr ++= ((0x000000ff) & (input >> 8));
   *curr_ptr ++= ((0x000000ff) & (input >> 16));
   *curr_ptr ++= ((0x000000ff) & (input >> 24));
#endif


}

static void
write_bigendian_2bytes(unsigned char *curr_ptr,ushort input)
{
#if !arch_is_big_endian
   *curr_ptr ++= ((0x000ff) & (input >> 8));
   *curr_ptr ++= ((0x000ff) & (input));
#else   
   *curr_ptr ++= ((0x000000ff) & (input));
   *curr_ptr ++= ((0x000000ff) & (input >> 8));
#endif
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
        gs_warn("Negative CIEXYZ in created ICC Profile");
    }

    s = (short) number_in;
    m = (unsigned short) ((number_in - s) * 65536.0);
    return((icS15Fixed16Number) ((s << 16) | m) );
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
        return(temp);
    } else {
        s = (short) number_in;
        m = (unsigned short) ((number_in - s) * 65536.0);
        return((icS15Fixed16Number) ((s << 16) | m) );
    }
}

static unsigned short
float2u8Fixed8(float number_in)
{
    unsigned short m;

    m = (unsigned short) (number_in * 256);
    return( m );
}

static unsigned short
lstar2_16bit(float number_in)
{
    unsigned short returnval; 
    float temp;

    temp = number_in/((float) 100.0);
    if (temp > 1) 
        temp = 1;
    if (temp < 0) 
        temp = 0;
    returnval = (unsigned short) ( (float) temp * (float) 0xff00);
    return(returnval);
}

static unsigned short
abstar2_16bit(float number_in)
{
    float temp;

    temp = number_in + ((float) 128.0);
    if (temp < 0) 
        temp = 0;
    temp = (0x8000 * temp)/ (float) 128.0;
    if (temp > 0xffff) 
        temp = 0xffff;
    return((unsigned short) temp);
}


static
void  init_common_tags(gsicc_tag tag_list[],int num_tags, int *last_tag)
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

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = icSigCopyrightTag;
    /* temp_size = DATATYPE_SIZE + strlen(copy_right) + 1; */
    temp_size = 2*strlen(copy_right) + 28;
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;
    *last_tag = curr_tag;
}

static void
add_desc_tag(unsigned char *buffer,const char text[], gsicc_tag tag_list[], int curr_tag)
{
    ulong value;
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr,icSigProfileDescriptionTag);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    value = strlen(text);
    write_bigendian_4bytes(curr_ptr,value+1); /* count includes NULL */
    curr_ptr += 4;
    memcpy(curr_ptr,text,value);
    curr_ptr += value;
    memset(curr_ptr,0,79);  /* Null + Unicode and Scriptcode */
    curr_ptr += value;
    memset(curr_ptr,0,tag_list[curr_tag].byte_padding);  /* padding */
}

static void
add_text_tag(unsigned char *buffer,const char text[], gsicc_tag tag_list[], int curr_tag)
{
    ulong value;
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    write_bigendian_4bytes(curr_ptr,icSigTextType);
    curr_ptr += 4;
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    value = strlen(text);
    memcpy(curr_ptr,text,value);
    curr_ptr += value;
    memset(curr_ptr,0,1);  /* Null */    
    curr_ptr++;
    memset(curr_ptr,0,tag_list[curr_tag].byte_padding);  /* padding */
}

/* Code to write out v4 text type which is a table of unicode text
   for different regions */
static void
add_v4_text_tag(unsigned char *buffer,const char text[], gsicc_tag tag_list[], int curr_tag)
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
}

static void
add_common_tag_data(unsigned char *buffer,gsicc_tag tag_list[])
{
#if USE_V4   
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    add_v4_text_tag(curr_ptr, desc_name, tag_list, 0);
    curr_ptr += tag_list[0].size;
    add_v4_text_tag(curr_ptr, copy_right, tag_list, 1);
#else
    unsigned char *curr_ptr;

    curr_ptr = buffer;
    add_desc_tag(curr_ptr, desc_name, tag_list, 0);
    curr_ptr += tag_list[0].size;
    add_text_tag(curr_ptr, copy_right, tag_list, 1);
#endif
}

static
void  init_tag(gsicc_tag tag_list[], int *last_tag, icTagSignature tagsig, int datasize)
{
    /* This should never be called first.  Common tags should be first taken care of */
 
    int curr_tag = (*last_tag)+1;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = tagsig;
    tag_list[curr_tag].byte_padding = get_padding(DATATYPE_SIZE + datasize);
    tag_list[curr_tag].size = DATATYPE_SIZE + datasize + tag_list[curr_tag].byte_padding;
    *last_tag = curr_tag;
}

static void
setheader_common(icHeader *header)
{
    /* This needs to all be predefined for a simple copy. MJV todo */
    header->cmmId = 0;
    header->version = 0x04200000;
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
    memset(header->reserved,0,44);  /* Version 4 includes a profile id, field which is an md5 sum */
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
    XYZ[0] = double2XYZtype(0.9642);
    XYZ[1] = double2XYZtype(1.0);
    XYZ[2] = double2XYZtype(0.8249);
}

static void
get_XYZ(icS15Fixed16Number XYZ[], gs_vector3 *vector)
{
    XYZ[0] = double2XYZtype(vector->u);
    XYZ[1] = double2XYZtype(vector->v);
    XYZ[2] = double2XYZtype(vector->w);
}

static void
get_XYZ_floatptr(icS15Fixed16Number XYZ[], float *vector)
{
    XYZ[0] = double2XYZtype(vector[0]);
    XYZ[1] = double2XYZtype(vector[1]);
    XYZ[2] = double2XYZtype(vector[2]);
}

static void
get_matrix_floatptr(icS15Fixed16Number matrix_fixed[], float *matrix)
{
    int k;

    for (k = 0; k < 9; k++) {
        matrix_fixed[k] = double2XYZtype(matrix[k]);
    }
}

static void
add_matrixdata(unsigned char *input_ptr, icS15Fixed16Number matrix_fixed[])
{
    int j;
    unsigned char *curr_ptr;

    curr_ptr = input_ptr;
    for (j = 0; j < 9; j++) {
        write_bigendian_4bytes(curr_ptr, matrix_fixed[j]);
        curr_ptr += 4;
    }
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
add_gammadata(unsigned char *input_ptr, unsigned short gamma, icTagSignature curveType)
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
merge_abc_lmn_curves(gx_cie_vector_cache *DecodeABC_caches, gx_cie_scalar_cache *DecodeLMN)
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

/* Hardcoded chad for D65 to D50. This should be computed on the fly
   based upon the PS specified white point and ICC D50. We don't use
   the chad tag with littleCMS since it takes care of the chromatic
   adaption itself based upon D50 and the media white point.  */
static void
add_chad_data(unsigned char *input_ptr)
{
    unsigned char *curr_ptr = input_ptr;
    float data[] = {1.04790738171017, 0.0229333845542104, -0.0502016347980104,
                 0.0296059594177168, 0.990456039910785, -0.01707552919587, 
                 -0.00924679432678241, 0.0150626801401488, 0.751791232609078};

    /* Signature should be sf32 */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigS15Fixed16ArrayType);
    curr_ptr += 4;
    /* Reserved */
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    add_matrixwithbias(curr_ptr,  &(data[0]), false);
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
    /* Now the data uInt16 Number 0 to 65535.  For now assume input is 0 to 1.  Need to fix this.  MJV */
    for (k = 0; k < num_samples; k++) {
        if (curve_data[k] < 0) curve_data[k] = 0;
        if (curve_data[k] > 1) curve_data[k] = 1; 
        value = curve_data[k]*0xFFFF;
        write_bigendian_2bytes(curr_ptr,value);
        curr_ptr+=2;
    }
}

/* See comments before add_lutAtoBtype about allowable forms, which will explain much
   of these size calculations */
static int
getsize_lutAtoBtype(float *a_curves, float *clut, int clut_grid_size, float *m_curves, 
               gs_matrix3 *matrix_input, float *b_curves,int numin, int numout)
{
    int data_offset, mlut_size;

    data_offset = 32; 
    /* B curves always present */
    if (b_curves != NULL) {
        data_offset += (numout*(CURVE_SIZE*2+12));
    } else {
        data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
    }
    /* M curves present if Matrix is present */
    if (matrix_input != NULL) {
        data_offset += (12*4);
        /* M curves */
        if (m_curves != NULL) {
            data_offset += (numout*(CURVE_SIZE*2+12));
        } else {
            data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
        }
    } 
    /* A curves present if clut is present */
    if (clut != NULL) {
        mlut_size = (long) pow((float) clut_grid_size, (long) numin)*numout*2;
        data_offset += mlut_size;
        if (a_curves != NULL) {
            data_offset += (numin*(CURVE_SIZE*2+12));
        } else {
            data_offset += (numin*(IDENT_CURVE_SIZE*2+12));
        }
    }
    return(data_offset);
}

/* Note:  ICC V4 fomat allows ONLY these forms 
B
M - Matrix - B
A - CLUT - B
A - CLUT - M - Matrix - B
Other forms are created by making some of these items identity.  In other words the B curves must always
be included.  If CLUT is present, A curves must be present.  Also, if Matrix is present M curves must be 
present.  A curves cannot be present if CLUT is not present. */
static void
add_lutAtoBtype(unsigned char *input_ptr, float *a_curves, float *clut, int clut_grid_size, float *m_curves, 
               gs_matrix3 *input_matrix, float *b_curves,int numin, int numout)
{
/* We need to figure out all the offsets to the various objects based upon which ones are actually present */
    unsigned char *curr_ptr;
    long mlut_size;
    int data_offset;
    int k;

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
    if (b_curves == NULL) {
        /* identity curve must be present */
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (numout*(CURVE_SIZE*2+12));
    }
    curr_ptr += 4;
    /* offset to matrix and M curves */
    if (input_matrix == NULL) {
        memset(curr_ptr,0,4);  /* Matrix */
        curr_ptr += 4;
        memset(curr_ptr,0,4);  /* M curves */
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        data_offset += (12*4);
        curr_ptr += 4;
        /* offset to M curves (Matrix curves -- only come with matrix) */
        if (m_curves == NULL) {
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
    if (clut == NULL) {
        memset(curr_ptr,0,4); /* CLUT */
        curr_ptr += 4;
        memset(curr_ptr,0,4); /* A curves */
    } else {
        write_bigendian_4bytes(curr_ptr,data_offset);
        mlut_size = (long) pow((float) clut_grid_size, (long) numin)*numout*2;
        data_offset += mlut_size;
        curr_ptr += 4;
        /* offset to A curves (first curves) */
        if (a_curves == NULL || clut == NULL) {
            /* identity curve must be present */
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numout*(IDENT_CURVE_SIZE*2+12));
        } else {
            write_bigendian_4bytes(curr_ptr,data_offset);
            data_offset += (numin*(CURVE_SIZE*2+12));
        }    
    }    
    curr_ptr += 4;
    /* Header is completed */
    /* Now write out the various parts (i.e. curves, matrix and clut)
    /* First the B curves */
    if (b_curves != NULL) {
        for (k = 0; k < numout; k++) {
            add_curve(curr_ptr, b_curves+k*CURVE_SIZE, CURVE_SIZE);
            curr_ptr += (12 + CURVE_SIZE*2);
        }
    } else {
        add_ident_curves(curr_ptr,numout);
        curr_ptr += numout*(12 + IDENT_CURVE_SIZE*2);
    }
    /* Then the matrix */
    if (input_matrix != NULL) {
        add_matrixwithbias(curr_ptr,&(input_matrix->cu.u),true);
        curr_ptr += (12*4);
        /* M curves */
        if (m_curves != NULL) {
            for (k = 0; k < numout; k++) {
                add_curve(curr_ptr, m_curves+k*CURVE_SIZE, CURVE_SIZE);
                curr_ptr += (12 + CURVE_SIZE*2);
            }
        } else {
            add_ident_curves(curr_ptr,numout);
            curr_ptr += numout*(12 + IDENT_CURVE_SIZE*2);
        }
    }
    /* Then the clut */
    if (clut != NULL) {

        /* The A curves */
        if (a_curves != NULL) {
            for (k = 0; k < numin; k++) {
                add_curve(curr_ptr, a_curves+k*CURVE_SIZE, CURVE_SIZE);
                curr_ptr += (12 + CURVE_SIZE*2);
            }
        } else {
            add_ident_curves(curr_ptr,numin);
            curr_ptr += numin*(12 + IDENT_CURVE_SIZE*2);
        }

    }
}

static void
add_clut_labdata_16bit(unsigned char *input_ptr, cielab_t *cielab, int num_colors, int num_samples)
{
    int k;
    unsigned short encoded_value;
    unsigned char *curr_ptr;
    long mlut_size;

    mlut_size = (long) pow((float) num_samples, (long) num_colors);
    curr_ptr = input_ptr;
    for ( k = 0; k < mlut_size; k++) {
        encoded_value = lstar2_16bit(cielab[k].lstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;
        encoded_value = abstar2_16bit(cielab[k].astar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;
        encoded_value = abstar2_16bit(cielab[k].bstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;
    }
}

/* Add 16bit CLUT data that is in CIELAB color space */
static void
add_tabledata(unsigned char *input_ptr, cielab_t *cielab, int num_colors, int num_samples)
{
   int gridsize, numin, numout, numinentries, numoutentries;
   unsigned char *curr_ptr;
   icS15Fixed16Number matrix_fixed[9];
   float ident[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
   int mlut_size;
   int k;

   gridsize = num_samples;  /* Sampling points in MLUT */
   numin = num_colors;      /* Number of input colorants */
   numout = 3;              /* Number of output colorants */
   numinentries = 2;        /* input 1-D LUT samples */
   numoutentries = 2;       /* output 1-D LUT samples */
   /* Signature */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigLut16Type);
    curr_ptr += 4;
    /* Reserved */
    memset(curr_ptr,0,4);
    curr_ptr += 4;
    /* Padded sizes */
    *curr_ptr++ = numin;
    *curr_ptr++ = numout;
    *curr_ptr++ = gridsize;
    *curr_ptr++ = 0;
   /* Identity Matrix */
    get_matrix_floatptr(matrix_fixed,(float*) &(ident[0]));
    add_matrixdata(curr_ptr,matrix_fixed);
    curr_ptr += 4*9;
    /* 1-D LUT sizes */
    write_bigendian_2bytes(curr_ptr, numinentries);
    curr_ptr += 2;
    write_bigendian_2bytes(curr_ptr, numoutentries);
    curr_ptr += 2;

    /* Now the input curve data */
    for (k = 0; k < num_colors; k++) {
        write_bigendian_2bytes(curr_ptr, 0);
        curr_ptr += 2;
        write_bigendian_2bytes(curr_ptr, 0xFFFF);
        curr_ptr += 2;
    }

    /* Now the CLUT data */
    add_clut_labdata_16bit(curr_ptr, cielab, num_colors, num_samples); 
    mlut_size = (int) pow((float) num_samples, (int) num_colors) * 2 * numout;
    curr_ptr += mlut_size;
    /* Now the output curve data */
    for (k = 0; k < numout; k++) {
        write_bigendian_2bytes(curr_ptr, 0);
        curr_ptr += 2;
        write_bigendian_2bytes(curr_ptr, 0xFFFF);
        curr_ptr += 2;
    }
}

/* This creates an ICC profile from the PDF calGray and calRGB definitions */
cmm_profile_t*
gsicc_create_from_cal(float *white, float *black, float *gamma, float *matrix,  gs_memory_t *memory, int num_colors)
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
    icTagSignature TRC_Tags[3] = {icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag};
    int trc_tag_size;
    unsigned char *buffer;
    cmm_profile_t *result;

    /* Fill in the common stuff */
    setheader_common(header);
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
        return(NULL);
    }
    tag_list = (gsicc_tag*) gs_alloc_bytes(memory,sizeof(gsicc_tag)*num_tags,"gsicc_create_from_cal");
    
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
    trc_tag_size = 8;  /* 4 for count, 2 for gamma, Extra 2 bytes for 4 byte alignment requirement */
    for (k = 0; k < num_colors; k++) {
        init_tag(tag_list, &last_tag, TRC_Tags[k], trc_tag_size);
    }
    for(k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    /* Now we can go ahead and fill our buffer with the data */
    buffer = gs_alloc_bytes(memory,profile_size,"gsicc_create_from_cal");
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
    add_common_tag_data(curr_ptr, tag_list);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }
    tag_location = NUMBER_COMMON_TAGS;
    /* The matrix */
    if (num_colors == 3) {
        for ( k = 0; k < 3; k++ ) {
            get_XYZ_floatptr(temp_XYZ,&(matrix[k*3]));
            add_xyzdata(curr_ptr,temp_XYZ);
            curr_ptr += tag_list[tag_location].size;
            tag_location++;
        }
    }
    /* White and black points */
    /* Need to adjust for the D65/D50 issue */
    get_XYZ_floatptr(temp_XYZ,white);
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    /* Black point */
    get_XYZ_floatptr(temp_XYZ,black);
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    /* Now the gamma values */
    for ( k = 0; k < num_colors; k++ ) {
        encode_gamma = float2u8Fixed8(gamma[k]);
        add_gammadata(curr_ptr, encode_gamma, icSigCurveType);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;
    }
    result = gsicc_profile_new(NULL, memory, NULL, 0);   
    result->buffer = buffer;
    result->buffer_size = profile_size;
    result->num_comps = num_colors;
    if (num_colors == 3) {
        result->data_cs = gsRGB;
    } else {
        result->data_cs = gsGRAY;
    } 
    /* Set the hash code  */
    gsicc_get_icc_buff_hash(buffer,&(result->hashcode));
    result->hash_is_valid = true;
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if (num_colors == 3)
        save_profile(buffer,"from_calRGB",profile_size);
    else
        save_profile(buffer,"from_calGray",profile_size);
#endif
    return(result);
}

/* A common form used for most of the PS CIE color spaces */
static void
create_lutAtoBprofile(unsigned char **pp_buffer_in, icHeader *header, float *a_curves, float *clut, int clut_grid_size, 
                      float *m_curves, gs_matrix3 *matrix_input, float *b_curves, int num_in, int num_out, 
                      gs_vector3 *white_point, gs_vector3 *black_point, gs_memory_t *memory)
{
    int num_tags = 5;  /* common (2), AToB0Tag,bkpt,and wtpt.*/
    int k;
    gsicc_tag *tag_list;
    int profile_size, last_tag, tag_location, tag_size;
    unsigned char *buffer,*curr_ptr;
    icS15Fixed16Number temp_XYZ[3];

    profile_size = HEADER_SIZE;
    tag_list = (gsicc_tag*) gs_alloc_bytes(memory,sizeof(gsicc_tag)*num_tags,"create_lutAtoBprofile");
    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE*num_tags;
    profile_size += 4; /* number of tags.... */
    last_tag = -1;
    init_common_tags(tag_list, num_tags, &last_tag);  
    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);
    /* init_tag(tag_list, &last_tag, icSigChromaticAdaptationTag, 9*4);  */  /* chad tag */
    /* Get the tag size of the A2B0 with the lutAtoBType */
    tag_size = getsize_lutAtoBtype(NULL, NULL, 0, m_curves, matrix_input, b_curves,num_in,num_out);
    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);
    /* Add all the tag sizes to get the new profile size */
    for(k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }
    /* End of tag table information */
    /* Now we can go ahead and fill our buffer with the data */
    buffer = gs_alloc_bytes(memory,profile_size,"create_lutAtoBprofile");
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
    add_common_tag_data(curr_ptr, tag_list);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }
    tag_location = NUMBER_COMMON_TAGS;
     /* get_D50(temp_XYZ); */
    get_XYZ(temp_XYZ,white_point);
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
    get_XYZ(temp_XYZ,black_point);
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;
 /* add_chad_data(curr_ptr);
    curr_ptr += tag_list[tag_location].size;
    tag_location++; */  
    /* Now the AToB0Tag Data. Here this will include the M curves, the matrix and the B curves.
       We may need to do some adustements with respect to encode and decode.  For now
       assume all is between 0 and 1. */
    add_lutAtoBtype(curr_ptr, a_curves, clut, clut_grid_size, m_curves, matrix_input, b_curves, num_in, num_out);
    *pp_buffer_in = buffer;
    gs_free_object(memory, tag_list, "gsicc_create_fromabc");

}

/* The ABC color space is modeled using the V4 lutAtoBType which has the flexibility to model 
   the various parameters.  Simplified versions are used it possible when certain parameters
   in the ABC color space definition are the identity. */
int
gsicc_create_fromabc(gs_cie_abc *pcie, unsigned char **pp_buffer_in, int *profile_size_out, gs_memory_t *memory, 
                     bool has_abc_procs, bool has_lmn_procs)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int debug_catch = 1;
    gs_matrix3 *matrix_input;
    gs_matrix3 combined_matrix, matrix_input_trans;
    float *a_curves, *m_curves, *b_curves, *curr_pos, *clut;
    int clut_grid_size;

    gsicc_matrix_init(&(pcie->common.MatrixLMN));  /* Need this set now */
    gsicc_matrix_init(&(pcie->MatrixABC));          /* Need this set now */
    /* Fill in the common stuff */
    setheader_common(header);

    /* We will use an input type class which keeps us from having to
       create an inverse.  We will keep the data a generic 3 color.  
       Since we are doing PS color management the PCS is XYZ */
    header->deviceClass = icSigDisplayClass;
    header->colorSpace = icSigRgbData;
    header->deviceClass = icSigInputClass;
    header->pcs = icSigXYZData;

    /* Check what combination we have with respect to the various
       LMN and ABC parameters. Depending upon the situation we
       may be able to use a standard 3 channel input profile type. If we
       do not have the LMN decode we can mash together the ABC and LMN
       matrix. Also, if ABC is identity we can mash the ABC and LMN
       decode procs.  If we have an ABC matrix, LMN procs and an LMN 
       matrix we will need to create a small (2x2x2) CLUT for the ICC format. */
    if (pcie->MatrixABC.is_identity || !has_lmn_procs || pcie->common.MatrixLMN.is_identity) {
        /* Determine the matrix that we will be using */
        /* AToB0Tag here is implemented as lutAtoBType (V4) with no CLUT or A curves. */     
        if (!(pcie->common.MatrixLMN.is_identity) && !(pcie->MatrixABC.is_identity)){
            /* Use the product of the ABC and LMN matrices, since lmn_procs identity.
               Product must be LMN_Matrix*ABC_Matrix */
            cie_matrix_mult3(&(pcie->common.MatrixLMN), &(pcie->MatrixABC), &(combined_matrix));
            matrix_input = &(combined_matrix);
        } else {
            if (pcie->MatrixABC.is_identity) {
                matrix_input = &(pcie->common.MatrixLMN);
            } else {
                matrix_input = &(pcie->MatrixABC);
            }
        }
        cie_matrix_transpose3(matrix_input, &matrix_input_trans);
        matrix_input = &matrix_input_trans;
        /* Now the AToB0Tag. Here this may include the M curves, the Matrix and the B curves */
        /* First clean up and merge the curves */
        if (has_abc_procs && has_lmn_procs && pcie->MatrixABC.is_identity) {
            /* Merge the curves into the abc curves.  Set LMN curves to identity  */
            merge_abc_lmn_curves(&(pcie->caches.DecodeABC.caches[0]),&(pcie->common.caches.DecodeLMN[0]));
            has_lmn_procs = false;
        }
        if (has_abc_procs) {
            m_curves = (float*) gs_alloc_bytes(memory,3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
            curr_pos = m_curves;
            memcpy(curr_pos,&(pcie->caches.DecodeABC.caches->floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[1]).floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[2]).floats.values[0]),CURVE_SIZE*sizeof(float));
        } else {
            m_curves = NULL;
        }
        if (has_lmn_procs) {
            b_curves = (float*) gs_alloc_bytes(memory,3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
            curr_pos = b_curves;
            memcpy(curr_pos,&(pcie->common.caches.DecodeLMN->floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[1]).floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[2]).floats.values[0]),CURVE_SIZE*sizeof(float));
        } else {
            b_curves = NULL;
        }
        a_curves = NULL;
        clut = NULL;
        clut_grid_size = 0;
        /* Note that if the b_curves are null and we have a matrix we need to scale the matrix values by 2.
           Otherwise an input value of 50% gray, which is 32767 would get mapped to 32767 by the matrix.  This
           will be interpreted as a max XYZ value (s15.16) when it is eventually mapped to u16.16 due to the
           mapping of X=Y by the identity table.  If there are b_curves these have an output that is 16 bit. */
        if (b_curves == NULL && matrix_input != NULL) {
            scale_matrix(&(matrix_input->cu.u),2.0);
        }
        /* Create the profile.  This is for the common generic form we will use for almost everything. */
        create_lutAtoBprofile(pp_buffer_in, header,a_curves, clut, clut_grid_size, m_curves, 
               matrix_input, b_curves, 3, 3, &(pcie->common.points.WhitePoint), 
               &(pcie->common.points.BlackPoint), memory);
        *profile_size_out = header->size;
        gs_free_object(memory, m_curves, "gsicc_create_fromabc");
        gs_free_object(memory, b_curves, "gsicc_create_fromabc");
    } else {
        /* This will be a bit more complex as we have an ABC matrix, LMN decode
           and an LMN matrix.  We will need to create an MLUT to handle this properly.
           Any ABC decode will be handled as the A curves.  ABC matrix will be the
           MLUT, LMN decode will be the M curves.  LMN matrix will be the Matrix
           and b curves will be identity. */
        if (has_abc_procs) {
            a_curves = (float*) gs_alloc_bytes(memory,3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
            curr_pos = a_curves;
            memcpy(curr_pos,&(pcie->caches.DecodeABC.caches->floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[1]).floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->caches.DecodeABC.caches[2]).floats.values[0]),CURVE_SIZE*sizeof(float));
        } else {
            a_curves = NULL;
        }
        if (has_lmn_procs) {
            m_curves = (float*) gs_alloc_bytes(memory,3*CURVE_SIZE*sizeof(float),"gsicc_create_fromabc");
            curr_pos = m_curves;
            memcpy(curr_pos,&(pcie->common.caches.DecodeLMN->floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[1]).floats.values[0]),CURVE_SIZE*sizeof(float));
            curr_pos += CURVE_SIZE;
            memcpy(curr_pos,&((pcie->common.caches.DecodeLMN[2]).floats.values[0]),CURVE_SIZE*sizeof(float));
        } else {
            m_curves = NULL;
        }
        b_curves = NULL;  /* No b_curves.  They will be identity */

        return gs_rethrow(-1, "Conversion of CIEABC color space type to ICC form not yet implemented");
    }
    *profile_size_out = header->size;
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if(debug_catch)
        save_profile(buffer,"fromabc",header->size);
#endif
    return(0);
}

void
gsicc_create_fromdef(gs_cie_def *pcie, unsigned char *buffer, gs_memory_t *memory,
                     bool has_def_procs, bool has_abc_procs, bool has_lmn_procs)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);
}

void
gsicc_create_fromdefg(gs_cie_defg *pcie, unsigned char *buffer, gs_memory_t *memory,
                       bool has_defg_procs, bool has_abc_procs, bool has_lmn_procs)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);
}

/* CIEA PS profiles can result in a surprisingly complex ICC profile.
   The Decode A and Matrix A (which
   is diagonal) are optional.  The Decode LMN and MatrixLMN are
   also optional.  If the Matrix LMN is present, this will map the
   gray value to CIEXYZ.  In any of those are present, we will need
   to do an MLUT.  If all are absent, we end up doing a linear
   mapping between the black point and the white point.  A very simple
   gray input profile with a linear TRC curve */
void
gsicc_create_froma(gs_cie_a *pcie, unsigned char *buffer, gs_memory_t *memory, 
                   bool has_a_proc, bool has_lmn_procs)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int profile_size,k;
    int num_tags;
    gsicc_tag *tag_list;
    unsigned char *curr_ptr;
    int last_tag;
    icS15Fixed16Number temp_XYZ[3];
    int tag_location;
    int trc_tag_size;
    int debug_catch = 1;

    gsicc_matrix_init(&(pcie->common.MatrixLMN));  /* Need this set now */
    /* Fill in the common stuff */
    setheader_common(header);
    /* We will use an input type class 
       which keeps us from having to
       create an inverse. */
    header->deviceClass = icSigInputClass;
    header->colorSpace = icSigGrayData;
    header->pcs = icSigXYZData;  /* If MLUT do we want CIELAB? */
    profile_size = HEADER_SIZE;

    /* Check if we have no LMN or A methods.  A simple profile.  The ICC format is 
       a bit limited in its options for monochrome color. */
    if(pcie->MatrixA.u == 1.0 && pcie->MatrixA.v == 1.0 && pcie->MatrixA.w == 1.0
        && pcie->common.MatrixLMN.is_identity && !has_lmn_procs) {
        num_tags = 5;  /* common (2) + grayTRC,bkpt,wtpt */     
        tag_list = (gsicc_tag*) gs_alloc_bytes(memory,sizeof(gsicc_tag)*num_tags,"gsicc_create_froma");
        /* Let us precompute the sizes of everything and all our offsets */
        profile_size += TAG_SIZE*num_tags;
        profile_size += 4; /* number of tags.... */
        last_tag = -1;
        init_common_tags(tag_list, num_tags, &last_tag);  
        init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);

        if (!has_a_proc) {
            trc_tag_size = 4;
        } else {
            trc_tag_size = CURVE_SIZE*2+4;  /* curv type */
            /* This will be populated during by the interpolator */
            debug_catch = 0;
        }
        init_tag(tag_list, &last_tag, icSigGrayTRCTag, trc_tag_size);
        for(k = 0; k < num_tags; k++) {
            profile_size += tag_list[k].size;
        }
        /* Now we can go ahead and fill our buffer with the data */
        buffer = gs_alloc_bytes(memory,profile_size,"gsicc_create_froma");
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
        add_common_tag_data(curr_ptr, tag_list);
        for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
            curr_ptr += tag_list[k].size;
        }
        tag_location = NUMBER_COMMON_TAGS;
        /* Need to adjust for the D65/D50 issue */
        get_XYZ(temp_XYZ,&(pcie->common.points.WhitePoint));
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;
        get_XYZ(temp_XYZ,&(pcie->common.points.BlackPoint));
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        write_bigendian_4bytes(curr_ptr,icSigCurveType);
        curr_ptr+=4;
        memset(curr_ptr,0,8); /* reserved + number points */
        curr_ptr+=8;
    } else {
        /* We will need to create a small 2x2 MLUT */
        debug_catch = 0;
    }
#if SAVEICCPROFILE
    /* Dump the buffer to a file for testing if its a valid ICC profile */
    if (debug_catch)
        save_profile(buffer,"froma",profile_size);
#endif
}

void
gsicc_create_fromcrd(unsigned char *buffer, gs_memory_t *memory)
{
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);
}