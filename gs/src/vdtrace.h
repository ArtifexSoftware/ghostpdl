/* Copyright (C) 2002 artofcode LLC. All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */
/* Visual tracer service interface */

#ifndef vdtrace_INCLUDED
#  define vdtrace_INCLUDED

/*  Painting contract :
    
    First use vd_get_dc.
    Then paint with vd_* functionns.
    When completed, use vd_release_dc.

    The following functions paint immediately, without vd_beg_path, vd_end_path, vd_fill, vd_stroke :
    vd_circle, vd_round, vd_bar, vd_bar_w, vd_curve, vd_curve_w

    The following functions require vd_fill or vd_stroke only if enclosed with vd_beg_path, vd_end_path :
    vd_moveto, vd_lineto, vd_curveto, vd_closepath.
    Otherwise they paint directly (this is useful for step-by-step execution).
*/

#if !defined(VD_TRACE)
#define VD_TRACE 1
#endif

typedef struct vd_trace_host_s vd_trace_host;
typedef struct vd_trace_interface_s vd_trace_interface;
struct gs_fixed_point_s;

struct  vd_trace_interface_s {
    vd_trace_host *host;
    double scale_x, scale_y;
    double orig_x, orig_y;
    double shift_x, shift_y;
    double (*get_size_x)(P1(vd_trace_interface *I));
    double (*get_size_y)(P1(vd_trace_interface *I));
    void (*get_dc)(P2(vd_trace_interface *I, vd_trace_interface **I1));
    void (*release_dc)(P2(vd_trace_interface *I, vd_trace_interface **I1));
    void (*erase)(P2(vd_trace_interface *I, unsigned long rgbcolor));
    void (*beg_path)(P1(vd_trace_interface *I));
    void (*end_path)(P1(vd_trace_interface *I));
    void (*moveto)(P3(vd_trace_interface *I, double x, double y));
    void (*lineto)(P3(vd_trace_interface *I, double x, double y));
    void (*curveto)(P7(vd_trace_interface *I, double x0, double y0, double x1, double y1, double x2, double y2));
    void (*closepath)(P1(vd_trace_interface *I));
    void (*circle)(P4(vd_trace_interface *I, double x, double y, int r)); /* Radius doesn't scale. */
    void (*round)(P4(vd_trace_interface *I, double x, double y, int r)); /* Radius doesn't scale. */
    void (*fill)(P1(vd_trace_interface *I));
    void (*stroke)(P1(vd_trace_interface *I));
    void (*setcolor)(P2(vd_trace_interface *I, unsigned long rgbcolor));
    void (*setlinewidth)(P2(vd_trace_interface *I, unsigned int width)); /* Width doesn't scale. */
    void (*text)(P4(vd_trace_interface *I, double x, double y, char *ASCIIZ)); /* Font doesn't scale. */
    void (*wait)(P1(vd_trace_interface *I));
};

extern vd_trace_interface * vd_trace0; /* Pointer to trace interface. */
extern vd_trace_interface * vd_trace1; /* A copy of vd_trace0, or NULL if trace is disabled. */
extern char vd_flags[];

void vd_impl_moveto(P2(double x, double y));
void vd_impl_lineto(P2(double x, double y));
void vd_impl_lineto_multi(P2(struct gs_fixed_point_s *p, int n));
void vd_impl_curveto(P6(double x0, double y0, double x1, double y1, double x2, double y2));
void vd_impl_bar(P6(double x0, double y0, double x1, double y1, int w, unsigned long c)); /* unscaled width */
void vd_impl_square(P4(double x0, double y0, int w, unsigned int c)); /* unscaled width */
void vd_impl_curve(P10(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, int w, unsigned long c)); /* unscaled width */
void vd_impl_circle(P4(double x, double y, int r, unsigned long c)); /* unscaled radius */
void vd_impl_round(P4(double x, double y, int r, unsigned long c));  /* unscaled radius */
void vd_impl_text(P4(double x, double y, char *s, unsigned long c)); /* unscaled font */
void vd_setflag(char f, char v);

#ifndef RGB
#    define RGB(r,g,b) ((unsigned long)(r) * 65536L + (unsigned long)(g) * 256L + (unsigned long)(b))
#endif

