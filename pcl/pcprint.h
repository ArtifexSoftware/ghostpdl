/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcprint.h */
/* Currently the only export is for storage of user defined patterns;
   for use by hpgl/2 */

#ifndef pcprint_INCLUDED
#  define pcprint_INCLUDED

/* extracts pattern data and stores results in a dictionary */
int pcl_store_user_defined_pattern(P5(pcl_state_t *pcls,
				      pl_dict_t *pattern_dict,
				      pcl_id_t pattern_id,
				      const byte *data, uint size));

#endif				/* pcprint_INCLUDED */
