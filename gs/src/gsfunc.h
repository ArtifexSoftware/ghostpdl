/* Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
  
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

/* Define calculation effort values (currently only used for monotonicity). */
typedef enum {
    EFFORT_EASY = 0,
    EFFORT_MODERATE = 1,
    EFFORT_ESSENTIAL = 2
} gs_function_effort_t;

/* Define abstract types. */
#ifndef gs_data_source_DEFINED
#  define gs_data_source_DEFINED
typedef struct gs_data_source_s gs_data_source_t;
#endif
#ifndef gs_param_list_DEFINED
#  define gs_param_list_DEFINED
typedef struct gs_param_list_s gs_param_list;
#endif

/* Define a generic function, for use as the target type of pointers. */
typedef struct gs_function_params_s {
    gs_function_params_common;
} gs_function_params_t;
#ifndef gs_function_DEFINED
typedef struct gs_function_s gs_function_t;
#  define gs_function_DEFINED
#endif
typedef struct gs_function_info_s {
    const gs_data_source_t *DataSource;
    ulong data_size;
    const gs_function_t *const *Functions;
    int num_Functions;
} gs_function_info_t;

/* Evaluate a function. */
#define FN_EVALUATE_PROC(proc)\
  int proc(P3(const gs_function_t * pfn, const float *in, float *out))
typedef FN_EVALUATE_PROC((*fn_evaluate_proc_t));

/* Test whether a function is monotonic. */
#define FN_IS_MONOTONIC_PROC(proc)\
  int proc(P4(const gs_function_t * pfn, const float *lower,\
	      const float *upper, gs_function_effort_t effort))
typedef FN_IS_MONOTONIC_PROC((*fn_is_monotonic_proc_t));

/* Get function information. */
#define FN_GET_INFO_PROC(proc)\
  void proc(P2(const gs_function_t *pfn, gs_function_info_t *pfi))
typedef FN_GET_INFO_PROC((*fn_get_info_proc_t));

/* Put function parameters on a parameter list. */
#define FN_GET_PARAMS_PROC(proc)\
  int proc(P2(const gs_function_t *pfn, gs_param_list *plist))
typedef FN_GET_PARAMS_PROC((*fn_get_params_proc_t));

/* Free function parameters. */
#define FN_FREE_PARAMS_PROC(proc)\
  void proc(P2(gs_function_params_t * params, gs_memory_t * mem))
typedef FN_FREE_PARAMS_PROC((*fn_free_params_proc_t));

/* Free a function. */
#define FN_FREE_PROC(proc)\
  void proc(P3(gs_function_t * pfn, bool free_params, gs_memory_t * mem))
typedef FN_FREE_PROC((*fn_free_proc_t));

/* Define the generic function structures. */
typedef struct gs_function_procs_s {
    fn_evaluate_proc_t evaluate;
    fn_is_monotonic_proc_t is_monotonic;
    fn_get_info_proc_t get_info;
    fn_get_params_proc_t get_params;
    fn_free_params_proc_t free_params;
    fn_free_proc_t free;
} gs_function_procs_t;
typedef struct gs_function_head_s {
    gs_function_type_t type;
    gs_function_procs_t procs;
    int is_monotonic;		/* cached when function is created */
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
  ((pfn)->head.procs.evaluate)(pfn, in, out)

/*
 * Test whether a function is monotonic on a given (closed) interval.  If
 * the test requires too much effort, the procedure may return
 * gs_error_undefined; normally, it returns 0 for false, >0 for true,
 * gs_error_rangecheck if any part of the interval is outside the function's
 * domain.  If lower[i] > upper[i], the result is not defined.
 */
#define gs_function_is_monotonic(pfn, lower, upper, effort)\
  ((pfn)->head.procs.is_monotonic)(pfn, lower, upper, effort)
/*
 * If the function is monotonic, is_monotonic returns the direction of
 * monotonicity for output value N in bits 2N and 2N+1.  (Functions with
 * more than sizeof(int) * 4 - 1 outputs are never identified as monotonic.)
 */
#define FN_MONOTONIC_INCREASING 1
#define FN_MONOTONIC_DECREASING 2

/* Get function information. */
#define gs_function_get_info(pfn, pfi)\
  ((pfn)->head.procs.get_info(pfn, pfi))

/* Write function parameters. */
#define gs_function_get_params(pfn, plist)\
  ((pfn)->head.procs.get_params(pfn, plist))

/* Free function parameters. */
#define gs_function_free_params(pfn, mem)\
  ((pfn)->head.procs.free_params(&(pfn)->params, mem))

/* Free a function's implementation, optionally including its parameters. */
#define gs_function_free(pfn, free_params, mem)\
  ((pfn)->head.procs.free(pfn, free_params, mem))

#endif /* gsfunc_INCLUDED */
