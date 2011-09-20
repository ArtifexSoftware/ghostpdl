/* Language wrapper for fitz/mupdf/muxps based interpreters */

#include "mooscript.h"

#define PARSER_MIN_INPUT_SIZE (8192 * 4)

typedef struct moo_interp_instance_s moo_interp_instance_t;

struct moo_interp_instance_s
{
	pl_interp_instance_t pl; /* common part: must be first */

	pl_page_action_t pre_page_action; /* action before page out */
	void *pre_page_closure; /* closure to call pre_page_action with */
	pl_page_action_t post_page_action; /* action before page out */
	void *post_page_closure; /* closure to call post_page_action with */

	FILE *scratch_file;
	char scratch_name[gp_file_name_sizeof];

	gs_memory_t *memory;
	gs_state *pgs;
};

static int moo_install_halftone(gs_state *pgs, gx_device *pdevice);

static fz_matrix
fz_matrix_from_gs_matrix(gs_matrix ctm)
{
	fz_matrix m;
	m.a = ctm.xx;
	m.b = ctm.xy;
	m.c = ctm.yx;
	m.d = ctm.yy;
	m.e = ctm.tx;
	m.f = ctm.ty;
	return m;
}

static int
moo_show_page(pl_interp_instance_t *pinstance, int num_copies, int flush)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;

	int code = 0;

	/* do pre-page action */
	if (instance->pre_page_action)
	{
		code = instance->pre_page_action(pinstance, instance->pre_page_closure);
		if (code < 0)
			return code;
		if (code != 0)
			return 0;    /* code > 0 means abort w/no error */
	}

	/* output the page */
	code = gs_output_page(instance->pgs, num_copies, flush);
	if (code < 0)
		return code;

	/* do post-page action */
	if (instance->post_page_action)
	{
		code = instance->post_page_action(pinstance, instance->post_page_closure);
		if (code < 0)
			return code;
	}

	return 0;
}

static fz_error
moo_draw_pdf_page(pl_interp_instance_t *instance, gs_memory_t *memory, gs_state *pgs, fz_device *ghost, pdf_xref *xref, int number)
{
	fz_error error;
	pdf_page *page;
	gs_matrix matrix;
	fz_matrix ctm;

	error = pdf_load_page(&page, xref, number);
	if (error)
		return fz_rethrow(error, "cannot load page %d", number);

	{
		gs_param_float_array fa;
		float fv[2];
		gs_c_param_list list;

		fv[0] = page->mediabox.x1 - page->mediabox.x0;
		fv[1] = page->mediabox.y1 - page->mediabox.y0;
		fa.persistent = false;
		fa.data = fv;
		fa.size = 2;

		gs_c_param_list_write(&list, memory);
		param_write_float_array((gs_param_list *)&list, ".MediaSize", &fa);
		gs_c_param_list_read(&list);
		gs_putdeviceparams(gs_currentdevice(pgs), (gs_param_list *)&list);
		gs_c_param_list_release(&list);
	}

	gs_initgraphics(pgs);
	gs_initmatrix(pgs);
	gs_erasepage(pgs);

	gs_currentmatrix(pgs, &matrix);
	ctm = fz_matrix_from_gs_matrix(matrix);

	error = pdf_run_page(xref, page, ghost, ctm);
	if (error) {
		pdf_free_page(page);
		return fz_rethrow(error, "cannot interpret page %d", number);
	}

	pdf_free_page(page);
	pdf_age_store(xref->store, 3);
	fz_flush_warnings();

	moo_show_page(instance, 1, 1);

	return fz_okay;
}

int
moo_draw_pdf(pl_interp_instance_t *instance, gs_memory_t *memory, gs_state *pgs, char *filename)
{
	pdf_xref *xref;
	fz_device *ghost;
	char *password = "";
	fz_error error;
	int i;

	error = pdf_open_xref(&xref, filename, password);
	if (error) {
		fz_catch(error, "cannot open file '%s'", filename);
		return -1;
	}

	error = pdf_load_page_tree(xref);
	if (error) {
		fz_catch(error, "cannot load page tree in '%s'", filename);
		return -1;
	}

	ghost = fz_new_ghost_device(memory, pgs);

	for (i = 0; i < pdf_count_pages(xref); i++) {
		error = moo_draw_pdf_page(instance, memory, pgs, ghost, xref, i);
		if (error)
			fz_catch(error, "cannot draw page %d in '%s'", i+1, filename);
	}

	fz_free_device(ghost);

	pdf_free_xref(xref);
	fz_flush_warnings();
	return 0;
}

