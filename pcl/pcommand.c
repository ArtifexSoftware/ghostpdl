/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcommand.c */
/* Utilities for PCL 5 commands */
#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gxstate.h"
#include "gsdevice.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcparam.h"

/* Get the command argument as an int, uint, or float. */
int
int_value(const pcl_value_t *pv)
{	return (int)(value_is_neg(pv) ? -pv->i : pv->i);
}
uint
uint_value(const pcl_value_t *pv)
{	return pv->i;
}
float
float_value(const pcl_value_t *pv)
{	return
	  (value_is_float(pv) ?
	   (value_is_neg(pv) ? -pv->i - pv->fraction : pv->i + pv->fraction) :
	   (float)int_value(pv));
}
pcl_fixed
pcl_fixed_value(const pcl_value_t *pv)
{	return (pcl_fixed)(float_value(pv) * (1L << _vshift));
}
pcl_ufixed
pcl_ufixed_value(const pcl_value_t *pv)
{	return (value_is_float(pv) ?
		(pcl_ufixed)((pv->i + pv->fraction) * (1L << _vshift)) :
		(pcl_ufixed)pv->i << _vshift);
}

/* Convert a 32-bit IEEE float to the local representation. */
float
word2float(uint32 word)
{
#if !arch_floats_are_IEEE
	/* Convert IEEE float to native float. */
	int sign_expt = word >> 23;
	int expt = sign_expt & 0xff;
	long mant = word & 0x7fffff;
	float fnum;

	if ( expt == 0 && mant == 0 )
	  return 0;
	mant += 0x800000;
	fnum = (float)ldexp((float)mant, expt - 127 - 24);
	if ( sign_expt & 0x100 )
	  fnum = -fnum;
	return fnum;
#else
	return *(float *)&word;
#endif
}

/* Set a parameter in the device.  Return the value from gs_putdeviceparams. */
#define begin_param1(list)\
  gs_c_param_list_write(&list, gs_state_memory(pcls->pgs))
#define end_param1(list)\
  end_param1_proc(&list, pcls)
private int
end_param1_proc(gs_c_param_list *alist, pcl_state_t *pcls)
{	int code;
	gs_c_param_list_read(alist);
	code = gs_putdeviceparams(gs_currentdevice(pcls->pgs),
				  (gs_param_list *)alist);
	gs_c_param_list_release(alist);
	return code;
}
#define plist ((gs_param_list *)&list)
/* Set a Boolean parameter. */
int
put_param1_bool(pcl_state_t *pcls, gs_param_name pkey, bool value)
{	gs_c_param_list list;

	begin_param1(list);
	/*code =*/ param_write_bool(plist, pkey, &value);
	return end_param1(list);
}
/* Set a float parameter. */
int
put_param1_float(pcl_state_t *pcls, gs_param_name pkey, floatp value)
{	gs_c_param_list list;
	float fval = value;

	begin_param1(list);
	/*code =*/ param_write_float(plist, pkey, &fval);
	return end_param1(list);
}
/* Set an integer parameter. */
int
put_param1_int(pcl_state_t *pcls, gs_param_name pkey, int value)
{	gs_c_param_list list;

	begin_param1(list);
	/*code =*/ param_write_int(plist, pkey, &value);
	return end_param1(list);
}

/* Run the reset code of all the modules. */
int
pcl_do_resets(pcl_state_t *pcls, pcl_reset_type_t type)
{	const pcl_init_t **init = pcl_init_table;
	int code = 0;

	for ( ; *init && code >= 0; ++init )
	  if ( (*init)->do_reset )
	    (*(*init)->do_reset)(pcls, type);
	return code;
}
