/******************************************************************************
  File:     $Id: pclcap.h,v 1.10 2000/11/19 07:05:17 Martin Rel $
  Contents: Header for describing capabilities of PCL printers
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

#ifndef _pclcap_h	/* Inclusion protection */
#define _pclcap_h

/*****************************************************************************/

#include "gdeveprn.h"
#include "pclgen.h"

/*****************************************************************************/

/* Identifiers for printers (26) */
typedef enum {
  HPDeskJet,
  HPDeskJetPlus,
  HPDJPortable,		/* belongs to the 3xx family (DJ3/4 p. 1) */
  HPDJ310,
  HPDJ320,
  HPDJ340,
  HPDJ400,
  HPDJ500,
  HPDJ500C,
  HPDJ510, HPDJ520,	/* may be treated identically (TRG500 p. 1-3) */
  HPDJ540,
  HPDJ550C,
  HPDJ560C,
  pcl3_generic_old,
  HPDJ600,
  HPDJ660C, HPDJ670C,	/* programmatically identical (DJ6/8 p. 2) */
  HPDJ680C,
  HPDJ690C,
  HPDJ850C, HPDJ855C,
    /* HP refers to them collectively as "HP DJ85xC" (DJ6/8) */
  HPDJ870C,
  HPDJ890C,
  HPDJ1120C,
  pcl3_generic_new
} pcl_Printer;

/*****************************************************************************/

/* PCL printer description */
typedef struct {
  pcl_Printer id;
  pcl_Level level;

  eprn_PrinterDescription desc;
} pcl_PrinterDescription;

extern const pcl_PrinterDescription pcl3_printers[];
  /* This array is indexed by the values of 'pcl_Printer'. */

/*****************************************************************************/

extern void pcl3_fill_defaults(pcl_Printer printer, pcl_FileData *data);

/*****************************************************************************/

#endif	/* Inclusion protection */
