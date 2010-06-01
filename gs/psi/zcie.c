/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* CIE color operators */
#include "math_.h"
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gxcspace.h"		/* gscolor2.h requires gscspace.h */
#include "gscolor2.h"
#include "gscie.h"
#include "estack.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "icie.h"
#include "isave.h"
#include "ivmspace.h"
#include "store.h"		/* for make_null */
#include "zcie.h"
#include "gsicc_create.h"
#include "gsicc_manage.h"
#include "gsicc_profilecache.h"

/* Prototype */
int cieicc_prepare_caches(i_ctx_t *i_ctx_p, const gs_range * domains,
		     const ref * procs,
		     cie_cache_floats * pc0, cie_cache_floats * pc1,
		     cie_cache_floats * pc2, cie_cache_floats * pc3,
		     void *container,
		     const gs_ref_memory_t * imem, client_name_t cname);
static int
cie_prepare_iccproc(i_ctx_t *i_ctx_p, const gs_range * domain, const ref * proc,
		  cie_cache_floats * pcache, void *container,
		  const gs_ref_memory_t * imem, client_name_t cname);

/* Empty procedures */
static const ref empty_procs[4] =
{
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable),
    empty_ref_data(t_array, a_readonly | a_executable)
};

/* ------ Parameter extraction utilities ------ */

/* Get a range array parameter from a dictionary. */
/* We know that count <= 4. */
int
dict_ranges_param(const gs_memory_t *mem,
		  const ref * pdref, const char *kstr, int count,
		  gs_range * prange)
{
    int code = dict_floats_param(mem, pdref, kstr, count * 2,
				 (float *)prange, NULL);

    if (code < 0)
	return code;
    else if (code == 0)
	memcpy(prange, Range4_default.ranges, count * sizeof(gs_range));
    return 0;
}

/* Get an array of procedures from a dictionary. */
/* We know count <= countof(empty_procs). */
int
dict_proc_array_param(const gs_memory_t *mem,
		      const ref *pdict, const char *kstr,
		      uint count, ref *pparray)
{
    ref *pvalue;

    if (dict_find_string(pdict, kstr, &pvalue) > 0) {
	uint i;

	check_array_only(*pvalue);
	if (r_size(pvalue) != count)
	    return_error(e_rangecheck);
	for (i = 0; i < count; i++) {
	    ref proc;

	    array_get(mem, pvalue, (long)i, &proc);
	    check_proc_only(proc);
	}
	*pparray = *pvalue;
        return 0;
    } else {
	make_const_array(pparray, a_readonly | avm_foreign,
			 count, &empty_procs[0]);
        return 1;
    }
}

/* Get 3 ranges from a dictionary. */
int
dict_range3_param(const gs_memory_t *mem,
		  const ref *pdref, const char *kstr, 
		  gs_range3 *prange3)
{
    return dict_ranges_param(mem, pdref, kstr, 3, prange3->ranges);
}

/* Get a 3x3 matrix from a dictionary. */
int
dict_matrix3_param(const gs_memory_t *mem,
		   const ref *pdref, const char *kstr, gs_matrix3 *pmat3)
{
    /*
     * We can't simply call dict_float_array_param with the matrix
     * cast to a 9-element float array, because compilers may insert
     * padding elements after each of the vectors.  However, we can be
     * confident that there is no padding within a single vector.
     */
    float values[9], defaults[9];
    int code;

    memcpy(&defaults[0], &Matrix3_default.cu, 3 * sizeof(float));
    memcpy(&defaults[3], &Matrix3_default.cv, 3 * sizeof(float));
    memcpy(&defaults[6], &Matrix3_default.cw, 3 * sizeof(float));
    code = dict_floats_param(mem, pdref, kstr, 9, values, defaults);
    if (code < 0)
	return code;
    memcpy(&pmat3->cu, &values[0], 3 * sizeof(float));
    memcpy(&pmat3->cv, &values[3], 3 * sizeof(float));
    memcpy(&pmat3->cw, &values[6], 3 * sizeof(float));
    return 0;
}

/* Get 3 procedures from a dictionary. */
int
dict_proc3_param(const gs_memory_t *mem, const ref *pdref, const char *kstr, ref proc3[3])
{
    return dict_proc_array_param(mem, pdref, kstr, 3, proc3);
}

/* Get WhitePoint and BlackPoint values. */
int
cie_points_param(const gs_memory_t *mem, 
		 const ref * pdref, gs_cie_wb * pwb)
{
    int code;

    if ((code = dict_floats_param(mem, pdref, "WhitePoint", 3, 
        (float *)&pwb->WhitePoint, NULL)) < 0 ||
	(code = dict_floats_param(mem, pdref, "BlackPoint", 3, 
        (float *)&pwb->BlackPoint, (const float *)&BlackPoint_default)) < 0
	)
	return code;
    if (pwb->WhitePoint.u <= 0 ||
	pwb->WhitePoint.v != 1 ||
	pwb->WhitePoint.w <= 0 ||
	pwb->BlackPoint.u < 0 ||
	pwb->BlackPoint.v < 0 ||
	pwb->BlackPoint.w < 0
	)
	return_error(e_rangecheck);
    return 0;
}

