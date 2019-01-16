/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* rtmisc.h - interface to raster rendering code */

#ifndef rtmisc_INCLUDED
#define rtmisc_INCLUDED

/* Export this so we can call it from HP-GL/2 configurations. */
int rtl_enter_pcl_mode(pcl_args_t * pargs, pcl_state_t * pcs);

/* Export this so the PCL parser will call this in RTL mode when it
   does not recognize the input, so it will be interpreted as HPGL/2. */
int rtl_enter_hpgl_mode(pcl_args_t * pargs, pcl_state_t * pcs);

#endif /* rtmisc_INCLUDED */
