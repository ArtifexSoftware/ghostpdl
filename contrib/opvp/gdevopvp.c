/* Copyright (c) 2003-2004, AXE, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  59 Temple Place, Suite 330, Boston, MA, 02111-1307.

*/

/* $Id: gdevopvp.c,v 1.13 2006/01/18 15:11:59 gishi Exp $ */
/* gdevopvp.c  ver.1.00 rel.1.0     26 Nov 2004 */
/* OpenPrinting Vector Printer Driver Glue Code */

#include        "std.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<string.h>
#include	<locale.h>
#include	<iconv.h>
#include	<langinfo.h>
#include	<dlfcn.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>

#include	"string_.h"
#include	"math_.h"
#include	"gx.h"
#include	"ghost.h"
#include	"gscdefs.h"
#include	"gsexit.h"
#include	"gsstruct.h"
#include	"gserrors.h"
#include	"gsmatrix.h"
#include	"gsparam.h"
#include	"gxdevice.h"
#include	"gscspace.h"
#include	"gsutil.h"
#include	"gdevprn.h"
#include	"gdevvec.h"
#include	"spprint.h"
#include	"ghost.h"
#include	"gzstate.h"
#include	"ialloc.h"
#include	"iddict.h"
#include	"dstack.h"
#include	"ilevel.h"
#include	"iinit.h"
#include	"iname.h"
#include	"imemory.h"
#include	"igstate.h"
#include	"interp.h"
#include	"ipacked.h"
#include	"iparray.h"
#include	"iutil.h"
#include	"ivmspace.h"
#include	"opdef.h"
#include	"store.h"
#include	"gspath.h"
#include	"gzpath.h"
#include	"gsropt.h"
#include	"gsiparam.h"
#include	"gxxfont.h"

/* added for image gamma correction */
#include	"gximage.h"
#include	"gxfmap.h"
#include	"gxfrac.h"
#include	"gxcvalue.h"

#include	"opvp_common.h"

#define	ENABLE_SIMPLE_MODE	1
#define	ENABLE_SKIP_RASTER	1
#define	ENABLE_AUTO_REVERSE	1

/* ----- data types/macros ----- */

/* for debug */
#ifdef	printf
#undef	printf
#endif
#ifdef	fprintf
#undef	fprintf
#endif

/* buffer */
#define	OPVP_BUFF_SIZE	1024

/* paper */
#define	PS_DPI		72
#define	MMPI		25.4
#define	TOLERANCE	3.0

typedef	struct {
const	char		*region;
const	char		*name;
	float		width;
	float		height;
} OPVP_Paper;

#define	X_DPI		300
#define	Y_DPI		300

/* driver */
#define	gx_device_opvp_common\
	char		*vectorDriver;\
	char		*printerModel;\
	void		*handle;\
	int		(*OpenPrinter)(int,char*,int*,OPVP_api_procs**);\
	int		*errorno;\
	int		outputFD;\
	int		nApiEntry;\
	OPVP_api_procs	*apiEntry;\
	int		printerContext;\
	char		*jobInfo;\
	char		*docInfo;\
	char		*pageInfo

typedef	struct	gx_device_opvp_s {
	gx_device_vector_common;
} gx_device_opvp;

typedef	struct	gx_device_oprp_s {
	gx_device_common;
	gx_prn_device_common;
} gx_device_oprp;

/* point (internal) */
typedef	struct {
	floatp		x;
	floatp		y;
} _fPoint;

/* ----- private function prototypes ----- */

/* Utilities */
static	int opvp_startpage(gx_device *);
static	int opvp_endpage(void);
static	char *opvp_alloc_string(char **, const char *);
static	char *opvp_cat_string(char **, const char *);
static	char *opvp_adjust_num_string(char *);
static	char **opvp_gen_dynamic_lib_name(void);
static	char *opvp_to_utf8(char *);
#define	opvp_check_in_page(pdev)	\
		((beginPage) || (inkjet) ? 0 \
		    : (*vdev_proc(pdev, beginpage))((gx_device_vector*)pdev))
static	int opvp_get_papertable_index(gx_device *);
static	char *opvp_get_sizestring(float, float);
/* not used	static	const char *opvp_get_papersize_region(gx_device *);*/
/* not used	static	const char *opvp_get_papersize_name(gx_device *);*/
/* not used	static	char *opvp_get_papersize_inch(gx_device *);*/
/* not used	static	const char *opvp_get_papersize(gx_device *);*/
static	char *opvp_get_mediasize(gx_device *);
static	char *opvp_gen_page_info(gx_device *);
static	char *opvp_gen_doc_info(gx_device *);
static	char *opvp_gen_job_info(gx_device *);
static	int opvp_set_brush_color(gx_device_opvp *, gx_color_index,
                                 OPVP_Brush *);
static	int opvp_draw_image(gx_device_opvp *, int,
                            int, int, int, int, int, /*const*/ byte *);

/* load/unload vector driver */
static	int opvp_load_vector_driver(void);
static	int opvp_unload_vector_driver(void);
static	int prepare_open(gx_device *);

/* driver procs */
static	int opvp_open(gx_device *);
static	int oprp_open(gx_device *);
static	void opvp_get_initial_matrix(gx_device *, gs_matrix *);
static	int opvp_output_page(gx_device *, int, int);
static	int opvp_close(gx_device *);
#if GS_VERSION_MAJOR >= 8
static	gx_color_index opvp_map_rgb_color(gx_device *, gx_color_value *);	/* modified for gs 8.15 */
#else
static	gx_color_index opvp_map_rgb_color(gx_device *, gx_color_value,
	               gx_color_value, gx_color_value);
#endif
static	int opvp_map_color_rgb(gx_device *, gx_color_index, gx_color_value *);
static	int opvp_copy_mono(gx_device *, /*const*/ byte *, int, int,
                           gx_bitmap_id, int, int, int, int,
                           gx_color_index, gx_color_index);
static	int opvp_copy_color(gx_device *, /*const*/ byte *, int, int,
                            gx_bitmap_id, int, int, int, int);
static	int _get_params(gs_param_list *);
static	int opvp_get_params(gx_device *, gs_param_list *);
static	int oprp_get_params(gx_device *, gs_param_list *);
static	int _put_params(gs_param_list *);
static	int opvp_put_params(gx_device *, gs_param_list *);
static	int oprp_put_params(gx_device *, gs_param_list *);
static	int opvp_fill_path(gx_device *, const gs_imager_state *, gx_path *,
	                   const gx_fill_params *, const gx_device_color *, 
	                   const gx_clip_path *);
static	int opvp_stroke_path(gx_device *, const gs_imager_state *, gx_path *,
	                     const gx_stroke_params *, const gx_drawing_color *,
	                     const gx_clip_path *);
static	int opvp_fill_mask(gx_device *, const byte *, int, int, gx_bitmap_id,
	                   int, int, int, int, const gx_drawing_color *,
	                   int, gs_logical_operation_t, const gx_clip_path *);

/* vector driver procs */
static	int opvp_beginpage(gx_device_vector *);
static	int opvp_setlinewidth(gx_device_vector *, floatp);
static	int opvp_setlinecap(gx_device_vector *, gs_line_cap);
static	int opvp_setlinejoin(gx_device_vector *, gs_line_join);
static	int opvp_setmiterlimit(gx_device_vector *, floatp);
static	int opvp_setdash(gx_device_vector *, const float *, uint, floatp);
static	int opvp_setflat(gx_device_vector *, floatp);
static	int opvp_setlogop(gx_device_vector *, gs_logical_operation_t,
	                  gs_logical_operation_t);
#if GS_VERSION_MAJOR >= 8
static	int opvp_can_handle_hl_color(gx_device_vector *, const gs_imager_state *, const gx_drawing_color *);
static	int opvp_setfillcolor(gx_device_vector *, const gs_imager_state *, const gx_drawing_color *);
static	int opvp_setstrokecolor(gx_device_vector *, const gs_imager_state *,const gx_drawing_color *);
#else
static	int opvp_setfillcolor(gx_device_vector *, const gx_drawing_color *);
static	int opvp_setstrokecolor(gx_device_vector *, const gx_drawing_color *);
#endif
static	int opvp_vector_dopath(gx_device_vector *, const gx_path *,
	                       gx_path_type_t, const gs_matrix *);
static	int opvp_vector_dorect(gx_device_vector *, fixed, fixed, fixed, fixed,
	                       gx_path_type_t);
static	int opvp_beginpath(gx_device_vector *, gx_path_type_t);
static	int opvp_moveto(gx_device_vector *, floatp, floatp, floatp, floatp,
	                gx_path_type_t);
static	int opvp_lineto(gx_device_vector *, floatp, floatp, floatp, floatp,
	                gx_path_type_t);
static	int opvp_curveto(gx_device_vector *, floatp, floatp, floatp, floatp,
	                 floatp, floatp, floatp, floatp, gx_path_type_t);
static	int opvp_closepath(gx_device_vector *, floatp, floatp, floatp, floatp,
	                   gx_path_type_t);
static	int opvp_endpath(gx_device_vector *, gx_path_type_t);

/* ----- Paper definition ----- */
OPVP_Paper	paperTable[] =
{
#if 0
	{"jpn","hagaki",100/MMPI*PS_DPI,148/MMPI*PS_DPI},
	{"iso","a6"    ,105/MMPI*PS_DPI,148/MMPI*PS_DPI},
	{"jis","b6"    ,128/MMPI*PS_DPI,182/MMPI*PS_DPI},
	{"jpn","oufuku",148/MMPI*PS_DPI,200/MMPI*PS_DPI},
	{"iso","a5"    ,148/MMPI*PS_DPI,210/MMPI*PS_DPI},
	{"jis","b5"    ,182/MMPI*PS_DPI,257/MMPI*PS_DPI},
	{"iso","a4"    ,210/MMPI*PS_DPI,297/MMPI*PS_DPI},
	{"na" ,"letter",     8.5*PS_DPI,      11*PS_DPI},/* 215.9x279.4 */
	{"na" ,"legal" ,     8.5*PS_DPI,      14*PS_DPI},/* 215.9x355.6 */
	{"jis","b4"    ,257/MMPI*PS_DPI,364/MMPI*PS_DPI},
	{"iso","a3"    ,297/MMPI*PS_DPI,420/MMPI*PS_DPI},
#else
#include "opvp_media.def"
#endif
	{NULL ,NULL    ,              0,              0}
};

/* ----- Driver definition ----- */

/* Driver procedures */
static	dev_proc_open_device(opvp_open);
static	dev_proc_open_device(oprp_open);
static	dev_proc_output_page(opvp_output_page);
static	dev_proc_print_page(oprp_print_page);
static	dev_proc_close_device(opvp_close);
static	dev_proc_get_params(opvp_get_params);
static	dev_proc_get_params(oprp_get_params);
static	dev_proc_put_params(opvp_put_params);
static	dev_proc_put_params(oprp_put_params);
static	dev_proc_fill_rectangle(opvp_fill_rectangle);
static	dev_proc_begin_image(opvp_begin_image);
static	image_enum_proc_plane_data(opvp_image_plane_data);
static	image_enum_proc_end_image(opvp_image_end_image);

gs_public_st_suffix_add0_final(
	st_device_opvp,
	gx_device_opvp,
	"gx_device_opvp",
	device_opvp_enum_ptrs,
	device_opvp_reloc_ptrs,
	gx_device_finalize,
	st_device_vector
);

#define	opvp_initial_values	\
	NULL,	/* *vectorDriver */\
	NULL,	/* *printerModel */\
	NULL,	/* *handle */\
	NULL,	/* (*OpenPrinter)() */\
	NULL,	/* *errorno */\
	-1,	/* outputFD */\
	0,	/* nApiEntry */\
	NULL,	/* *apiEntry */\
	-1,	/* printerContext */\
	NULL,	/* *jobInfo */\
	NULL	/* *docInfo */

/* device procs */
#define	opvp_procs \
{\
	opvp_open,\
	opvp_get_initial_matrix,\
	NULL,	/* sync_output */\
	opvp_output_page,\
	opvp_close,\
	opvp_map_rgb_color,\
	opvp_map_color_rgb,\
	opvp_fill_rectangle, /*gdev_vector_fill_rectangle,*/\
	NULL,	/* tile_rectangle OBSOLETE */\
	opvp_copy_mono,\
	opvp_copy_color,\
	NULL,	/* draw_line OBSOLETE */\
	NULL,	/* get_bits */\
	opvp_get_params,\
	opvp_put_params,\
	NULL,	/* map_cmyk_color */\
	NULL,	/* get_xfont_procs */\
	NULL,	/* get_xfont_device */\
	NULL,	/* map_rgb_alpha_color */\
	gx_page_device_get_page_device,\
	NULL,	/* get_alpha_bits OBSOLETE */\
	NULL,	/* copy_alpha */\
	NULL,	/* get_band */\
	NULL,	/* copy_rop */\
	opvp_fill_path,\
	opvp_stroke_path,\
	opvp_fill_mask,\
	gdev_vector_fill_trapezoid,\
	gdev_vector_fill_parallelogram,\
	gdev_vector_fill_triangle,\
	NULL,	/* draw_thin_line */\
	opvp_begin_image,\
	NULL,	/* image_data */\
	NULL,	/* end_image */\
	NULL,	/* strip_tile_rectangle */\
	NULL,	/* strip_copy_rop */\
	NULL,	/* get_clipping_box */\
	NULL,	/* begin_typed_image */\
        NULL,   /* get_bits_rectangle */\
        NULL,   /* map_color_rgb_alpha */\
        NULL,   /* create_compositor */\
        NULL,   /* get_hardware_params */\
        NULL,   /* text_begin */\
        NULL,   /* finish_copydevice */\
        NULL,   /* begin_transparency_group */\
        NULL,   /* end_transparency_group */\
        NULL,   /* begin_transparency_mask */\
        NULL,   /* end_transparency_mask */\
        NULL    /* discard_transparency_layer */\
}

/* vector procs */
static	gx_device_vector_procs	opvp_vector_procs =
{
	/* Page management */
	opvp_beginpage,
	/* Imager state */
	opvp_setlinewidth,
	opvp_setlinecap,
	opvp_setlinejoin,
	opvp_setmiterlimit,
	opvp_setdash,
	opvp_setflat,
	opvp_setlogop,
	/* Other state */
#if GS_VERSION_MAJOR >= 8
	opvp_can_handle_hl_color,		/* added for gs 8.15 */
#endif
	opvp_setfillcolor,
	opvp_setstrokecolor,
	/* Paths */
	opvp_vector_dopath,
	opvp_vector_dorect,
	opvp_beginpath,
	opvp_moveto,
	opvp_lineto,
	opvp_curveto,
	opvp_closepath,
	opvp_endpath
};

const	gx_device_opvp		gs_opvp_device =
{
	std_device_dci_type_body(
		gx_device_opvp,
		0,
		"opvp",
		&st_device_opvp,
		DEFAULT_WIDTH_10THS_A4  * X_DPI / 10,
		DEFAULT_HEIGHT_10THS_A4 * Y_DPI / 10,
		X_DPI,
		Y_DPI,
		3,
		24,
		255,
		255,
		256,
		256
	),
	opvp_procs
};

/* for inkjet */
static	gx_device_procs		prn_oprp_procs =
	prn_color_params_procs(
		oprp_open,
		opvp_output_page,
		opvp_close,
		opvp_map_rgb_color,
		opvp_map_color_rgb,
		oprp_get_params,
		oprp_put_params
	);