/* version and build date are not currently used */
#define VERSION NULL
#define BUILD_DATE NULL

static const pl_interp_characteristics_t *
moo_imp_characteristics(const pl_interp_implementation_t *pimpl)
{
	static pl_interp_characteristics_t moo_characteristics = {
		"MuPDF",
		"%PDF", /* string to recognize our files */
		"Artifex",
		VERSION,
		BUILD_DATE,
		PARSER_MIN_INPUT_SIZE, /* Minimum input size */
	};
	return &moo_characteristics;
}

static int
moo_imp_allocate_interp(pl_interp_t **ppinterp, const pl_interp_implementation_t *pimpl, gs_memory_t *pmem)
{
	static pl_interp_t interp; /* there's only one interpreter */
	*ppinterp = &interp;
	return 0;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
moo_imp_allocate_interp_instance(pl_interp_instance_t **ppinstance, pl_interp_t *pinterp, gs_memory_t *pmem)
{
	moo_interp_instance_t *instance;
	gs_state *pgs;

	instance = (moo_interp_instance_t *) gs_alloc_bytes(pmem, sizeof(moo_interp_instance_t), "moo_imp_allocate_interp_instance");

	pgs = gs_state_alloc(pmem);

	if (!instance || !pgs) {
		if (instance)
			gs_free_object(pmem, instance, "moo_imp_allocate_interp_instance");
		if (pgs)
			gs_state_free(pgs);
		return gs_error_VMerror;
	}

	gsicc_init_iccmanager(pgs);

	/* Declare PDL client support for high level patterns, for the benefit
	 * of pdfwrite and other high-level devices */
//	pgs->have_pattern_streams = true;

	instance->memory = pmem;
	instance->pgs = pgs;

	instance->pre_page_action = 0;
	instance->pre_page_closure = 0;
	instance->post_page_action = 0;
	instance->post_page_closure = 0;

	instance->scratch_file = NULL;
	instance->scratch_name[0] = 0;

	*ppinstance = (pl_interp_instance_t *)instance;

	return 0;
}

/* Set a client language into an interperter instance */
static int
moo_imp_set_client_instance(pl_interp_instance_t *pinstance, pl_interp_instance_t *pclient, pl_interp_instance_clients_t which_client)
{
	/* ignore */
	return 0;
}

static int
moo_imp_set_pre_page_action(pl_interp_instance_t *pinstance, pl_page_action_t action, void *closure)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	instance->pre_page_action = action;
	instance->pre_page_closure = closure;
	return 0;
}

static int
moo_imp_set_post_page_action(pl_interp_instance_t *pinstance, pl_page_action_t action, void *closure)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	instance->post_page_action = action;
	instance->post_page_closure = closure;
	return 0;
}

static int
moo_imp_set_device(pl_interp_instance_t *pinstance, gx_device *pdevice)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	int code;

	gs_opendevice(pdevice);

	code = gsicc_init_device_profile_struct(pdevice, NULL, 0);
	if (code < 0)
		return code;

	code = gs_setdevice_no_erase(instance->pgs, pdevice);
	if (code < 0)
		goto cleanup_setdevice;

	gs_setaccuratecurves(instance->pgs, true); /* NB not sure */
	gs_setfilladjust(instance->pgs, 0, 0);

	/* gsave and grestore (among other places) assume that */
	/* there are at least 2 gstates on the graphics stack. */
	/* Ensure that now. */
	code = gs_gsave(instance->pgs);
	if (code < 0)
		goto cleanup_gsave;

	code = gs_erasepage(instance->pgs);
	if (code < 0)
		goto cleanup_erase;

	code = moo_install_halftone(instance->pgs, pdevice);
	if (code < 0)
		goto cleanup_halftone;

	return 0;