/* Process a 3- or 4-dimensional lookup table from a dictionary. */
/* The caller has set pclt->n and pclt->m. */
/* ptref is known to be a readable array of size at least n+1. */
static int cie_3d_table_param(const ref * ptable, uint count, uint nbytes,
			       gs_const_string * strings);
int
cie_table_param(const ref * ptref, gx_color_lookup_table * pclt,
		const gs_memory_t * mem)
{
    int n = pclt->n, m = pclt->m;
    const ref *pta = ptref->value.const_refs;
    int i;
    uint nbytes;
    int code;
    gs_const_string *table;

    for (i = 0; i < n; ++i) {
	check_type_only(pta[i], t_integer);
	if (pta[i].value.intval <= 1 || pta[i].value.intval > max_ushort)
	    return_error(e_rangecheck);
	pclt->dims[i] = (int)pta[i].value.intval;
    }
    nbytes = m * pclt->dims[n - 2] * pclt->dims[n - 1];
    if (n == 3) {
	table =
	    gs_alloc_struct_array(mem->stable_memory, pclt->dims[0], gs_const_string,
				  &st_const_string_element, "cie_table_param");
	if (table == 0)
	    return_error(e_VMerror);
	code = cie_3d_table_param(pta + 3, pclt->dims[0], nbytes, table);
    } else {			/* n == 4 */
	int d0 = pclt->dims[0], d1 = pclt->dims[1];
	uint ntables = d0 * d1;
	const ref *psuba;

	check_read_type(pta[4], t_array);
	if (r_size(pta + 4) != d0)
	    return_error(e_rangecheck);
	table =
	    gs_alloc_struct_array(mem->stable_memory, ntables, gs_const_string,
				  &st_const_string_element, "cie_table_param");
	if (table == 0)
	    return_error(e_VMerror);
	psuba = pta[4].value.const_refs;
	/*
	 * We know that d0 > 0, so code will always be set in the loop:
	 * we initialize code to 0 here solely to pacify stupid compilers.
	 */
	for (code = 0, i = 0; i < d0; ++i) {
	    code = cie_3d_table_param(psuba + i, d1, nbytes, table + d1 * i);
	    if (code < 0)
		break;
	}
    }
    if (code < 0) {
	gs_free_object(mem, table, "cie_table_param");
	return code;
    }
    pclt->table = table;
    return 0;
}
static int
cie_3d_table_param(const ref * ptable, uint count, uint nbytes,
		   gs_const_string * strings)
{
    const ref *rstrings;
    uint i;

    check_read_type(*ptable, t_array);
    if (r_size(ptable) != count)
	return_error(e_rangecheck);
    rstrings = ptable->value.const_refs;
    for (i = 0; i < count; ++i) {
	const ref *const prt2 = rstrings + i;

	check_read_type(*prt2, t_string);
	if (r_size(prt2) != nbytes)
	    return_error(e_rangecheck);
	strings[i].data = prt2->value.const_bytes;
	strings[i].size = nbytes;
    }
    return 0;
}

/* ------ CIE setcolorspace ------ */

/* Common code for the CIEBased* cases of setcolorspace. */
static int
cie_lmnp_param(const gs_memory_t *mem, const ref * pdref, gs_cie_common * pcie, 
               ref_cie_procs * pcprocs, bool *has_lmn_procs)
{
    int code;

    if ((code = dict_range3_param(mem, pdref, "RangeLMN", &pcie->RangeLMN)) < 0 ||
	(code = dict_matrix3_param(mem, pdref, "MatrixLMN", &pcie->MatrixLMN)) < 0 ||
	(code = cie_points_param(mem, pdref, &pcie->points)) < 0
	)
	return code;
    code = dict_proc3_param(mem, pdref, "DecodeLMN", &pcprocs->DecodeLMN);
    if (code < 0)
        return code;
    *has_lmn_procs = !code;  /* Need to know for efficient creation of ICC profile */  
    pcie->DecodeLMN = DecodeLMN_default;
    return 0;
}