const	gx_device_oprp		gs_oprp_device =
{
	prn_device_std_margins_body(
		gx_device_oprp,
		prn_oprp_procs,
		"oprp",
		DEFAULT_WIDTH_10THS_A4,
		DEFAULT_HEIGHT_10THS_A4,
		X_DPI,
		Y_DPI,
		0, 0, 0, 0, 0, 0,
		24,
		oprp_print_page
	)
};

/* driver mode */
static	bool			vector = true;
static	bool			inkjet = false;
static	char			*vectorDriver = NULL;
static	char			*printerModel = NULL;
static	void			*handle = NULL;
static	int			(*OpenPrinter)(int,char*,int*,OPVP_api_procs**)
				    = NULL;
static	int			*errorno = NULL;
static	int			outputFD = -1;
static	int			nApiEntry = 0;
static	OPVP_api_procs		*apiEntry = NULL;
static	int			printerContext = -1;
static	char			*jobInfo = NULL;
static	char			*docInfo = NULL;
static	OPVP_ColorSpace		colorSpace = OPVP_cspaceStandardRGB;
static	OPVP_Brush		*vectorFillColor = NULL;
static	float			margins[4] = {0, 0, 0, 0};
static	float			zoom[2] = {1, 1};
static	float			shift[2] = {0, 0};
static	bool			zoomAuto = false;
static	bool			zooming = false;
static  bool			beginPage = false;

/* for image */
static	const
gx_image_enum_procs_t	opvp_image_enum_procs =
{
	opvp_image_plane_data,
	opvp_image_end_image
};
typedef	enum	_FastImageSupportMode {
	FastImageDisable,
	FastImageNoCTM,
	FastImageNoRotate,
	FastImageRightAngle,
	FastImageReverseAngle,
	FastImageAll
} FastImageSupportMode;
static	char			*fastImage = NULL;
static	FastImageSupportMode	FastImageMode = FastImageDisable;
static	bool			begin_image = false;
static	gs_color_space_index	color_index = 0;
static	byte			palette[3*256];
static  float                   imageDecode[GS_IMAGE_MAX_COMPONENTS * 2];
static	bool			reverse_image = false;

/* added for image gamma correction */
typedef struct bbox_image_enum_s {
							gx_image_enum_common;
	gs_memory_t				*memory;
	gs_matrix				matrix;		/* map from image space to device dpace */
	const	gx_clip_path	*pcpath;
	gx_image_enum_common_t	*target_info;
	bool					params_are_const;
	int						x0, x1;
	int						y, height;
} bbox_image_enum;

/* The following is already defined in stdpre.h */
/*#define	min(a, b) (((a) < (b))? (a) : (b))*/


