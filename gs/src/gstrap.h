/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Definitions for trapping parameters and zones */

#ifndef gstrap_INCLUDED
#  define gstrap_INCLUDED

#include "gsparam.h"

/* ---------------- Types and structures ---------------- */

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;

#endif

/* Define the placement of image traps. */
typedef enum {
    tp_Center,
    tp_Choke,
    tp_Spread,
    tp_Normal
} gs_trap_placement_t;

#define gs_trap_placement_names\
  "Center", "Choke", "Spread", "Normal"

/* Define a trapping parameter set. */
typedef struct gs_trap_params_s {
    float BlackColorLimit;	/* 0-1 */
    float BlackDensityLimit;	/* > 0 */
    float BlackWidth;		/* > 0 */
    /* ColorantZoneDetails; */
    bool Enabled;
    /* HalftoneName; */
    bool ImageInternalTrapping;
    int ImageResolution;
    bool ImageToObjectTrapping;
    gs_trap_placement_t ImageTrapPlacement;
    float SlidingTrapLimit;	/* 0-1 */
    float StepLimit;		/* 0-1 */
    float TrapColorScaling;	/* 0-1 */
    float TrapWidth;		/* > 0 */
} gs_trap_params_t;

/* Define a trapping zone.  ****** SUBJECT TO CHANGE ****** */
typedef struct gs_trap_zone_s {
    gs_trap_params_t params;
    gx_path *zone;
} gs_trap_zone_t;

/* ---------------- Procedures ---------------- */

int gs_settrapparams(P2(gs_trap_params_t * params, gs_param_list * list));

#endif /* gstrap_INCLUDED */