cleanup_halftone:
cleanup_erase:
	/* undo gsave */
	gs_grestore_only(instance->pgs);     /* destroys gs_save stack */

cleanup_gsave:
	/* undo setdevice */
	gs_nulldevice(instance->pgs);

cleanup_setdevice:
	/* nothing to undo */
	return code;
}

static int
moo_imp_get_device_memory(pl_interp_instance_t *pinstance, gs_memory_t **ppmem)
{
	/* huh? we do nothing here */
	return 0;
}

/* Parse an entire random access file */
static int
moo_imp_process_file(pl_interp_instance_t *pinstance, char *filename)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	dprintf1("do stuff with file '%s'\n", filename);
	return moo_draw_pdf(pinstance, instance->memory, instance->pgs, filename);
}

/* Parse a cursor-full of data */
static int
moo_imp_process(pl_interp_instance_t *pinstance, stream_cursor_read *cursor)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	int avail, n;

	if (!instance->scratch_file)
	{
		instance->scratch_file = gp_open_scratch_file(instance->memory,
				"ghostmoo-scratch-", instance->scratch_name, "wb");
		if (!instance->scratch_file)
		{
			gs_catch(gs_error_invalidfileaccess, "cannot open scratch file");
			return e_ExitLanguage;
		}
		if_debug1('|', "moo: open scratch file '%s'\n", instance->scratch_name);
	}

	avail = cursor->limit - cursor->ptr;
	n = fwrite(cursor->ptr + 1, 1, avail, instance->scratch_file);
	if (n != avail)
	{
		gs_catch(gs_error_invalidfileaccess, "cannot write to scratch file");
		return e_ExitLanguage;
	}
	cursor->ptr = cursor->limit;

	return 0;
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
moo_imp_flush_to_eoj(pl_interp_instance_t *pinstance, stream_cursor_read *pcursor)
{
	/* assume we cannot be pjl embedded */
	pcursor->ptr = pcursor->limit;
	return 0;
}

/* Parser action for end-of-file */
static int
moo_imp_process_eof(pl_interp_instance_t *pinstance)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	int code;

	if (instance->scratch_file)
	{
		if_debug0('|', "moo: executing scratch file\n");
		fclose(instance->scratch_file);
		instance->scratch_file = NULL;
		code = moo_imp_process_file(pinstance, instance->scratch_name);
		unlink(instance->scratch_name);
		if (code < 0)
		{
			gs_catch(code, "cannot process file");
			return e_ExitLanguage;
		}
	}

	return 0;
}

/* Report any errors after running a job */
static int
moo_imp_report_errors(pl_interp_instance_t *pinstance,
		int code,           /* prev termination status */
		long file_position, /* file position of error, -1 if unknown */
		bool force_to_cout  /* force errors to cout */
		)
{
	return 0;
}

/* Prepare interp instance for the next "job" */
static int
moo_imp_init_job(pl_interp_instance_t *pinstance)
{
	return 0;
}

/* Wrap up interp instance after a "job" */
static int
moo_imp_dnit_job(pl_interp_instance_t *pinstance)
{
	return 0;
}

/* Remove a device from an interperter instance */
static int
moo_imp_remove_device(pl_interp_instance_t *pinstance)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;

	int code = 0; /* first error status encountered */
	int error;

	/* return to original gstate */
	gs_grestore_only(instance->pgs); /* destroys gs_save stack */

	/* Deselect device */
	/* NB */
	error = gs_nulldevice(instance->pgs);
	if (code >= 0)
		code = error;

	return code;
}

/* Deallocate a interpreter instance */
static int
moo_imp_deallocate_interp_instance(pl_interp_instance_t *pinstance)
{
	moo_interp_instance_t *instance = (moo_interp_instance_t *)pinstance;
	gs_memory_t *mem = instance->memory;

	/* language clients don't free the font cache machinery */

	// free gstate?
	gs_free_object(mem, instance, "moo_imp_deallocate_interp_instance");

	return 0;
}

/* Do static deinit of interpreter */
static int
moo_imp_deallocate_interp(pl_interp_t *pinterp)
{
	return 0;
}

/*
 * We need to install a halftone ourselves, this is not
 * done automatically.
 */

