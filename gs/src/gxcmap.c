/* Copyright (C) 1992, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gxcmap.c */
/* Color mapping for Ghostscript */
#include "gx.h"
#include "gserrors.h"
#include "gsccolor.h"
#include "gxcspace.h"
#include "gxfarith.h"
#include "gxfrac.h"
#include "gxdcconv.h"
#include "gxdevice.h"
#include "gxcmap.h"
#include "gxlum.h"
#include "gzstate.h"
#include "gxdither.h"

/* Structure descriptor */
public_st_device_color();
#define cptr ((gx_device_color *)vptr)
private ENUM_PTRS_BEGIN(device_color_enum_ptrs) {
	struct_proc_enum_ptrs((*proc)) = cptr->type->enum_ptrs;
	if ( proc == 0 )
	  return 0;
	return (*proc)(vptr, size, index, pep);
} ENUM_PTRS_END
private RELOC_PTRS_BEGIN(device_color_reloc_ptrs) {
	struct_proc_reloc_ptrs((*proc)) = cptr->type->reloc_ptrs;
	if ( proc != 0 )
	  (*proc)(vptr, size, gcst);
} RELOC_PTRS_END
#undef cptr

/* ------ Trace device mapping procedures ------ */

/* If DEBUG is defined, these procedures substitute for direct calls */
/* on the device map_{rgb,cmyk}_color procedures. */

gx_color_index
gx_proc_map_rgb_color(gx_device *dev,
  gx_color_value vr, gx_color_value vg, gx_color_value vb)
{	gx_color_index cindex =
	  (*dev_proc(dev, map_rgb_color))(dev, vr, vg, vb);
	if_debug5('C', "%s [C]RGB %u,%u,%u -> color 0x%lx\n",
		  dev->dname, (uint)vr, (uint)vg, (uint)vb, (ulong)cindex);
	return cindex;
}

gx_color_index
gx_proc_map_rgb_alpha_color(gx_device *dev,
  gx_color_value vr, gx_color_value vg, gx_color_value vb, gx_color_value va)
{	gx_color_index cindex =
	  (*dev_proc(dev, map_rgb_alpha_color))(dev, vr, vg, vb, va);
	if_debug6('C', "[C]%s RGBA %u,%u,%u,%u -> color 0x%lx\n",
		  dev->dname, (uint)vr, (uint)vg, (uint)vb, (uint)va,
		  (ulong)cindex);
	return cindex;
}

gx_color_index
gx_proc_map_cmyk_color(gx_device *dev,
  gx_color_value vc, gx_color_value vm, gx_color_value vy, gx_color_value vk)
{	gx_color_index cindex =
	  (*dev_proc(dev, map_cmyk_color))(dev, vc, vm, vy, vk);
	if_debug6('C', "[C]%s CMYK %u,%u,%u,%u -> color 0x%lx\n",
		  dev->dname, (uint)vc, (uint)vm, (uint)vy, (uint)vk,
		  (ulong)cindex);
	return cindex;
}

/* ---------------- Device color rendering ---------------- */

private cmap_proc_gray(cmap_gray_halftoned);
private cmap_proc_gray(cmap_gray_direct);
private cmap_proc_gray(cmap_gray_to_rgb_halftoned);
private cmap_proc_gray(cmap_gray_to_rgb_direct);
private cmap_proc_gray(cmap_gray_to_cmyk_halftoned);
private cmap_proc_gray(cmap_gray_to_cmyk_direct);
private cmap_proc_rgb(cmap_rgb_halftoned);
private cmap_proc_rgb(cmap_rgb_direct);
private cmap_proc_rgb(cmap_rgb_to_gray_halftoned);
private cmap_proc_rgb(cmap_rgb_to_gray_direct);
private cmap_proc_rgb(cmap_rgb_to_cmyk);
#define cmap_cmyk_halftoned cmap_cmyk_direct
private cmap_proc_cmyk(cmap_cmyk_direct);
private cmap_proc_cmyk(cmap_cmyk_to_gray);
private cmap_proc_cmyk(cmap_cmyk_to_rgb);
#ifdef DPNEXT
private cmap_proc_rgb_alpha(cmap_rgb_alpha_halftoned);
private cmap_proc_rgb_alpha(cmap_rgb_alpha_direct);
/* Procedure names are only guaranteed unique to 23 characters.... */
private cmap_proc_rgb_alpha(cmap_rgb_alpha2gray_halftoned);
private cmap_proc_rgb_alpha(cmap_rgb_alpha2gray_direct);
private cmap_proc_rgb_alpha(cmap_rgb_alpha_to_cmyk);
#endif