/* Get objects associated with cie color space */
static int
cie_a_param(const gs_memory_t *mem, const ref * pdref, gs_cie_a * pcie, 
            ref_cie_procs * pcprocs, bool *has_a_procs, bool *has_lmn_procs)
{
    int code;

    code = dict_floats_param(mem, pdref, "RangeA", 2, (float *)&pcie->RangeA, 
                            (const float *)&RangeA_default);
    if (code < 0)
	return code;
    code = dict_floats_param(mem, pdref, "MatrixA", 3, (float *)&pcie->MatrixA, 
                            (const float *)&MatrixA_default);
    if (code < 0)
	return code;
    code = cie_lmnp_param(mem, pdref, &pcie->common, pcprocs, has_lmn_procs);
    if (code < 0)
	return code;
    if ((code = dict_proc_param(pdref, "DecodeA", &(pcprocs->Decode.A), true)) < 0)
	return code;
    *has_a_procs = !code;
    return 0;
}

/* Common code for the CIEBasedABC/DEF[G] cases of setcolorspace. */
static int
cie_abc_param(i_ctx_t *i_ctx_p, const gs_memory_t *mem, const ref * pdref, 
               gs_cie_abc * pcie, ref_cie_procs * pcprocs, 
              bool *has_abc_procs, bool *has_lmn_procs)
{
    int code;
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

    if ((code = dict_range3_param(mem, pdref, "RangeABC", &pcie->RangeABC)) < 0 ||
	(code = dict_matrix3_param(mem, pdref, "MatrixABC", &pcie->MatrixABC)) < 0 ||
	(code = cie_lmnp_param(mem, pdref, &pcie->common, pcprocs, has_lmn_procs)) < 0
	)
	return code;
    code = dict_proc3_param(mem, pdref, "DecodeABC", &pcprocs->Decode.ABC);
    if (code < 0)
        return code;
    *has_abc_procs = !code;
    pcie->DecodeABC = DecodeABC_default;
   /* At this point, we have all the parameters in pcie including knowing if 
    there
       are procedures present.  If there are no procedures, life is simple for us.
       If there are procedures, we can not create the ICC profile until we have the procedures 
       sampled, which requires pushing the appropriate commands upon the postscript execution stack 
       to create the sampled procs and then having a follow up operation to create the ICC profile.
       Because the procs may have to be merged with other operators and/or packed
       in a particular form, we will have the PS operators stuff them in the already
       existing static buffers that already exist for this purpose in the cie structures
       e.g. gx_cie_vector_cache3_t that are in the common (params.abc.common.caches.DecodeLMN) 
       and unique entries (e.g. params.abc.caches.DecodeABC.caches) */
    if (*has_abc_procs) {
        cieicc_prepare_caches(i_ctx_p, (&pcie->RangeABC)->ranges,
                 pcprocs->Decode.ABC.value.const_refs,
                 &(pcie->caches.DecodeABC.caches)->floats,
                 &(pcie->caches.DecodeABC.caches)[1].floats,
                 &(pcie->caches.DecodeABC.caches)[2].floats,
                 NULL, pcie, imem, "Decode.ABC(ICC)");
    } else {
        pcie->caches.DecodeABC.caches->floats.params.is_identity = true;
        (pcie->caches.DecodeABC.caches)[1].floats.params.is_identity = true;
        (pcie->caches.DecodeABC.caches)[2].floats.params.is_identity = true;
    }
    if (*has_lmn_procs) {
        cieicc_prepare_caches(i_ctx_p, (&pcie->common.RangeLMN)->ranges,
	            pcprocs->DecodeLMN.value.const_refs,
                    &(pcie->common.caches.DecodeLMN)->floats,
                    &(pcie->common.caches.DecodeLMN)[1].floats,
                    &(pcie->common.caches.DecodeLMN)[2].floats,
                    NULL, pcie, imem, "Decode.LMN(ICC)");
    } else {
        pcie->common.caches.DecodeLMN->floats.params.is_identity = true;
        (pcie->common.caches.DecodeLMN)[1].floats.params.is_identity = true;
        (pcie->common.caches.DecodeLMN)[2].floats.params.is_identity = true;
    }
    return 0;
}

/* Finish setting a CIE space (successful or not). */
int
cie_set_finish(i_ctx_t *i_ctx_p, gs_color_space * pcs,
	       const ref_cie_procs * pcprocs, int edepth, int code)
{
    if (code >= 0)
	code = gs_setcolorspace(igs, pcs);
    /* Delete the extra reference to the parameter tables. */
    rc_decrement_only_cs(pcs, "cie_set_finish");
    if (code < 0) {
	ref_stack_pop_to(&e_stack, edepth);
	return code;
    }
    istate->colorspace[0].procs.cie = *pcprocs;
    pop(1);
    return (ref_stack_count(&e_stack) == edepth ? 0 : o_push_estack);
}

/* Forward references */
static int cache_common(i_ctx_t *, gs_cie_common *, const ref_cie_procs *,
			 void *, gs_ref_memory_t *);
static int cache_abc_common(i_ctx_t *, gs_cie_abc *, const ref_cie_procs *,
			     void *, gs_ref_memory_t *);

