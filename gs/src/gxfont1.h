/* Copyright (C) 1994, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gxfont1.h */
/* Type 1 font data definition (including Type 2 charstrings) */

/*
 * This is the type-specific information for an Adobe Type 1 font.
 * It also includes the information for Type 2 charstrings, because
 * there isn't very much of it and it's less trouble to include here.
 */
/*
 * The zone_table values should be ints, according to the Adobe
 * specification, but some fonts have arbitrary floats here.
 */
#define zone_table(size)\
	struct {\
		int count;\
		float values[(size)*2];\
	}
#define float_array(size)\
	struct {\
		int count;\
		float values[size];\
	}
#define stem_table(size)\
	float_array(size)
typedef struct gs_type1_data_s gs_type1_data;
/* The garbage collector really doesn't want the client data pointer */
/* from a gs_type1_state to point to the gs_type1_data in the middle of */
/* a gs_font_type1, so we make the client data pointer (which is passed */
/* to the callback procedures) point to the gs_font_type1 itself. */
#ifndef gs_font_type1_DEFINED
#  define gs_font_type1_DEFINED
typedef struct gs_font_type1_s gs_font_type1;
#endif
struct gs_type1_data_s {
	/*int PaintType;*/		/* in gs_font_common */
	int CharstringType;		/* 1 or 2 */
	int (*subr_proc)(P4(gs_font_type1 *, int, bool, gs_const_string *));
	int (*seac_proc)(P3(gs_font_type1 *, int, gs_const_string *));
	int (*push_proc)(P3(gs_font_type1 *, const fixed *, int));
	int (*pop_proc)(P2(gs_font_type1 *, fixed *));
	void *proc_data;		/* data for subr_proc & seac_proc */
	int lenIV;			/* -1 means no encryption */
					/* (undocumented feature!) */
	uint subroutineNumberBias;	/* added to operand of callsubr */
					/* (undocumented feature!) */
		/* Type 2 charstring additions */
	uint gsubrNumberBias;		/* added to operand of callgsubr */
	long initialRandomSeed;
	fixed defaultWidthX;
	fixed nominalWidthX;
		/* For a description of the following hint information, */
		/* see chapter 5 of the "Adobe Type 1 Font Format" book. */
	int BlueFuzz;
	float BlueScale;
	float BlueShift;
#define max_BlueValues 7
	zone_table(max_BlueValues) BlueValues;
	float ExpansionFactor;
	bool ForceBold;
#define max_FamilyBlues 7
	zone_table(max_FamilyBlues) FamilyBlues;
#define max_FamilyOtherBlues 5
	zone_table(max_FamilyOtherBlues) FamilyOtherBlues;
	int LanguageGroup;
#define max_OtherBlues 5
	zone_table(max_OtherBlues) OtherBlues;
	bool RndStemUp;
	stem_table(1) StdHW;
	stem_table(1) StdVW;
#define max_StemSnap 12
	stem_table(max_StemSnap) StemSnapH;
	stem_table(max_StemSnap) StemSnapV;
		/* Additional information for Multiple Master fonts */
#define max_WeightVector 16
	float_array(max_WeightVector) WeightVector;
};
#define gs_type1_data_s_DEFINED

struct gs_font_type1_s {
	gs_font_base_common;
	gs_type1_data data;
};
extern_st(st_gs_font_type1);
#define public_st_gs_font_type1()	/* in gstype1.c */\
  gs_public_st_suffix_add1_final(st_gs_font_type1, gs_font_type1,\
    "gs_font_type1", font_type1_enum_ptrs, font_type1_reloc_ptrs,\
    gs_font_finalize, st_gs_font_base, data.proc_data)