private const gx_color_map_procs
  cmap_gray_few =
   { cmap_gray_halftoned, cmap_rgb_to_gray_halftoned, cmap_cmyk_to_gray
#ifdef DPNEXT
     , cmap_rgb_alpha2gray_halftoned
#endif
   },
  cmap_gray_many =
   { cmap_gray_direct, cmap_rgb_to_gray_direct, cmap_cmyk_to_gray
#ifdef DPNEXT
     , cmap_rgb_alpha2gray_direct
#endif
   },
  cmap_rgb_few =
   { cmap_gray_to_rgb_halftoned, cmap_rgb_halftoned, cmap_cmyk_to_rgb
#ifdef DPNEXT
     , cmap_rgb_alpha_halftoned
#endif
   },
  cmap_rgb_many =
   { cmap_gray_to_rgb_direct, cmap_rgb_direct, cmap_cmyk_to_rgb
#ifdef DPNEXT
     , cmap_rgb_alpha_direct
#endif
   },
  cmap_cmyk_few =
   { cmap_gray_to_cmyk_halftoned, cmap_rgb_to_cmyk, cmap_cmyk_halftoned
#ifdef DPNEXT
     , cmap_rgb_alpha_to_cmyk
#endif
   },
  cmap_cmyk_many =
   { cmap_gray_to_cmyk_direct, cmap_rgb_to_cmyk, cmap_cmyk_direct
#ifdef DPNEXT
     , cmap_rgb_alpha_to_cmyk
#endif
   };

const gx_color_map_procs *cmap_procs_default = &cmap_gray_many;

private const gx_color_map_procs _ds *cmap_few[] = {
	0, &cmap_gray_few, 0, &cmap_rgb_few, &cmap_cmyk_few
};

private const gx_color_map_procs _ds *cmap_many[] = {
	0, &cmap_gray_many, 0, &cmap_rgb_many, &cmap_cmyk_many
};

/* Determine the color mapping procedures for a device. */
const gx_color_map_procs *
gx_device_cmap_procs(const gx_device *dev)
{	return ((gx_device_has_color(dev) ? dev->color_info.max_color :
		 dev->color_info.max_gray) >= 31 ? cmap_many : cmap_few)
		[dev->color_info.num_components];
}

/* Set the color mapping procedures in the graphics state. */
void
gx_set_cmap_procs(gs_imager_state *pis, const gx_device *dev)
{	pis->cmap_procs = gx_device_cmap_procs(dev);
}

/* Remap the color in the graphics state. */
int
gx_remap_color(gs_state *pgs)
{	const gs_color_space *pcs = pgs->color_space;
	/* The current color in the graphics state is always used for */
	/* the texture, never for the source. */
	return (*pcs->type->remap_color)(pgs->ccolor, pcs, pgs->dev_color,
					 (gs_imager_state *)pgs, pgs->device,
					 gs_color_select_texture);
}

/* Indicate that a color space has no underlying concrete space. */
const gs_color_space *
gx_no_concrete_space(const gs_color_space *pcs, const gs_imager_state *pis)
{	return NULL;
}

/* Indicate that a color space is concrete. */
const gs_color_space *
gx_same_concrete_space(const gs_color_space *pcs, const gs_imager_state *pis)
{	return pcs;
}

/* Indicate that a color cannot be concretized. */
int
gx_no_concretize_color(const gs_client_color *pcc, const gs_color_space *pcs,
  frac *pconc, const gs_imager_state *pis)
{	return_error(gs_error_rangecheck);
}

/* By default, remap a color by concretizing it and then */
/* remapping the concrete color. */
int
gx_default_remap_color(const gs_client_color *pcc, const gs_color_space *pcs,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	frac conc[4];
	const gs_color_space *pconcs;
	int code = (*pcs->type->concretize_color)(pcc, pcs, conc, pis);

	if ( code < 0 )
	  return code;
	pconcs = cs_concrete_space(pcs, pis);
	return (*pconcs->type->remap_concrete_color)(conc, pdc, pis, dev,
						     select);
}