static int cie_defg_finish(i_ctx_t *);

static int
cie_defg_param(i_ctx_t *i_ctx_p, const gs_memory_t *mem, const ref * pdref, 
               gs_cie_defg * pcie, ref_cie_procs * pcprocs, bool *has_abc_procs, 
               bool *has_lmn_procs, bool *has_defg_procs, ref *ptref)
{
    int code; 
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

    /* First get all the ABC and LMN information related to this space */
    code = cie_abc_param(i_ctx_p, mem, pdref, (gs_cie_abc *) pcie, pcprocs, 
                            has_abc_procs, has_lmn_procs);
    if (code < 0) 
        return code;
    code = dict_ranges_param(mem, pdref, "RangeDEFG", 4, pcie->RangeDEFG.ranges);
    if (code < 0) 
        return code;
    code = dict_ranges_param(mem, pdref, "RangeHIJK", 4, pcie->RangeHIJK.ranges);
    if (code < 0) 
        return code;
    code = cie_table_param(ptref, &pcie->Table, mem);
    if (code < 0) 
        return code;
    code = dict_proc_array_param(mem, pdref, "DecodeDEFG", 4, 
                                    &(pcprocs->PreDecode.DEFG));
    if (code < 0) 
        return code;
    *has_defg_procs = !code;
    if (*has_defg_procs) {
        cieicc_prepare_caches(i_ctx_p, (&pcie->RangeDEFG)->ranges,
                 pcprocs->PreDecode.DEFG.value.const_refs,
                 &(pcie->caches_defg.DecodeDEFG)->floats,
                 &(pcie->caches_defg.DecodeDEFG)[1].floats,
                 &(pcie->caches_defg.DecodeDEFG)[2].floats,
                 &(pcie->caches_defg.DecodeDEFG)[3].floats,
                    pcie, imem, "Decode.DEFG(ICC)");
    } else {
         pcie->caches_defg.DecodeDEFG->floats.params.is_identity = true;
        (pcie->caches_defg.DecodeDEFG)[1].floats.params.is_identity = true;
        (pcie->caches_defg.DecodeDEFG)[2].floats.params.is_identity = true;
        (pcie->caches_defg.DecodeDEFG)[3].floats.params.is_identity = true;
    }
    return(0);
}
int
ciedefgspace(i_ctx_t *i_ctx_p, ref *CIEDict, ulong dictkey)
{
    os_ptr op = osp;
    int edepth = ref_stack_count(&e_stack);
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_cie_procs procs;
    gs_cie_defg *pcie;
    int code = 0;
    ref *ptref;
    bool has_defg_procs, has_abc_procs, has_lmn_procs;
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

/*    pcs = gsicc_find_cs(dictkey, igs); */
    pcs = NULL;
    push(1); /* Sacrificial */
    procs = istate->colorspace[0].procs.cie;
    if (pcs == NULL ) {
    if ((code = dict_find_string(CIEDict, "Table", &ptref)) <= 0)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    check_read_type(*ptref, t_array);
    if (r_size(ptref) != 5)
	return_error(e_rangecheck);
        /* Stable memory due to current caching of color space */
        code = gs_cspace_build_CIEDEFG(&pcs, NULL, mem->stable_memory);
    if (code < 0)
	return code;
    pcie = pcs->params.defg;
    pcie->Table.n = 4;
    pcie->Table.m = 3;
        code = cie_cache_push_finish(i_ctx_p, cie_defg_finish, imem, pcie);
        code = cie_defg_param(i_ctx_p, imemory, CIEDict, pcie, &procs, 
            &has_abc_procs, &has_lmn_procs, &has_defg_procs,ptref);
        /* Add the color space to the profile cache */
        gsicc_add_cs(igs, pcs,dictkey);
    } else {
        rc_increment(pcs);
    }
    return cie_set_finish(i_ctx_p, pcs, &procs, edepth, code);
}

static int
cie_defg_finish(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_cie_defg *pcie = r_ptr(op, gs_cie_defg);

    pcie->DecodeDEFG = DecodeDEFG_from_cache;
    pcie->DecodeABC = DecodeABC_from_cache;
    pcie->common.DecodeLMN = DecodeLMN_from_cache;
    gs_cie_defg_complete(pcie);
    pop(1);
    return 0;
}