#if VD_TRACE && defined(DEBUG)
#    define vd_get_dc(f)        while (vd_trace0 && vd_flags[(f) & 127]) { vd_trace0->get_dc(vd_trace0, &vd_trace1); break; }
#    define vd_release_dc       while (vd_trace1) { vd_trace1->release_dc(vd_trace1, &vd_trace1); break; }
#    define vd_get_size_unscaled_x      (vd_trace1 ? vd_trace1->get_size_x(vd_trace1) : 100)
#    define vd_get_size_unscaled_y      (vd_trace1 ? vd_trace1->get_size_y(vd_trace1) : 100)
#    define vd_get_size_scaled_x        (vd_trace1 ? vd_trace1->get_size_x(vd_trace1) / vd_trace1->scale_x : 100)
#    define vd_get_size_scaled_y        (vd_trace1 ? vd_trace1->get_size_y(vd_trace1) / vd_trace1->scale_y : 100)
#    define vd_get_scale_x              (vd_trace1 ? vd_trace1->scale_x : 100)
#    define vd_get_scale_y              (vd_trace1 ? vd_trace1->scale_y : 100)
#    define vd_get_origin_x             (vd_trace1 ? vd_trace1->orig_x : 0)
#    define vd_get_origin_y             (vd_trace1 ? vd_trace1->orig_y : 0)
#    define vd_set_scale(s)    while (vd_trace1) { vd_trace1->scale_x = vd_trace1->scale_y = s; break; }
#    define vd_set_scaleXY(sx,sy)       while (vd_trace1) { vd_trace1->scale_x = sx, vd_trace1->scale_y = sy; break; }
#    define vd_set_origin(x,y)  while (vd_trace1) { vd_trace1->orig_x  = x, vd_trace1->orig_y  = y; break; }
#    define vd_set_shift(x,y)   while (vd_trace1) { vd_trace1->shift_x = x, vd_trace1->shift_y = y; break; }
#    define vd_set_central_shift        while (vd_trace1) { vd_trace1->shift_x = vd_trace1->get_size_x(vd_trace1)/2, vd_trace1->shift_y = vd_trace1->get_size_y(vd_trace1)/2; break; }
#    define vd_erase(c)         while (vd_trace1) { vd_trace1->erase(vd_trace1,c); break; }
#    define vd_beg_path         while (vd_trace1) { vd_trace1->beg_path(vd_trace1); break; }
#    define vd_end_path         while (vd_trace1) { vd_trace1->end_path(vd_trace1); break; }
#    define vd_moveto(x,y)      vd_impl_moveto(x,y)
#    define vd_lineto(x,y)      vd_impl_lineto(x,y)
#    define vd_lineto_multi(p,n)        vd_impl_lineto_multi(p,n)
#    define vd_curveto(x0,y0,x1,y1,x2,y2) vd_impl_curveto(x0,y0,x1,y1,x2,y2)
#    define vd_closepath        while (vd_trace1) { vd_trace1->closepath(vd_trace1); break; }
#    define vd_bar(x0,y0,x1,y1,c)       vd_impl_bar(x0,y0,x1,y1,1,c)
#    define vd_bar_w(x0,y0,x1,y1,w,c)   vd_impl_bar(x0,y0,x1,y1,w,c)
#    define vd_square(x0,y0,w,c)        vd_impl_square(x0,y0,w,c)
#    define vd_curve(x0,y0,x1,y1,x2,y2,x3,y3,c)     vd_impl_curve(x0,y0,x1,y1,x2,y2,x3,y3,1,c)
#    define vd_curve_w(x0,y0,x1,y1,x2,y2,x3,y3,c,w) vd_impl_curve(x0,y0,x1,y1,x2,y2,x3,y3,w,c)
#    define vd_circle(x,y,r,c)  vd_impl_circle(x,y,r,c)
#    define vd_round(x,y,r,c)   vd_impl_round(x,y,r,c)
#    define vd_fill             while (vd_trace1) { vd_trace1->fill(vd_trace1); break; }
#    define vd_stroke           while (vd_trace1) { vd_trace1->stroke(vd_trace1); break; }
#    define vd_setcolor(c)      while (vd_trace1) { vd_trace1->setcolor(vd_trace1,c); break; }
#    define vd_setlinewidth(w)  while (vd_trace1) { vd_trace1->setlinewidth(vd_trace1,w); break; }
#    define vd_text(x,y,s,c)    vd_impl_text(x,y,s,c)
#    define vd_wait             while (vd_trace1) { vd_trace1->wait(vd_trace1); break; }
#else
#    define vd_get_dc(f)
#    define vd_release_dc
#    define vd_get_size_unscaled_x      100
#    define vd_get_size_unscaled_y      100
#    define vd_get_size_scaled_x        100
#    define vd_get_size_scaled_y        100
#    define vd_get_scale_x              100
#    define vd_get_scale_y              100
#    define vd_get_origin_x             0
#    define vd_get_origin_y             0
#    define vd_set_scale(sx)
#    define vd_set_scaleXY(sx,sy)
#    define vd_set_origin(x,y)
#    define vd_set_shift(x,y)
#    define vd_set_central_shift
#    define vd_erase(c)
#    define vd_beg_path
#    define vd_end_path
#    define vd_moveto(x,y)
#    define vd_lineto(x,y)
#    define vd_lineto_multi(p,n)
#    define vd_curveto(x0,y0,x1,y1,x2,y2)
#    define vd_closepath
#    define vd_bar(x0,y0,x1,y1,c)
#    define vd_bar_w(x0,y0,x1,y1,w,c)
#    define vd_square(x0,y0,w,c)
#    define vd_curve(x0,y0,x1,y1,x2,y2,x3,y3,c)
#    define vd_curve_w(x0,y0,x1,y1,x2,y2,x3,y3,w,c)
#    define vd_circle(x,y,r,c)
#    define vd_round(x,y,r,c)
#    define vd_fill
#    define vd_stroke
#    define vd_setcolor(c)
#    define vd_setlinewidth(w)
#    define vd_text(x,y,s,c)
#    define vd_wait
#endif

#endif /* vdtrace_INCLUDED */