/* ----- Utilities ----- */
static	int
opvp_startpage(
	gx_device		*dev
	)
{
	int			ecode = 0;
	int			code = -1;
static	char			*page_info = NULL;

	/* page info */
	page_info = opvp_alloc_string(&page_info, OPVP_INFO_PREFIX);
	page_info = opvp_cat_string(&page_info, opvp_gen_page_info(dev));

	/* call StartPage */
	if (printerContext != -1) {
		if (apiEntry->StartPage)
		code = apiEntry->StartPage(printerContext,
		                           opvp_to_utf8(page_info));
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	return ecode;
}

static	int
opvp_endpage(
	void
	)
{
	int			ecode = 0;
	int			code = -1;

	/* call EndPage */
	if (printerContext != -1) {
		if (apiEntry->EndPage)
		code = apiEntry->EndPage(printerContext);
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	return ecode;
}

static	char *
opvp_alloc_string(
	char			**destin,
const	char			*source
	)
{
	if (!destin) return NULL;

	if (*destin) {
		if (source) {
			*destin = realloc(*destin, strlen(source)+1);
		} else {
			free(*destin);
			*destin = NULL;
		}
	} else {
		if (source) {
			*destin = malloc(strlen(source)+1);
		}
	}
	if (*destin && source) {
		if (*destin != source) {
			strcpy(*destin, source);
		}
	}

	return *destin;
}

static	char *
opvp_cat_string(
	char			**destin,
const	char			*string
	)
{
	if (!destin) return NULL;
	if (!(*destin)) return opvp_alloc_string(destin, string);

	if (string) {
		*destin = realloc(*destin, strlen(*destin)+strlen(string)+1);
		strcat(*destin, string);
	}

	return *destin;
}

static	char *
opvp_adjust_num_string(
	char			*num_string
	)
{
	char			*pp;
	char			*lp;

	if (!num_string) return NULL;

	if ((pp = strrchr(num_string, '.'))) {
		for (lp = &(num_string[strlen(num_string)-1]); lp > pp; lp--) {
			if (*lp == '0') {
				*lp = '\0';
			} else {
				break;
			}
		}
		if (lp == pp) *lp = '\0';
	}

	return num_string;
}

static	char **
opvp_gen_dynamic_lib_name(
	void
	)
{
static	char			*buff[5] = {NULL,NULL,NULL,NULL,NULL};
	char			tbuff[OPVP_BUFF_SIZE];

	if (!vectorDriver) {
		return NULL;
	}

	memset((void*)tbuff, 0, OPVP_BUFF_SIZE);
	strncpy(tbuff, vectorDriver, OPVP_BUFF_SIZE - 1);
	opvp_alloc_string(&(buff[0]), tbuff);

	memset((void*)tbuff, 0, OPVP_BUFF_SIZE);
	strncpy(tbuff, vectorDriver, OPVP_BUFF_SIZE - 4);
	strcat(tbuff, ".so");
	opvp_alloc_string(&(buff[1]), tbuff);

	memset((void*)tbuff, 0, OPVP_BUFF_SIZE);
	strncpy(tbuff, vectorDriver, OPVP_BUFF_SIZE - 5);
	strcat(tbuff, ".dll");
	opvp_alloc_string(&(buff[2]), tbuff);

	memset((void*)tbuff, 0, OPVP_BUFF_SIZE);
	strcpy(tbuff, "lib");
	strncat(tbuff, vectorDriver, OPVP_BUFF_SIZE - 7);
	strcat(tbuff, ".so");
	opvp_alloc_string(&(buff[3]), tbuff);

	buff[4] = NULL;

	return buff;
}

static	char *
opvp_to_utf8(
	char			*string
	)
{
	char			*locale;
	iconv_t			cd;
	char			*buff = NULL;
	size_t			ib, ob;
	int			complete = false;
	char			*ibuff, *obuff;
	char			*ostring = NULL;

	if (string) {
		ib = strlen(string);
		if (ib > 0) {
			ob = ib * 4;
			buff = malloc(ob+1);
			setlocale(LC_CTYPE, "");
#ifdef CODESET
			locale = nl_langinfo(CODESET);
#else
			locale = "UTF-8";
#endif /* CODESET */
			if (locale) {
				if (strcmp(locale, "C") && buff) {
					if ((cd = iconv_open("UTF-8", locale))
					     != (iconv_t)-1) {
						ibuff = string;
						obuff = buff;
						if (iconv(cd, &ibuff, &ib,
						          &obuff, &ob)
						    != -1) {
							*obuff = 0;
							complete = true;
						}
						iconv_close(cd);
					}
				}
			}
		}
	}

	if (complete) {
		ostring = opvp_alloc_string(&ostring, buff);
	} else {
		ostring = string;
	}

	if (buff) free(buff);
	return ostring;
}

static float
opvp_fabsf(float f)
{
	return (float)fabs((double)f);
}

static	int
opvp_get_papertable_index(
	gx_device		*pdev
	)
{
	int			i;
	float			width, height;
	bool			landscape;
	float			paper_w, paper_h;
	float			prev = -1;
	int			paper = -1;
	int			candidate = -1;
	int			smaller = -1;
	int			larger  = -1;
	int			s_candi = -1;
	int			l_candi = -1;
	float			h_delta = TOLERANCE;
	float			sw_delta = TOLERANCE;
	float			sh_delta = TOLERANCE;
	float			lw_delta = TOLERANCE;
	float			lh_delta = TOLERANCE;
	bool			match = false;
	float			f;

	/* portrait or landscape */
	landscape = (pdev->MediaSize[0] < pdev->MediaSize[1] ? false : true);

	/* paper size */
	width  = (landscape ? pdev->MediaSize[1] : pdev->MediaSize[0]);
	height = (landscape ? pdev->MediaSize[0] : pdev->MediaSize[1]);

#if 0
	for (i=0; paperTable[i].name != NULL; i++) {
		if ((width  <= ceilf(paperTable[i].width)) &&
		    (height <= ceilf(paperTable[i].height))) {
			break;
		}
	}
	return i;
#endif

	for (i=0; paperTable[i].name != NULL; i++) {
		paper_w = paperTable[i].width;
		paper_h = paperTable[i].height;
		if (width == paper_w) {
			if (height == paper_h) {
				paper = i;
				match = true;
				break;
			} else if ((f = opvp_fabsf(height - paper_h)) < TOLERANCE) {
				if (f < h_delta) {
					h_delta = f;
					candidate = i;
				}
			}
		} else if (candidate != -1) {
			paper = candidate;
			match = true;
			break;
		} else if (prev != paper_w) {
			prev = paper_w;
			if (paper_w < width) {
				if ((f = opvp_fabsf(width - paper_w)) < TOLERANCE) {
					if (f < sw_delta) {
						sw_delta = f;
						smaller  = i;
					}
				}
			} else {
				if ((f = opvp_fabsf(width - paper_w)) < TOLERANCE) {
					if (f < lw_delta) {
						lw_delta = f;
						larger   = i;
					}
				}
			}
		}
	}
	if (!match) {
		paper = i;
		if (smaller != -1) {
			paper_w = paperTable[smaller].width;
			for (i = smaller; paperTable[i].width == paper_w; i++) {
				paper_h = paperTable[i].height;
				if (height == paper_h) {
					sh_delta = 0;
					s_candi  = i;
					break;
				} else if ((f = opvp_fabsf(height - paper_h))
				           < TOLERANCE) {
					if (f < sh_delta) {
						sh_delta = f;
						s_candi  = i;
					}
				}
			}
		}
		if (larger != -1) {
			paper_w = paperTable[larger].width;
			for (i = larger; paperTable[i].width == paper_w; i++) {
				paper_h = paperTable[i].height;
				if (height == paper_h) {
					lh_delta = 0;
					l_candi  = i;
					break;
				} else if ((f = opvp_fabsf(height - paper_h))
				           < TOLERANCE) {
					if (f < lh_delta) {
						lh_delta = f;
						l_candi  = i;
					}
				}
			}
		}
		if (s_candi != -1) {
			if (l_candi != -1) {
				if ((sw_delta + sh_delta)
				  < (lw_delta + lh_delta)) {
					paper = s_candi;
				} else {
					paper = l_candi;
				}
			} else {
				paper = s_candi;
			}
		} else {
			if (l_candi != -1) {
				paper = l_candi;
			}
		}
	}

	return paper;
}

static	char *
opvp_get_sizestring(
	float			width,
	float			height
	)
{
	char nbuff[OPVP_BUFF_SIZE];
	char nbuff1[OPVP_BUFF_SIZE / 2];
	char nbuff2[OPVP_BUFF_SIZE / 2];
static	char			*buff = NULL;

	memset((void*)nbuff, 0, OPVP_BUFF_SIZE);
	memset((void*)nbuff1, 0, OPVP_BUFF_SIZE / 2);
	memset((void*)nbuff2, 0, OPVP_BUFF_SIZE / 2);

	snprintf(nbuff1, OPVP_BUFF_SIZE / 2 - 1, "%.3f", width);
	snprintf(nbuff2, OPVP_BUFF_SIZE / 2 - 1, "%.3f", height);
	snprintf(nbuff, OPVP_BUFF_SIZE - 1, "%sx%s",
	                        opvp_adjust_num_string(nbuff1),
	                        opvp_adjust_num_string(nbuff2));

	return opvp_alloc_string(&buff, nbuff);
}

/* not used
static	const	char *
opvp_get_papersize_region(
	gx_device		*pdev
	)
{
	return paperTable[opvp_get_papertable_index(pdev)].region;
}
*/

/* not used
static	const	char *
opvp_get_papersize_name(
	gx_device		*pdev
	)
{
	return paperTable[opvp_get_papertable_index(pdev)].name;
}
*/

/* not used
static	char *
opvp_get_papersize_inch(
	gx_device		*pdev
	)
{
	bool			landscape;
	float			width, height;

	landscape = (pdev->MediaSize[0] < pdev->MediaSize[1] ? false : true);
	width  = (landscape ? pdev->MediaSize[1] : pdev->MediaSize[0]) / PS_DPI;
	height = (landscape ? pdev->MediaSize[0] : pdev->MediaSize[1]) / PS_DPI;

	return opvp_get_sizestring(width, height);
}
*/

/* not used
static	const	char *
opvp_get_papersize(
	gx_device		*pdev
	)
{
const	char			*paper;

	paper = opvp_get_papersize_name(pdev);
	if (!paper) paper = opvp_get_papersize_inch(pdev);

	return paper;
}
*/

static	char *
opvp_get_mediasize(
	gx_device		*pdev
	)
{
	int			i;
	char			wbuff[OPVP_BUFF_SIZE];
static	char			*buff = NULL;
const	char			*region;
const	char			*name;
	float			width;
	float			height;
const	char			*unit;
	bool			landscape;

	i = opvp_get_papertable_index(pdev);

	if (paperTable[i].name) {
		region = paperTable[i].region;
		name   = paperTable[i].name;
		width  = paperTable[i].width  / PS_DPI;
		height = paperTable[i].height / PS_DPI;
		if((strcmp(region, "na"  ) == 0) ||
		   (strcmp(region, "asme") == 0) ||
		   (strcmp(region, "roc" ) == 0) ||
		   (strcmp(region, "oe"  ) == 0)) {
			unit    = "in";
		} else {
			width  *= MMPI;
			height *= MMPI;
			unit    = "mm";
		}
	} else {
		landscape = (pdev->MediaSize[0] < pdev->MediaSize[1] ?
		             false : true);
		region = "custom";
		name   = "opvp";
		width  = (landscape ? pdev->MediaSize[1] : pdev->MediaSize[0])
		       / PS_DPI;
		height = (landscape ? pdev->MediaSize[0] : pdev->MediaSize[1])
		       / PS_DPI;
		unit   = "in";
	}
	memset((void*)wbuff, 0, OPVP_BUFF_SIZE);
	snprintf(wbuff, OPVP_BUFF_SIZE - 1, "%s_%s_%s%s", region, name,
				     opvp_get_sizestring(width, height),
				     unit);
	buff = opvp_alloc_string(&buff, wbuff);

	return buff;
}

static	char *
opvp_gen_page_info(
	gx_device		*dev
	)
{
static	char			*buff = NULL;
	int num_copies = 1;
	bool landscape;
	char tbuff[OPVP_BUFF_SIZE];

	/* copies */
	if (!inkjet) {
		if (dev->IgnoreNumCopies) {
			num_copies = 1;
		} else if (dev->NumCopies_set > 0) {
			num_copies = dev->NumCopies;
		}
	}

	landscape = (dev->MediaSize[0] < dev->MediaSize[1] ? false
	                                                   : true);
	memset((void*)tbuff, 0, OPVP_BUFF_SIZE);
	snprintf(tbuff, OPVP_BUFF_SIZE - 1,
	"MediaCopy=%d;DeviceResolution=deviceResolution_%s;MediaPageRotation=%s;MediaSize=%s",
	num_copies,
	opvp_get_sizestring(dev->x_pixels_per_inch, dev->y_pixels_per_inch),
	(landscape ? "landscape" : "portrait"),
	opvp_get_mediasize(dev));

	opvp_alloc_string(&buff, tbuff);

	return buff;
}

static	char *
opvp_gen_doc_info(
	gx_device		*dev
	)
{
	return opvp_gen_page_info(dev);
}

static	char *
opvp_gen_job_info(
	gx_device		*dev
	)
{
	return opvp_gen_doc_info(dev);
}

static	int
opvp_set_brush_color(
	gx_device_opvp		*pdev,
	gx_color_index		color,
	OPVP_Brush		*brush
	)
{
	int			code;
	int			ecode = 0;
	gx_color_value		rgb[3];

	code = opvp_map_color_rgb((gx_device *)pdev, color, rgb);
	if (code) {
		ecode = -1;
	} else {
#if ENABLE_SIMPLE_MODE
		brush->colorSpace = colorSpace;
#else
		/* call GetColorSpace */
		if (apiEntry->GetColorSpace)
		code = apiEntry->GetColorSpace(printerContext,
		                               &(brush->colorSpace));
		if (code != OPVP_OK) {
			brush->colorSpace = OPVP_cspaceDeviceRGB;
		}
#endif
		brush->pbrush = NULL;
		brush->xorg = brush->yorg = 0;
		brush->color[3] = (color == gx_no_color_index ? -1 : 0);
		brush->color[2] = rgb[0];
		brush->color[1] = rgb[1];
		brush->color[0] = rgb[2];
	}

	return ecode;
}

static	int
opvp_draw_image(
	gx_device_opvp		*pdev,
	int			depth,
	int			sw,
	int			sh,
	int			dw,
	int			dh,
	int			raster,
/*const*/
	byte			*data
	)
{
	int			code = -1;
	int			ecode = 0;
	int			count;
	OPVP_Rectangle		rect;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* image size */
/*	count = (((((sw * depth + 7) >> 3) + 3) >> 2) << 2) * sh; */
	count = raster * sh;
	OPVP_i2Fix(0, rect.p0.x);
	OPVP_i2Fix(0, rect.p0.y);
	OPVP_i2Fix(dw, rect.p1.x);
	OPVP_i2Fix(dh, rect.p1.y);

	/* call DrawImage */
	if (apiEntry->DrawImage)
	code = apiEntry->DrawImage(printerContext,
	                           sw,
	                           sh,
	                           depth,
	                           OPVP_iformatRaw,
	                           rect,
	                           count,
	                           (void *)data);
	if (code != OPVP_OK) {

		/* call StartDrawImage */
		if (apiEntry->StartDrawImage)
		code = apiEntry->StartDrawImage(printerContext,
		                                sw,
		                                sh,
		                                depth,
		                                OPVP_iformatRaw,
		                                rect);
		if (code == OPVP_OK) {

			/* call TansferDrawImage */
			if (apiEntry->TransferDrawImage)
			code = apiEntry->TransferDrawImage(printerContext,
			                                   count,
			                                   (void *)data);
			if (code != OPVP_OK) ecode = -1;

			/* call EndDrawImage */
			if (apiEntry->EndDrawImage)
			apiEntry->EndDrawImage(printerContext);

		} else {
			/* ecode = -1;*/
			ecode = 0;	/* continue... */
		}
	}

	return ecode;
}

/* ----- load/unload vector driver ----- */

/*
 * load vector-driver
 */
static	int
opvp_load_vector_driver(
	void
	)
{
	char			**list = NULL;
	int			i;
	void			*h;

	if (handle) {
		opvp_unload_vector_driver();
	}

	if (vectorDriver) {
		list = opvp_gen_dynamic_lib_name();
	}

	if (list) {
		i = 0;
		while (list[i]) {
			if ((h = dlopen(list[i],RTLD_NOW))) {
				OpenPrinter = dlsym(h,"OpenPrinter");
				errorno = dlsym(h,"errorno");
				if (OpenPrinter && errorno) {
					handle = h;
					break;
				}
				OpenPrinter = NULL;
				errorno = NULL;
			}
			i++;
		}
	}

	if (handle) {
		return 0;
	} else {
		return -1;
	}
}

/*
 * unload vector-driver
 */
static	int
opvp_unload_vector_driver(
	void
	)
{
	if (handle) {
		dlclose(handle);
		handle = NULL;
		OpenPrinter = NULL;
		errorno = NULL;
	}
	return 0;
}

/*
 * prepare open
 */
static	int
prepare_open(
	gx_device		*dev
	)
{
	int			ecode = 0;
	int			code = -1;
	OPVP_api_procs		*api_entry;
	int			dumFD = -1;
	int			dumContext = -1;
	OPVP_ColorSpace		cspace = OPVP_cspaceStandardRGB;

	/* open dummy device */
	code = open("/dev/null", O_RDWR);
	if (code < 0) ecode = code;
	else dumFD = code;

	/* load vector driver */
	if (!ecode) {
		if ((code = opvp_load_vector_driver())) {
			ecode = code;
		}
	}

	/* prepare array of function pointer for PDAPI */
	if (!ecode) {
		if (!apiEntry) {
			if (!(apiEntry = calloc(sizeof(OPVP_api_procs), 1))) {
				ecode = -1;
			}
		} else {
			memset(apiEntry, 0, sizeof(OPVP_api_procs));
		}
	}

	/* call OpenPrinter as dummy */
	if (!ecode) {
		code = OpenPrinter(dumFD, printerModel,
				   &nApiEntry, &api_entry);
		if (code == -1) ecode = code;
		else dumContext = code;
	}

	/* set apiEntry */
	if (!ecode) {
		if (nApiEntry > sizeof(OPVP_api_procs)/sizeof(void *)) {
			nApiEntry = sizeof(OPVP_api_procs)/sizeof(void *);
		}
		memcpy(apiEntry, api_entry, nApiEntry*sizeof(void *));
	} else {
		if (apiEntry) free(apiEntry);
		apiEntry = NULL;
	}

	/* check vector fucntion */
	if (apiEntry) {
		if (!inkjet) {
			if (!(apiEntry->NewPath) ||
			    !(apiEntry->EndPath) ||
			    !(apiEntry->StrokePath) ||
			    !(apiEntry->SetCurrentPoint) ||
			    !(apiEntry->LinePath) ||
			    !(apiEntry->BezierPath)) {
				/* NOT avail vector drawing mode */
				vector = false;
			}
		}
		/* call GetColorSpace */
		if (apiEntry->GetColorSpace)
		code = apiEntry->GetColorSpace(dumContext, &cspace);
		if (cspace == OPVP_cspaceBW) {
			/* mono-color */
			colorSpace = cspace;
			dev->color_info.num_components = 1;
			dev->color_info.depth = 1;
			dev->color_info.max_gray = 0;
			dev->color_info.max_color = 0;
			dev->color_info.dither_grays = 1;
			dev->color_info.dither_colors = 1;
		} else if (cspace == OPVP_cspaceDeviceGray) {
			/* gray-scale */
			colorSpace = cspace;
			dev->color_info.num_components = 1;
			dev->color_info.depth = 8;
			dev->color_info.max_gray = 255;
			dev->color_info.max_color = 255;
			dev->color_info.dither_grays = 256;
			dev->color_info.dither_colors = 256;
		} else {
			/* rgb color */
			colorSpace = OPVP_cspaceStandardRGB;
			dev->color_info.num_components = 3;
			dev->color_info.depth = 24;
			dev->color_info.max_gray = 255;
			dev->color_info.max_color = 255;
			dev->color_info.dither_grays = 256;
			dev->color_info.dither_colors = 256;
		}
#if GS_VERSION_MAJOR >= 8
		dev->procs.get_color_mapping_procs = NULL;
		dev->procs.get_color_comp_index = NULL;
		gx_device_fill_in_procs(dev);
#endif
	}

	/* call Closerinter as dummy */
	if (dumContext != -1) {
		/* call ClosePrinter */
		if (apiEntry->ClosePrinter)
		apiEntry->ClosePrinter(dumContext);
		dumContext = -1;
	}

	/* close device for dummy */
	if (dumFD != -1) {
		close(dumFD);
		dumFD = -1;
	}

	/* un-load vector driver */
	opvp_unload_vector_driver();

	return ecode;
}

/* ----- driver procs ----- */
/*
 * open device
 */
static	int
opvp_open(
	gx_device		*dev
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	gx_device_oprp		*rdev = (gx_device_oprp *)dev;
	int			ecode = 0;
	int			code;
	OPVP_api_procs		*api_entry;
	char			*job_info = NULL;
	char			*doc_info = NULL;
	char			*tmp_info = NULL;
	float			margin_width = 0;
	float			margin_height = 0;
	float			adj_margins[4];

	/* prepare open : load and open as dummy */
	code = prepare_open(dev);
	if (code) {
		ecode = code;
		return ecode;
	}

	/* set margins */
	if (zoomAuto) {
		margin_width = (margins[0] + margins[2])
		             * dev->HWResolution[0];
		margin_height = (margins[1] + margins[3])
		              * dev->HWResolution[1];
		zoom[0] = (dev->width - margin_width) / dev->width;
		zoom[1] = (dev->height - margin_height) / dev->height;
		if (zoom[0] < zoom[1]) zoom[1] = zoom[0];
		else zoom[0] = zoom[1];
	}
	if (inkjet) {
		if ((margins[0] != 0) ||
		    (margins[1] != 0) || (margins[3] != 0)) {
			shift[0] = margins[0] * dev->HWResolution[0];
			shift[1] = (margins[1] + margins[3])
				 * dev->HWResolution[1];
			zooming = true;
		}
		dev->width -= margins[2] * dev->HWResolution[0];
		dev->height -= margins[1] * dev->HWResolution[1];
	} else {
		if ((margins[0] != 0) || (margins[1] != 0)) {
			shift[0] = margins[0] * dev->HWResolution[0];
			shift[1] = margins[3] * dev->HWResolution[1];
			zooming = true;
		}
		adj_margins[0] = 0;
		adj_margins[3] = 0;
		adj_margins[1] = dev->height * zoom[1] / dev->HWResolution[1]
		                - (dev->MediaSize[1] / PS_DPI
		                 - (margins[1] + margins[3]));
		if (adj_margins[1] < 0) adj_margins[0] = 0;
		adj_margins[2] = dev->width * zoom[0] / dev->HWResolution[0]
		                - (dev->MediaSize[0] / PS_DPI
		                 - (margins[0] + margins[2]));
		if (adj_margins[2] < 0) adj_margins[2] = 0;
		gx_device_set_margins(dev, adj_margins, true);
	}
	if ((zoom[0] != 1) || (zoom[1] != 1)) zooming = true;

	/* open file for output device */
	if (!inkjet) {
		pdev->v_memory = gs_memory_stable(pdev->memory);
		/* open output stream */
		code = gdev_vector_open_file_options((gx_device_vector*)dev,
		           512,
		           (VECTOR_OPEN_FILE_SEQUENTIAL
		           |VECTOR_OPEN_FILE_BBOX
		           ));
		if (code < 0) {
			ecode = code;
			return ecode;
		}
#if GS_VERSION_MAJOR >= 8
		if (pdev->bbox_device != NULL) {
			if (pdev->bbox_device->memory == NULL)
				pdev->bbox_device->memory = gs_memory_stable(dev->memory);
		}
#endif
		outputFD = fileno(pdev->file);
	} else {
		/* open printer device */
		code = gdev_prn_open(dev);
		if (code < 0) {
			ecode = ecode;
			return ecode;
		}
		/* open output stream */
		code = gdev_prn_open_printer_seekable(dev, true, false);
		if (code < 0) {
			ecode = code;
			return ecode;
		}
		outputFD = fileno(rdev->file);
	}

	/* RE-load vector driver */
	if ((code = opvp_load_vector_driver())) {
		ecode = code;
		return ecode;
	}

	/* call OpenPrinter */
	code = OpenPrinter(outputFD, printerModel,
	                   &nApiEntry, &api_entry);
	if (!apiEntry) {
		if (!(apiEntry = calloc(sizeof(OPVP_api_procs), 1))) {
			ecode = -1;
		}
	} else {
		memset(apiEntry, 0, sizeof(OPVP_api_procs));
	}
	if (code == -1) {
		ecode =  code;
		if (apiEntry) free(apiEntry);
		apiEntry = NULL;
		opvp_unload_vector_driver();
		if (inkjet) gdev_prn_close(dev);
		else gdev_vector_close_file((gx_device_vector *)pdev);
		return ecode;
	}
	printerContext = code;
	if (nApiEntry > sizeof(OPVP_api_procs)/sizeof(void *)) {
		nApiEntry = sizeof(OPVP_api_procs)/sizeof(void *);
	}
	memcpy(apiEntry, api_entry, nApiEntry*sizeof(void *));

	/* call SetColorSpace */
	if (!ecode) {
		if (apiEntry->SetColorSpace)
		apiEntry->SetColorSpace(printerContext, colorSpace);
	}

	/* initialize */
	if ((!ecode) && (!inkjet)) {
		pdev->vec_procs = &opvp_vector_procs;
		if (vector) gdev_vector_init((gx_device_vector *)pdev);
	}

	/* start job */
	if (!ecode) {
		/* job info */
		if (jobInfo) {
			if (strlen(jobInfo) > 0) {
				job_info = opvp_alloc_string(&job_info,
				                             jobInfo);
			}
		}
		tmp_info = opvp_alloc_string(&tmp_info,
		                             opvp_gen_job_info(dev));
		if (tmp_info) {
			if (strlen(tmp_info) > 0) {
				if (job_info) {
					if (strlen(job_info) > 0) {
						opvp_cat_string(&job_info, ";");
					}
				}
				job_info = opvp_cat_string(&job_info,
				                           OPVP_INFO_PREFIX);
				job_info = opvp_cat_string(&job_info, tmp_info);
			}
		}

		/* call StartJob */
		if (apiEntry->StartJob)
		code = apiEntry->StartJob(printerContext,
		                          opvp_to_utf8(job_info));
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	/* start doc */
	if (!ecode) {
		/* doc info */
		if (docInfo) {
			if (strlen(docInfo) > 0) {
				doc_info = opvp_alloc_string(&doc_info,
					                     docInfo);
			}
		}
		tmp_info = opvp_alloc_string(&tmp_info,
		                             opvp_gen_doc_info(dev));
		if (tmp_info) {
			if (strlen(tmp_info) > 0) {
				if (doc_info) {
					if (strlen(doc_info) > 0) {
						opvp_cat_string(&doc_info, ";");
					}
				}
				doc_info = opvp_cat_string(&doc_info,
				                           OPVP_INFO_PREFIX);
				doc_info = opvp_cat_string(&doc_info, tmp_info);
			}
		}

		/* call StartDoc */
		if (apiEntry->StartDoc)
		code = apiEntry->StartDoc(printerContext,
		                          opvp_to_utf8(doc_info));
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	if (tmp_info) opvp_alloc_string(&tmp_info, NULL);
	if (doc_info) opvp_alloc_string(&doc_info, NULL);
	if (job_info) opvp_alloc_string(&job_info, NULL);

	return ecode;
}

/*
 * open device for inkjet
 */
static	int
oprp_open(
	gx_device		*dev
	)
{
	/* set inkjet mode */
	vector = false;
	inkjet = true;

	/* matrix */
	dev->procs.get_initial_matrix = opvp_get_initial_matrix;
	return opvp_open(dev);
}

/*
 * get initial matrix
 */
static	void
opvp_get_initial_matrix(
	gx_device		*dev,
	gs_matrix		*pmat
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	OPVP_CTM		omat;

	gx_default_get_initial_matrix(dev,pmat);
	if (zooming) {
		/* gs matrix */
		pmat->xx *= zoom[0];
		pmat->xy *= zoom[1];
		pmat->yx *= zoom[0];
		pmat->yy *= zoom[1];
		pmat->tx = pmat->tx * zoom[0] + shift[0];
		pmat->ty = pmat->ty * zoom[1] + shift[1];
	}

	if (pdev->is_open) {

		/* call ResetCTM */
		if (apiEntry->ResetCTM)
		apiEntry->ResetCTM(printerContext);
		else {
			/* call SetCTM */
			omat.a = 1;
			omat.b = 0;
			omat.c = 0;
			omat.d = 1;
			omat.e = 0;
			omat.f = 0;
			if (apiEntry->SetCTM)
			apiEntry->SetCTM(printerContext, &omat);
		}
	}

	return;
}

/*
 * output page
 */
static	int
opvp_output_page(
	gx_device		*dev,
	int			num_copies,
	int			flush
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	int			ecode = 0;
	int			code = OPVP_OK;

	if (inkjet) return gdev_prn_output_page(dev, num_copies, flush);

#ifdef OPVP_IGNORE_BLANK_PAGE
	if (pdev->in_page) {
#else
	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;
#endif
		/* end page */
		code = opvp_endpage();
		if (code) ecode = code;

		pdev->in_page = false;
		beginPage = false;
#ifdef OPVP_IGNORE_BLANK_PAGE
	}
#endif

	if (vector) {
		gdev_vector_reset((gx_device_vector *)pdev);
	}

	code = gx_finish_output_page(dev, num_copies, flush);
	if (code) ecode = code;

	return ecode;
}

/*
 * print page
 */
static	int
oprp_print_page(
	gx_device_printer	*pdev,
	FILE			*prn_stream
	)
{
	int			ecode = 0;
	int			code = -1;
	int			raster_size;
	int			buff_size;
	byte			*buff = NULL;
	int			line;
	int			scan_lines;
	byte			*data;
	int			rasterWidth;
	bool			start_page = false;
	bool			start_raster = false;
#if ENABLE_SKIP_RASTER
	int			i;
	byte			check;
#endif

	/* get raster/pixel size */
	raster_size = gx_device_raster((gx_device *)pdev, 0);
	buff_size = ((raster_size + 3) >> 2) << 2;
	scan_lines = dev_print_scan_lines(pdev);
	rasterWidth = pdev->width;

	/* allocate buffer */
	buff = (byte*)gs_malloc(gs_lib_ctx_get_non_gc_memory_t(), 1, buff_size, "oprp_print_page(buff)");
	if (!buff) return ecode = -1;

	/* start page */
	if (!ecode) {
		code = opvp_startpage((gx_device *)pdev);
		if (code) ecode = code;
		else start_page = true;
	}

	/* moveto origin */
	if (!ecode)
	opvp_moveto((gx_device_vector*)pdev, 0, 0, 0, 0, 0);

	/* call StartRaster */
	if (!ecode) {
		if (apiEntry->StartRaster)
		code = apiEntry->StartRaster(printerContext, rasterWidth);
		if (code != OPVP_OK) ecode = code;
		else start_raster = true;
	}

	/* line */
	for (line = 0; (line < scan_lines) && (!ecode); line++) {
		/* get raster data */
		if (!ecode) {
			code = gdev_prn_get_bits(pdev, line, buff, &data);
			if (code) {
				ecode = code;
				break;
			}
		}
#if ENABLE_SKIP_RASTER
		/* check support SkipRaster */
		if (apiEntry->SkipRaster) {
			/* check all white */
			if (pdev->color_info.depth > 8) {
				for (check = 0xff, i = 0; i < raster_size; i++)
				{
					check &= data[i];
					if (check != 0xff) break;
				}
				/* if all white call SkipRaster */
				if (check == 0xff) {
					code = apiEntry->SkipRaster(
					           printerContext, 1);
					if (code == OPVP_OK) continue;
				}
			} else {
				for (check = 0, i = 0; i < raster_size; i++)
				{
					check |= data[i];
					if (check) break;
				}
				/* if all zero call SkipRaster */
				if (check) {
					code = apiEntry->SkipRaster(
					           printerContext, 1);
					if (code == OPVP_OK) continue;
				}
			}
		}
#endif
		/* call TransferRasterData */
		if (!ecode) {
			if (apiEntry->TransferRasterData)
			code = apiEntry->TransferRasterData(printerContext,
			                                    raster_size,
			                                    data);
			if (code != OPVP_OK) ecode = code;
		}
	}

	/* call StartRaster */
	if (start_raster) {
		if (apiEntry->EndRaster)
		code = apiEntry->EndRaster(printerContext);
		if (code != OPVP_OK) ecode = code;
		start_raster = false;
	}

	/* end page */
	if (start_page) {
		code = opvp_endpage();
		if (code) ecode = code;
		start_page = false;
	}

	/* free buffer */
	if (buff) {
		gs_free(gs_lib_ctx_get_non_gc_memory_t(), (char*)buff, 1, buff_size, "oprp_print_page(buff)");
		buff = NULL;
	}

	return ecode;
}

/*
 * close device
 */
static	int
opvp_close(
	gx_device		*dev
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	int			ecode = 0;

	/* finalize */
	if (printerContext != -1) {

		/* call EndDoc */
		if (apiEntry->EndDoc)
		apiEntry->EndDoc(printerContext);

		/* call EndJob */
		if (apiEntry->EndJob)
		apiEntry->EndJob(printerContext);

		/* call ClosePrinter */
		if (apiEntry->ClosePrinter)
		apiEntry->ClosePrinter(printerContext);
		printerContext = -1;
	}

	/* unload vector driver */
	if (apiEntry) free(apiEntry);
	apiEntry = NULL;
	opvp_unload_vector_driver();

	if (inkjet) {
		/* close printer */
		gdev_prn_close(dev);
	} else {
		/* close output stream */
		gdev_vector_close_file((gx_device_vector *)pdev);
	}
	outputFD = -1;

	return ecode;
}

/*
 * map rgb color
 */
#if GS_VERSION_MAJOR >= 8
static	gx_color_index
opvp_map_rgb_color(
	gx_device		*dev,
	gx_color_value	*prgb		/* modified for gs 8.15 */
	)
#else
static	gx_color_index
opvp_map_rgb_color(
	gx_device		*dev,
	gx_color_value		r,
	gx_color_value		g,
	gx_color_value		b
	)
#endif
{
	OPVP_ColorSpace		cs;
	uint				c, m, y, k;

#if !(ENABLE_SIMPLE_MODE)
	gx_device_opvp		*pdev;
	int				code;
#endif

#if GS_VERSION_MAJOR >= 8
	gx_color_value	r, g, b;	/* added for gs 8.15 */
	r = prgb[0];
	g = prgb[1];
	b = prgb[2];
#endif

#if !(ENABLE_SIMPLE_MODE)
	pdev = (gx_device_opvp *)dev;
	code = -1;
#endif
	cs = OPVP_cspaceStandardRGB;

#if ENABLE_SIMPLE_MODE
	cs = colorSpace;
#else
	if (pdev->is_open) {
		/* call GetColorSpace */
		if (apiEntry->GetColorSpace)
		code = apiEntry->GetColorSpace(printerContext, &cs);
		if (code != OPVP_OK) {
			if (pdev->color_info.depth > 32) {
				cs = OPVP_cspaceStandardRGB64;
			} else if (pdev->color_info.depth > 8 ) {
				cs = OPVP_cspaceStandardRGB;
			} else if (pdev->color_info.depth > 1 ) {
				cs = OPVP_cspaceDeviceGray;
			} else {
				cs = OPVP_cspaceBW;
			}
		}
	}
#endif

	switch (cs) {
		case	OPVP_cspaceStandardRGB64 :
			/* unsupported */
			if (sizeof(gx_color_index) >= 6) {
				return (long long)b
				     + ((long long)g << 16)
				     + ((long long)b << 32);
			} else {
				return gx_color_value_to_byte(b)
				     + ((uint)gx_color_value_to_byte(g) << 8)
				     + ((ulong)gx_color_value_to_byte(r) << 16);
			}
			break;
		case	OPVP_cspaceDeviceCMYK :
		case	OPVP_cspaceDeviceCMY :
			/* unsupported */
			c = gx_color_value_to_byte(~r);
			m = gx_color_value_to_byte(~g);
			y = gx_color_value_to_byte(~b);
			if (cs == OPVP_cspaceDeviceCMYK) {
				k = (c<m ? (c<y ? c : y) : (m<y ? m : y));
				c -= k;
				m -= k;
				y -= k;
			} else {
				k = 0;
			}
			return (k + (y << 8) + (m << 16) + (c << 24));
			break;
		case	OPVP_cspaceDeviceGray :
#if GS_VERSION_MAJOR >= 8
			{
				gx_color_value rgb[3];
				rgb[0] = rgb[1] = rgb[2] = r;
				return gx_default_gray_map_rgb_color(dev, rgb);
			}
#else
			return gx_default_gray_map_rgb_color(dev, r, g, b);
#endif
			break;
		case	OPVP_cspaceBW :
#if GS_VERSION_MAJOR >= 8
			return gx_default_b_w_map_rgb_color(dev, prgb);
#else
			return gx_default_b_w_map_rgb_color(dev, r, g, b);
#endif
			break;
		case	OPVP_cspaceStandardRGB :
		case	OPVP_cspaceDeviceRGB :
		default :
#if GS_VERSION_MAJOR >= 8
			return gx_default_rgb_map_rgb_color(dev, prgb);
#else
			return gx_default_rgb_map_rgb_color(dev, r, g, b);
#endif
			break;
	}
}

/*
 * map color rgb
 */
static	int
opvp_map_color_rgb(
	gx_device		*dev,
	gx_color_index		color,
	gx_color_value		prgb[3]
	)
{
#if !(ENABLE_SIMPLE_MODE)
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	int			code = -1;
#endif
	OPVP_ColorSpace		cs = OPVP_cspaceStandardRGB;
	uint			c, m, y, k;

#if ENABLE_SIMPLE_MODE
	cs = colorSpace;
#else
	/* call GetColorSpace */
	if (pdev->is_open) {
		if (apiEntry->GetColorSpace)
		code = apiEntry->GetColorSpace(printerContext, &cs);
		if (code != OPVP_OK) {
			if (pdev->color_info.depth > 32) {
				cs = OPVP_cspaceStandardRGB64;
			} else if (pdev->color_info.depth > 8 ) {
				cs = OPVP_cspaceStandardRGB;
			} else if (pdev->color_info.depth > 1 ) {
				cs = OPVP_cspaceDeviceGray;
			} else {
				cs = OPVP_cspaceBW;
			}
		}
	}
#endif

	switch (cs) {
		case	OPVP_cspaceStandardRGB64 :
			/* unsupported */
			if (sizeof(gx_color_index) >= 6) {
				prgb[0] = ((long long)color >> 32) & 0xffff;
				prgb[1] = ((long long)color >> 16) & 0xffff;
				prgb[2] = color & 0xffff;
			} else {
				prgb[0] = gx_color_value_from_byte(
				              (color >> 16) & 0xff);
				prgb[1] = gx_color_value_from_byte(
				              (color >> 8) & 0xff);
				prgb[2] = gx_color_value_from_byte(
				              color & 0xff);
			}
			break;
		case	OPVP_cspaceDeviceCMYK :
		case	OPVP_cspaceDeviceCMY :
			/* unsupported */
			c = gx_color_value_from_byte((color >> 24) & 0xff);
			m = gx_color_value_from_byte((color >> 16) & 0xff);
			y = gx_color_value_from_byte((color >> 8) & 0xff);
			if (cs == OPVP_cspaceDeviceCMYK) {
				k = gx_color_value_from_byte(color & 0xff);
				c += k; if (c > 255) c = 255;
				m += k; if (m > 255) m = 255;
				y += k; if (y > 255) y = 255;
			}
			prgb[0] = gx_color_value_from_byte(~c & 0xff);
			prgb[1] = gx_color_value_from_byte(~m & 0xff);
			prgb[2] = gx_color_value_from_byte(~y & 0xff);
			break;
		case	OPVP_cspaceDeviceGray :
			return gx_default_gray_map_color_rgb(dev, color, prgb);
			break;
		case	OPVP_cspaceBW :
			return gx_default_b_w_map_color_rgb(dev, color, prgb);
			break;
		case	OPVP_cspaceStandardRGB :
		case	OPVP_cspaceDeviceRGB :
		default :
			return gx_default_rgb_map_color_rgb(dev, color, prgb);
			break;
	}

	return 0;
}

/*
 * fill rectangle
 */
static	int
opvp_fill_rectangle(
	gx_device		*dev,
	int			x,
	int			y,
	int			w,
	int			h,
	gx_color_index		color
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	byte			data[8] = {0xC0, 0, 0, 0, 0xC0, 0, 0, 0};
	int			code = -1;
	int			ecode = 0;
	OPVP_Brush		brush;
	OPVP_Point		point;
#if !(ENABLE_SIMPLE_MODE)
	int			rop = 0;
#endif

	if (vector) {
		return gdev_vector_fill_rectangle( dev, x, y, w, h, color);
	}

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

#if !(ENABLE_SIMPLE_MODE)
	/* call SaveGS */
	if (apiEntry->SaveGS)
	apiEntry->SaveGS(printerContext);
#endif

	/* zero-color */
	opvp_set_brush_color(pdev, gx_no_color_index, &brush);

	/* call SetBgColor */
	if (apiEntry->SetBgColor)
	apiEntry->SetBgColor(printerContext, &brush);

	/* one-color */
	opvp_set_brush_color(pdev, color, &brush);

	/* call SetFillColor */
	if (apiEntry->SetFillColor)
	apiEntry->SetFillColor(printerContext, &brush);

#if !(ENABLE_SIMPLE_MODE)
	/* save ROP */
	if (apiEntry->GetROP)
	apiEntry->GetROP(printerContext, &rop);
#endif
	/* call SetROP */
	if (apiEntry->SetROP)
	apiEntry->SetROP(printerContext, 0xB8);

	/* call SetCurrentPoint */
	OPVP_i2Fix(x, point.x);
	OPVP_i2Fix(y, point.y);
	if (apiEntry->SetCurrentPoint)
	apiEntry->SetCurrentPoint(printerContext, point.x, point.y);

	/* draw image */
	code = opvp_draw_image(pdev,
	                       1,
	                       2, 2,
	                       w, h,
	                       4,
	                       data);
	if (code) {
		ecode = code;
	}

#if !(ENABLE_SIMPLE_MODE)
	/* restore ROP */
	if (rop) {
		/* call SetROP */
		if (apiEntry->SetROP)
		apiEntry->SetROP(printerContext, rop);
	}
#endif

	/* restore fill color */
	if (vectorFillColor) {
		/* call SetFillColor */
		if (apiEntry->SetFillColor)
		apiEntry->SetFillColor(printerContext, vectorFillColor);
	}

#if !(ENABLE_SIMPLE_MODE)
	/* call RestoreGS */
	if (apiEntry->RestoreGS)
	apiEntry->RestoreGS(printerContext);
#endif

	return ecode;
}

/*
 * copy mono
 */
static	int
opvp_copy_mono(
	gx_device		*dev,
/*const*/
	byte			*data,
	int			data_x,
	int			raster,
	gx_bitmap_id		id,
	int			x,
	int			y,
	int			w,
	int			h,
	gx_color_index		zero,
	gx_color_index		one
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Brush		brush;
	OPVP_Point		point;
	byte			*buff = data;
	int			i, j;
	byte			*d;
	byte			*s;
	int			byte_offset = 0;
	int			bit_offset = 0;
	int			byte_length = raster;
	int			bit_shift = 0;
	int			trans_color = -1;
	int			adj_raster = raster;
	int			adj_height = h;
	char			bit_mask = 0xff;
	int			loop = 1;
#if !(ENABLE_SIMPLE_MODE)
	int			rop = 0;
#endif

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* data offset */
	if (data_x) {
		byte_offset = data_x >> 3;
		bit_offset = data_x & 0x07;
		if (bit_offset) bit_mask <<= (8 - bit_offset);

		if (zero == gx_no_color_index) {
			trans_color = 0;
		} else if (one == gx_no_color_index) {
			trans_color = 1;
		} else {
			trans_color = -1;
			bit_shift = bit_offset;
			bit_offset = 0;
		}
		byte_length = (((w + (bit_offset - bit_shift)) + 7) >> 3);
		adj_raster = ((byte_length + 3) >> 2) << 2;

		if (trans_color != -1) {
			loop = 1;
			adj_height = h;
		}

		buff = calloc(adj_raster, h);
		if (buff) {
			s = &(data[byte_offset]);
			d = buff;
			if (bit_shift) {
				for (i = 0;i < h; i++, d += adj_raster,
				                  s+= raster) {
					for (j = 0; j < byte_length; j++) {
					d[j] = ((s[j] & ~bit_mask)
					        << bit_shift)
					     | ((s[j + 1] & bit_mask)
					        >> (8 - bit_shift));
					}
				}
			} else {
				for (i = 0;i < h; i++, d += adj_raster,
				                  s += raster) {
					memcpy(d, s, byte_length);
					if (bit_offset) {
						if (trans_color == 0) {
							*d &= ~bit_mask;
						} else {
							*d |= bit_mask;
						}
					}
				}
			}
			byte_offset = 0;
		} else {
			buff = data;
			loop = y;
			adj_raster = raster;
			adj_height = 1;
			if (bit_shift) bit_offset = bit_shift;
		}
	}

#if !(ENABLE_SIMPLE_MODE)
	/* call SaveGS */
	if (apiEntry->SaveGS)
	apiEntry->SaveGS(printerContext);
#endif

	/* zero-color */
	opvp_set_brush_color(pdev, zero, &brush);

	/* call SetBgColor */
	if (apiEntry->SetBgColor)
	apiEntry->SetBgColor(printerContext, &brush);

	/* one-color */
	opvp_set_brush_color(pdev, one, &brush);

	/* call SetFillColor */
	if (apiEntry->SetFillColor)
	apiEntry->SetFillColor(printerContext, &brush);

#if !(ENABLE_SIMPLE_MODE)
	/* save ROP */
	if (apiEntry->GetROP)
	apiEntry->GetROP(printerContext, &rop);
#endif
	/* call SetROP */
	if (zero == gx_no_color_index) {
		if (apiEntry->SetROP)
		apiEntry->SetROP(printerContext, 0xB8);
	} else {
		if (apiEntry->SetROP)
		apiEntry->SetROP(printerContext, 0xCC);
	}

	/* adjust */
	x -= bit_offset;
	w += bit_offset;
	h = adj_height;

	/* loop */
	for (i = 0; i < loop; i++, y += h) {
		/* call SetCurrentPoint */
		OPVP_i2Fix(x, point.x);
		OPVP_i2Fix(y, point.y);
		if (apiEntry->SetCurrentPoint)
		apiEntry->SetCurrentPoint(printerContext, point.x, point.y);

		/* draw image */
		code = opvp_draw_image(pdev,
				       1,
				       w, h,
				       w, h,
				       adj_raster,
				       &(buff[byte_offset + (adj_raster * i)]));
		if (code) {
			ecode = code;
			break;
		}
	}

#if !(ENABLE_SIMPLE_MODE)
	/* restore ROP */
	if (rop) {
		/* call SetROP */
		if (apiEntry->SetROP)
		apiEntry->SetROP(printerContext, rop);
	}
#endif

	/* restore fill color */
	if (vectorFillColor) {
		/* call SetFillColor */
		if (apiEntry->SetFillColor)
		apiEntry->SetFillColor(printerContext, vectorFillColor);
	}

#if !(ENABLE_SIMPLE_MODE)
	/* call RestoreGS */
	if (apiEntry->RestoreGS)
	apiEntry->RestoreGS(printerContext);
#endif

	if (buff != data) {
		if (buff) free(buff);
	}

	return ecode;
}

/*
 * copy color
 */
static	int
opvp_copy_color(
	gx_device		*dev,
/*const*/
	byte			*data,
	int			data_x,
	int			raster,
	gx_bitmap_id		id,
	int			x,
	int			y,
	int			w,
	int			h
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)dev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Point		point;
	byte			*buff = data;
	int			i;
	byte			*d;
	byte			*s;
	int			byte_length = raster;
	int			depth;
	int			pixel;
	int			adj_raster = raster;
	int			adj_height = h;
	int			loop = 1;
#if !(ENABLE_SIMPLE_MODE)
	int			j;
	int			rop = 0;
#endif

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* data offset */
	if (data_x) {
		depth = pdev->color_info.depth;
		pixel = (depth + 7) >> 3;
		byte_length = pixel * w;
		adj_raster = ((byte_length + 3) >> 2) << 2;

		buff = malloc(adj_raster * h);
		if (buff) {
			s = &(data[data_x]);
			d = buff;
			for (i = 0;i < h; i++, d += adj_raster, s += raster) {
				memcpy(d, s, byte_length);
			}
		} else {
			buff = data;
			loop = y;
			adj_height = 1;
			adj_raster = raster;
		}
	}

#if !(ENABLE_SIMPLE_MODE)
	/* call SaveGS */
	if (apiEntry->SaveGS)
	apiEntry->SaveGS(printerContext);
#endif

#if !(ENABLE_SIMPLE_MODE)
	/* save ROP */
	if (apiEntry->GetROP)
	apiEntry->GetROP(printerContext, &rop);
#endif
	/* call SetROP */
	if (apiEntry->SetROP)
	apiEntry->SetROP(printerContext, 0xCC); /* SRCCOPY */

	/* adjust */
	h = adj_height;

	/* loop */
	for (i = 0; i < loop; i++, y += h) {
		/* call SetCurrentPoint */
		OPVP_i2Fix(x, point.x);
		OPVP_i2Fix(y, point.y);
		if (apiEntry->SetCurrentPoint)
		apiEntry->SetCurrentPoint(printerContext, point.x, point.y);

		/* draw image */
		code = opvp_draw_image(pdev,
				       pdev->color_info.depth,
				       w, h,
				       w, h,
					   adj_raster,
				       &(buff[data_x + (adj_raster * i)]));
		if (code) {
			ecode = code;
			break;
		}
	}

#if !(ENABLE_SIMPLE_MODE)
	/* restore ROP */
	if (rop) {
		/* call SetROP */
		if (apiEntry->SetROP)
		apiEntry->SetROP(printerContext, rop);
	}
#endif

#if !(ENABLE_SIMPLE_MODE)
	/* call RestoreGS */
	if (apiEntry->RestoreGS)
	apiEntry->RestoreGS(printerContext);
#endif

	if (buff != data) {
		if (buff) free(buff);
	}

	return ecode;
}

/*
 * get params
 */
static	int
_get_params(
	gs_param_list		*plist
	)
{
	int			code;
	int			ecode = 0;
	gs_param_name		pname;
	gs_param_string		vdps;
	gs_param_string		pmps;
	gs_param_string		jips;
	gs_param_string		dips;
	gs_param_string		fips;
	gs_param_string		mlps;
	gs_param_string		mtps;
	gs_param_string		mrps;
	gs_param_string		mbps;
	gs_param_string		zmps;
	char			buff[OPVP_BUFF_SIZE];

	/* get params */

	/* vector driver name */
	pname = "Driver";
	vdps.data = vectorDriver;
	vdps.size = (vectorDriver ? strlen(vectorDriver) + 1 : 0);
	vdps.persistent = false;
	code = param_write_string(plist, pname, &vdps);
	if (code) ecode = code;

	/* printer model name */
	pname = "Model";
	pmps.data = printerModel;
	pmps.size = (printerModel ? strlen(printerModel) + 1 : 0);
	pmps.persistent = false;
	code = param_write_string(plist, pname, &pmps);
	if (code) ecode = code;

	/* job info */
	pname = "JobInfo";
	jips.data = jobInfo;
	jips.size = (jobInfo ? strlen(jobInfo) + 1 : 0);
	jips.persistent = false;
	code = param_write_string(plist, pname, &jips);
	if (code) ecode = code;

	/* doc info */
	pname = "DocInfo";
	dips.data = docInfo;
	dips.size = (docInfo ? strlen(docInfo) + 1 : 0);
	dips.persistent = false;
	code = param_write_string(plist, pname, &dips);
	if (code) ecode = code;

	/* fast image support */
	switch (FastImageMode) {
		case	FastImageNoCTM :
			opvp_alloc_string(&fastImage, "NoCTM");
			break;
		case	FastImageNoRotate :
			opvp_alloc_string(&fastImage, "NoRotateCTM");
			break;
		case	FastImageRightAngle :
			opvp_alloc_string(&fastImage, "RightAngleCTM");
			break;
		case	FastImageReverseAngle :
			opvp_alloc_string(&fastImage, "ReverseAngleCTM");
			break;
		case	FastImageAll :
			opvp_alloc_string(&fastImage, "All");
			break;
		case	FastImageDisable :
		default :
			opvp_alloc_string(&fastImage, NULL);
			break;
	}
	pname = "FastImage";
	fips.data = fastImage;
	fips.size = (fastImage ? strlen(fastImage) + 1 : 0);
	fips.persistent = false;
	code = param_write_string(plist, pname, &fips);
	if (code) ecode = code;
	
	/* margins */
	memset((void*)buff, 0, OPVP_BUFF_SIZE);
	pname = "MarginLeft";
	snprintf(buff, OPVP_BUFF_SIZE - 1, "%f",margins[0]);
	mlps.data = buff;
	mlps.size = strlen(buff) + 1;
	mlps.persistent = false;
	code = param_write_string(plist, pname, &mlps);
	if (code) ecode = code;
	pname = "MarginTop";
	snprintf(buff, OPVP_BUFF_SIZE - 1, "%f",margins[3]);
	mtps.data = buff;
	mtps.size = strlen(buff) + 1;
	mtps.persistent = false;
	code = param_write_string(plist, pname, &mtps);
	if (code) ecode = code;
	pname = "MarginRight";
	snprintf(buff, OPVP_BUFF_SIZE - 1, "%f",margins[2]);
	mrps.data = buff;
	mrps.size = strlen(buff) + 1;
	mrps.persistent = false;
	code = param_write_string(plist, pname, &mrps);
	if (code) ecode = code;
	pname = "MarginBottom";
	snprintf(buff, OPVP_BUFF_SIZE - 1, "%f",margins[1]);
	mbps.data = buff;
	mbps.size = strlen(buff) + 1;
	mbps.persistent = false;
	code = param_write_string(plist, pname, &mbps);
	if (code) ecode = code;

	/* zoom */
	pname = "Zoom";
	snprintf(buff, OPVP_BUFF_SIZE - 1, "%f",zoom[0]);
	zmps.data = buff;
	zmps.size = strlen(buff) + 1;
	zmps.persistent = false;
	code = param_write_string(plist, pname, &zmps);
	if (code) ecode = code;

	return ecode;
}

/*
 * get params for vector
 */
static	int
opvp_get_params(
	gx_device		*dev,
	gs_param_list		*plist
	)
{
	int			code;

	/* get default params */
	code = gdev_vector_get_params(dev, plist);
	if (code) return code;

	/* get params */
	return _get_params(plist);
}

/*
 * get params for inkjet
 */
static	int
oprp_get_params(
	gx_device		*dev,
	gs_param_list		*plist
	)
{
	int			code;

	/* get default params */
	code = gdev_prn_get_params(dev, plist);
	if (code) return code;

	/* get params */
	return _get_params(plist);
}

/*
 * put params
 */
static	int
_put_params(
	gs_param_list		*plist
	)
{
	int			code;
	int			ecode = 0;
	gs_param_name		pname;
	char			*buff = NULL;
	gs_param_string		vdps;
	gs_param_string		pmps;
	gs_param_string		jips;
	gs_param_string		dips;
	gs_param_string		fips;
	gs_param_string		mlps;
	gs_param_string		mtps;
	gs_param_string		mrps;
	gs_param_string		mbps;
	gs_param_string		zmps;

	/* vector driver name */
	pname = "Driver";
	code = param_read_string(plist, pname, &vdps);
	switch (code) {
		case	0 :
			buff = realloc(buff, vdps.size + 1);
			memcpy(buff, vdps.data, vdps.size);
			buff[vdps.size] = 0;
			opvp_alloc_string(&vectorDriver, buff);
			break;
		case	1 :
			/* opvp_alloc_string(&vectorDriver, NULL);*/
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* printer model name */
	pname = "Model";
	code = param_read_string(plist, pname, &pmps);
	switch (code) {
		case	0 :
			buff = realloc(buff, pmps.size + 1);
			memcpy(buff, pmps.data, pmps.size);
			buff[pmps.size] = 0;
			opvp_alloc_string(&printerModel, buff);
			break;
		case	1 :
			/*opvp_alloc_string(&printerModel, NULL);*/
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* job info */
	pname = "JobInfo";
	code = param_read_string(plist, pname, &jips);
	switch (code) {
		case	0 :
			buff = realloc(buff, jips.size + 1);
			memcpy(buff, jips.data, jips.size);
			buff[jips.size] = 0;
			opvp_alloc_string(&jobInfo, buff);
			break;
		case	1 :
			/*opvp_alloc_string(&jobInfo, NULL);*/
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* doc info */
	pname = "DocInfo";
	code = param_read_string(plist, pname, &dips);
	switch (code) {
		case	0 :
			buff = realloc(buff, dips.size + 1);
			memcpy(buff, dips.data, dips.size);
			buff[dips.size] = 0;
			opvp_alloc_string(&docInfo, buff);
			break;
		case	1 :
			/*opvp_alloc_string(&docInfo, NULL);*/
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* fast image support */
	pname = "FastImage";
	code = param_read_string(plist, pname, &fips);
	switch (code) {
		case	0 :
			buff = realloc(buff, fips.size + 1);
			memcpy(buff, fips.data, fips.size);
			buff[fips.size] = 0;
			opvp_alloc_string(&fastImage, buff);
			if (strcasecmp(fastImage,"NoCTM")==0) {
				FastImageMode = FastImageNoCTM;
			} else if (strncasecmp(fastImage,"NoRotate",8)==0) {
				FastImageMode = FastImageNoRotate;
			} else if (strncasecmp(fastImage,"Right",5)==0) {
				FastImageMode = FastImageRightAngle;
			} else if (strncasecmp(fastImage,"Reverse",7)==0) {
				FastImageMode = FastImageReverseAngle;
			} else if (strncasecmp(fastImage,"All",3)==0) {
				FastImageMode = FastImageAll;
			} else {
				FastImageMode = FastImageDisable;
			}
			break;
		case	1 :
			/*opvp_alloc_string(&fastImage, NULL);*/
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* margins */
	pname = "MarginLeft";
	code = param_read_string(plist, pname, &mlps);
	switch (code) {
		case	0 :
			buff = realloc(buff, mlps.size + 1);
			memcpy(buff, mlps.data, mlps.size);
			buff[mlps.size] = 0;
			margins[0] = atof(buff);
			break;
		case	1 :
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}
	pname = "MarginTop";
	code = param_read_string(plist, pname, &mtps);
	switch (code) {
		case	0 :
			buff = realloc(buff, mtps.size + 1);
			memcpy(buff, mtps.data, mtps.size);
			buff[mtps.size] = 0;
			margins[3] = atof(buff);
			break;
		case	1 :
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}
	pname = "MarginRight";
	code = param_read_string(plist, pname, &mrps);
	switch (code) {
		case	0 :
			buff = realloc(buff, mrps.size + 1);
			memcpy(buff, mrps.data, mrps.size);
			buff[mrps.size] = 0;
			margins[2] = atof(buff);
			break;
		case	1 :
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}
	pname = "MarginBottom";
	code = param_read_string(plist, pname, &mbps);
	switch (code) {
		case	0 :
			buff = realloc(buff, mbps.size + 1);
			memcpy(buff, mbps.data, mbps.size);
			buff[mbps.size] = 0;
			margins[1] = atof(buff);
			break;
		case	1 :
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	/* zoom */
	pname = "Zoom";
	code = param_read_string(plist, pname, &zmps);
	switch (code) {
		case	0 :
			buff = realloc(buff, zmps.size + 1);
			memcpy(buff, zmps.data, zmps.size);
			buff[zmps.size] = 0;
			if (strncasecmp(buff, "Auto", 4)) {
				zoom[0] = atof(buff);
				if (zoom[0] > 0) {
					zoom[1] = zoom[0];
				} else {
					zoom[0] = zoom[1] = 1;
				}
			} else {
				zoom[0] = zoom[1] = 1;
				zoomAuto = true;
			}
			break;
		case	1 :
			break;
		default :
			ecode = code;
			param_signal_error(plist, pname, ecode);
	}

	if (buff) free(buff);

	return ecode;
}

/*
 * put params for vector
 */
static	int
opvp_put_params(
	gx_device		*dev,
	gs_param_list		*plist
	)
{
	int			code;

	/* put params */
	code = _put_params(plist);
	if (code) return code;

	/* put default params */
	return gdev_vector_put_params(dev, plist);
}

/*
 * put params for inkjet
 */
static	int
oprp_put_params(
	gx_device		*dev,
	gs_param_list		*plist
	)
{
	int			code;

	/* put params */
	code = _put_params(plist);
	if (code) return code;

	/* put default params */
	return gdev_prn_put_params(dev, plist);
}

/*
 * fill path
 */
static	int
opvp_fill_path(
	gx_device		*dev,
const	gs_imager_state		*pis,
	gx_path			*ppath,
const	gx_fill_params		*params,
const	gx_device_color		*pdevc,
const	gx_clip_path		*pxpath
	)
{
	bool			draw_image = false;
	gs_fixed_rect		inner, outer;

	/* check clippath support */
	if (!(apiEntry->SetClipPath)) {
		/* get clipping box area */
		gx_cpath_inner_box(pxpath,&inner);
		gx_cpath_outer_box(pxpath,&outer);
		/* check difference between inner-box and outer-box */
		if ((inner.p.x != outer.p.x) || (inner.p.y != outer.p.y) ||
		    (inner.q.x != outer.q.x) || (inner.q.y != outer.q.y)) {
			/* set draw by image */
			draw_image = true;
		}
	}

	if (!vector || draw_image) {
		return gx_default_fill_path(dev, pis, ppath,
	                                    params, pdevc, pxpath);
	}

	return gdev_vector_fill_path(dev, pis, ppath, params, pdevc, pxpath);
}

/*
 * stroke path
 */
static	int
opvp_stroke_path(
	gx_device		*dev,
const	gs_imager_state		*pis,
	gx_path			*ppath,
const	gx_stroke_params	*params,
const	gx_drawing_color	*pdcolor,
const	gx_clip_path		*pxpath
	)
{
	bool			draw_image = false;
	gs_fixed_rect		inner, outer;

	/* check clippath support */
	if (!(apiEntry->SetClipPath)) {
		/* get clipping box area */
		gx_cpath_inner_box(pxpath,&inner);
		gx_cpath_outer_box(pxpath,&outer);
		/* check difference between inner-box and outer-box */
		if ((inner.p.x != outer.p.x) || (inner.p.y != outer.p.y) ||
		    (inner.q.x != outer.q.x) || (inner.q.y != outer.q.y)) {
			/* set draw by image */
			draw_image = true;
		}
	}

	if (!vector || draw_image) {
		return gx_default_stroke_path(dev, pis, ppath,
		                              params, pdcolor, pxpath);
	}

	return gdev_vector_stroke_path(dev, pis, ppath,
	                               params, pdcolor, pxpath);
}

/*
 * fill mask
 */
static	int
opvp_fill_mask(
	gx_device		*dev,
const	byte			*data,
	int			data_x,
	int			raster,
	gx_bitmap_id		id,
	int			x,
	int			y,
	int			w,
	int			h,
const	gx_drawing_color	*pdcolor,
	int			depth,
	gs_logical_operation_t	lop,
const	gx_clip_path		*pcpath
	)
{
	if (vector) {
#if GS_VERSION_MAJOR >= 8
		gdev_vector_update_fill_color((gx_device_vector *)dev, NULL, pdcolor);	/* for gs 8.15 */
#else
		gdev_vector_update_fill_color((gx_device_vector *)dev, pdcolor);
#endif
		gdev_vector_update_clip_path((gx_device_vector *)dev, pcpath);
		gdev_vector_update_log_op((gx_device_vector *)dev, lop);
	}

	return gx_default_fill_mask(dev, data, data_x, raster, id,
	                            x, y, w, h, pdcolor, depth, lop, pcpath);
}

/*
 * begin image
 */
static	int
opvp_begin_image(
		gx_device				*dev,
const	gs_imager_state			*pis,
const	gs_image_t				*pim,
		gs_image_format_t		format,
const	gs_int_rect				*prect,
const	gx_drawing_color		*pdcolor,
const	gx_clip_path			*pcpath,
		gs_memory_t				*mem,
		gx_image_enum_common_t	**pinfo
	)
{
	gx_device_vector	*vdev =(gx_device_vector *)dev;
	gdev_vector_image_enum_t	*vinfo;
	gs_matrix		mtx;
	OPVP_CTM		ctm;
	bool			draw_image = false;
	bool			supported_angle = false;
	int			code = -1;
	int			ecode = 0;
	OPVP_Rectangle		rect;
	OPVP_Brush		brush;
	int			bits_per_pixel;
	bool			can_reverse = false;
	int			p;
	float			mag[2] = {1, 1};
const	gs_color_space		*pcs = pim->ColorSpace;
	gs_color_space_index	index;

	color_index = 0;

	vinfo = gs_alloc_struct(mem, gdev_vector_image_enum_t,
				&st_vector_image_enum,
				"opvp_begin_image");

	if (vinfo) {
		memcpy(imageDecode,pim->Decode,sizeof(pim->Decode));
		vinfo->memory =mem;
		code = gdev_vector_begin_image(vdev, pis, pim, format, prect,
		                               pdcolor, pcpath, mem,
		                               &opvp_image_enum_procs,
		                               vinfo);
		if (code) ecode = code;

		if (!ecode) {
#if 0	/* deleted to speed up 256-color image printing */
			/* check 256-color image, Thanks to T */
			/* Unavailiavle color space check */
			if (!(pim->ImageMask)) {
				index = gs_color_space_get_index(pcs);
				if (index == gs_color_space_index_Indexed)
					return gx_default_begin_image(
					           dev, pis, pim, format,
					           prect, pdcolor, pcpath,
					           mem, pinfo);
			}
#endif
			/* bits per pixel */
			for (bits_per_pixel=0, p=0; p < vinfo->num_planes; p++) 
				bits_per_pixel += vinfo->plane_depths[p];

			/* for indexed color */
			if (!(pim->ImageMask)) {
				color_index = gs_color_space_get_index(pcs);
				if (color_index == gs_color_space_index_Indexed) {
					if (pcs->params.indexed.lookup.table.size > 3*256) {
						return gx_default_begin_image(
							dev, pis, pim, format,
							prect, pdcolor, pcpath,
							mem, pinfo);
					} else {
						memcpy(palette, pcs->params.indexed.lookup.table.data,\
						pcs->params.indexed.lookup.table.size);
						if (bits_per_pixel == 8)
							bits_per_pixel = 24;
					}
				}
			}

#if ENABLE_AUTO_REVERSE
			if (bits_per_pixel % 8 == 0) {
				can_reverse = true;
			}
#endif
			/* adjust matrix */
			reverse_image = false;
			gs_matrix_invert(&pim->ImageMatrix, &mtx);
			gs_matrix_multiply(&mtx, &ctm_only(pis), &mtx);
			switch (FastImageMode) {
				case	FastImageNoCTM :
					if ((mtx.xy==0)&&(mtx.yx==0)&&
					    (mtx.yy>=0)) {
						if (mtx.xx>=0) {
							mag[0] = mtx.xx;
							mag[1] = mtx.yy;
							mtx.xx = 1;
							mtx.yy = 1;
							supported_angle = true;
						} else if (can_reverse) {
							mtx.xx *= -1;
							mag[0] = mtx.xx;
							mag[1] = mtx.yy;
							mtx.xx = 1;
							mtx.yy = 1;
							mtx.tx -= vinfo->width
							        * mag[0];
							supported_angle = true;
							reverse_image = true;
						}
					}
					break;
				case	FastImageNoRotate :
					if ((mtx.xy==0)&&(mtx.yx==0)&&
					    (mtx.yy>=0)) {
						if (mtx.xx>=0) {
							supported_angle = true;
						} else if (can_reverse) {
							mtx.xx *= -1;
							mtx.tx -= vinfo->width
							        * mtx.xx;
							supported_angle = true;
							reverse_image = true;
						}
					}
					break;
				case	FastImageRightAngle :
					if ((mtx.xy==0)&&(mtx.yx==0)) {
						if (((mtx.xx>=0)&&(mtx.yy>=0))||
						    ((mtx.xx<=0)&&(mtx.yy<=0))){
							supported_angle = true;
						} else if (can_reverse) {
							mtx.xx *= -1;
							mtx.tx -= vinfo->width
							        * mtx.xx;
							supported_angle = true;
							reverse_image = true;
						}
					} else if ((mtx.xx==0)&&(mtx.yy==0)) {
						if (((mtx.xy>=0)&&(mtx.yx<=0))||
						    ((mtx.xy<=0)&&(mtx.yx>=0))){
							supported_angle = true;
						} else if (can_reverse) {
							mtx.xy *= -1;
							mtx.ty -= vinfo->height
							        * mtx.xy;
							supported_angle = true;
							reverse_image = true;
						}
					}
					break;
				case	FastImageReverseAngle :
					if (((mtx.xy==0)&&(mtx.yx==0))||
					    ((mtx.xx==0)&&(mtx.yy==0))) {
						supported_angle = true;
					}
					break;
				case	FastImageAll :
					supported_angle = true;
					break;
				case	FastImageDisable :
				default :
					break;
			}
#if 1
			if (supported_angle) {
				/* moveto */
				opvp_moveto(vdev, 0, 0, mtx.tx, mtx.ty, 0);
			}
#endif
			if ((supported_angle) &&
			    (FastImageMode != FastImageNoCTM)) {
				/* call SetCTM */
				ctm.a = mtx.xx;
				ctm.b = mtx.xy;
				ctm.c = mtx.yx;
				ctm.d = mtx.yy;
				ctm.e = mtx.tx;
				ctm.f = mtx.ty;
				if (apiEntry->SetCTM)
				code = apiEntry->SetCTM(printerContext, &ctm);
				else code = -1;
				if (code != OPVP_OK) ecode = code;
			}
		}

		if ((!ecode) && supported_angle) {
			if ((!prect) &&
			    ((vinfo->num_planes == 1) ||
			     ((vinfo->num_planes == 3) &&
			      (vinfo->plane_depths[0] == 8) &&
			      (vinfo->plane_depths[1] == 8) &&
			      (vinfo->plane_depths[2] == 8))
			    )
			   ) {
				/*
				 * avail only
				 * 1 plane image
				 * or
				 * 3 planes 24 bits color image
				 * (8 bits per plane)
				 */
				if (apiEntry->StartDrawImage) {
					draw_image = true;
				}
			}
		}
	}

	if (draw_image) {
		*pinfo = (gx_image_enum_common_t *)vinfo;

		if (!ecode) {
#if 0
			if (FastImageMode == FastImageNoCTM) {
				/* moveto */
				opvp_moveto(vdev, 0, 0, mtx.tx, mtx.ty, 0);
			} else {
				/* moveto new origin */
				opvp_moveto(vdev, 0, 0, 0, 0, 0);
			}
#endif

#if 0
			if ((vinfo->num_planes == 1) &&
			    (vinfo->plane_depths[0] == 1)) {
				/* zero-color */
				opvp_set_brush_color((gx_device_opvp*)vdev,
				                     vdev->white, &brush);
				/* call SetBgColor */
				if (apiEntry->SetBgColor)
				apiEntry->SetBgColor(printerContext, &brush);
				/* one-color */
				opvp_set_brush_color((gx_device_opvp*)vdev,
				                     vdev->black, &brush);
				/* call SetFillColor */
				if (apiEntry->SetFillColor)
				apiEntry->SetFillColor(printerContext, &brush);
			}
#endif
			/* call SetROP */
			if (apiEntry->SetROP)
			apiEntry->SetROP(printerContext, 0xCC);
			/* image size */
			OPVP_i2Fix(0, rect.p0.x);
			OPVP_i2Fix(0, rect.p0.y);
			if (mag[0] != 1) {
				OPVP_f2Fix(vinfo->width * mag[0], rect.p1.x);
			} else {
				OPVP_i2Fix(vinfo->width, rect.p1.x);
			}
			if (mag[1] != 1) {
				OPVP_f2Fix(vinfo->height * mag[1], rect.p1.y);
			} else {
				OPVP_i2Fix(vinfo->height, rect.p1.y);
			}
			/* call StartDrawImage */
			if (apiEntry->StartDrawImage)
			code = apiEntry->StartDrawImage(printerContext,
							vinfo->width,
							vinfo->height,
							bits_per_pixel,
							OPVP_iformatRaw,
							rect);
			
			/* bugfix for 32bit CMYK image print error */
			if(code != OPVP_OK) {
				if(apiEntry->ResetCTM)
					apiEntry->ResetCTM(printerContext);	/* reset CTM */
				return gx_default_begin_image(dev, pis, pim, format,
									prect, pdcolor, pcpath, mem, pinfo);
				/* ecode = code; */
			}
		}

		if (!ecode) {
			begin_image = true;
		}

		return ecode;
	}

	return gx_default_begin_image(dev, pis, pim, format, prect,
	                              pdcolor, pcpath, mem, pinfo);
}

/*
 * plane data
 */
static	int
opvp_image_plane_data(
		gx_image_enum_common_t	*info,
const	gx_image_plane_t		*planes,
		int						height,
		int						*rows_used
	)
{
	gdev_vector_image_enum_t	*vinfo;
	byte		*tmp_buf = NULL;
	byte		*buf = NULL;
	int			bits_per_pixel;
	int			data_bytes, dst_bytes;
	int			raster_length, dst_length;
	int			p;
	int			x;
	int			d;
	int			h;
	int			ecode = 0;
	int			i, j;
	byte		*src_ptr, *dst_ptr, *ppalette;
	byte		*ptr;
	bbox_image_enum	*pbe;
	gx_image_enum	*tinfo;
	const gs_imager_state	*pis;

	vinfo = (gdev_vector_image_enum_t *)info;

	if (!begin_image)
		return 0;

	for (bits_per_pixel=0, p=0; p < vinfo->num_planes; p++) 
		bits_per_pixel += vinfo->plane_depths[p];
	
	data_bytes = (bits_per_pixel * vinfo->width + 7) >> 3;
	raster_length = ((data_bytes + 3) >> 2) << 2;
	buf = calloc(raster_length, height);

	if (vinfo->default_info) {
		gx_image_plane_data(vinfo->default_info, planes, height);
	}
	if (vinfo->bbox_info) {
		gx_image_plane_data(vinfo->bbox_info, planes, height);
	}

	if (buf) {
		if (vinfo->num_planes == 1) {
			for (h = 0; h < height; h++) {
				d = raster_length * h;
				if ((reverse_image) && (bits_per_pixel == 8)) {
					for (x = data_bytes * (h + 1) - 1;
					     x >= data_bytes * h;
					     x--, d++) {
						buf[d]
						    = (byte)planes[0].data[x];
					}
				} else {
					memcpy(buf + d,
					       planes[0].data
					       + (data_bytes * h),
					       data_bytes);
				}
			}
		} else {
			for (h = 0; h < height; h++) {
				d = raster_length * h;
				if (reverse_image) {
					for (x = vinfo->width * (h + 1) - 1;
					     x >= vinfo->width * h;
					     x--) {
						for (p = 0;
						     p < vinfo->num_planes;
						     p++, d++) {
							buf[d] = (byte)
							   (planes[p].data[x]);
						}
					}
				} else {
					for (x = vinfo->width * h;
					     x < vinfo->width * (h + 1);
					     x++) {
						for (p = 0;
						     p < vinfo->num_planes;
						     p++, d++) {
							buf[d] = (byte)
							   (planes[p].data[x]);
						}
					}
				}
			}
		}
		
		/* Convert 256 color -> RGB */
		if(color_index == gs_color_space_index_Indexed) {	/* 256 color */
			if (vinfo->bits_per_pixel >= 8) {		/* 8bit image */
				dst_bytes = data_bytes * 3;
				dst_length = ((dst_bytes + 3) >> 2) << 2;
				
				tmp_buf = calloc(dst_length, height);
				if (tmp_buf) {
					for (i = 0; i < height; i++) {
						src_ptr = buf + raster_length * i;
						dst_ptr = tmp_buf + dst_length * i;
						for (j = 0; j < data_bytes; j++) {
							ppalette = palette + src_ptr[j] * 3;
							dst_ptr[j*3]     = ppalette[0];		/* R */
							dst_ptr[j*3 + 1] = ppalette[1];		/* G */
							dst_ptr[j*3 + 2] = ppalette[2];		/* B */
						}
					}
					
					free (buf);
					buf = tmp_buf;
					data_bytes = dst_bytes;
					raster_length = dst_length;
					vinfo->bits_per_pixel = 24;
				}
			} else {		/* 1bit image */
				if (palette[0] == 0) {		/* 0/1 reverse is needed */
					for (i = 0; i < height; i++) {
						src_ptr = buf + raster_length * i;
						for (j = 0; j < data_bytes; j++)
							src_ptr[j] ^= 0xff;
					}
				}
			}
		}
		
		/* Adjust image data gamma */
		pbe = (bbox_image_enum *)vinfo->bbox_info;
		tinfo = pbe->target_info;
		pis = tinfo->pis;
		
#if GS_VERSION_MAJOR >= 8
		if (vinfo->bits_per_pixel == 24) {	/* 24bit RGB color */
			for (i = 0; i < height; i++) {
				ptr = buf + raster_length * i;
				for (j = 0; j < vinfo->width; j++) {
					ptr[j*3] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3]),\
													effective_transfer[0])));
					ptr[j*3+1] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3+1]),\
													effective_transfer[1])));
					ptr[j*3+2] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3+2]),\
													effective_transfer[2])));
				}
			}
		} else if (vinfo->bits_per_pixel == 8) {	/* 8bit Gray image */
			for (i = 0; i < height; i++) {
				ptr = buf + raster_length * i;
				for (j=0; j < vinfo->width; j++) {
					ptr[j] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j]),\
													effective_transfer[3])));
				}
			}
		} else if ((vinfo->bits_per_pixel == 1) && (color_index != gs_color_space_index_Indexed)) {/* 1bit gray */
		    if (imageDecode[0] == 0) {
			for (i = 0; i < height; i++) {
				src_ptr = buf + raster_length * i;
				for (j = 0; j < data_bytes; j++)
					src_ptr[j] ^= 0xff;
			}
		    }
		}
