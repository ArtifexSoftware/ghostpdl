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

#include "stdafx.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif



#include "icc34.h"   /* Note this header is needed even if lcms is not compiled as default CMS */
#include <string.h>
#include "CIELAB.h"
#include <math.h>
#include "icc_create.h"

typedef unsigned long ulong;
typedef unsigned short ushort;


#define ROUND( a )      ( ( (a) < 0 ) ? (int) ( (a) - 0.5 ) : \
                                                  (int) ( (a) + 0.5 ) )

static void
add_xyzdata(unsigned char *input_ptr, icS15Fixed16Number temp_XYZ[]);

#define SAVEICCPROFILE 1
#define HEADER_SIZE 128
#define TAG_SIZE 12
#define XYZPT_SIZE 12
#define DATATYPE_SIZE 8
#define CURVE_SIZE 512
#define NUMBER_COMMON_TAGS 2 
#define icMultiUnicodeText 0x6d6c7563    /* 'mluc' v4 text type */

#define icSigColorantTableTag (icTagSignature) 0x636c7274L  /* 'clrt' */
#define icSigColorantTableType (icTagSignature) 0x636c7274L /* clrt */

static const float D50WhitePoint[] = {(float)0.9642, 1, (float)0.8249};
static const float BlackPoint[] = {0, 0, 0};


static const char desc_name[] = "Artifex DeviceN Profile";
static const char desc_name_link[] = "Artifex DeviceLink Profile";
static const char copy_right[] = "Copyright Artifex Software 2010";

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


/* Debug dump of internally created ICC profile for testing */

static void
save_profile(unsigned char *buffer, char filename[], int buffer_size)

{
    FILE *fid;

    fid = fopen(filename,"wb");

    fwrite(buffer,sizeof(unsigned char),buffer_size,fid);

    fclose(fid);

}


