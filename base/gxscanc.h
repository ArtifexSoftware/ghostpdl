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

/* $Id: gxpath.h 8022 2007-06-05 22:23:38Z giles $ */
/* Fixed-point path procedures */
/* Requires gxfixed.h */

#ifndef gxscanc_INCLUDED
#  define gxscanc_INCLUDED

#include "gxpath.h"
#include "gxdevice.h"

/* The routines and types in this interface use */
/* device, rather than user, coordinates, and fixed-point, */
/* rather than floating, representation. */

#ifndef inline
#define inline __inline
#endif /* inline */

/* Opaque type for an edgebuffer */
typedef struct gx_edgebuffer_s gx_edgebuffer;

struct gx_edgebuffer_s {
    int  base;
    int  height;
    int  xmin;
    int  xmax;
    int *index;
    int *table;
};

typedef struct {
    int (*scan_convert)(gx_device     * gs_restrict pdev,
                        gx_path       * gs_restrict path,
                  const gs_fixed_rect * gs_restrict rect,
                        gx_edgebuffer * gs_restrict edgebuffer,
                        fixed                       flatness);
    int (*filter)(gx_device       * gs_restrict pdev,
                  gx_edgebuffer   * gs_restrict edgebuffer,
                  int                           rule);
    int (*fill)(gx_device       * gs_restrict pdev,
          const gx_device_color * gs_restrict pdevc,
                gx_edgebuffer   * gs_restrict edgebuffer,
                int                           log_op);
} gx_scan_converter_t;

/* "Pixel centre" scanline routines */
int
gx_scan_convert(gx_device     * gs_restrict pdev,
                gx_path       * gs_restrict path,
          const gs_fixed_rect * gs_restrict rect,
                gx_edgebuffer * gs_restrict edgebuffer,
                fixed                       flatness);

int
gx_filter_edgebuffer(gx_device       * gs_restrict pdev,
                     gx_edgebuffer   * gs_restrict edgebuffer,
                     int                           rule);

int
gx_fill_edgebuffer(gx_device       * gs_restrict pdev,
             const gx_device_color * gs_restrict pdevc,
                   gx_edgebuffer   * gs_restrict edgebuffer,
                   int                           log_op);

/* "Any Part of a Pixel" (app) scanline routines */
int
gx_scan_convert_app(gx_device     * gs_restrict pdev,
                    gx_path       * gs_restrict path,
              const gs_fixed_rect * gs_restrict rect,
                    gx_edgebuffer * gs_restrict edgebuffer,
                    fixed                       flatness);

int
gx_filter_edgebuffer_app(gx_device       * gs_restrict pdev,
                         gx_edgebuffer   * gs_restrict edgebuffer,
                         int                           rule);

int
gx_fill_edgebuffer_app(gx_device       * gs_restrict pdev,
                 const gx_device_color * gs_restrict pdevc,
                       gx_edgebuffer   * gs_restrict edgebuffer,
                       int                           log_op);

/* "Pixel centre" trapezoid routines */
int
gx_scan_convert_tr(gx_device     * gs_restrict pdev,
                   gx_path       * gs_restrict path,
             const gs_fixed_rect * gs_restrict rect,
                   gx_edgebuffer * gs_restrict edgebuffer,
                   fixed                       flatness);

int
gx_filter_edgebuffer_tr(gx_device       * gs_restrict pdev,
                        gx_edgebuffer   * gs_restrict edgebuffer,
                        int                           rule);

int
gx_fill_edgebuffer_tr(gx_device       * gs_restrict pdev,
                const gx_device_color * gs_restrict pdevc,
                      gx_edgebuffer   * gs_restrict edgebuffer,
                      int                           log_op);

/* "Any Part of a Pixel" (app) trapezoid routines */
int
gx_scan_convert_tr_app(gx_device     * gs_restrict pdev,
                       gx_path       * gs_restrict path,
                 const gs_fixed_rect * gs_restrict rect,
                       gx_edgebuffer * gs_restrict edgebuffer,
                       fixed                       flatness);

int
gx_filter_edgebuffer_tr_app(gx_device       * gs_restrict pdev,
                            gx_edgebuffer   * gs_restrict edgebuffer,
                            int                           rule);

int
gx_fill_edgebuffer_tr_app(gx_device       * gs_restrict pdev,
                    const gx_device_color * gs_restrict pdevc,
                          gx_edgebuffer   * gs_restrict edgebuffer,
                          int                           log_op);

extern gx_scan_converter_t gx_scan_converter;
extern gx_scan_converter_t gx_scan_converter_app;
extern gx_scan_converter_t gx_scan_converter_tr;
extern gx_scan_converter_t gx_scan_converter_tr_app;

int
gx_scan_convert_and_fill(const gx_scan_converter_t *sc,
                               gx_device       *dev,
                               gx_path         *ppath,
                         const gs_fixed_rect   *ibox,
                               fixed            flat,
                               int              rule,
                         const gx_device_color *pdevc,
                               int              lop);

/* Equivalent to filling it full of 0's */
void gx_edgebuffer_init(gx_edgebuffer * edgebuffer);

void gx_edgebuffer_fin(gx_device     * pdev,
                       gx_edgebuffer * edgebuffer);


#endif /* gxscanc_INCLUDED */