#else
		if (vinfo->bits_per_pixel == 24) {	/* 24bit RGB color */
			for (i = 0; i < height; i++) {
				ptr = buf + raster_length * i;
				for (j = 0; j < vinfo->width; j++) {
					ptr[j*3] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3]),\
													effective_transfer.colored.red)));
					ptr[j*3+1] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3+1]),\
													effective_transfer.colored.green)));
					ptr[j*3+2] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j*3+2]),\
													effective_transfer.colored.blue)));
				}
			}
		} else if (vinfo->bits_per_pixel == 8) {	/* 8bit Gray image */
			for (i = 0; i < height; i++) {
				ptr = buf + raster_length * i;
				for (j = 0; j < vinfo->width; j++) {
					ptr[j] = min(255, frac2cv(gx_map_color_frac(pis, cv2frac(ptr[j]),\
													effective_transfer.colored.gray)));
				}
			}
		} else if ((vinfo->bits_per_pixel == 1) && (color_index != gs_color_space_index_Indexed)) {/* 1bit gray */
		    if (imageDecode[0] == 0) {
			for (i = 0; i < height; i++) {
				src_ptr = buf + raster_length * i;
				for (j = 0; j < data_bytes; j++)
					src_ptr[j] ^= 0xff;
			}
		    }
		}
