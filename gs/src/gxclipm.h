/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/*Id: gxclipm.h */
/* Mask clipping device */
/* Requires gsstruct.h, gxdevice.h, gxdevmem.h */
#include "gxmclip.h"

extern_st(st_device_mask_clip);
#define public_st_device_mask_clip()	/* in gxclipm.c */\
  gx_public_st_device_mask_clip(st_device_mask_clip, "gx_device_mask_clip")

extern const gx_device_mask_clip gs_mask_clip_device;