static float
identity_transfer(floatp tint, const gx_transfer_map *ignore_map)
{
    return tint;
}

/* The following is a 45 degree spot screen with the spots enumerated
 * in a defined order. */
static byte order16x16[256] = {
    38, 11, 14, 32, 165, 105, 90, 171, 38, 12, 14, 33, 161, 101, 88, 167,
    30, 6, 0, 16, 61, 225, 231, 125, 30, 6, 1, 17, 63, 222, 227, 122,
    27, 3, 8, 19, 71, 242, 205, 110, 28, 4, 9, 20, 74, 246, 208, 106,
    35, 24, 22, 40, 182, 46, 56, 144, 36, 25, 22, 41, 186, 48, 58, 148,
    152, 91, 81, 174, 39, 12, 15, 34, 156, 95, 84, 178, 40, 13, 16, 34,
    69, 212, 235, 129, 31, 7, 2, 18, 66, 216, 239, 133, 32, 8, 2, 18,
    79, 254, 203, 114, 28, 4, 10, 20, 76, 250, 199, 118, 29, 5, 10, 21,
    193, 44, 54, 142, 36, 26, 23, 42, 189, 43, 52, 139, 37, 26, 24, 42,
    39, 12, 15, 33, 159, 99, 87, 169, 38, 11, 14, 33, 163, 103, 89, 172,
    31, 7, 1, 17, 65, 220, 229, 123, 30, 6, 1, 17, 62, 223, 233, 127,
    28, 4, 9, 20, 75, 248, 210, 108, 27, 3, 9, 19, 72, 244, 206, 112,
    36, 25, 23, 41, 188, 49, 60, 150, 35, 25, 22, 41, 184, 47, 57, 146,
    157, 97, 85, 180, 40, 13, 16, 35, 154, 93, 83, 176, 39, 13, 15, 34,
    67, 218, 240, 135, 32, 8, 3, 19, 70, 214, 237, 131, 31, 7, 2, 18,
    78, 252, 197, 120, 29, 5, 11, 21, 80, 255, 201, 116, 29, 5, 10, 21,
    191, 43, 51, 137, 37, 27, 24, 43, 195, 44, 53, 140, 37, 26, 23, 42
};

#define source_phase_x 4
#define source_phase_y 0

static int
moo_install_halftone(gs_state *pgs, gx_device *pdevice)
{
    gs_halftone ht;
    gs_string thresh;
    int code;

    int width = 16;
    int height = 16;
    thresh.data = order16x16;
    thresh.size = width * height;

    if (gx_device_must_halftone(pdevice))
    {
        ht.type = ht_type_threshold;
        ht.params.threshold.width = width;
        ht.params.threshold.height = height;
        ht.params.threshold.thresholds.data = thresh.data;
        ht.params.threshold.thresholds.size = thresh.size;
        ht.params.threshold.transfer = 0;
        ht.params.threshold.transfer_closure.proc = 0;

        gs_settransfer(pgs, identity_transfer);

        code = gs_sethalftone(pgs, &ht);
        if (code < 0)
            return gs_throw(code, "could not install halftone");

        code = gs_sethalftonephase(pgs, 0, 0);
        if (code < 0)
            return gs_throw(code, "could not set halftone phase");
    }

    return 0;
}
/* Parser implementation descriptor */
const pl_interp_implementation_t moo_implementation =
{
	moo_imp_characteristics,
	moo_imp_allocate_interp,
	moo_imp_allocate_interp_instance,
	moo_imp_set_client_instance,
	moo_imp_set_pre_page_action,
	moo_imp_set_post_page_action,
	moo_imp_set_device,
	moo_imp_init_job,
	moo_imp_process_file,
	moo_imp_process,
	moo_imp_flush_to_eoj,
	moo_imp_process_eof,
	moo_imp_report_errors,
	moo_imp_dnit_job,
	moo_imp_remove_device,
	moo_imp_deallocate_interp_instance,
	moo_imp_deallocate_interp,
	moo_imp_get_device_memory,
};

/* We override plimlp.c here: */
/* Zero-terminated list of pointers to implementations */
pl_interp_implementation_t const * const pdl_implementation[] = {
	&moo_implementation,
	0
};
