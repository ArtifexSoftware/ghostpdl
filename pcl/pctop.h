/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pctop.h */
/* Interface to main program utilities for PCL5 interpreter */

#ifndef pctop_INCLUDED
#  define pctop_INCLUDED

/* Set PCL's target device */
void pcl_set_target_device(P2(pcl_state_t *pcs, gx_device *pdev));

/* Get PCL's curr target device */
gx_device * pcl_get_target_device(P1(pcl_state_t *pcs));


#endif				/* pctop_INCLUDED */