#endif

		/* call TansferDrawImage */
		if (apiEntry->TransferDrawImage)
		apiEntry->TransferDrawImage(printerContext,
		                            raster_length * height,
		                            (void *)buf);
		if (buf)
			free(buf);		/* free buffer */
	}

	vinfo->y += height;
	ecode = (vinfo->y >= vinfo->height);

	return ecode;
}

/*
 * end image
 */
static	int
opvp_image_end_image(
	gx_image_enum_common_t	*info,
	bool					draw_last
	)
{
	gx_device		*dev = info->dev;
	gx_device_vector	*vdev = (gx_device_vector *)dev;
	gdev_vector_image_enum_t	*vinfo;
	OPVP_CTM		ctm;

	vinfo = (gdev_vector_image_enum_t *)info;

	if (begin_image) {
		/* call EndDrawImage */
		if (apiEntry->EndDrawImage)
		apiEntry->EndDrawImage(printerContext);

		begin_image = false;

		if (FastImageMode != FastImageNoCTM) {
			/* call ResetCTM */
			if (apiEntry->ResetCTM)
			apiEntry->ResetCTM(printerContext);
			else {
				/* call SetCTM */
				ctm.a = 1;
				ctm.b = 0;
				ctm.c = 0;
				ctm.d = 1;
				ctm.e = 0;
				ctm.f = 0;
				if (apiEntry->SetCTM)
				apiEntry->SetCTM(printerContext, &ctm);
			}
		}
		if ((vinfo->num_planes == 1) &&
		    (vinfo->plane_depths[0] == 1)) {
			/* restore fill color */
			if (vectorFillColor) {
				/* call SetFillColor */
				if (apiEntry->SetFillColor)
				apiEntry->SetFillColor(printerContext,
				                       vectorFillColor);
			}
		}
	}

	return gdev_vector_end_image(vdev, vinfo, draw_last, vdev->white);
}

