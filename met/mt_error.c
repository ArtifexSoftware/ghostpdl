/*
 * Error handling
 */
#include "memory_.h"
#include "gp.h"
#include "scommon.h"
#include "gsmemory.h"
#include "scommon.h"

#include "mt_error.h"
#include "metparse.h"
#include "stdarg.h"

struct mt_error_s
{
	char msg[256];
	char *func;
	char *file;
	int line;
	mt_error_t *cause;
};

static mt_error_t mt_outofmem =
{
	"outofmemory: could not allocate error object", "static", __FILE__, __LINE__, 0
};

mt_error_t *mt_throw_imp(const char *func, const char *file, int line, mt_error_t *cause, char *fmt, ...)
{
	gs_memory_t *gsmem;
	mt_error_t *error;
	va_list ap;

	/* allocations here are freed by mt_free_error */
	gsmem = (gs_memory_t *)gs_lib_ctx_get_non_gc_memory_t();

	error = (mt_error_t*) gs_alloc_bytes(gsmem, sizeof(mt_error_t), "mt_error_t");
	if (!error)
		return &mt_outofmem;

	va_start(ap, fmt);
	vsnprintf(error->msg, sizeof(error->msg), fmt, ap);
	va_end(ap);

	error->msg[sizeof(error->msg) - 1] = 0;
	error->func = (char*)func;
	error->file = (char*)file;
	error->line = line;
	error->cause = cause;

	return error;
}

void mt_free_error(mt_error_t *error)
{
	gs_memory_t *gsmem = (gs_memory_t *)gs_lib_ctx_get_non_gc_memory_t();
	mt_error_t *tmp;

	while (error)
	{
		tmp = error->cause;
		gs_free_object(gsmem, error, "mt_error_t");
		error = tmp;
	}
}

void mt_print_error(mt_error_t *error)
{
	while (error)
	{
	    dprintf4(NULL,
		     "%s:%d: in %s(): %s\n", error->file, error->line, error->func, error->msg);
		error = error->cause;
	}
}