/* Color remappers for the standard color spaces. */
/* Note that we use D... instead of Device... in some places because */
/* gcc under VMS only retains 23 characters of procedure names. */

#define unit_frac(v, ftemp)\
  (ftemp = (v),\
   (is_fneg(ftemp) ? frac_0 : is_fge1(ftemp) ? frac_1 : float2frac(ftemp)))

/* DeviceGray */
int
gx_concretize_DeviceGray(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_imager_state *pis)
{	float ftemp;
	pconc[0] = unit_frac(pc->paint.values[0], ftemp);
	return 0;
}
int
gx_remap_concrete_DGray(const frac *pconc,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	(*pis->cmap_procs->map_gray)
	  (pconc[0], pdc, pis, dev, select);
	return 0;
}
int
gx_remap_DeviceGray(const gs_client_color *pc, const gs_color_space *pcs,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	float ftemp;
	(*pis->cmap_procs->map_gray)
	  (unit_frac(pc->paint.values[0], ftemp),
	   pdc, pis, dev, select);
	return 0;
}

/* DeviceRGB */
int
gx_concretize_DeviceRGB(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_imager_state *pis)
{	float ftemp;
	pconc[0] = unit_frac(pc->paint.values[0], ftemp);
	pconc[1] = unit_frac(pc->paint.values[1], ftemp);
	pconc[2] = unit_frac(pc->paint.values[2], ftemp);
	return 0;
}
int
gx_remap_concrete_DRGB(const frac *pconc,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	gx_remap_concrete_rgb(pconc[0], pconc[1], pconc[2], pdc, pis, dev,
			      select);
	return 0;
}
int
gx_remap_DeviceRGB(const gs_client_color *pc, const gs_color_space *pcs,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	float ft0, ft1, ft2;
	gx_remap_concrete_rgb(unit_frac(pc->paint.values[0], ft0),
			      unit_frac(pc->paint.values[1], ft1),
			      unit_frac(pc->paint.values[2], ft2),
			      pdc, pis, dev, select);
	return 0;
}

/* DeviceCMYK */
int
gx_concretize_DeviceCMYK(const gs_client_color *pc, const gs_color_space *pcs,
  frac *pconc, const gs_imager_state *pis)
{	float ftemp;
	pconc[0] = unit_frac(pc->paint.values[0], ftemp);
	pconc[1] = unit_frac(pc->paint.values[1], ftemp);
	pconc[2] = unit_frac(pc->paint.values[2], ftemp);
	pconc[3] = unit_frac(pc->paint.values[3], ftemp);
	return 0;
}
int
gx_remap_concrete_DCMYK(const frac *pconc,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	gx_remap_concrete_cmyk(pconc[0], pconc[1], pconc[2], pconc[3], pdc,
			       pis, dev, select);
	return 0;
}
int
gx_remap_DeviceCMYK(const gs_client_color *pc, const gs_color_space *pcs,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	float ft0, ft1, ft2, ft3;
	gx_remap_concrete_cmyk(unit_frac(pc->paint.values[0], ft0),
			       unit_frac(pc->paint.values[1], ft1),
			       unit_frac(pc->paint.values[2], ft2),
			       unit_frac(pc->paint.values[3], ft3),
			       pdc, pis, dev, select);
	return 0;
}

/* ------ Render Gray color. ------ */

