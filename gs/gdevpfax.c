/* Copyright (C) 1993, 1994, 1998 Aladdin Enterprises.  All rights reserved.

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

/*Id: gdevpfax.c  */
/* Generic PostScript system compatible fax support */
#include "gdevprn.h"
#include "gsparam.h"
#include "gxiodev.h"

/* This code defines a %Fax% IODevice, and also provides decoding for */
/* the FaxOptions dictionary in a page device. */
/* It is still fragmentary and incomplete. */

#define limited_string(len)\
  struct { byte data[len]; uint size; }

/* ------ %Fax% implementation ------ */

/* Define the structure for the Fax IODevice state. */
typedef struct gx_io_device_fax_state_s {
    bool ActivityReport;
    bool DefaultCaptionOn;
    bool DefaultConfirmOn;
    bool DefaultCoversOn;
    int DefaultResolution;
    int DefaultRetryCount;
    int DefaultRetryInterval;
    int DialToneWaitPeriod;
        limited_string(20) ID;
    long MaxFaxBuffer;
         limited_string(32) PostScriptPassword;
    bool ReceivePostScript;
    int Rings;
    int ServiceEnable;
    int Speaker;
    const char *StorageDevice;
    bool WaitForDialTone;
} gx_io_device_fax_state_t;

private iodev_proc_get_params(fax_get_params);
private iodev_proc_put_params(fax_put_params);
const gx_io_device gs_iodev_Fax =
{
    "%Fax%", "Parameters",
    {iodev_no_init, iodev_no_open_device, iodev_no_open_file,
     iodev_os_fopen, iodev_os_fclose,
     iodev_no_delete_file, iodev_no_rename_file, iodev_no_file_status,
     iodev_no_enumerate_files, NULL, NULL,
     fax_get_params, fax_put_params
    }
};
private const gx_io_device_fax_state_t gx_io_device_fax_state_default =
{
    false,			/* A */
    true, true, true, 1, 0, 3, 1,	/* D */
    {
	{0}, 0},		/* I */
    350000,			/* M */
    {
	{0}, 0},		/* P */
    true, 4 /* ? */ ,		/* R */
    3, 1, "%ram%",		/* S */
    true			/* W */
};

/* The following code is shared between get and put parameters. */
typedef struct fax_strings_s {
    gs_param_string id, pwd, sd;
} fax_strings;
private int
fax_xfer_params(gx_io_device_fax_state_t * fds, gs_param_list * plist,
		fax_strings * pfs)
{
    int code;

    pfs->id.data = fds->ID.data, pfs->id.size = fds->ID.size,
	pfs->id.persistent = false;
    pfs->pwd.data = fds->PostScriptPassword.data,
	pfs->pwd.size = fds->PostScriptPassword.size,
	pfs->pwd.persistent = false;
    pfs->sd.data = (const byte *)fds->StorageDevice,
	pfs->sd.size = strlen(pfs->sd.data),
	pfs->sd.persistent = true;
    if (
	   (code = param_bool_param(plist, "ActivityReport", &fds->ActivityReport)) < 0 ||
	   (code = param_bool_param(plist, "DefaultCaptionOn", &fds->DefaultCaptionOn)) < 0 ||
	   (code = param_bool_param(plist, "DefaultConfirmOn", &fds->DefaultConfirmOn)) < 0 ||
	   (code = param_bool_param(plist, "DefaultCoversOn", &fds->DefaultCoversOn)) < 0 ||
	   (code = param_int_param(plist, "DefaultResolution", &fds->DefaultResolution)) < 0 ||
	   (code = param_int_param(plist, "DefaultRetryCount", &fds->DefaultRetryCount)) < 0 ||
	   (code = param_int_param(plist, "DefaultRetryInterval", &fds->DefaultRetryInterval)) < 0 ||
	   (code = param_int_param(plist, "DialToneWaitPeriod", &fds->DialToneWaitPeriod)) < 0 ||
	   (code = param_string_param(plist, "ID", &pfs->id)) < 0 ||
	   (code = param_long_param(plist, "MaxFaxBuffer", &fds->MaxFaxBuffer)) < 0 ||
	   (code = param_string_param(plist, "PostScriptPassword", &pfs->pwd)) < 0 ||
	   (code = param_bool_param(plist, "ReceivePostScript", &fds->ReceivePostScript)) < 0 ||
	   (code = param_int_param(plist, "Rings", &fds->Rings)) < 0 ||
	   (code = param_int_param(plist, "ServiceEnable", &fds->ServiceEnable)) < 0 ||
	   (code = param_int_param(plist, "Speaker", &fds->Speaker)) < 0 ||
	(code = param_string_param(plist, "StorageDevice", &pfs->sd)) < 0 ||
	   (code = param_bool_param(plist, "WaitForDialTone", &fds->WaitForDialTone))
	)
	return code;
    return 0;
}