static int
cie_def_param(i_ctx_t *i_ctx_p, const gs_memory_t *mem, const ref * pdref, 
              gs_cie_def * pcie, ref_cie_procs * pcprocs, 
              bool *has_abc_procs, bool *has_lmn_procs, 
              bool *has_def_procs, ref *ptref)
{
    int code; 
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

    /* First get all the ABC and LMN information related to this space */
    code = cie_abc_param(i_ctx_p, mem, pdref, (gs_cie_abc *) pcie, pcprocs, 
                            has_abc_procs, has_lmn_procs);
    if (code < 0) 
        return code;
    code = dict_range3_param(mem, pdref, "RangeDEF", &pcie->RangeDEF);
    if (code < 0) 
        return code;
    code = dict_range3_param(mem, pdref, "RangeHIJ", &pcie->RangeHIJ);
    if (code < 0) 
        return code;
    code = cie_table_param(ptref, &pcie->Table, mem);
    if (code < 0)
        return code;
    /* The DEF procs */
    code = dict_proc3_param(mem, pdref, "DecodeDEF", &(pcprocs->PreDecode.DEF));
    if (code < 0)
        return code;
    *has_def_procs = !code;
    if (*has_def_procs) {
        cieicc_prepare_caches(i_ctx_p, (&pcie->RangeDEF)->ranges,
                 pcprocs->PreDecode.DEF.value.const_refs,
                 &(pcie->caches_def.DecodeDEF)->floats,
                 &(pcie->caches_def.DecodeDEF)[1].floats,
                 &(pcie->caches_def.DecodeDEF)[2].floats,
                 NULL, pcie, imem, "Decode.DEF(ICC)");
    } else {
         pcie->caches_def.DecodeDEF->floats.params.is_identity = true;
        (pcie->caches_def.DecodeDEF)[1].floats.params.is_identity = true;
        (pcie->caches_def.DecodeDEF)[2].floats.params.is_identity = true;
    }
    return(0);
}

static int cie_def_finish(i_ctx_t *);
int
ciedefspace(i_ctx_t *i_ctx_p, ref *CIEDict, ulong dictkey)
{
    os_ptr op = osp;
    int edepth = ref_stack_count(&e_stack);
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_cie_procs procs;
    gs_cie_def *pcie;
    int code = 0;
    ref *ptref;
    bool has_def_procs, has_lmn_procs, has_abc_procs;
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

/*    pcs = gsicc_find_cs(dictkey, igs); */
    pcs = NULL;
    push(1); /* Sacrificial */
    procs = istate->colorspace[0].procs.cie;
    if (pcs == NULL ) {
    if ((code = dict_find_string(CIEDict, "Table", &ptref)) <= 0)
	return (code < 0 ? code : gs_note_error(e_rangecheck));
    check_read_type(*ptref, t_array);
    if (r_size(ptref) != 4)
	return_error(e_rangecheck);
        /* Stable memory due to current caching of color space */
        code = gs_cspace_build_CIEDEF(&pcs, NULL, mem->stable_memory);
    if (code < 0)
	return code;
    pcie = pcs->params.def;
    pcie->Table.n = 3;
    pcie->Table.m = 3;
        code = cie_cache_push_finish(i_ctx_p, cie_def_finish, imem, pcie);
        code = cie_def_param(i_ctx_p, imemory, CIEDict, pcie, &procs, 
            &has_abc_procs, &has_lmn_procs, &has_def_procs, ptref);
        /* Add the color space to the profile cache */
        gsicc_add_cs(igs, pcs,dictkey);
    } else {
        rc_increment(pcs);
    }
    return cie_set_finish(i_ctx_p, pcs, &procs, edepth, code);
}

static int
cie_def_finish(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_cie_def *pcie = r_ptr(op, gs_cie_def);

    pcie->DecodeDEF = DecodeDEF_from_cache;
    pcie->DecodeABC = DecodeABC_from_cache;
    pcie->common.DecodeLMN = DecodeLMN_from_cache;
    gs_cie_def_complete(pcie);
    pop(1);
    return 0;
}

static int cie_abc_finish(i_ctx_t *);

int
cieabcspace(i_ctx_t *i_ctx_p, ref *CIEDict, ulong dictkey)
{
    os_ptr op = osp;
    int edepth = ref_stack_count(&e_stack);
    gs_memory_t *mem = gs_state_memory(igs);
    gs_color_space *pcs;
    ref_cie_procs procs;
    gs_cie_abc *pcie;
    int code = 0;
    bool has_lmn_procs, has_abc_procs;
    gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;

/* See if the color space is in the profile cache */
/*    pcs = gsicc_find_cs(dictkey, igs); */
    pcs = NULL;
    push(1); /* Sacrificial */
    procs = istate->colorspace[0].procs.cie;
    if (pcs == NULL ) {
        /* Stable memory due to current caching of color space */
        code = gs_cspace_build_CIEABC(&pcs, NULL, mem->stable_memory);
    if (code < 0)
	return code;
    pcie = pcs->params.abc;
        code = cie_cache_push_finish(i_ctx_p, cie_abc_finish, imem, pcie);
        code = cie_abc_param(i_ctx_p, imemory, CIEDict, pcie, &procs, 
            &has_abc_procs, &has_lmn_procs);
        /* Set the color space in the graphic state.  The ICC profile 
            will be set later if we actually use the space.  Procs will be 
            sampled now though. Also, the finish procedure is on the stack
            since that is where the vector cache is completed from the scalar
            caches.  We may need the vector cache if we are going to go 
            ahead and create an MLUT for this thing */
        /* Add the color space to the profile cache */
        gsicc_add_cs(igs, pcs,dictkey);
    } else {
        rc_increment(pcs);
    }
    return cie_set_finish(i_ctx_p, pcs, &procs, edepth, code);
}

