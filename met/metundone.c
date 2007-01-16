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

/*$Id: */

/* metro undone elments.

   Remove an UNDONE_STUB from the list.

   Generate a template - use the template with instructions below or
   make your own.

*/

#include "metelement.h"

#define UNDONE_STUB(element)\
    const met_element_t met_element_procs_ ## element = {\
        #element, { NULL, NULL, NULL } }

//UNDONE_STUB(Canvas_Clip);
UNDONE_STUB(Canvas_OpacityMask);
//UNDONE_STUB(Canvas_RenderTransform);
UNDONE_STUB(Canvas_Resources);
UNDONE_STUB(DocumentReference);
UNDONE_STUB(FixedDocument);
UNDONE_STUB(FixedDocumentSequence);
UNDONE_STUB(FixedPage_RenderTransform);
UNDONE_STUB(FixedPage_Resources);
UNDONE_STUB(Glyphs_Clip);
UNDONE_STUB(Glyphs_OpacityMask);
UNDONE_STUB(Glyphs_RenderTransform);
// UNDONE_STUB(GradientStop);
UNDONE_STUB(ImageBrush_Transform);
// UNDONE_STUB(LinearGradientBrush);
UNDONE_STUB(LinearGradientBrush_GradientStops);
UNDONE_STUB(LinearGradientBrush_Transform);
UNDONE_STUB(LinkTarget);
//UNDONE_STUB(MatrixTransform);
UNDONE_STUB(PageContent);
UNDONE_STUB(PageContent_LinkTargets);
UNDONE_STUB(Path_Clip);
//UNDONE_STUB(Path_Data);
UNDONE_STUB(Path_OpacityMask);
UNDONE_STUB(Path_RenderTransform);
UNDONE_STUB(PathGeometry_Transform);
// UNDONE_STUB(RadialGradientBrush);
UNDONE_STUB(RadialGradientBrush_GradientStops);
UNDONE_STUB(RadialGradientBrush_Transform);
UNDONE_STUB(ResourceDictionary);
UNDONE_STUB(VisualBrush_Transform);
UNDONE_STUB(VisualBrush_Visual);

#undef UNDONE_STUB

#if 0

/* the following is a template for an element implemenation.  Copy the
   template and then replace the word ELEMENT (case matters) with the
   name of the element - i.e. FixedPage, Path, etc.  This fills in the
   boilerplate of the template

   -------------  BEGIN TEMPLATE --------------- */

/* element constructor */
private int
ELEMENT_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_ELEMENT *aELEMENT = 
        (CT_ELEMENT *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_ELEMENT),
                                       "ELEMENT_cook");
    int i;

    memset(aELEMENT, 0, sizeof(CT_ELEMENT));

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {

    }

    /* copy back the data for the parser. */
    *ppdata = aELEMENT;
    return 0;
}

/* action associated with this element */
private int
ELEMENT_action(void *data, met_state_t *ms)
{
    CT_ELEMENT *aELEMENT = data;
    return 0;
}

private int
ELEMENT_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "ELEMENT_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_ELEMENT = {
    "ELEMENT",
    {
        ELEMENT_cook,
        ELEMENT_action,
        ELEMENT_done
    }
};

#endif /* 0 */