static 
ulong swapbytes32(ulong input)
{
    ulong output = ((0x000000ff) & (input >> 24)
                    | (0x0000ff00) & (input >> 8)
                    | (0x00ff0000) & (input << 8)
                    | (0xff000000) & (input << 24));

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
double2XYZtype(float number_in){

    short s;
    unsigned short m;

    s = (short) number_in;
    m = (unsigned short) ((number_in - s) * 65536.0);
    return((icS15Fixed16Number) ((s << 16) | m) );
}

static unsigned short
lstar2_16bit(float number_in){

    unsigned short returnval; 
    float temp;

    temp = number_in/((float) 100.0);

    if (temp > 1) temp = 1;
    if (temp < 0) temp = 0;

    returnval = (unsigned short) ( (float) temp * (float) 0xff00);

    return(returnval);
}

static unsigned short
abstar2_16bit(float number_in){

    float temp;

    temp = number_in + ((float) 128.0);

    if (temp < 0) temp = 0;

    temp = (0x8000 * temp)/ (float) 128.0;

    if (temp > 0xffff) temp = 0xffff;

    return((unsigned short) temp);
}

static unsigned short
float2u8Fixed8(float number_in){

    unsigned short m;

    m = (unsigned short) (number_in * 256);
    return( m );
}


static
void  init_common_tags(gsicc_tag tag_list[],int num_tags, int *last_tag,
                       int is_link)
{

 /*    profileDescriptionTag
       copyrightTag
       mediaWhitePointTag  */

    int curr_tag, temp_size;

    if (*last_tag < 0) {
        curr_tag = 0;
    } else {
        curr_tag = (*last_tag)+1;
    }
 
    tag_list[curr_tag].offset = HEADER_SIZE+num_tags*TAG_SIZE + 4;
    tag_list[curr_tag].sig = icSigProfileDescriptionTag;
    /* temp_size = DATATYPE_SIZE + 4 + strlen(desc_name) + 1 + 4 + 4 + 3 + 67; */
    if (is_link) {
        temp_size = 2*strlen(desc_name) + 28;
    } else {
        temp_size = 2*strlen(desc_name) + 28;
    }
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
add_common_tag_data(unsigned char *buffer,gsicc_tag tag_list[], int is_link)
{

    unsigned char *curr_ptr;

    curr_ptr = buffer;

    if (is_link) {
        add_v4_text_tag(curr_ptr, desc_name_link, tag_list, 0);
    } else {
        add_v4_text_tag(curr_ptr, desc_name, tag_list, 0);
    }
    curr_ptr += tag_list[0].size;

    add_v4_text_tag(curr_ptr, copy_right, tag_list, 1);

    
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
    /* header->version = 0x03400000;   Back during a simplier time.... */
    header->version = 0x04200000; 
    setdatetime(&(header->date));
    header->magic = icMagicNumber;
    header->platform = icSigMacintosh;
    header->flags = 0;
    header->manufacturer = 0;
    header->model = 0;
    header->attributes[0] = 0;
    header->attributes[1] = 0;
    header->renderingIntent = 0;
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

    for (k = 0; k < num_tags; k++){

        write_bigendian_4bytes(curr_ptr,tag_list[k].sig);
        curr_ptr += 4;

        write_bigendian_4bytes(curr_ptr,tag_list[k].offset);
        curr_ptr += 4;

        write_bigendian_4bytes(curr_ptr,tag_list[k].size);
        curr_ptr += 4;


    }

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

    for (j = 0; j < 9; j++){

        write_bigendian_4bytes(curr_ptr, matrix_fixed[j]);
        curr_ptr += 4;

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

    for (j = 0; j < 3; j++){

        write_bigendian_4bytes(curr_ptr, temp_XYZ[j]);
        curr_ptr += 4;

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

    for ( k = 0; k < mlut_size; k++){
        
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

static void
add_colorantdata(unsigned char *input_ptr, colornames_t *colorant_names, int num_colors,  cielab_t *cielab, int num_samples)
{
    unsigned char *curr_ptr;
    int k, name_len;
    int offset1, offset2, offset;
    unsigned short encoded_value;

    curr_ptr = input_ptr;

    /* Signature */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigColorantTableType);
    curr_ptr += 4;

    /* Reserved */
    memset(curr_ptr,0,4);
    curr_ptr += 4;

    /* Number of colors */
    write_bigendian_4bytes(curr_ptr,num_colors);
    curr_ptr += 4;

    /* Now write out each name and the encoded 16bit CIELAB value */

    offset1 = 1;
    offset2 = num_samples;

    for ( k = 0; k < num_colors; k++){

        name_len = colorant_names[k].length;
        
        if (name_len > 31){

            memcpy(curr_ptr, &(colorant_names[k].name[0]), 31);

        } else {

            memcpy(curr_ptr, &(colorant_names[k].name[0]), name_len);

        }

        *(curr_ptr+name_len) = 0x0;
        curr_ptr += 32;

        /* Now the CIELAB data */

        offset = offset2 - offset1;
        offset2 = offset2 * num_samples;
        offset1 = offset1 * num_samples;

        encoded_value = lstar2_16bit(cielab[offset].lstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

        encoded_value = abstar2_16bit(cielab[offset].astar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

        encoded_value = abstar2_16bit(cielab[offset].bstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

    }


}




static void
add_namesdata(unsigned char *input_ptr, colornames_t *colorant_names, int num_colors,  cielab_t *cielab, int num_samples)
{

    unsigned char *curr_ptr;
    int k, name_len;
    int offset1, offset2, offset;
    unsigned short encoded_value;

    curr_ptr = input_ptr;

    /* Signature */
    curr_ptr = input_ptr;
    write_bigendian_4bytes(curr_ptr,icSigNamedColor2Type);
    curr_ptr += 4;

    /* Reserved */
    memset(curr_ptr,0,8);
    curr_ptr += 8;

    /* Number of colors */
    write_bigendian_4bytes(curr_ptr,num_colors);
    curr_ptr += 4;

    /* No device values */
    write_bigendian_4bytes(curr_ptr,0);
    curr_ptr += 4;

    /* No Name Prefix */
    memset(curr_ptr,0,1);
    curr_ptr += 32;

    /* No Name Suffix */
    memset(curr_ptr,0,1);
    curr_ptr += 32;

    /* Now write out each name and the encoded 16bit CIELAB value */

    offset1 = 1;
    offset2 = num_samples;

    for ( k = 0; k < num_colors; k++){

        name_len = colorant_names[k].length;
        
        if (name_len > 31){

            memcpy(curr_ptr, &(colorant_names[k].name[0]), 31);

        } else {

            memcpy(curr_ptr, &(colorant_names[k].name[0]), name_len);

        }

        *(curr_ptr+name_len) = 0x0;
        curr_ptr += 32;

        /* Now the CIELAB data */

        offset = offset2 - offset1;
        offset2 = offset2 * num_samples;
        offset1 = offset1 * num_samples;

        encoded_value = lstar2_16bit(cielab[offset].lstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

        encoded_value = abstar2_16bit(cielab[offset].astar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

        encoded_value = abstar2_16bit(cielab[offset].bstar);
        write_bigendian_2bytes( curr_ptr, encoded_value);
        curr_ptr += 2;

    }


}

static void
add_tabledata(unsigned char *input_ptr, void *table_data, int num_colors, 
              int num_samples, int is_link, int numout)
{

   int gridsize, numin, numinentries, numoutentries;
   unsigned char *curr_ptr;
   icS15Fixed16Number matrix_fixed[9];
   float ident[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
   int mlut_size;
   int k;
   int clut_size, num_entries;
   unsigned short *curr_clut_ptr;


   gridsize = num_samples;  /* Sampling points in MLUT */
   numin = num_colors;      /* Number of input colorants */
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

    if (is_link) {
        clut_size = (int) pow((float) num_samples, (int) num_colors);
        num_entries = clut_size*numout;
        curr_clut_ptr = (unsigned short*) table_data;
        for ( k = 0; k < num_entries; k++) {
            write_bigendian_2bytes(curr_ptr, *curr_clut_ptr);
            curr_ptr += 2;
            curr_clut_ptr++;
        }
    } else {
        add_clut_labdata_16bit(curr_ptr, (cielab_t*) table_data, num_colors, num_samples);
        mlut_size = (int) pow((float) num_samples, (int) num_colors) * 2 * numout;
        curr_ptr += mlut_size;
    }

    /* Now the output curve data */
 
    for (k = 0; k < numout; k++) {

        write_bigendian_2bytes(curr_ptr, 0);
        curr_ptr += 2;

        write_bigendian_2bytes(curr_ptr, 0xFFFF);
        curr_ptr += 2;

    }



}

/*
 * The CMYK to RGB algorithms specified by Adobe are, e.g.,
 *      R = 1.0 - min(1.0, C + K)
 *      C = max(0.0, min(1.0, 1 - R - UCR))
 * We got better results on displays with
 *      R = (1.0 - C) * (1.0 - K)
 *      C = max(0.0, min(1.0, 1 - R / (1 - UCR)))
 * For PLRM compatibility, we use the Adobe algorithms by default,
 * but what Adobe says and what they do are two different things.
 * Testing on CPSI shows that they use the 'better' algorithm.
 */


float
color_rgb_to_gray(float r, float g, float b)
{
    return (r * 0.3 + g * 0.59 + b * 0.11);
}

/* Convert CMYK to Gray. */
float
color_cmyk_to_gray(float c, float m, float y, float k)
{
    float not_gray = color_rgb_to_gray(c, m, y);

    return (not_gray > 1.0 - k ? 0 : 1.0 - (not_gray + k));
}

#if 0
/* Convert RGB to CMYK. */
/* Note that this involves black generation and undercolor removal. */
void
color_rgb_to_cmyk(frac r, frac g, frac b, const gs_imager_state * pis,
		  frac cmyk[4], gs_memory_t *mem)
{
    frac c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
    frac k = (c < m ? min(c, y) : min(m, y));

    /*
     * The default UCR and BG functions are pretty arbitrary,
     * but they must agree with the ones in gs_init.ps.
     */
    frac bg =
	(pis == NULL ? k : pis->black_generation == NULL ? frac_0 :
	 gx_map_color_frac(pis, k, black_generation));
    signed_frac ucr =
	(pis == NULL ? k : pis->undercolor_removal == NULL ? frac_0 :
	 gx_map_color_frac(pis, k, undercolor_removal));

    if (ucr == frac_1)
	cmyk[0] = cmyk[1] = cmyk[2] = 0;
    else if (ucr == frac_0)
	cmyk[0] = c, cmyk[1] = m, cmyk[2] = y;
    else {
	if (!gs_currentcpsimode(mem)) {
	    /* C = max(0.0, min(1.0, 1 - R - UCR)), etc. */
	    signed_frac not_ucr = (ucr < 0 ? frac_1 + ucr : frac_1);

	    cmyk[0] = (c < ucr ? frac_0 : c > not_ucr ? frac_1 : c - ucr);
	    cmyk[1] = (m < ucr ? frac_0 : m > not_ucr ? frac_1 : m - ucr);
	    cmyk[2] = (y < ucr ? frac_0 : y > not_ucr ? frac_1 : y - ucr);
	} else {
	    /* Adobe CPSI method */
	    /* C = max(0.0, min(1.0, 1 - R / (1 - UCR))), etc. */
	    float denom = frac2float(frac_1 - ucr);		/* unscaled */
	    float v;

	    v = (float)frac_1 - r / denom;	/* unscaled */
	    cmyk[0] =
		(is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
	    v = (float)frac_1 - g / denom;	/* unscaled */
	    cmyk[1] =
		(is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
	    v = (float)frac_1 - b / denom;	/* unscaled */
	    cmyk[2] =
		(is_fneg(v) ? frac_0 : v >= (float)frac_1 ? frac_1 : (frac) v);
	}
    }
    cmyk[3] = bg;
    if_debug7('c', "[c]RGB 0x%x,0x%x,0x%x -> CMYK 0x%x,0x%x,0x%x,0x%x\n",
	      r, g, b, cmyk[0], cmyk[1], cmyk[2], cmyk[3]);
}


/* Convert CMYK to RGB. */
void
color_cmyk_to_rgb(frac c, frac m, frac y, frac k, const gs_imager_state * pis,
		  frac rgb[3], gs_memory_t *mem)
{
    switch (k) {
	case frac_0:
	    rgb[0] = frac_1 - c;
	    rgb[1] = frac_1 - m;
	    rgb[2] = frac_1 - y;
	    break;
	case frac_1:
	    rgb[0] = rgb[1] = rgb[2] = frac_0;
	    break;
	default:
	    if (!gs_currentcpsimode(mem)) {
		/* R = 1.0 - min(1.0, C + K), etc. */
		frac not_k = frac_1 - k;

		rgb[0] = (c > not_k ? frac_0 : not_k - c);
		rgb[1] = (m > not_k ? frac_0 : not_k - m);
		rgb[2] = (y > not_k ? frac_0 : not_k - y);
	    } else {
		/* R = (1.0 - C) * (1.0 - K), etc. */
		ulong not_k = frac_1 - k;

		/* Compute not_k * (frac_1 - v) / frac_1 efficiently. */
		ulong prod;

#define deduct_black(v)\
  (prod = (frac_1 - (v)) * not_k, frac_1_quo(prod))
		rgb[0] = deduct_black(c);
		rgb[1] = deduct_black(m);
		rgb[2] = deduct_black(y);
#undef deduct_black
	    }
    }
    if_debug7('c', "[c]CMYK 0x%x,0x%x,0x%x,0x%x -> RGB 0x%x,0x%x,0x%x\n",
	      c, m, y, k, rgb[0], rgb[1], rgb[2]);
}

#endif

static void
create_cmyk2gray(unsigned short *table_data, int mlut_size, int num_samples)
{

    int c,m,y,k;
    float cyan,magenta,yellow,black,gray;
    unsigned short *buffptr = table_data;

    
    /* Step through the CMYK values, compute the gray and place in the table */
    for (c = 0; c < num_samples; c++) {
        cyan = (float) c / (float) (num_samples -1);
        for (m = 0; m < num_samples; m++) {
            magenta = (float) m / (float) (num_samples -1);
            for (y = 0; y < num_samples; y++) {
                yellow = (float) y / (float) (num_samples -1);
                for (k = 0; k < num_samples; k++) {
                    black = (float) k / (float) (num_samples -1);

                    gray =  color_cmyk_to_gray(cyan, magenta, yellow, black);
                    if (gray < 0) gray = 0;
                    if (gray > 1) gray = 1;

                    *buffptr ++= ROUND(gray * 65535);

                }
            }
        }
    }

}

static void
create_cmyk2rgb(unsigned short *table_data, int mlut_size, int num_samples)
{




}

static void
create_rgb2gray(unsigned short *table_data, int mlut_size, int num_samples)
{




}

static void
create_rgb2cmyk(unsigned short *table_data, int mlut_size, int num_samples)
{




}

static void
create_gray2cmyk(unsigned short *table_data, int mlut_size, int num_samples)
{




}

static void
create_gray2rgb(unsigned short *table_data, int mlut_size, int num_samples)
{




}

int
create_devicelink_profile(TCHAR FileName[],link_t link_type)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int profile_size,k;
    int num_tags;
    gsicc_tag *tag_list;
    int tag_offset = 0;
    unsigned char *curr_ptr;
    int last_tag;
    int tag_location;
    int debug_catch = 1;
    unsigned char *buffer;
    int numinentries = 2;        /* input 1-D LUT samples */
    int numoutentries = 2;       /* output 1-D LUT samples */
    int mlut_size;
    int tag_size;
    int num_colors, numout;
    int num_samples = 9;  /* Should be a settable variable */
    unsigned short *table_data;

    /* Fill in the common stuff */

    setheader_common(header);
    profile_size = HEADER_SIZE;
    header->deviceClass = icSigLinkClass;

    switch(link_type) {
        case CMYK2GRAY:
            header->colorSpace = icSigCmykData;
            header->pcs = icSigGrayData;
            num_colors = 4;
            numout = 1;
            break;
        case CMYK2RGB:
            header->colorSpace = icSigCmykData;
            header->pcs = icSigRgbData;
            num_colors = 4;
            numout = 3;
            break;
        case GRAY2RGB:
            header->colorSpace = icSigGrayData;
            header->pcs = icSigRgbData;
            num_colors = 1;
            numout = 3;
            break;
        case GRAY2CMYK:
            header->colorSpace = icSigGrayData;
            header->pcs = icSigCmykData;
            num_colors = 1;
            numout = 4;
            break;
        case RGB2GRAY:
            header->colorSpace = icSigRgbData;
            header->pcs = icSigGrayData;
            num_colors = 3;
            numout = 1;
            break;
        case RGB2CMYK:
            header->colorSpace = icSigRgbData;
            header->pcs = icSigCmykData;
            num_colors = 3;
            numout = 4;
            break;
    }
    num_tags = 3;  /* common (2) + ATOB0 */   
    tag_list = (gsicc_tag*) malloc(sizeof(gsicc_tag)*num_tags);

    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE*num_tags;
    profile_size += 4; /* number of tags.... */

    last_tag = -1;
    init_common_tags(tag_list, num_tags, &last_tag, 1);  

    /* Now the ATOB0 Tag */
    mlut_size = (int) pow((float) num_samples, (int) num_colors);
    tag_size = 52+num_colors*numinentries*2+numout*numoutentries*2+mlut_size*numout*2;
    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);
    for (k = 0; k < num_tags; k++) {
        profile_size += tag_list[k].size;
    }

    table_data = (unsigned short*) malloc(mlut_size*sizeof(unsigned short)*numout);
    /* Create the table */
    switch(link_type) {
        case CMYK2GRAY:
            create_cmyk2gray(table_data, mlut_size, num_samples); 
            break;
        case CMYK2RGB:
            create_cmyk2rgb(table_data, mlut_size, num_samples); 
            break;
        case GRAY2RGB:
            create_gray2rgb(table_data, mlut_size, num_samples); 
            break;
        case GRAY2CMYK:
            create_gray2cmyk(table_data, mlut_size, num_samples); 
            break;
        case RGB2GRAY:
            create_rgb2gray(table_data, mlut_size, num_samples); 
            break;
        case RGB2CMYK:
            create_rgb2cmyk(table_data, mlut_size, num_samples); 
            break;
    }

    /* Now we can go ahead and fill our buffer with the data */
    buffer = (unsigned char*) malloc(profile_size);
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
    add_common_tag_data(curr_ptr, tag_list, 1);
    for (k = 0; k< NUMBER_COMMON_TAGS; k++) {
        curr_ptr += tag_list[k].size;
    }
    tag_location = NUMBER_COMMON_TAGS;

    /* Now the ATOB0 */
    add_tabledata(curr_ptr, (void *) table_data, num_colors, num_samples, 1, numout);

    /* Save the profile */
    save_profile(buffer,FileName,profile_size);
    return(0);
}

/* This creates an NCLR input (source) ICC profile with the Artifex Private Color Names Tag */

int
create_devicen_profile(cielab_t *cielab, colornames_t *colorant_names, int num_colors, int num_samples, TCHAR FileName[])
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int profile_size,k;
    int num_tags;
    gsicc_tag *tag_list;
    int tag_offset = 0;
    unsigned char *curr_ptr;
    int last_tag;
    icS15Fixed16Number temp_XYZ[3];
    int tag_location;
    int debug_catch = 1;
    unsigned char *buffer;
    int numout = 3;              /* Number of output colorants */
    int numinentries = 2;        /* input 1-D LUT samples */
    int numoutentries = 2;       /* output 1-D LUT samples */
    int mlut_size;
    int tag_size;

    /* Fill in the common stuff */

    setheader_common(header);
    header->pcs = icSigLabData;
    profile_size = HEADER_SIZE;
    header->deviceClass = icSigInputClass;

    switch(num_colors) {
        case 2:
            header->colorSpace = icSig2colorData;
            break;
        case 3:
            header->colorSpace = icSig3colorData;
            break;
        case 4:
            header->colorSpace = icSig4colorData;
            break;
        case 5:
            header->colorSpace = icSig5colorData;
            break;
        case 6:
            header->colorSpace = icSig6colorData;
            break;
        case 7:
            header->colorSpace = icSig7colorData;
            break;
        case 8:
            header->colorSpace = icSig8colorData;
            break;
        case 9:
            header->colorSpace = icSig9colorData;
            break;
        case 10:
            header->colorSpace = icSig10colorData;
            break;
        case 11:
            header->colorSpace = icSig11colorData;
            break;
        case 12:
            header->colorSpace = icSig12colorData;
            break;
        case 13:
            header->colorSpace = icSig13colorData;
            break;
        case 14:
            header->colorSpace = icSig14colorData;
            break;
        case 15:
            header->colorSpace = icSig15colorData;
            break;
    }
    num_tags = 6;  /* common (2) + ATOB0,bkpt,wtpt + color names */   
    tag_list = (gsicc_tag*) malloc(sizeof(gsicc_tag)*num_tags);

    /* Let us precompute the sizes of everything and all our offsets */
    profile_size += TAG_SIZE*num_tags;
    profile_size += 4; /* number of tags.... */

    last_tag = -1;
    init_common_tags(tag_list, num_tags, &last_tag, 0);  

    init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
    init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);

    /* Now add the names Tag */

    /* tag_size = 84  + (32 + 6) * num_colors;
    init_tag(tag_list, &last_tag, icSigNamedColor2Tag, tag_size); */

    /* Lets not use the names tag but the newer clrt tag  */

    tag_size = 12  + (32 + 6) * num_colors;
    init_tag(tag_list, &last_tag, icSigColorantTableTag, tag_size); 

    /* Now the ATOB0 Tag */

    mlut_size = (int) pow((float) num_samples, (int) num_colors);
    
    tag_size = 52+num_colors*numinentries*2+numout*numoutentries*2+mlut_size*numout*2;

    init_tag(tag_list, &last_tag, icSigAToB0Tag, tag_size);

    for(k = 0; k < num_tags; k++){

        profile_size += tag_list[k].size;

    }

    /* Now we can go ahead and fill our buffer with the data */

    buffer = (unsigned char*) malloc(profile_size);
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

    add_common_tag_data(curr_ptr, tag_list, 0);

    for (k = 0; k< NUMBER_COMMON_TAGS; k++)
    {
        curr_ptr += tag_list[k].size;
    }

    tag_location = NUMBER_COMMON_TAGS;

    /* White and black points */
    get_XYZ_floatptr(temp_XYZ,(float*) &(D50WhitePoint[0]));
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    get_XYZ_floatptr(temp_XYZ,(float*) &(BlackPoint[0]));
    add_xyzdata(curr_ptr,temp_XYZ);
    curr_ptr += tag_list[tag_location].size;
    tag_location++;

    /* Now add the names tag data */

    /* add_namesdata(curr_ptr, colorant_names, num_colors, cielab, num_samples);
    curr_ptr += tag_list[tag_location].size;
    tag_location++; */

    /* Instead let use the newer clrt tag */

    add_colorantdata(curr_ptr, colorant_names, num_colors, cielab, num_samples);
    curr_ptr += tag_list[tag_location].size;
    tag_location++; 

    /* Now the ATOB0 */

    add_tabledata(curr_ptr, (void*) cielab, num_colors, num_samples, 0, 3);


    /* Dump the buffer to a file for testing if its a valid ICC profile */
    
    save_profile(buffer,FileName,profile_size);

    return(0);

}