static int
cie_abc_finish(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_cie_abc *pcie = r_ptr(op, gs_cie_abc);

    pcie->DecodeABC = DecodeABC_from_cache;
    pcie->common.DecodeLMN = DecodeLMN_from_cache;
    gs_cie_abc_complete(pcie);
    pop(1);
    return 0;
}

static int cie_a_finish(i_ctx_t *);

int
cieaspace(i_ctx_t *i_ctx_p, ref *CIEdict, ulong dictkey)
{
    os_ptr op = osp;
    int edepth = ref_stack_count(&e_stack);
    gs_memory_t *mem = gs_state_memory(igs);
    const gs_ref_memory_t *imem = (gs_ref_memory_t *)mem;
    gs_color_space *pcs;
    ref_cie_procs procs;
    gs_cie_a *pcie;
    int code = 0;
    bool has_a_procs = false;
    bool has_lmn_procs;

/* See if the color space is in the profile cache */
/*    pcs = gsicc_find_cs(dictkey, igs); */
    pcs = NULL;
    push(1); /* Sacrificial */
    procs = istate->colorspace[0].procs.cie;
    if (pcs == NULL ) {
        /* Stable memory due to current caching of color space */
        code = gs_cspace_build_CIEA(&pcs, NULL, mem->stable_memory);
    if (code < 0)
	return code;
    pcie = pcs->params.a;
        code = cie_a_param(imemory, CIEdict, pcie, &procs, &has_a_procs, 
                                &has_lmn_procs);
        /* Push finalize procedure on the execution stack */
        code = cie_cache_push_finish(i_ctx_p, cie_a_finish, imem, pcie);
        if (!has_a_procs && !has_lmn_procs) {
            pcie->common.caches.DecodeLMN->floats
                .params.is_identity = true;
            (pcie->common.caches.DecodeLMN)[1].floats.params.is_identity = true;
            (pcie->common.caches.DecodeLMN)[2].floats.params.is_identity = true;
            pcie->caches.DecodeA.floats.params.is_identity = true;
        } else {
            if (has_a_procs) {
                code = cie_prepare_iccproc(i_ctx_p, &pcie->RangeA, 
                    &procs.Decode.A, &pcie->caches.DecodeA.floats, pcie, imem, "Decode.A");
            } else {
                pcie->caches.DecodeA.floats.params.is_identity = true;
            }
            if (has_lmn_procs) {
                cieicc_prepare_caches(i_ctx_p, (&pcie->common.RangeLMN)->ranges,
		         procs.DecodeLMN.value.const_refs,
                         &(pcie->common.caches.DecodeLMN)->floats,
                         &(pcie->common.caches.DecodeLMN)[1].floats,
                         &(pcie->common.caches.DecodeLMN)[2].floats,
                         NULL, pcie, imem, "Decode.LMN(ICC)");
            } else {
                pcie->common.caches.DecodeLMN->floats.params.is_identity = true;
                (pcie->common.caches.DecodeLMN)[1].floats.params.is_identity = true;
                (pcie->common.caches.DecodeLMN)[2].floats.params.is_identity = true;
            }
        }
        /* Add the color space to the profile cache */
        gsicc_add_cs(igs, pcs,dictkey);
    } else {
        rc_increment(pcs);
    }
    /* Set the color space in the graphic state.  The ICC profile may be set after this
           due to the needed sampled procs */
    return cie_set_finish(i_ctx_p, pcs, &procs, edepth, code);
}

static int
cie_a_finish(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_cie_a *pcie = r_ptr(op, gs_cie_a);

    pcie->DecodeA = DecodeA_from_cache;
    pcie->common.DecodeLMN = DecodeLMN_from_cache;
    gs_cie_a_complete(pcie);
    pop(1);
    return 0;
}

/* Common cache code */

static int
cache_abc_common(i_ctx_t *i_ctx_p, gs_cie_abc * pcie,
		 const ref_cie_procs * pcprocs,
		 void *container, gs_ref_memory_t * imem)
{
    int code =
	cie_prepare_cache3(i_ctx_p, &pcie->RangeABC,
			   pcprocs->Decode.ABC.value.const_refs,
			   pcie->caches.DecodeABC.caches, pcie, imem,
			   "Decode.ABC");

    return (code < 0 ? code :
	    cache_common(i_ctx_p, &pcie->common, pcprocs, pcie, imem));
}

