/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/* $Id:*/

/* metelement.c */

#include "memory_.h"
#include "metelement.h"

/* generate table entries */
#define ENTRY(element)\
    &met_element_procs_ ## element

/* table of elements (not the periodic table) - these should be in
   alpha order. */
const met_element_t *met_element_table[] = {
    ENTRY(ArcSegment),
    ENTRY(Canvas),
    ENTRY(Canvas_Clip),
    ENTRY(Canvas_OpacityMask),
    ENTRY(Canvas_RenderTransform),
    ENTRY(Canvas_Resources),
    ENTRY(DocumentReference),
    ENTRY(FixedDocument),
    ENTRY(FixedDocumentSequence),
    ENTRY(FixedPage),
    ENTRY(FixedPage_RenderTransform),
    ENTRY(FixedPage_Resources),
    ENTRY(Glyphs),
    ENTRY(Glyphs_Clip),
    ENTRY(Glyphs_Fill),
    ENTRY(Glyphs_OpacityMask),
    ENTRY(Glyphs_RenderTransform),
    ENTRY(GradientStop),
    ENTRY(ImageBrush),
    ENTRY(ImageBrush_Transform),
    ENTRY(LinearGradientBrush),
    ENTRY(LinearGradientBrush_GradientStops),
    ENTRY(LinearGradientBrush_Transform),
    ENTRY(LinkTarget),
    ENTRY(MatrixTransform),
    ENTRY(PageContent),
    ENTRY(PageContent_LinkTargets),
    ENTRY(Path),
    ENTRY(Path_Clip),
    ENTRY(Path_Data),
    ENTRY(Path_Fill),
    ENTRY(Path_OpacityMask),
    ENTRY(Path_RenderTransform),
    ENTRY(Path_Stroke),
    ENTRY(PathFigure),
    ENTRY(PathGeometry),
    ENTRY(PathGeometry_Transform),
    ENTRY(PolyBezierSegment),
    ENTRY(PolyLineSegment),
    ENTRY(PolyQuadraticBezierSegment),
    ENTRY(RadialGradientBrush),
    ENTRY(RadialGradientBrush_GradientStops),
    ENTRY(RadialGradientBrush_Transform),
    ENTRY(ResourceDictionary),
    ENTRY(SolidColorBrush),
    ENTRY(VisualBrush),
    ENTRY(VisualBrush_Transform),
    ENTRY(VisualBrush_Visual),
    0
};

#undef ENTRY

met_element_procs_t *
met_get_element_definition(const char *element)
{
    int index = 0;
    /* gcc'ism */
    char elstr[strlen(element) + 1];
    char *pelstr;
    /* the schema uses a period in element names, when "cooked" these
       become underscores */
    strcpy(elstr, element);
    pelstr = strchr(elstr, '.');
    if (pelstr) 
        *pelstr = '_';

    while (true) {
        met_element_t *metdef = met_element_table[index];
        if (!metdef) 
            /* end of table, element not found */
            return NULL;
        if (!strcmp(elstr, metdef->element))
            return &metdef->procs;
        index++;
    }
}
            
