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
#define EXTERN(element)\
    extern  const met_element_t met_element_procs_ ## element

EXTERN(ArcSegment);
EXTERN(Canvas);
EXTERN(Canvas_Clip);
EXTERN(Canvas_OpacityMask);
EXTERN(Canvas_RenderTransform);
EXTERN(Canvas_Resources);
EXTERN(DocumentReference);
EXTERN(FixedDocument);
EXTERN(FixedDocumentSequence);
EXTERN(FixedPage);
EXTERN(FixedPage_RenderTransform);
EXTERN(FixedPage_Resources);
EXTERN(Glyphs);
EXTERN(Glyphs_Clip);
EXTERN(Glyphs_Fill);
EXTERN(Glyphs_OpacityMask);
EXTERN(Glyphs_RenderTransform);
EXTERN(GradientStop);
EXTERN(ImageBrush);
EXTERN(ImageBrush_Transform);
EXTERN(LinearGradientBrush);
EXTERN(LinearGradientBrush_GradientStops);
EXTERN(LinearGradientBrush_Transform);
EXTERN(LinkTarget);
EXTERN(MatrixTransform);
EXTERN(PageContent);
EXTERN(PageContent_LinkTargets);
EXTERN(Path);
EXTERN(Path_Clip);
EXTERN(Path_Data);
EXTERN(Path_Fill);
EXTERN(Path_OpacityMask);
EXTERN(Path_RenderTransform);
EXTERN(Path_Stroke);
EXTERN(PathFigure);
EXTERN(PathGeometry);
EXTERN(PathGeometry_Transform);
EXTERN(PolyBezierSegment);
EXTERN(PolyLineSegment);
EXTERN(PolyQuadraticBezierSegment);
EXTERN(RadialGradientBrush);
EXTERN(RadialGradientBrush_GradientStops);
EXTERN(RadialGradientBrush_Transform);
EXTERN(ResourceDictionary);
EXTERN(SolidColorBrush);
EXTERN(VisualBrush);
EXTERN(VisualBrush_Transform);
EXTERN(VisualBrush_Visual);
#undef EXTERNS

#endif /* metelement_INCLUDED */