/* ----- vector driver procs ----- */
/*
 * begin page
 */
static	int
opvp_beginpage(
	gx_device_vector	*vdev
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;

#ifdef OPVP_IGNORE_BLANK_PAGE
	if (pdev->in_page) return 0;
#endif
	/* start page */
	code = opvp_startpage((gx_device *)pdev);
	if (code) {
		ecode = code;
	} else {
		pdev->in_page = true;	/* added '05.12.07 */
		beginPage = true;
	}

	return ecode;
}

/*
 * set line width
 */
static	int
opvp_setlinewidth(
	gx_device_vector	*vdev,
	floatp			width
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Fix		w;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

#if 0
	if (width < 1) width = 1;
#endif

	/* call SetLineWidth */
	OPVP_f2Fix(width, w);
	if (apiEntry->SetLineWidth)
	code = apiEntry->SetLineWidth(printerContext, w);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * set line cap
 */
static	int
opvp_setlinecap(
	gx_device_vector	*vdev,
	gs_line_cap		cap
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_LineCap		linecap;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	switch (cap) {
		case	gs_cap_butt :
			linecap = OPVP_lineCapButt;
			break;
		case	gs_cap_round :
			linecap = OPVP_lineCapRound;
			break;
		case	gs_cap_square :
			linecap = OPVP_lineCapSquare;
			break;
		case	gs_cap_triangle :
		default :
			linecap = OPVP_lineCapButt;
			break;
	}

	/* call SetLineCap */
	if (apiEntry->SetLineCap)
	code = apiEntry->SetLineCap(printerContext, linecap);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * set line join
 */
static	int
opvp_setlinejoin(
	gx_device_vector	*vdev,
	gs_line_join		join
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_LineJoin		linejoin;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	switch (join) {
		case	gs_join_miter :
			linejoin = OPVP_lineJoinMiter;
			break;
		case	gs_join_round :
			linejoin = OPVP_lineJoinRound;
			break;
		case	gs_join_bevel :
			linejoin = OPVP_lineJoinBevel;
			break;
		case	gs_join_none :
		case	gs_join_triangle :
		default :
			linejoin = OPVP_lineCapButt;
			break;
	}

	/* call SetLineJoin */
	if (apiEntry->SetLineJoin)
	code = apiEntry->SetLineJoin(printerContext, linejoin);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * set miter limit
 */
static	int
opvp_setmiterlimit(
	gx_device_vector	*vdev,
	floatp			limit
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Fix		l;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* call SetMiterLimit */
	OPVP_f2Fix(limit, l);
	if (apiEntry->SetMiterLimit)
	code = apiEntry->SetMiterLimit(printerContext, l);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * set dash
 */
static	int
opvp_setdash(
	gx_device_vector	*vdev,
	const	float		*pattern,
	uint			count,
	floatp			offset
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Fix		*p = NULL;
	OPVP_Fix		o;
	int			i;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* pattern */
	if (count) {
		p = calloc(sizeof(OPVP_Fix), count);
		if (p) {
			for (i = 0; i < count; i++) {
				OPVP_f2Fix(pattern[i], p[i]);
			}
		} else {
			ecode = -1;
		}
	}

	/* call SetLineDash */
	if (!ecode) {
		if (apiEntry->SetLineDash)
		code = apiEntry->SetLineDash(printerContext, p, count);
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	/* call SetLineDashOffset */
	if (!ecode) {
		OPVP_f2Fix(offset, o);
		if (apiEntry->SetLineDashOffset)
		code = apiEntry->SetLineDashOffset(printerContext, o);
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	/* call SetLineStyle */
	if (!ecode) {
		if (apiEntry->SetLineStyle)
		code = apiEntry->SetLineStyle(printerContext,
		                              (count ?
		                               OPVP_lineStyleDash :
		                               OPVP_lineStyleSolid));
		if (code != OPVP_OK) {
			ecode = -1;
		}
	}

	if (p) free(p);

	return ecode;
}

/*
 * set flat
 */
static	int
opvp_setflat(
	gx_device_vector	*vdev,
	floatp			flatness
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
/*	int			code = -1;*/
	int			ecode = 0;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* what to do ? */

	return ecode;
}

/*
 * set logical operation
 */
static	int
opvp_setlogop(
	gx_device_vector	*vdev,
	gs_logical_operation_t	lop,
	gs_logical_operation_t	diff
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
static	int			*prop = NULL;
static	int			pnum = 0;
	int			exist =0;
	int			i;
	int			rop = (lop_rop(lop)) & 0x00ff;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	if (!prop) {
		/* call QueryROP with NULL */
		if (!ecode) {
			if (apiEntry->QueryROP) {
				code = apiEntry->QueryROP(printerContext,
				                          &pnum, NULL);
				if (code != OPVP_OK) {
					ecode = -1;
				}
			}
		}
	}

	/* call QueryROP with array-pointer */
	if (!ecode && (pnum > 0)) {
		if (!prop) {
			if ((prop = calloc(sizeof(int), pnum))) {
				if (apiEntry->QueryROP)
				code = apiEntry->QueryROP(printerContext,
				                          &pnum, prop);
				if (code != OPVP_OK) {
					ecode = -1;
				}
			}
		}
		if (!ecode) {
			for (i = 0; i < pnum; i++ ) {
				if (prop[i] == rop) {
					exist = 1;
					break;
				}
			}
		}
	}

	/* call SetROP */
	if (!ecode) {
		if (!exist) {
			/* what to do ? */
			if (apiEntry->SetROP)
			apiEntry->SetROP(printerContext, 0xCC); /* SRCCOPY */
		} else {
			if (apiEntry->SetROP)
			code = apiEntry->SetROP(printerContext, rop);
			if (code != OPVP_OK) {
				ecode = -1;
			}
		}
	}

#if !(ENABLE_SIMPLE_MODE)
	/* free buffer */
	if (prop) {
		free(prop);
		prop = NULL;
	}
#endif

	return ecode;
}

#if GS_VERSION_MAJOR >= 8
/*--- added for Ghostscritp 8.15 ---*/
static int
opvp_can_handle_hl_color(gx_device_vector * vdev, const gs_imager_state * pis1, 
              const gx_drawing_color * pdc)
{
    return false; /* High level color is not implemented yet. */
}
#endif

/*
 * set fill color
 */
#if GS_VERSION_MAJOR >= 8
static	int
opvp_setfillcolor(
	gx_device_vector	*vdev,
	const	gs_imager_state		*pis,		/* added for gs 8.15 */
const	gx_drawing_color	*pdc
	)
#else
static	int
opvp_setfillcolor(
	gx_device_vector	*vdev,
const	gx_drawing_color	*pdc
	)
#endif
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	gx_color_index		color;
static	OPVP_Brush		brush;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	if (!gx_dc_is_pure(pdc)) return_error(gs_error_rangecheck);

	/* color */
	if (!vectorFillColor) vectorFillColor = &brush;
	color = gx_dc_pure_color(pdc);
	opvp_set_brush_color(pdev, color, vectorFillColor);

	/* call SetFillColor */
	if (apiEntry->SetFillColor)
	code = apiEntry->SetFillColor(printerContext, vectorFillColor);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * set stroke color
 */
#if GS_VERSION_MAJOR >= 8
static	int
opvp_setstrokecolor(
	gx_device_vector	*vdev,
	const	gs_imager_state		*pis,		/* added for gs 8.15 */
const	gx_drawing_color	*pdc
	)
#else
static	int
opvp_setstrokecolor(
	gx_device_vector	*vdev,
const	gx_drawing_color	*pdc
	)
#endif
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	gx_color_index		color;
	OPVP_Brush		brush;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	if (!gx_dc_is_pure(pdc)) return_error(gs_error_rangecheck);

	/* color */
	color = gx_dc_pure_color(pdc);
	opvp_set_brush_color(pdev, color, &brush);

	/* call SetStrokeColor */
	if (apiEntry->SetStrokeColor)
	code = apiEntry->SetStrokeColor(printerContext, &brush);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

#define	OPVP_OPT_MULTI_PATH

/*
 * vector do path
 */
static	int
opvp_vector_dopath(
	gx_device_vector	*vdev,
const	gx_path			*ppath,
	gx_path_type_t		type,
const	gs_matrix		*pmat
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	gs_fixed_rect		rect;
	gs_path_enum		path;
	gs_point		scale;
	int			op;
#ifdef	OPVP_OPT_MULTI_PATH
	int			i;
	int			pop = 0;
	int			npoints = 0;
	int			*cp_num = NULL;
	_fPoint 		*points = NULL;
	OPVP_Point		*opvp_p = NULL;
	_fPoint 		current;
	_fPoint 		check_p;
#else
	_fPoint 		points[4];
#endif
	_fPoint 		start;
	fixed			vs[6];
	bool			begin = true;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	if (gx_path_is_rectangle(ppath, &rect))
	return (*vdev_proc(vdev, dorect))(vdev,
	                                  rect.p.x, rect.p.y,
	                                  rect.q.x, rect.q.y,
	                                  type);

	/* begin path */
	code = (*vdev_proc(vdev, beginpath))(vdev, type);
	if (code) ecode = code;
	scale = vdev->scale;
	gx_path_enum_init(&path, ppath);

	while (!ecode) {

		op = gx_path_enum_next(&path, (gs_fixed_point *)vs);
		if (begin) {
			/* start point */
			start.x = fixed2float(vs[0]) / scale.x;
			start.y = fixed2float(vs[1]) / scale.y;
			begin = false;

#ifdef	OPVP_OPT_MULTI_PATH
			npoints = 1;
			points = realloc(points, sizeof(_fPoint));
			current = start;
#endif

			points[0] = start;

#ifdef	OPVP_OPT_MULTI_PATH

		} else if (op != pop) {

			/* convert float to Fix */
			opvp_p = realloc(opvp_p, sizeof(OPVP_Point) * npoints);
			for (i = 0; i < npoints; i++) {
				OPVP_f2Fix(points[i].x, opvp_p[i].x);
				OPVP_f2Fix(points[i].y, opvp_p[i].y);
			}

			switch (pop) {

				case	gs_pe_moveto :

					/* call SetCurrentPoint */
					if (apiEntry->SetCurrentPoint)
					code = apiEntry->SetCurrentPoint(
					           printerContext,
					           opvp_p[npoints-1].x,
					           opvp_p[npoints-1].y);
					if (code != OPVP_OK) ecode = -1;

					break;

				case	gs_pe_lineto :

					/* call LinePath */
					if (apiEntry->LinePath)
					code = apiEntry->LinePath(
					           printerContext,
					           OPVP_PathOpen,
					           npoints - 1,
					           &(opvp_p[1]));
					if (code != OPVP_OK) ecode = -1;

					break;

				case	gs_pe_curveto :

					/* npoints */
					if (!cp_num)
					cp_num = calloc(sizeof(int), 2);
					cp_num[0] = npoints;
					cp_num[1] = 0;

					/* call BezierPath */
					if (apiEntry->BezierPath)
					code = apiEntry->BezierPath(
					           printerContext,
#if (_PDAPI_VERSION_MAJOR_ == 0 && _PDAPI_VERSION_MINOR_ < 2)
					           cp_num,
					           opvp_p
#else
					           npoints - 1,
					           &(opvp_p[1])
#endif
					           );
					if (code != OPVP_OK) ecode = -1;

					break;

				case	gs_pe_closepath :

					/* close path */
					break;

				default :

					/* error */
					return_error(gs_error_unknownerror);

					break;
			}

			/* reset */
			npoints = 1;
			if (cp_num) free(cp_num), cp_num = NULL;
			points = realloc(points, sizeof(_fPoint));
			points[0] = current;

#endif
		}

		if (!op) break; /* END */

		switch (op) {

			case	gs_pe_moveto :

#ifdef	OPVP_OPT_MULTI_PATH

				/* move to */
				i = npoints;
				npoints += 1;
				points = realloc(points,
				                 sizeof(_fPoint) * npoints);
				points[i].x = fixed2float(vs[0]) / scale.x;
				points[i].y = fixed2float(vs[1]) / scale.y;
				current = points[i];
				start = current;

#else

				/* move to */
				points[1].x = fixed2float(vs[0]) / scale.x;
				points[1].y = fixed2float(vs[1]) / scale.y;
				code = (*vdev_proc(vdev, moveto))(vdev,
				                                  points[0].x,
				                                  points[0].y,
				                                  points[1].x,
				                                  points[1].y,
				                                  type);
				if (code) ecode = code;
				points[0] = points[1];
				start = points[0];
#endif

				break;

			case	gs_pe_lineto :

#ifdef	OPVP_OPT_MULTI_PATH

				/* line to */
				i = npoints;
				npoints += 1;
				points = realloc(points,
				                 sizeof(_fPoint) * npoints);
				points[i].x = fixed2float(vs[0]) / scale.x;
				points[i].y = fixed2float(vs[1]) / scale.y;
				current = points[i];

#else

				/* line to */
				points[1].x = fixed2float(vs[0]) / scale.x;
				points[1].y = fixed2float(vs[1]) / scale.y;
				code = (*vdev_proc(vdev, lineto))(vdev,
				                                  points[0].x,
				                                  points[0].y,
				                                  points[1].x,
				                                  points[1].y,
				                                  type);
				if (code) ecode = code;
				points[0] = points[1];
#endif

				break;

			case	gs_pe_curveto :

#ifdef	OPVP_OPT_MULTI_PATH

				/* curve to */
				check_p.x = fixed2float(vs[0]) / scale.x;
				check_p.y = fixed2float(vs[1]) / scale.y;

				i = npoints;
				npoints += 3;
				points = realloc(points,
				                 sizeof(_fPoint) * npoints);
				points[i  ].x = fixed2float(vs[0]) / scale.x;
				points[i  ].y = fixed2float(vs[1]) / scale.y;
				points[i+1].x = fixed2float(vs[2]) / scale.x;
				points[i+1].y = fixed2float(vs[3]) / scale.y;
				points[i+2].x = fixed2float(vs[4]) / scale.x;
				points[i+2].y = fixed2float(vs[5]) / scale.y;
				current = points[i+2];

#else

				/* curve to */
				points[1].x = fixed2float(vs[0]) / scale.x;
				points[1].y = fixed2float(vs[1]) / scale.y;
				points[2].x = fixed2float(vs[2]) / scale.x;
				points[2].y = fixed2float(vs[3]) / scale.y;
				points[3].x = fixed2float(vs[4]) / scale.x;
				points[3].y = fixed2float(vs[5]) / scale.y;
				code = (*vdev_proc(vdev, curveto))(vdev,
				                                   points[0].x,
				                                   points[0].y,
				                                   points[1].x,
				                                   points[1].y,
				                                   points[2].x,
				                                   points[2].y,
				                                   points[3].x,
				                                   points[3].y,
				                                   type);
				if (code) ecode = code;
				points[0] = points[3];
#endif

				break;

			case	gs_pe_closepath :

				/* close path */
				code = (*vdev_proc(vdev, closepath))(
				           vdev,
				           points[0].x,
				           points[0].y,
				           start.x,
				           start.y,
				           type);
				if (code) ecode = code;
				points[0] = start;

#ifdef	OPVP_OPT_MULTI_PATH
				current = start;
#endif

				break;

			default :

				/* error */
				return_error(gs_error_unknownerror);

				break;
		}

#ifdef	OPVP_OPT_MULTI_PATH
		pop = op;
#endif
	}

	/* end path */
	code = (*vdev_proc(vdev, endpath))(vdev, type);
	if (code) ecode = code;

#ifdef	OPVP_OPT_MULTI_PATH
	if (points) free(points);
	if (opvp_p) free(opvp_p);
	if (cp_num) free(cp_num);
#endif
	return ecode;
}

/*
 * vector do rect
 */
static	int
opvp_vector_dorect(
	gx_device_vector	*vdev,
	fixed			x0,
	fixed			y0,
	fixed			x1,
	fixed			y1,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	gs_point		scale;
	OPVP_Rectangle		rectangles[1];
	_fPoint			p;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* begin path */
	code = (*vdev_proc(vdev, beginpath))(vdev, type);
	if (code) ecode = code;
	scale = vdev->scale;

	if (!ecode) {

		/* rectangle */
		p.x = fixed2float(x0) / scale.x;
		p.y = fixed2float(y0) / scale.y;
		OPVP_f2Fix(p.x, rectangles[0].p0.x);
		OPVP_f2Fix(p.y, rectangles[0].p0.y);
		p.x = fixed2float(x1) / scale.x;
		p.y = fixed2float(y1) / scale.y;
		OPVP_f2Fix(p.x, rectangles[0].p1.x);
		OPVP_f2Fix(p.y, rectangles[0].p1.y);

		/* call RectanglePath */
		if (apiEntry->RectanglePath)
		code = apiEntry->RectanglePath(printerContext,
		                               1,
		                               rectangles);
		if (code != OPVP_OK) {
			ecode = -1;
		}

	}

	/* end path */
	if (!ecode) {
		code = (*vdev_proc(vdev, endpath))(vdev, type);
		if (code) ecode = code;
	}

	/* fallback */
	if (ecode) return gdev_vector_dorect(vdev,x0,y0,x1,y1,type);

	return ecode;
}

/*
 * begin path
 */
static	int
opvp_beginpath(
	gx_device_vector	*vdev,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

#if (_PDAPI_VERSION_MAJOR_ > 0 || _PDAPI_VERSION_MINOR_ >= 2)
	/* check clip-path */
	if (type & gx_path_type_clip) {
		if (apiEntry->ResetClipPath)
		apiEntry->ResetClipPath(printerContext);
	}
#endif

	/* call NewPath */
	if (apiEntry->NewPath)
	code = apiEntry->NewPath(printerContext);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * move to
 */
static	int
opvp_moveto(
	gx_device_vector	*vdev,
	floatp			x0,
	floatp			y0,
	floatp			x1,
	floatp			y1,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Point		p;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* call SetCurrentPoint */
	OPVP_f2Fix(x1, p.x);
	OPVP_f2Fix(y1, p.y);
	if (apiEntry->SetCurrentPoint)
	code = apiEntry->SetCurrentPoint(printerContext, p.x, p.y);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * line to
 */
static	int
opvp_lineto(
	gx_device_vector	*vdev,
	floatp			x0,
	floatp			y0,
	floatp			x1,
	floatp			y1,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Point		points[1];

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* point */
	OPVP_f2Fix(x1, points[0].x);
	OPVP_f2Fix(y1, points[0].y);
	
	/* call LinePath */
	if (apiEntry->LinePath)
	code = apiEntry->LinePath(printerContext, OPVP_PathOpen, 1, points);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * curve to
 */
static	int
opvp_curveto(
	gx_device_vector	*vdev,
	floatp			x0,
	floatp			y0,
	floatp			x1,
	floatp			y1,
	floatp			x2,
	floatp			y2,
	floatp			x3,
	floatp			y3,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	int			npoints[2];
	OPVP_Point		points[4];

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* points */
	npoints[0] = 4;
	npoints[1] = 0;
	OPVP_f2Fix(x0, points[0].x);
	OPVP_f2Fix(y0, points[0].y);
	OPVP_f2Fix(x1, points[1].x);
	OPVP_f2Fix(y1, points[1].y);
	OPVP_f2Fix(x2, points[2].x);
	OPVP_f2Fix(y2, points[2].y);
	OPVP_f2Fix(x3, points[3].x);
	OPVP_f2Fix(y3, points[3].y);
	
	/* call BezierPath */
	if (apiEntry->BezierPath)
	code = apiEntry->BezierPath(printerContext,
#if (_PDAPI_VERSION_MAJOR_ == 0 && _PDAPI_VERSION_MINOR_ < 2)
	                            npoints,
	                            points
#else
	                            3,
	                            &(points[1])
#endif
	                            );
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * close path
 */
static	int
opvp_closepath(
	gx_device_vector	*vdev,
	floatp			x,
	floatp			y,
	floatp			x_start,
	floatp			y_start,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;
	OPVP_Point		points[1];

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* point */
	OPVP_f2Fix(x_start, points[0].x);
	OPVP_f2Fix(y_start, points[0].y);
	
	/* call LinePath */
	if (apiEntry->LinePath)
	code = apiEntry->LinePath(printerContext, OPVP_PathClose, 1, points);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	return ecode;
}

/*
 * end path
 */
static	int
opvp_endpath(
	gx_device_vector	*vdev,
	gx_path_type_t		type
	)
{
	gx_device_opvp		*pdev = (gx_device_opvp *)vdev;
	int			code = -1;
	int			ecode = 0;

	/* check page-in */
	if (opvp_check_in_page(pdev)) return -1;

	/* call EndPath */
	if (apiEntry->EndPath)
	code = apiEntry->EndPath(printerContext);
	if (code != OPVP_OK) {
		ecode = -1;
	}

	if (type & gx_path_type_fill) {

		/* fill mode */
		if (type & gx_path_type_even_odd) {
			/* call SetFillMode */
			if (apiEntry->SetFillMode)
			code = apiEntry->SetFillMode(
			           printerContext,
			           OPVP_fillModeEvenOdd
			       );
			if (code != OPVP_OK) {
				ecode = -1;
			}
		} else {
			/* call SetFillMode */
			if (apiEntry->SetFillMode)
			code = apiEntry->SetFillMode(
			           printerContext,
			           OPVP_fillModeWinding
			       );
			if (code != OPVP_OK) {
				ecode = -1;
			}
		}

		if (type & gx_path_type_stroke) {
			/* call StrokeFillPath */
			if (apiEntry->StrokeFillPath)
			code = apiEntry->StrokeFillPath(
			           printerContext);
			if (code != OPVP_OK) {
				ecode = -1;
			}
		} else {
			/* call FillPath */
			if (apiEntry->FillPath)
			code = apiEntry->FillPath(printerContext);
			if (code != OPVP_OK) {
				ecode = -1;
			}
		}

	} else if (type & gx_path_type_clip) {

		/* call SetClipPath */
		if (apiEntry->SetClipPath)
		code = apiEntry->SetClipPath(
		           printerContext,
		           (type & gx_path_type_even_odd ? OPVP_clipRuleEvenOdd
		                                         : OPVP_clipRuleWinding)
		       );
		if (code != OPVP_OK) {
			ecode = -1;
		}

	} else if (type & gx_path_type_stroke) {

		/* call StrokePath */
		if (apiEntry->StrokePath)
		code = apiEntry->StrokePath(printerContext);
		if (code != OPVP_OK) {
			ecode = -1;
		}

	}

	return ecode;
}
