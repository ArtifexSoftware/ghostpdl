/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gdevpdfp.c */
/* Get/put parameters for PDF-writing driver */
#include "gx.h"
#include "gserrors.h"
#include "gdevpdfx.h"

/*
 * The pdfwrite device supports the following "real" parameters:
 *	OutputFile <string>
 *	(all the Distiller parameters except *ImageDict)
 * Currently, very few Distiller parameters actually have any effect.
 *
 * The device also supports the following write-only pseudo-parameters that
 * serve only to communicate other information from the PostScript file.
 * Their "value" is an array of strings, some of which may be the result
 * of converting arbitrary PostScript objects to string form.
 *	pdfmark - see gdevpdfm.c
 *	show - see gdevpdft.c
 */

private const int CoreDistVersion = 3000;	/* Distiller 3.0 */

/* ---------------- Get parameters ---------------- */

/* Get parameters. */
int
gdev_pdf_get_params(gx_device *dev, gs_param_list *plist)
{	gx_device_pdf *pdev = (gx_device_pdf *)dev;
	int code = gdev_psdf_get_params(dev, plist);

	if ( code < 0 ||
	     (code = param_write_float(plist, "CompatibilityLevel",
				       &pdev->CompatibilityLevel)) < 0 ||
	     (code = param_write_int(plist, "CoreDistVersion",
				     (int *)&CoreDistVersion)) < 0 ||
			/* ****** DoThumbnails is OBSOLETE ****** */
	     (code = param_write_bool(plist, "DoThumbnails",
				      &pdev->DoThumbnails)) < 0 ||
	     (code = param_write_bool(plist, "ReAssignCharacters",
				      &pdev->ReAssignCharacters)) < 0 ||
	     (code = param_write_bool(plist, "ReEncodeCharacters",
				      &pdev->ReEncodeCharacters)) < 0 ||
	     (code = param_write_long(plist, "FirstObjectNumber",
				      &pdev->FirstObjectNumber)) < 0
	   )
	  ;
	return code;
}

/* ---------------- Put parameters ---------------- */

/* Put parameters. */
int
gdev_pdf_put_params(gx_device *dev, gs_param_list *plist)
{	gx_device_pdf *pdev = (gx_device_pdf *)dev;
	int ecode = 0;
	int code;
	float cl = pdev->CompatibilityLevel;
	bool dt = pdev->DoThumbnails;
	bool rac = pdev->ReAssignCharacters;
	bool rec = pdev->ReEncodeCharacters;
	long fon = pdev->FirstObjectNumber;
	gs_param_name param_name;

		/* General parameters. */

	switch ( code = param_read_float(plist, (param_name = "CompatibilityLevel"), &cl) )
	{
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 0:
	case 1:
		break;
	}

	{ int cdv = CoreDistVersion;
	  ecode = psdf_put_int_param(plist, (param_name = "CoreDistVersion"), &cdv, ecode);
	  if ( cdv != CoreDistVersion )
	    param_signal_error(plist, param_name, ecode = gs_error_rangecheck);
	}
			/* ****** DoThumbnails is OBSOLETE ****** */
	ecode = psdf_put_bool_param(plist, "DoThumbnails", &dt, ecode);

	ecode = psdf_put_bool_param(plist, "ReAssignCharacters", &rac, ecode);
	ecode = psdf_put_bool_param(plist, "ReEncodeCharacters", &rec, ecode);
	switch ( code = param_read_long(plist, (param_name = "FirstObjectNumber"), &fon) )
	{
	case 0:
		/*
		 * Setting this parameter is only legal if the file
		 * has just been opened and nothing has been written,
		 * or if we are setting it to the same value.
		 */
		if ( fon == pdev->FirstObjectNumber )
		  break;
		if ( fon <= 0 || fon > 0x7fff0000 ||
		     (pdev->next_id != 0 &&
		      pdev->next_id !=
		        pdev->FirstObjectNumber + pdf_num_initial_ids)
		   )
		  ecode = gs_error_rangecheck;
		else
		  break;
	default:
		ecode = code;
		param_signal_error(plist, param_name, ecode);
	case 1:
		break;
	}

	/* Handle pseudo-parameters. */

	{ gs_param_string_array ppa;
	  switch ( code = param_read_string_array(plist,
						  (param_name = "pdfmark"),
						  &ppa) )
	  {
	  case 0:
	    pdf_open_document(pdev);
	    code = pdfmark_process(pdev, &ppa);
	    if ( code >= 0 )
	      break;
	    /* falls through for errors */
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	  case 1:
	    break;
	  }
	}

	{ gs_param_dict ppd;
	  switch ( code = param_begin_read_dict(plist,
						(param_name = "show"),
						&ppd, false) )
	  {
	  case 0:
	    pdf_open_document(pdev);
	    code = pdfshow_process(pdev, &ppd);
	    param_end_read_dict(plist, param_name, &ppd);
	    if ( code >= 0 )
	      break;
	    /* falls through for errors */
	  default:
	    ecode = code;
	    param_signal_error(plist, param_name, ecode);
	  case 1:
	    break;
	  }
	}

	if ( ecode < 0 )
	  return ecode;
	code = gdev_psdf_put_params(dev, plist);
	if ( code < 0 )
	  return code;

	pdev->CompatibilityLevel = cl;
	pdev->DoThumbnails = dt;
	pdev->ReAssignCharacters = rac;
	pdev->ReEncodeCharacters = rec;
	if ( fon != pdev->FirstObjectNumber )
	  { pdev->FirstObjectNumber = fon;
	    if ( pdev->tfile != 0 )
	      { fseek(pdev->tfile, 0L, SEEK_SET);
		pdf_initialize_ids(pdev);
	      }
	  }
	return 0;
}