/* Get parameters from device. */
private int
fax_get_params(gx_io_device * iodev, gs_param_list * plist)
{
    fax_strings fs;

    return fax_xfer_params((gx_io_device_fax_state_t *) iodev->state,
			   plist, &fs);
}

/* Put parameters to device. */
private int
fax_put_params(gx_io_device * iodev, gs_param_list * plist)
{
    gx_io_device_fax_state_t *const fds = iodev->state;
    gx_io_device_fax_state_t new_state;
    fax_strings fs;
    int code;
    gx_io_device *sdev;

    new_state = *fds;
    code = fax_xfer_params(&new_state, plist, &fs);
    if (code < 0)
	return code;
#define between(v, lo, hi) (new_state.v >= (lo) && new_state.v <= (hi))
    if (!(between(DefaultResolution, 0, 1) &&
	  between(DefaultRetryCount, 0, 100) &&
	  between(DefaultRetryInterval, 1, 60) &&
	  between(DialToneWaitPeriod, 1, 10) &&
	  fs.id.size <= 20 &&
	  new_state.MaxFaxBuffer >= 350000 &&
	  fs.pwd.size <= 32 &&
	  between(Rings, 1, 30) &&
	  between(ServiceEnable, 0, 3) &&
	  between(Speaker, 0, 2) &&
	  (sdev = gs_findiodevice(fs.sd.data, fs.sd.size)) != 0
	)
	)
	return_error(gs_error_rangecheck);
#undef between
    memcpy(new_state.ID.data, fs.id.data, fs.id.size);
    new_state.ID.size = fs.id.size;
    memcpy(new_state.PostScriptPassword.data, fs.pwd.data, fs.pwd.size);
    new_state.PostScriptPassword.size = fs.pwd.size;
    new_state.StorageDevice = sdev->dname;
    *fds = new_state;
    return 0;
}

/* ------ FaxOptions decoding ------ */

typedef struct fax_options_s fax_options_t;
typedef struct fax_custom_params_s fax_custom_params_t;
typedef int (*fax_custom_proc) (P2(const fax_options_t *,
				   const fax_custom_params_t *));
struct fax_options_s {
    gs_param_string CalleePhone;
                    limited_string(20) CallerID;
    gs_param_string CallerPhone;
    fax_custom_proc Confirmation;
    struct {
	fax_options_t *options;
	uint size;
    } Copies;
    /* CoverNote */
    fax_custom_proc CoverSheet;
    bool CoverSheetOnly;
         limited_string(100) DialCallee;
    bool ErrorCorrect;
    int FaxType;
    int MailingTime[6];
    int MaxRetries;
    int nPages;
    fax_custom_proc PageCaption;
                    limited_string(32) PostScriptPassword;
    void *ProcInfo;
    gs_param_string RecipientID;
    gs_param_string RecipientMailStop;
    gs_param_string RecipientName;
    gs_param_string RecipientOrg;
    gs_param_string RecipientPhone;
    gs_param_string Regarding;
    int RetryInterval;
    bool RevertToRaster;
    gs_param_string SenderID;
    gs_param_string SenderMailStop;
    gs_param_string SenderName;
    gs_param_string SenderOrg;
    gs_param_string SenderPhone;
    bool TrimWhite;
};

/* ------ Custom fax procedure parameters ------ */

struct fax_custom_params_s {
    limited_string(20) CalleeID;
    int CallLength;
    int CoverType;
    int CurrentPageNo;
    /* ErrorArray */
    int ErrorIndex;
    bool IncludesFinalPage;
    int InitialPage, LimitPage;
    int NumberOfCalls;
    int PagesSent;
    bool SendPostScript;
    int TimeSent[6];
};