static int
cache_common(i_ctx_t *i_ctx_p, gs_cie_common * pcie,
	     const ref_cie_procs * pcprocs,
	     void *container, gs_ref_memory_t * imem)
{
    return cie_prepare_cache3(i_ctx_p, &pcie->RangeLMN,
			      pcprocs->DecodeLMN.value.const_refs,
			      &pcie->caches.DecodeLMN[0], container, imem,
			      "Decode.LMN");
}

/* ------ Internal routines ------ */

/* Prepare to cache the values for one or more procedures. */
/* RJW: No longer used, but keeping it around in case it becomes useful
 * again in future.
 * static int cie_cache_finish1(i_ctx_t *);
 */
static int cie_cache_finish(i_ctx_t *);
int
cie_prepare_cache(i_ctx_t *i_ctx_p, const gs_range * domain, const ref * proc,
		  cie_cache_floats * pcache, void *container,
		  gs_ref_memory_t * imem, client_name_t cname)
{
    int space = imemory_space(imem);
    gs_sample_loop_params_t lp;
    es_ptr ep;

    gs_cie_cache_init(&pcache->params, &lp, domain, cname);
    pcache->params.is_identity = r_size(proc) == 0;
    check_estack(9);
    ep = esp;
    make_real(ep + 9, lp.A);
    make_int(ep + 8, lp.N);
    make_real(ep + 7, lp.B);
    ep[6] = *proc;
    r_clear_attrs(ep + 6, a_executable);
    make_op_estack(ep + 5, zcvx);
    make_op_estack(ep + 4, zfor_samples);
    make_op_estack(ep + 3, cie_cache_finish);
    esp += 9;
    /*
     * The caches are embedded in the middle of other
     * structures, so we represent the pointer to the cache
     * as a pointer to the container plus an offset.
     */
    make_int(ep + 2, (char *)pcache - (char *)container);
    make_struct(ep + 1, space, container);
    return o_push_estack;
}
/* Note that pc3 may be 0, indicating that there are only 3 caches to load. */
int
cie_prepare_caches_4(i_ctx_t *i_ctx_p, const gs_range * domains,
		     const ref * procs,
		     cie_cache_floats * pc0, cie_cache_floats * pc1,
		     cie_cache_floats * pc2, cie_cache_floats * pc3,
		     void *container,
		     gs_ref_memory_t * imem, client_name_t cname)
{
    cie_cache_floats *pcn[4];
    int i, n, code = 0;

    pcn[0] = pc0, pcn[1] = pc1, pcn[2] = pc2;
    if (pc3 == 0)
	n = 3;
    else
	pcn[3] = pc3, n = 4;
    for (i = 0; i < n && code >= 0; ++i)
	code = cie_prepare_cache(i_ctx_p, domains + i, procs + i, pcn[i],
				 container, imem, cname);
    return code;
}

/* Store the result of caching one procedure. */
static int
cie_cache_finish_store(i_ctx_t *i_ctx_p, bool replicate)
{
    os_ptr op = osp;
    cie_cache_floats *pcache;
    int code;

    check_esp(2);
    /* See above for the container + offset representation of */
    /* the pointer to the cache. */
    pcache = (cie_cache_floats *) (r_ptr(esp - 1, char) + esp->value.intval);

    pcache->params.is_identity = false;	/* cache_set_linear computes this */
    if_debug3('c', "[c]cache 0x%lx base=%g, factor=%g:\n",
	      (ulong) pcache, pcache->params.base, pcache->params.factor);
    if (replicate ||
	(code = float_params(op, gx_cie_cache_size, &pcache->values[0])) < 0
	) {
	/* We might have underflowed the current stack block. */
	/* Handle the parameters one-by-one. */
	uint i;

	for (i = 0; i < gx_cie_cache_size; i++) {
	    code = float_param(ref_stack_index(&o_stack,
			       (replicate ? 0 : gx_cie_cache_size - 1 - i)),
			       &pcache->values[i]);
	    if (code < 0)
		return code;
	}
    }
#ifdef DEBUG
    if (gs_debug_c('c')) {
	int i;

	for (i = 0; i < gx_cie_cache_size; i += 4)
	    dlprintf5("[c]  cache[%3d]=%g, %g, %g, %g\n", i,
		      pcache->values[i], pcache->values[i + 1],
		      pcache->values[i + 2], pcache->values[i + 3]);
    }
#endif
    ref_stack_pop(&o_stack, (replicate ? 1 : gx_cie_cache_size));
    esp -= 2;			/* pop pointer to cache */
    return o_pop_estack;
}
static int
cie_cache_finish(i_ctx_t *i_ctx_p)
{
    return cie_cache_finish_store(i_ctx_p, false);
}
#if 0
/* RJW: No longer used, but might be useful in future. */
static int
cie_cache_finish1(i_ctx_t *i_ctx_p)
{
    return cie_cache_finish_store(i_ctx_p, true);
}
#endif

