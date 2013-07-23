/*************************************************************************
*
* This software module was originally contributed by Microsoft
* Corporation in the course of development of the
* ITU-T T.832 | ISO/IEC 29199-2 ("JPEG XR") format standard for
* reference purposes and its performance may not have been optimized.
*
* This software module is an implementation of one or more
* tools as specified by the JPEG XR standard.
*
* ITU/ISO/IEC give You a royalty-free, worldwide, non-exclusive
* copyright license to copy, distribute, and make derivative works
* of this software module or modifications thereof for use in
* products claiming conformance to the JPEG XR standard as
* specified by ITU-T T.832 | ISO/IEC 29199-2.
*
* ITU/ISO/IEC give users the same free license to this software
* module or modifications thereof for research purposes and further
* ITU/ISO/IEC standardization.
*
* Those intending to use this software module in products are advised
* that its use may infringe existing patents. ITU/ISO/IEC have no
* liability for use of this software module or modifications thereof.
*
* Copyright is not released for products that do not conform to
* to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* Microsoft Corporation retains full right to modify and use the code
* for its own purpose, to assign or donate the code to a third party,
* and to inhibit third parties from using the code for products that
* do not conform to the JPEG XR standard as specified by ITU-T T.832 |
* ISO/IEC 29199-2.
*
* This copyright notice must be included in all copies or derivative
* works.
*
* Copyright (c) ITU-T/ISO/IEC 2008, 2009.
***********************************************************************/

#ifdef _MSC_VER
#pragma comment (user,"$Id: file.h,v 1.2 2008/02/26 23:52:44 steve Exp $")
#else
#ident "$Id: file.h,v 1.2 2008/02/26 23:52:44 steve Exp $"
#endif

void *open_input_file(const char *name, const raw_info *raw_info_t, int *alpha_mode, int *padded_format);
void *open_output_file(const char *name);
void get_file_parameters(void *handle, int *wid, int *hei, int *ncomp, int *bpi, short *sf, short *photometric, int *padBytes);
void read_file(jxr_image_t image, int mx, int my, int *data);
void write_file(jxr_image_t image, int mx, int my, int *data);
void write_file_combine_primary_alpha(jxr_image_t image, FILE *fpPrimary, FILE *fpAlpha);
void separate_primary_alpha(jxr_image_t image, void *input_handle, char *path_out, char * path_primary, char *path_alpha, jxr_container_t container);
void close_file(void *handle);
void set_ncomp(void *input_handle, int ncomp);

/*
* $Log: file.h,v $
* Revision 1.4 2009/05/29 12:00:00 microsoft
* Reference Software v1.6 updates.
*
* Revision 1.3 2009/04/13 12:00:00 microsoft
* Reference Software v1.5 updates.
*
* Revision 1.2 2008/02/26 23:52:44 steve
* Remove ident for MS compilers.
*
* Revision 1.1 2008/01/19 02:30:46 rick
* Re-implement and extend file interface.
*
*/

