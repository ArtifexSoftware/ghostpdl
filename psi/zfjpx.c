/* Copyright (C) 2001-2023 Artifex Software, Inc.
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



/* This is the ps interpreter interface to the JPXDecode filter
   used for (JPEG2000) scanned image compression. PDF only specifies
   a decoder filter, and we don't currently implement anything else. */

#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gstypes.h"
#include "ialloc.h"
#include "idict.h"
#include "store.h"
#include "stream.h"
#include "strimpl.h"
#include "ifilter.h"
#include "iname.h"
#include "gdebug.h"

#include "igstate.h"  /* For igs macro */
#include "gxdevcli.h" /* for dev_spec_op */
#include "gxdevsop.h" /* For spec_op enumerated types */

#if defined(USE_OPENJPEG_JP2)
#  include "sjpx_openjpeg.h"
#else
#  include "sjpx.h"
#endif

/* macro to test a name ref against a C string */
# define ISTRCMP(ref, string) (memcmp((ref)->value.const_bytes, string, \
        min(strlen(string), r_size(ref))))

static int PS_JPXD_PassThrough(void *d, byte *Buffer, int Size)
{
    gx_device *dev = (gx_device *)d;

    if (Buffer == NULL) {
        if (Size == 0)
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_end, NULL, 0);
        else
            dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_begin, NULL, 0);
    } else {
        dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_data, Buffer, Size);
    }
    return 0;
}

/* <source> /JPXDecode <file> */
/* <source> <dict> /JPXDecode <file> */
static int
z_jpx_decode(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;
    ref *sop = NULL;
    ref *csname = NULL;
    stream_jpxd_state state;
    gx_device *dev = gs_currentdevice(igs);

    /* it's our responsibility to call set_defaults() */
    state.memory = imemory->non_gc_memory;
    if (s_jpxd_template.set_defaults)
      (*s_jpxd_template.set_defaults)((stream_state *)&state);
    if (r_has_type(op, t_dictionary)) {
        check_dict_read(*op);
        if ( dict_find_string(op, "Alpha", &sop) > 0) {
            check_type(*sop, t_boolean);
            if (sop->value.boolval)
                state.alpha = true;
        }
        if ( dict_find_string(op, "ColorSpace", &sop) > 0) {
            /* parse the value */
            if (r_is_array(sop)) {
                /* assume it's the first array element */
                csname =  sop->value.refs;
            } else if (r_has_type(sop,t_name)) {
                /* use the name directly */
                csname = sop;
            } else {
                dmprintf(imemory, "warning: JPX ColorSpace value is an unhandled type!\n");
            }
            if (csname != NULL) {
                ref sref;
                /* get a reference to the name's string value */
                if (r_has_type(csname, t_name))
                    name_string_ref(imemory, csname, &sref);
                else if (r_has_type(csname, t_string))
                    sref = *csname;
                else
                    return_error(gs_error_typecheck);

                /* request raw index values if the colorspace is /Indexed */
                if (!ISTRCMP(&sref, "Indexed"))
                    state.colorspace = gs_jpx_cs_indexed;
                /* tell the filter what output we want for other spaces */
                else if (!ISTRCMP(&sref, "DeviceGray"))
                    state.colorspace = gs_jpx_cs_gray;
                else if (!ISTRCMP(&sref, "DeviceRGB"))
                    state.colorspace = gs_jpx_cs_rgb;
                else if (!ISTRCMP(&sref, "DeviceCMYK"))
                    state.colorspace = gs_jpx_cs_cmyk;
                else if (!ISTRCMP(&sref, "ICCBased")) {
                    /* The second array element should be the profile's
                       stream dict */
                    ref *csdict = sop->value.refs + 1;
                    ref *nref;
                    ref altname;
                    if (r_is_array(sop) && (r_size(sop) > 1) &&
                      r_has_type(csdict, t_dictionary)) {
                        check_dict_read(*csdict);
                        /* try to look up the alternate space */
                        if (dict_find_string(csdict, "Alternate", &nref) > 0) {
                          name_string_ref(imemory, csname, &altname);
                          if (!ISTRCMP(&altname, "DeviceGray"))
                            state.colorspace = gs_jpx_cs_gray;
                          else if (!ISTRCMP(&altname, "DeviceRGB"))
                            state.colorspace = gs_jpx_cs_rgb;
                          else if (!ISTRCMP(&altname, "DeviceCMYK"))
                            state.colorspace = gs_jpx_cs_cmyk;
                        }
                        /* else guess based on the number of components */
                        if (state.colorspace == gs_jpx_cs_unset &&
                                dict_find_string(csdict, "N", &nref) > 0) {
                          if_debug1m('w', imemory, "[w] JPX image has an external %"PRIpsint
                                     " channel colorspace\n", nref->value.intval);
                          if (r_type(nref) != t_integer)
                              return gs_note_error(gs_error_typecheck);
                          switch (nref->value.intval) {
                            case 1: state.colorspace = gs_jpx_cs_gray;
                                break;
                            case 3: state.colorspace = gs_jpx_cs_rgb;
                                break;
                            case 4: state.colorspace = gs_jpx_cs_cmyk;
                                break;
                          }
                        }
                    }
                }
            } else {
                if_debug0m('w', imemory, "[w] Couldn't read JPX ColorSpace key!\n");
            }
        }
    }

    if (dev_proc(dev, dev_spec_op)(dev, gxdso_JPX_passthrough_query, NULL, 0) > 0) {
        state.StartedPassThrough = 0;
        state.PassThrough = 1;
        state.PassThroughfn = (PS_JPXD_PassThrough);
        state.device = (void *)dev;
    }
    else {
        state.PassThrough = 0;
        state.device = (void *)NULL;
    }

    /* we pass npop=0, since we've no arguments left to consume */
    /* we pass 0 instead of the usual rspace(sop) which will allocate storage
       for filter state from the same memory pool as the stream it's coding.
       this causes no trouble because we maintain no pointers */
    return filter_read(i_ctx_p, 0, &s_jpxd_template,
                       (stream_state *) & state, 0);
}

/* Match the above routine to the corresponding filter name.
   This is how our static routines get called externally. */
const op_def zfjpx_op_defs[] = {
    op_def_begin_filter(),
    {"2JPXDecode", z_jpx_decode},
    op_def_end(0)
};
