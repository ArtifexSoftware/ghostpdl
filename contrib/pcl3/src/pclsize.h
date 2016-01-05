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

extern pcl_PageSize pcl3_page_size(ms_MediaCode code);
extern ms_MediaCode pcl3_media_code(pcl_PageSize code);
extern const ms_SizeDescription *pcl3_size_description(pcl_PageSize size);

/*****************************************************************************/

#endif	/* Inclusion protection */
