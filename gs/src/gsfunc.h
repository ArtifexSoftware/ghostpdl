/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

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


/* Generic definitions for Functions */

#ifndef gsfunc_INCLUDED
#  define gsfunc_INCLUDED

/* ---------------- Types and structures ---------------- */

/*
 * gs_function_type_t is defined as equivalent to int, rather than as an
 * enum type, because we can't enumerate all its possible values here in the
 * generic definitions.
 */
typedef int gs_function_type_t;

/*
 * Define information common to all Function types.
 * We separate the private part from the parameters so that
 * clients can create statically initialized parameter structures.
 */
#define gs_function_params_common\
    int m;			/* # of inputs */\
    const float *Domain;	/* 2 x m */\
    int n;			/* # of outputs */\
    const float *Range		/* 2 x n, optional except for type 0 */

/* Define a generic function, for use as the target type of pointers. */
typedef struct gs_function_params_s {
    gs_function_params_common;
} gs_function_params_t;
typedef struct gs_function_s gs_function_t;
typedef int (*fn_evaluate_proc_t)(P3(const gs_function_t * pfn,
				     const float *in, float *out));
typedef int (*fn_is_monotonic_proc_t)(P4(const gs_function_t * pfn,
					 const float *lower,
					 const float *upper,
					 bool must_know));
typedef void (*fn_free_params_proc_t)(P2(gs_function_params_t * params,
					 gs_memory_t * mem));
typedef void (*fn_free_proc_t)(P3(gs_function_t * pfn,
				  bool free_params, gs_memory_t * mem));
typedef struct gs_function_head_s {
    gs_function_type_t type;
    fn_evaluate_proc_t evaluate;
    fn_is_monotonic_proc_t is_monotonic;
    fn_free_params_proc_t free_params;
    fn_free_proc_t free;
} gs_function_head_t;
struct gs_function_s {
    gs_function_head_t head;
    gs_function_params_t params;
};

#define FunctionType(pfn) ((pfn)->head.type)

/*
 * Each specific function type has a definition in its own header file
 * for its parameter record.  In order to keep names from overflowing
 * various compilers' limits, we take the name of the function type and
 * reduce it to the first and last letter of each word, e.g., for
 * Sampled functions, XxYy is Sd.

typedef struct gs_function_XxYy_params_s {
     gs_function_params_common;
    << P additional members >>
} gs_function_XxYy_params_t;
#define private_st_function_XxYy()\
  gs_private_st_suffix_addP(st_function_XxYy, gs_function_XxYy_t,\
    "gs_function_XxYy_t", function_XxYy_enum_ptrs, function_XxYy_reloc_ptrs,\
    st_function, <<params.additional_members>>)

 */

/* ---------------- Procedures ---------------- */

/*
 * Each specific function type has a pair of procedures in its own
 * header file, one to allocate and initialize an instance of that type,
 * and one to free the parameters of that type.

int gs_function_XxYy_init(P3(gs_function_t **ppfn,
			     const gs_function_XxYy_params_t *params,
			     gs_memory_t *mem));

void gs_function_XxYy_free_params(P2(gs_function_XxYy_params_t *params,
				     gs_memory_t *mem));

 */

/* Evaluate a function. */
#define gs_function_evaluate(pfn, in, out)\
  (*(pfn)->head.evaluate)(pfn, in, out)

/*
 * Test whether a function is monotonic on a given (closed) interval.  If
 * must_know is true, returns 0 for false, 1 for true, gs_error_rangecheck
 * if any part of the interval is outside the function's domain; if
 * must_know is false, may also return gs_error_undefined to mean "can't
 * determine quickly".  If lower[i] > upper[i], the result is not defined.
 */
#define gs_function_is_monotonic(pfn, lower, upper, must_know)\
  (*(pfn)->head.is_monotonic)(pfn, lower, upper, must_know)

/* Free function parameters. */
#define gs_function_free_params(pfn, mem)\
  (*(pfn)->head.free_params)(&(pfn)->params, mem)

/* Free a function's implementation, optionally including its parameters. */
#define gs_function_free(pfn, free_params, mem)\
  (*(pfn)->head.free)(pfn, free_params, mem)

#endif /* gsfunc_INCLUDED */
