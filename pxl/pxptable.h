/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxptable.h */
/* Definitions for PCL XL parser tables */
/* Requires pxenum.h, pxoper.h, pxvalue.h */

#ifndef pxptable_INCLUDED
#  define pxptable_INCLUDED

/*
 * Define the table for checking attribute values.
 * The 'and' of the mask and the actual data type must be non-zero.
 * If the data type is ubyte, the value must be less than or equal to
 * the limit value.
 * If the procedure is not null, it provides an extra check, returning
 * 0 or an error code.
 */
#define value_check_proc(proc)\
  int proc(P1(const px_value_t *))
typedef struct px_attr_value_type_s {
  ushort mask;
  ushort limit;
  value_check_proc((*proc));
} px_attr_value_type_t;

extern const px_attr_value_type_t px_attr_value_types[];

/*
 * Define the table for checking and dispatching operators.
 * Each operator references a string of attributes: first a list of
 * required attributes, then 0, then a list of optional attributes,
 * then another 0.
 */
typedef struct px_operator_definition_s {
  px_operator_proc((*proc));
  const byte /*px_attribute*/ *attrs;
} px_operator_definition_t;

extern const px_operator_definition_t px_operator_definitions[];

/* Define tag and attribute names for debugging. */
#ifdef DEBUG
extern const char *px_tag_0_names[0x40];	/* tags 0-3f */
extern const char *px_tag_c0_names[0x40];	/* tags c0-ff */
extern const char *px_attribute_names[];
#endif

/* Define the table of operator names. */
/* This is needed even when not debugging, for producing error reports. */
extern const char *px_operator_names[0x80];	/* tags 40-bf */

#endif				/* pxptable_INCLUDED */