/* Push a finishing procedure on the e-stack. */
/* ptr will be the top element of the o-stack. */
int
cie_cache_push_finish(i_ctx_t *i_ctx_p, op_proc_t finish_proc,
		      gs_ref_memory_t * imem, void *data)
{
    check_estack(2);
    push_op_estack(finish_proc);
    ++esp;
    make_struct(esp, imemory_space(imem), data);
    return o_push_estack;
}

/* Special functions related to the creation of ICC profiles
   from the PS CIE color management objects.  These basically
   make use of the existing objects in the CIE stuctures to
   store the sampled procs.  These sampled procs are then
   used in the creation of the ICC profiles */

/* Push the sequence of commands onto the execution stack
   so that we sample the procs */
static int cie_create_icc(i_ctx_t *);
static int
cie_prepare_iccproc(i_ctx_t *i_ctx_p, const gs_range * domain, const ref * proc,
		  cie_cache_floats * pcache, void *container,
		  const gs_ref_memory_t * imem, client_name_t cname)
{
    int space = imemory_space(imem);
    gs_sample_loop_params_t lp;
    es_ptr ep;

    gs_cie_cache_init(&pcache->params, &lp, domain, cname);
    pcache->params.is_identity = r_size(proc) == 0;
    check_estack(9);
    ep = esp;
    make_real(ep + 9, lp.A);
    make_int(ep + 8, lp.N);
    make_real(ep + 7, lp.B);
    ep[6] = *proc;
    r_clear_attrs(ep + 6, a_executable);
    make_op_estack(ep + 5, zcvx);
    make_op_estack(ep + 4, zfor_samples);
    make_op_estack(ep + 3, cie_create_icc);
    esp += 9;
    /*
     * The caches are embedded in the middle of other
     * structures, so we represent the pointer to the cache
     * as a pointer to the container plus an offset.
     */
    make_int(ep + 2, (char *)pcache - (char *)container);
    make_struct(ep + 1, space, container);
    return o_push_estack;
}

int
cieicc_prepare_caches(i_ctx_t *i_ctx_p, const gs_range * domains,
		     const ref * procs,
		     cie_cache_floats * pc0, cie_cache_floats * pc1,
		     cie_cache_floats * pc2, cie_cache_floats * pc3,
		     void *container,
		     const gs_ref_memory_t * imem, client_name_t cname)
{
    cie_cache_floats *pcn[4];
    int i, n, code = 0;

    pcn[0] = pc0, pcn[1] = pc1, pcn[2] = pc2;
    if (pc3 == 0)
	n = 3;
    else
	pcn[3] = pc3, n = 4;
    for (i = 0; i < n && code >= 0; ++i)
	code = cie_prepare_iccproc(i_ctx_p, domains + i, procs + i, pcn[i],
				 container, imem, cname);
    return code;
}

/* We have sampled the procs. Go ahead and create the ICC profile.  */
static int
cie_create_icc(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    cie_cache_floats *pcache;
    int code;

    check_esp(2);
    /* See above for the container + offset representation of */
    /* the pointer to the cache. */
    pcache = (cie_cache_floats *) (r_ptr(esp - 1, char) + esp->value.intval);

    pcache->params.is_identity = false;	/* cache_set_linear computes this */
    if_debug3('c', "[c]icc_sample_proc 0x%lx base=%g, factor=%g:\n",
	      (ulong) pcache, pcache->params.base, pcache->params.factor);
    if ((code = float_params(op, gx_cie_cache_size, &pcache->values[0])) < 0) {
	/* We might have underflowed the current stack block. */
	/* Handle the parameters one-by-one. */
	uint i;

	for (i = 0; i < gx_cie_cache_size; i++) {
	    code = float_param(ref_stack_index(&o_stack,gx_cie_cache_size - 1 - i),
			       &pcache->values[i]);
	    if (code < 0)
		return code;
	}
    }
#ifdef DEBUG
    if (gs_debug_c('c')) {
	int i;

	for (i = 0; i < gx_cie_cache_size; i += 4)
	    dlprintf5("[c]  icc_sample_proc[%3d]=%g, %g, %g, %g\n", i,
		      pcache->values[i], pcache->values[i + 1],
		      pcache->values[i + 2], pcache->values[i + 3]);
    }
#endif
    ref_stack_pop(&o_stack, gx_cie_cache_size);
    esp -= 2;			/* pop pointer to cache */
    return o_pop_estack;
}
