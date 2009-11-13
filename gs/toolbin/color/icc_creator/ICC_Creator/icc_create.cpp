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

typedef unsigned long ulong;
typedef unsigned short ushort;

static void
add_xyzdata(unsigned char *input_ptr, icS15Fixed16Number temp_XYZ[]);

#define SAVEICCPROFILE 1
#define HEADER_SIZE 128
#define TAG_SIZE 12
#define XYZPT_SIZE 12
#define DATATYPE_SIZE 8
#define CURVE_SIZE 512
#define NUMBER_COMMON_TAGS 2 

#define icSigColorantTableTag (icTagSignature) 0x636c7274L  /* 'clrt' */
#define icSigColorantTableType (icTagSignature) 0x636c7274L /* clrt */

static const float D50WhitePoint[] = {(float)0.9642, 1, (float)0.8249};
static const float BlackPoint[] = {0, 0, 0};


static const char desc_name[] = "Artifex DeviceN Profile";
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
void  init_common_tags(gsicc_tag tag_list[],int num_tags, int *last_tag)
{


 /*    profileDescriptionTag
       copyrightTag
       mediaWhitePointTag  */

    int curr_tag, temp_size;

    if (*last_tag < 0)
    {
        curr_tag = 0;
    
    } else {

        curr_tag = (*last_tag)+1;

    }
 
    tag_list[curr_tag].offset = HEADER_SIZE+num_tags*TAG_SIZE + 4;
    tag_list[curr_tag].sig = icSigProfileDescriptionTag;
    temp_size = DATATYPE_SIZE + 4 + strlen(desc_name) + 1 + 4 + 4 + 3 + 67;
    /* +1 for NULL + 4 + 4 for unicode + 3 + 67 script code */
    tag_list[curr_tag].byte_padding = get_padding(temp_size);
    tag_list[curr_tag].size = temp_size + tag_list[curr_tag].byte_padding;

    curr_tag++;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = icSigCopyrightTag;
    temp_size = DATATYPE_SIZE + strlen(copy_right) + 1;
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

static void
add_common_tag_data(unsigned char *buffer,gsicc_tag tag_list[])
{

    unsigned char *curr_ptr;

    curr_ptr = buffer;

    add_desc_tag(curr_ptr, desc_name, tag_list, 0);
    curr_ptr += tag_list[0].size;

    add_text_tag(curr_ptr, copy_right, tag_list, 1);

    
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
    header->version = 0x03400000;  /* Back during a simplier time.... */
   /* header->version = 0x04200000;  */
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
    init_common_tags(tag_list, num_tags, &last_tag);  

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

    add_common_tag_data(curr_ptr, tag_list);

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

    add_tabledata(curr_ptr, cielab, num_colors, num_samples);


    /* Dump the buffer to a file for testing if its a valid ICC profile */
    
    save_profile(buffer,FileName,profile_size);

    return(0);

}