private void
cmap_gray_halftoned(frac gray, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	if ( gx_render_gray(gx_map_color_frac(pis, gray, effective_transfer.colored.gray), pdc, pis, dev, select) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_gray_direct(frac gray, gx_device_color *pdc, const gs_imager_state *pis,
  gx_device *dev, gs_color_select_t select)
{	frac mgray = gx_map_color_frac(pis, gray, effective_transfer.colored.gray);
	gx_color_value cv_gray = frac2cv(mgray);
	gx_color_index color =
	  (pis->alpha == gx_max_color_value ?
	   gx_map_rgb_color(dev, cv_gray, cv_gray, cv_gray) :
	   gx_map_rgb_alpha_color(dev, cv_gray, cv_gray, cv_gray, pis->alpha));
	if ( color == gx_no_color_index )
	{	if ( gx_render_gray(mgray, pdc, pis, dev, select) == 1 )
		  gx_color_load_select(pdc, pis, dev, select);
		return;
	}
	color_set_pure(pdc, color);
}

private void
cmap_gray_to_rgb_halftoned(frac gray, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	cmap_rgb_halftoned(gray, gray, gray, pdc, pis, dev, select);
}

private void
cmap_gray_to_rgb_direct(frac gray, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	cmap_rgb_direct(gray, gray, gray, pdc, pis, dev, select);
}

private void
cmap_gray_to_cmyk_halftoned(frac gray, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	/*
	 * Per the last paragraph of section 6.3 (p. 309) of the
	 * PostScript Language Reference Manual, 2nd Edition,
	 * we must bypass the C, M, and Y transfer functions in this case.
	 */
	frac mgray = gx_map_color_frac(pis, gray, effective_transfer.colored.gray);
	if ( gx_render_gray(mgray, pdc, pis, dev, select) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_gray_to_cmyk_direct(frac gray, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	/*
	 * Per the last paragraph of section 6.3 (p. 309) of the
	 * PostScript Language Reference Manual, 2nd Edition,
	 * we must bypass the C, M, and Y transfer functions in this case.
	 */
	frac mgray = gx_map_color_frac(pis, gray, effective_transfer.colored.gray);
	frac mblack = frac_1 - mgray;
	gx_color_index color =
	  gx_map_cmyk_color(dev,
			    frac2cv(frac_0), frac2cv(frac_0),
			    frac2cv(frac_0), frac2cv(mblack));
	if ( color != gx_no_color_index )
	{	color_set_pure(pdc, color);
		return;
	}
	if ( gx_render_gray(mgray, pdc, pis, dev, select) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

/* ------ Render RGB color. ------ */

/*
 * This code should test r == g and g == b and then use the gray
 * rendering procedures.  The Adobe documentation allows this:
 * conversion between color spaces occurs before the transfer function
 * and halftoning.  However, output from FrameMaker (mis)uses the
 * transfer function to provide the equivalent of indexed color;
 * it requires the color components to be passed through unchanged.
 * For this reason, we have to make the check after the transfer
 * function rather than before.
 *
 * Since this procedure is used so heavily, we duplicate most of its code
 * rather than making a text for color_info.max_color >= 31.
 */

private void
cmap_rgb_halftoned(frac r, frac g, frac b, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac mred = gx_map_color_frac(pis, r, effective_transfer.colored.red);
	frac mgreen = gx_map_color_frac(pis, g, effective_transfer.colored.green);
	frac mblue = gx_map_color_frac(pis, b, effective_transfer.colored.blue);
	if ( (mred == mgreen && mred == mblue ?	/* gray shade */
	      gx_render_gray(mred, pdc, pis, dev, select) :
	      gx_render_rgb(mred, mgreen, mblue, pdc, pis, dev, select)) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_rgb_direct(frac r, frac g, frac b, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac mred = gx_map_color_frac(pis, r, effective_transfer.colored.red);
	frac mgreen = gx_map_color_frac(pis, g, effective_transfer.colored.green);
	frac mblue = gx_map_color_frac(pis, b, effective_transfer.colored.blue);
	gx_color_index color =
	  (pis->alpha == gx_max_color_value ?
	   gx_map_rgb_color(dev, frac2cv(mred), frac2cv(mgreen),
			    frac2cv(mblue)) :
	   gx_map_rgb_alpha_color(dev, frac2cv(mred), frac2cv(mgreen),
				  frac2cv(mblue), pis->alpha));
	if ( color != gx_no_color_index )
	  {	color_set_pure(pdc, color);
		return;
	  }
	if ( (mred == mgreen && mred == mblue ?	/* gray shade */
	      gx_render_gray(mred, pdc, pis, dev, select) :
	      gx_render_rgb(mred, mgreen, mblue, pdc, pis, dev, select)) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_rgb_to_gray_halftoned(frac r, frac g, frac b, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	cmap_gray_halftoned(color_rgb_to_gray(r, g, b, pis), pdc, pis, dev, select);
}

private void
cmap_rgb_to_gray_direct(frac r, frac g, frac b, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	cmap_gray_direct(color_rgb_to_gray(r, g, b, pis), pdc, pis, dev, select);
}

private void
cmap_rgb_to_cmyk(frac r, frac g, frac b, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac cmyk[4];
	color_rgb_to_cmyk(r, g, b, pis, cmyk);
	(*pis->cmap_procs->map_cmyk)(cmyk[0], cmyk[1], cmyk[2], cmyk[3], pdc, pis, dev, select);
}

/* ------ Render CMYK color. ------ */

private void
cmap_cmyk_to_gray(frac c, frac m, frac y, frac k, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	(*pis->cmap_procs->map_gray)(color_cmyk_to_gray(c, m, y, k, pis), pdc, pis, dev, select);
}

private void
cmap_cmyk_direct(frac c, frac m, frac y, frac k, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac mcyan = frac_1 - gx_map_color_frac(pis, frac_1 - c, effective_transfer.colored.red);
	frac mmagenta = frac_1 - gx_map_color_frac(pis, frac_1 - m, effective_transfer.colored.green);
	frac myellow = frac_1 - gx_map_color_frac(pis, frac_1 - y, effective_transfer.colored.blue);
	frac mblack = frac_1 - gx_map_color_frac(pis, frac_1 - k, effective_transfer.colored.gray);
	/* We make a test for direct vs. halftoned, rather than */
	/* duplicating most of the code of this procedure. */
	if ( dev->color_info.max_color >= 31 )
	{	gx_color_index color =
			gx_map_cmyk_color(dev,
					  frac2cv(mcyan), frac2cv(mmagenta),
					  frac2cv(myellow), frac2cv(mblack));
		if ( color != gx_no_color_index )
		{	color_set_pure(pdc, color);
			return;
		}
	}
	/* Don't convert colors with C = M = Y to gray shades: */
	/* on a CMYK device, this may produce quite different output. */
	if ( gx_render_cmyk(mcyan, mmagenta, myellow, mblack, pdc, pis, dev, select) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_cmyk_to_rgb(frac c, frac m, frac y, frac k, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac rgb[3];
	color_cmyk_to_rgb(c, m, y, k, pis, rgb);
	(*pis->cmap_procs->map_rgb)(rgb[0], rgb[1], rgb[2], pdc, pis, dev, select);
}

#ifdef DPNEXT

/* ------ Render RGB+alpha color. ------ */

private void
cmap_rgb_alpha2gray_halftoned(frac r, frac g, frac b, frac alpha,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	frac gray = color_rgb_to_gray(r, g, b, pis);

	if ( gx_render_gray_alpha(gx_map_color_frac(pis, gray, effective_transfer.colored.gray), frac2cv(alpha), pdc, pis, dev, select) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_rgb_alpha2gray_direct(frac r, frac g, frac b, frac alpha,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	frac gray = color_rgb_to_gray(r, g, b, pis);
	frac mgray =
	  gx_map_color_frac(pis, gray, effective_transfer.colored.gray);
	gx_color_value cv_gray = frac2cv(mgray);
	gx_color_value cv_alpha = frac2cv(alpha);
	gx_color_index color =
	  (cv_alpha == gx_max_color_value ?
	   gx_map_rgb_color(dev, cv_gray, cv_gray, cv_gray) :
	   gx_map_rgb_alpha_color(dev, cv_gray, cv_gray, cv_gray, cv_alpha));

	if ( color == gx_no_color_index )
	  { if ( gx_render_gray_alpha(mgray, cv_alpha, pdc, pis, dev, select) == 1 )
	      gx_color_load_select(pdc, pis, dev, select);
	    return;
	  }
	color_set_pure(pdc, color);
}

private void
cmap_rgb_alpha_halftoned(frac r, frac g, frac b, frac alpha,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	frac mred = gx_map_color_frac(pis, r, effective_transfer.colored.red);
	frac mgreen = gx_map_color_frac(pis, g, effective_transfer.colored.green);
	frac mblue = gx_map_color_frac(pis, b, effective_transfer.colored.blue);
	gx_color_value cv_alpha = frac2cv(alpha);

	if ( (mred == mgreen && mred == mblue ?	/* gray shade */
	      gx_render_gray_alpha(mred, cv_alpha, pdc, pis, dev, select) :
	      gx_render_rgb_alpha(mred, mgreen, mblue, cv_alpha, pdc, pis, dev, select)) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

private void
cmap_rgb_alpha_direct(frac r, frac g, frac b, frac alpha, gx_device_color *pdc,
  const gs_imager_state *pis, gx_device *dev, gs_color_select_t select)
{	frac mred = gx_map_color_frac(pis, r, effective_transfer.colored.red);
	frac mgreen = gx_map_color_frac(pis, g, effective_transfer.colored.green);
	frac mblue = gx_map_color_frac(pis, b, effective_transfer.colored.blue);
	gx_color_value cv_alpha = frac2cv(alpha);
	gx_color_index color =
	  (cv_alpha == gx_max_color_value ?
	   gx_map_rgb_color(dev, frac2cv(mred), frac2cv(mgreen),
			    frac2cv(mblue)) :
	   gx_map_rgb_alpha_color(dev, frac2cv(mred), frac2cv(mgreen),
				  frac2cv(mblue), cv_alpha));

	if ( color != gx_no_color_index )
	  {	color_set_pure(pdc, color);
		return;
	  }
	if ( (mred == mgreen && mred == mblue ?	/* gray shade */
	      gx_render_gray_alpha(mred, cv_alpha, pdc, pis, dev, select) :
	      gx_render_rgb_alpha(mred, mgreen, mblue, cv_alpha, pdc, pis, dev, select)) == 1 )
	  gx_color_load_select(pdc, pis, dev, select);
}

/* Currently CMYK devices can't handle alpha. */
/* Just multiply the values towards white. */
private void
cmap_rgb_alpha_to_cmyk(frac r, frac g, frac b, frac alpha,
  gx_device_color *pdc, const gs_imager_state *pis, gx_device *dev,
  gs_color_select_t select)
{	frac white = frac_1 - alpha;

	cmap_rgb_to_cmyk((frac)((long)r * alpha / frac_1) + white,
			 (frac)((long)g * alpha / frac_1) + white,
			 (frac)((long)b * alpha / frac_1) + white,
			 pdc, pis, dev, select);
}

#endif

/* ------ Transfer function mapping ------ */

/* Define the generic transfer function for the library layer. */
/* This just returns what's already in the map. */
float
gs_mapped_transfer(floatp value, const gx_transfer_map *pmap)
{	return gx_map_color_float(pmap, value);
}

#if FRAC_MAP_INTERPOLATE		/* NOTA BENE */

/* Map a color fraction through a transfer map. */
/* We only use this if we are interpolating. */
frac
gx_color_frac_map(frac cv, const frac *values)
{
#define cp_frac_bits (frac_bits - log2_transfer_map_size)
	int cmi = frac2bits_floor(cv, log2_transfer_map_size);
	frac mv = values[cmi];
	int rem, mdv;
	/* Interpolate between two adjacent values if needed. */
	rem = cv - bits2frac(cmi, log2_transfer_map_size);
	if ( rem == 0 ) return mv;
	mdv = values[cmi + 1] - mv;
#if arch_ints_are_short
	/* Only use long multiplication if necessary. */
	if ( mdv < -1 << (16 - cp_frac_bits) ||
	     mdv > 1 << (16 - cp_frac_bits)
	   )
		return mv + (uint)(((ulong)rem * mdv) >> cp_frac_bits);
#endif
	return mv + ((rem * mdv) >> cp_frac_bits);
#undef cp_frac_bits
}

#endif					/* FRAC_MAP_INTERPOLATE */

/* ------ Default device color mapping ------ */

/* RGB mapping for black-and-white devices */

/* White-on-black */
gx_color_index
gx_default_w_b_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	/* Map values >= 1/2 to 1, < 1/2 to 0. */
	return ((r | g | b) > gx_max_color_value / 2 ?
		(gx_color_index)1 : (gx_color_index)0);
}
int
gx_default_w_b_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	/* Map 1 to max_value, 0 to 0. */
	prgb[0] = prgb[1] = prgb[2] = -(gx_color_value)color;
	return 0;
}
/* Black-on-white */
gx_color_index
gx_default_b_w_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	/* Map values >= 1/2 to 0, < 1/2 to 1. */
	return ((r | g | b) > gx_max_color_value / 2 ?
		(gx_color_index)0 : (gx_color_index)1);
}
int
gx_default_b_w_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	/* Map 0 to max_value, 1 to 0. */
	prgb[0] = prgb[1] = prgb[2] = -((gx_color_value)color ^ 1);
	return 0;
}

/* RGB mapping for gray-scale devices */

gx_color_index
gx_default_gray_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	/* We round the value rather than truncating it. */
	gx_color_value gray =
	  (((r * (ulong)lum_red_weight) +
	    (g * (ulong)lum_green_weight) +
	    (b * (ulong)lum_blue_weight) +
	    (lum_all_weights / 2)) / lum_all_weights
	   * dev->color_info.max_gray +
	   (gx_max_color_value / 2)) / gx_max_color_value;
	return gray;
}

int
gx_default_gray_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	gx_color_value gray =
		color * gx_max_color_value / dev->color_info.max_gray;
	prgb[0] = gray;
	prgb[1] = gray;
	prgb[2] = gray;
	return 0;
}

/* RGB mapping for 24-bit true (RGB) color devices */

gx_color_index
gx_default_rgb_map_rgb_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b)
{	if ( dev->color_info.depth == 24 )
		return gx_color_value_to_byte(b) +
			((uint)gx_color_value_to_byte(g) << 8) +
			((ulong)gx_color_value_to_byte(r) << 16);
	else
	{	uint bits_per_color = dev->color_info.depth / 3;
		ulong max_value = (1 << bits_per_color) - 1;

		return ((r * max_value / gx_max_color_value) << (bits_per_color * 2)) +
			((g * max_value / gx_max_color_value) << (bits_per_color)) +
			(b * max_value / gx_max_color_value);
	}
}

/* Map a color index to a r-g-b color. */
int
gx_default_rgb_map_color_rgb(gx_device *dev, gx_color_index color,
  gx_color_value prgb[3])
{	if ( dev->color_info.depth == 24 )
	{	prgb[0] = gx_color_value_from_byte(color >> 16);
		prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
		prgb[2]	= gx_color_value_from_byte(color & 0xff);
	}
	else
	{	uint bits_per_color = dev->color_info.depth / 3;
		uint color_mask = (1 << bits_per_color) - 1;

		prgb[0] = ((color >> (bits_per_color * 2)) & color_mask) *
			(ulong)gx_max_color_value / color_mask;
		prgb[1] = ((color >> (bits_per_color)) & color_mask) *
			(ulong)gx_max_color_value / color_mask;
		prgb[2] = (color & color_mask) *
			(ulong)gx_max_color_value / color_mask;
	}
	return 0;
}

/* CMYK mapping for RGB devices (should never be called!) */

gx_color_index
gx_default_map_cmyk_color(gx_device *dev,
  gx_color_value c, gx_color_value m, gx_color_value y, gx_color_value k)
{	/* Convert to RGB */
	frac rgb[3];
	color_cmyk_to_rgb(cv2frac(c), cv2frac(m), cv2frac(y), cv2frac(k),
			  NULL, rgb);
	return gx_map_rgb_color(dev, frac2cv(rgb[0]),
				frac2cv(rgb[1]), frac2cv(rgb[2]));
}

/* CMYK mapping for CMYK devices */

gx_color_index
gx_default_cmyk_map_cmyk_color(gx_device *dev,
  gx_color_value c, gx_color_value m, gx_color_value y, gx_color_value k)
{	gx_color_index color =
	  (gx_color_value_to_byte(k) +
	   ((uint)gx_color_value_to_byte(y) << 8)) +
	  ((ulong)(gx_color_value_to_byte(m) +
		   ((uint)gx_color_value_to_byte(c) << 8)) << 16);

	return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Default mapping from RGB+alpha to RGB. */

gx_color_index
gx_default_map_rgb_alpha_color(gx_device *dev,
  gx_color_value r, gx_color_value g, gx_color_value b, gx_color_value alpha)
{	/* Premultiply the values towards white. */
	gx_color_value white = gx_max_color_value - alpha;

	if ( white == 0 )
	  return gx_map_rgb_color(dev, r, g, b);
	return gx_map_rgb_color(dev,
	  (gx_color_value)((ulong)r * alpha / gx_max_color_value) + white,
	  (gx_color_value)((ulong)g * alpha / gx_max_color_value) + white,
	  (gx_color_value)((ulong)b * alpha / gx_max_color_value) + white);
}
