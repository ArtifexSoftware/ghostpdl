/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$ */

/* metro page related elements */

#include "memory_.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "gspaint.h"
#include "gserror.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metutil.h"
#include <stdlib.h> /* nb for atof */

private int
FixedPage_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_FixedPage *aFixedPage = 
        (CT_FixedPage *)gs_alloc_bytes(ms->memory,
                                       sizeof(CT_FixedPage),
                                       "FixedPage_cook");
    int i;
    memset(aFixedPage, 0, sizeof(CT_FixedPage));
    for(i = 0; attr[i]; i += 2) {
        if (!strcmp(attr[i], "Width"))
            aFixedPage->Width = atof(attr[i+1]);
        else if (!strcmp(attr[i], "Height"))
            aFixedPage->Height = atof(attr[i+1]); /* nb wrong - assumes the datatype */
        else if (!MYSET(&aFixedPage->ContentBox, "ContentBox"))
            ;
        else if (!MYSET(&aFixedPage->BleedBox, "BleedBox"))
            ;
        else {
            gs_warn2("unsupported attribute %s=%s",
                     attr[i], attr[i+1]);
        }
    }
    *ppdata = aFixedPage;
    return 0; /* incomplete */
}

private int
FixedPage_action(void *data, met_state_t *ms)
{

    CT_FixedPage *aFixedPage = data;
    
    gs_state *pgs = ms->pgs;
    gx_device *dev = gs_currentdevice(pgs);
    gs_memory_t *mem = ms->memory;
    gs_param_float_array fa;
    float fv[2];
    gs_c_param_list list;
    int code;

    fa.data = fv;
    fa.persistent = false;
    
    /* nb review handling of error codes for parameter writing */
    gs_c_param_list_write(&list, mem);
    fv[0] = aFixedPage->Width / 96.0 * 72.0;
    fv[1] = aFixedPage->Height / 96.0 * 72.0;
    fa.size = 2;
    code = param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
    gs_c_param_list_write(&list, mem);
    if ( code >= 0 ) { 
        gs_c_param_list_read(&list);
        code = gs_putdeviceparams(dev, (gs_param_list *)&list);
    }
    gs_c_param_list_release(&list);
    
    /* nb this is for the demo it is wrong and should be removed */
    gs_initgraphics(pgs);

    gs_initmatrix(pgs);
    /* 96 dpi default - and put the origin at the top of the page */
    code = gs_scale(pgs, 72.0/96.0, -72.0/96.0);
    if (code < 0)
        return code;
    code = gs_translate(pgs, 0.0, -aFixedPage->Height);
    if (code < 0)
        return code;
    code = gs_erasepage(pgs);
    return code;
}

private int
FixedPage_done(void *data, met_state_t *ms)
{
    CT_FixedPage *aFixedPage = data;
    (ms->end_page)(ms, 1 /* Copies */, true /* flush */);

    gs_free_object(ms->memory, aFixedPage->ContentBox, "FixedPage_done");
    gs_free_object(ms->memory, aFixedPage->BleedBox, "FixedPage_done");
    gs_free_object(ms->memory, aFixedPage, "FixedPage_done");
    return 0; 
}


const met_element_t met_element_procs_FixedPage = {
    "FixedPage",
    {
        FixedPage_cook,
        FixedPage_action,
        FixedPage_done
    }
};
