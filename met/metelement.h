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

/* metelement.h */

#ifndef metelement_INCLUDED
#  define metelement_INCLUDED

#include "metstate.h"

/* implementation procedures for commands */

typedef struct met_element_procs_s {
    /* allocate memory and "cook" the associated C struct */
    int (*init)(void **data, met_state_t *ms, const char *el, const char **attr);
    /* actual parsing - cook the c struct */
    int (*action)(void *data, met_state_t *ms);
    /* end element action */
    int (*done)(void *data, met_state_t *ms);
} met_element_procs_t;

typedef struct met_element_s {
    char element[40];
    met_element_procs_t procs;
} met_element_t;

met_element_procs_t *met_get_element_definition(const char *element);

/* generate externs */
#define EXTERNS(element)\
    extern  const met_element_t met_element_procs_ ## element

EXTERNS(ArcSegment);
EXTERNS(Canvas);
EXTERNS(Canvas_Clip);
EXTERNS(Canvas_OpacityMask);
EXTERNS(Canvas_RenderTransform);
EXTERNS(Canvas_Resources);
EXTERNS(DocumentReference);
EXTERNS(FixedDocument);
EXTERNS(FixedDocumentSequence);
EXTERNS(FixedPage);
EXTERNS(FixedPage_RenderTransform);
EXTERNS(FixedPage_Resources);
EXTERNS(Glyphs);
EXTERNS(Glyphs_Clip);
EXTERNS(Glyphs_Fill);
EXTERNS(Glyphs_OpacityMask);
EXTERNS(Glyphs_RenderTransform);
EXTERNS(GradientStop);
EXTERNS(ImageBrush);
EXTERNS(ImageBrush_Transform);
EXTERNS(LinearGradientBrush);
EXTERNS(LinearGradientBrush_GradientStops);
EXTERNS(LinearGradientBrush_Transform);
EXTERNS(LinkTarget);
EXTERNS(MatrixTransform);
EXTERNS(PageContent);
EXTERNS(PageContent_LinkTargets);
EXTERNS(Path);
EXTERNS(Path_Clip);
EXTERNS(Path_Data);
EXTERNS(Path_Fill);
EXTERNS(Path_OpacityMask);
EXTERNS(Path_RenderTransform);
EXTERNS(Path_Stroke);
EXTERNS(PathFigure);
EXTERNS(PathGeometry);
EXTERNS(PathGeometry_Transform);
EXTERNS(PolyBezierSegment);
EXTERNS(PolyLineSegment);
EXTERNS(PolyQuadraticBezierSegment);
EXTERNS(RadialGradientBrush);
EXTERNS(RadialGradientBrush_GradientStops);
EXTERNS(RadialGradientBrush_Transform);
EXTERNS(ResourceDictionary);
EXTERNS(SolidColorBrush);
EXTERNS(VisualBrush);
EXTERNS(VisualBrush_Transform);
EXTERNS(VisualBrush_Visual);
#undef EXTERNS

#endif /* metelement_INCLUDED */
