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

#include "icc34.h"
#include "string_.h"
#include "gsmemory.h"
#include "gx.h"
#include "gstypes.h"
#include "gscspace.h"
#include "gscie.h"

#define HEADER_SIZE 128
#define TAG_SIZE 12
#define XYZPT_SIZE 12
#define DATATYPE_SIZE 8
#define CURVE_SIZE 512
#define NUMBER_COMMON_TAGS 3 


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

static
void  init_common_tags(icTag tag_list[],int num_tags, int *last_tag)
{


 /*    profileDescriptionTag
       copyrightTag
       mediaWhitePointTag  */

    int curr_tag;

    if (*last_tag < 0)
    {
        curr_tag = 0;
    
    } else {

        curr_tag = (*last_tag)+1;

    }
 
    tag_list[curr_tag].offset = HEADER_SIZE+num_tags*TAG_SIZE;
    tag_list[curr_tag].sig = icSigProfileDescriptionTag;
    tag_list[curr_tag].size = DATATYPE_SIZE + strlen("GhostScript Profile");
    curr_tag++;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = icSigCopyrightTag;
    tag_list[curr_tag].size = DATATYPE_SIZE + strlen("Copyright Artifex Software 2009");
    curr_tag++;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = icSigMediaWhitePointTag;
    tag_list[curr_tag].size = DATATYPE_SIZE + XYZPT_SIZE;

    *last_tag = curr_tag;

}

static
void  init_tag(icTag tag_list[], int *last_tag, icTagSignature tagsig, int datasize)
{

    /* This should never be called first.  Common tags should be first taken care of */
 
    int curr_tag = (*last_tag)+1;

    tag_list[curr_tag].offset = tag_list[curr_tag-1].offset + tag_list[curr_tag-1].size;
    tag_list[curr_tag].sig = tagsig;
    tag_list[curr_tag].size = DATATYPE_SIZE + datasize;
    *last_tag = curr_tag;

}



static void
setheader_common(icHeader *header)
{
    /* This needs to all be predefined for a simple copy. MJV todo */
    header->cmmId = 0;
    header->version = 0x03400000;  /* Back during a simplier time.... */
    setdatetime(&(header->date));
    header->magic = 0x61637379;
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

    



}


static void
copy_tagtable(unsigned char *buffer,icTag *tag_list, int num_tags)
{





}

static void
get_XYZ(icS15Fixed16Number XYZ[], gs_vector3 vector)
{

    XYZ[0] = double2XYZtype(vector.u);
    XYZ[1] = double2XYZtype(vector.v);
    XYZ[2] = double2XYZtype(vector.w);

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



/* The ABC matrix and decode ABC parameters
   are optional in the PS spec.  If they
   are not present, we will use a standard
   TRC and Matrix to CIEXYZ ICC profile to 
   model the LMN decode and MatrixLMN, which 
   ARE required parameters.  If the ABC
   matrix and decode ABC parameters ARE present,
   we will need to use a small MLUT with 1-D 
   LUTs to model the ABC portion since
   the ICC profile cannot have multiple matrices */

void
gsicc_create_fromabc(gs_cie_abc *pcie, unsigned char *buffer, gs_memory_t *memory)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);
    int profile_size,k;
    int num_tags;
    icTag *tag_list;
    int tag_offset = 0;
    unsigned char *curr_ptr;
    int last_tag;
    icS15Fixed16Number temp_XYZ[3];
    int tag_location;

    /* Fill in the common stuff */

    setheader_common(header);

    /* We will use an input type class 
       which keeps us from having to
       create an inverse.  We will keep
       the data a generic 3 color.  
       Since we are doing PS color management
       the PCS is XYZ */

    header->deviceClass = icSigInputClass;
    header->colorSpace = icSig3colorData;
    header->pcs = icSigXYZData;

    /* We really will want to consider adding a detection for 
       a PS definition of CIELAB.  This could 
       be achieved by looking at the
       /RangeABC and then running a couple 
       points through to verify that they
       get mapped to CIEXYZ.  */

    profile_size = HEADER_SIZE;

    /* Check if we only have LMN methods */

    if(pcie->MatrixABC.is_identity /* && pcie->DecodeABC == DecodeABC_default */){

        /* Only LMN parameters, we can do this with a very simple ICC profile */

        num_tags = 11;  /* common + rXYZ,gXYZ,bXYZ,rTRC,gTRC,bTRC,wtpt,bkpt */     
        tag_list = (icTag*) gs_alloc_bytes(memory,sizeof(icTag)*num_tags,"gsicc_create_fromabc");

        /* Let us precompute the sizes of everything and all our offsets */

        profile_size += TAG_SIZE*num_tags;

        last_tag = -1;
        init_common_tags(tag_list, num_tags, &last_tag);  

        init_tag(tag_list, &last_tag, icSigRedColorantTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigGreenColorantTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigBlueColorantTag, XYZPT_SIZE);

        init_tag(tag_list, &last_tag, icSigMediaWhitePointTag, XYZPT_SIZE);
        init_tag(tag_list, &last_tag, icSigMediaBlackPointTag, XYZPT_SIZE);

        init_tag(tag_list, &last_tag, icSigRedTRCTag, CURVE_SIZE*2+4);
        init_tag(tag_list, &last_tag, icSigGreenTRCTag, CURVE_SIZE*2+4);
        init_tag(tag_list, &last_tag, icSigBlueTRCTag, CURVE_SIZE*2+4);

        for(k = 0; k < num_tags; k++){

            profile_size += tag_list[k].size;

        }

        /* Now we can go ahead and fill our buffer with the data */

        buffer = gs_alloc_bytes(memory,profile_size,"gsicc_create_fromabc");
        curr_ptr = buffer;

        /* The header */

        header->size = profile_size;
        copy_header(curr_ptr,header);
        curr_ptr += HEADER_SIZE;

        /* Tag table */

        copy_tagtable(curr_ptr,tag_list,num_tags);
        curr_ptr += TAG_SIZE*num_tags;

        /* Now the data.  Must be in same order as we created the tag table */

        tag_location = NUMBER_COMMON_TAGS-1;

        get_XYZ(temp_XYZ,pcie->common.MatrixLMN.cu);
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;

        get_XYZ(temp_XYZ,pcie->common.MatrixLMN.cv);
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;

        get_XYZ(temp_XYZ,pcie->common.MatrixLMN.cw);
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;

        get_XYZ(temp_XYZ,pcie->common.points.WhitePoint);
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;

        get_XYZ(temp_XYZ,pcie->common.points.BlackPoint);
        add_xyzdata(curr_ptr,temp_XYZ);
        curr_ptr += tag_list[tag_location].size;
        tag_location++;

        /* Now we need to create the curves from the PS procedures */


  


    } else {

        /* This will be a bit more complex.  The ABC stuff is going
           to be put into a 2x2 MLUT with TRCs */




    }


}



void
gsicc_create_fromdef(gs_cie_def *pcie, unsigned char *buffer, gs_memory_t *memory)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);




}





void
gsicc_create_fromdefg(gs_cie_defg *pcie, unsigned char *buffer, gs_memory_t *memory)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);



}


void
gsicc_create_froma(gs_cie_a *pcie, unsigned char *buffer, gs_memory_t *memory)
{

    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);
 



}


void
gsicc_create_fromcrd(unsigned char *buffer, gs_memory_t *memory)
{

 
    icProfile iccprofile;
    icHeader  *header = &(iccprofile.header);

    setheader_common(header);



}