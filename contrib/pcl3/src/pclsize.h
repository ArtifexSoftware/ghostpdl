/******************************************************************************
  File:     $Id: pclsize.h,v 1.7 2000/11/19 07:05:17 Martin Rel $
  Contents: Header file for maps between PCL Page Size codes and size
            information
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

#ifndef _pclsize_h	/* Inclusion protection */
#define _pclsize_h

/*****************************************************************************/

#include "mediasize.h"
#include "pclgen.h"

/* ms_MediaCode flag for distinguishing between sheets and cards */
#define PCL_CARD_FLAG	MS_USER_FLAG_1
#define PCL_CARD_STRING	"Card"

/*
    This structure is based on the assumption that one needs only a single
    Page Size code for each supported media code. See the discussion in
    pclgen.h.
*/
typedef struct {
  ms_MediaCode mc;
  pcl_PageSize ps;
} CodeEntry;

#define MAX_CODEENTRIES 64

typedef struct {
    int inited_code_map;
    CodeEntry code_map[MAX_CODEENTRIES];
    int inited_inverse_map;
    CodeEntry inverse_map[MAX_CODEENTRIES];
} pcl3_sizetable;

extern pcl_PageSize pcl3_page_size(pcl3_sizetable *table, ms_MediaCode code);
extern ms_MediaCode pcl3_media_code(pcl3_sizetable *table, pcl_PageSize code);
extern const ms_SizeDescription *pcl3_size_description(pcl3_sizetable *table, pcl_PageSize size);

/*****************************************************************************/

#endif	/* Inclusion protection */
