/* Copyright (C) 1995, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* opextern.h */
/* Externally accessible operator declarations */

/*
 * Normally, the procedures that implement PostScript operators (named zX
 * where X is the name of the operator, e.g., zadd) are private to the
 * file in which they are defined.  There are, however, a surprising
 * number of these procedures that are used from other files.
 * This file, opextern.h, declares all z* procedures that are
 *	- referenced from outside their defining file, and
 *	- present in *all* configurations of the interpreter.
 * For z* procedures referenced from outside their file but not present
 * in all configurations (e.g., Level 2 operators), the file making the
 * reference must include a local extern.  Not pretty, but c'est la vie.
 */

/* Operators exported for the special operator encoding in interp.c. */
int zadd(P1(os_ptr));
int zdef(P1(os_ptr));
int zdup(P1(os_ptr));
int zexch(P1(os_ptr));
int zif(P1(os_ptr));
int zifelse(P1(os_ptr));
int zindex(P1(os_ptr));
int zpop(P1(os_ptr));
int zroll(P1(os_ptr));
int zsub(P1(os_ptr));

/* Operators exported for server loop implementations. */
int zflush(P1(os_ptr));
int zflushpage(P1(os_ptr));
int zsave(P1(os_ptr));
int zrestore(P1(os_ptr));

/* Operators exported for save/restore. */
int zgsave(P1(os_ptr));
int zgrestore(P1(os_ptr));

/* Operators exported for Level 2 pagedevice facilities. */
int zcopy_gstate(P1(os_ptr));
int zcurrentgstate(P1(os_ptr));
int zgrestoreall(P1(os_ptr));
int zgstate(P1(os_ptr));
int zreadonly(P1(os_ptr));
int zsetdevice(P1(os_ptr));
int zsetgstate(P1(os_ptr));

/* Operators exported for Level 2 "wrappers". */
int zimage(P1(os_ptr));
int zimagemask(P1(os_ptr));
int zwhere(P1(os_ptr));

/* Operators exported for specific-VM operators. */
int zarray(P1(os_ptr));
int zdict(P1(os_ptr));
int zpackedarray(P1(os_ptr));
int zstring(P1(os_ptr));

/* Operators exported for user path decoding. */
/* Note that only operators defined in all configurations are declared here. */
int zclosepath(P1(os_ptr));
int zcurveto(P1(os_ptr));
int zlineto(P1(os_ptr));
int zmoveto(P1(os_ptr));
int zrcurveto(P1(os_ptr));
int zrlineto(P1(os_ptr));
int zrmoveto(P1(os_ptr));

/* Operators exported for CIE cache loading. */
int zcvx(P1(os_ptr));
int zexec(P1(os_ptr));		/* also for .runexec */
int zfor(P1(os_ptr));

/* Odds and ends */
int zbegin(P1(os_ptr));
int zcleartomark(P1(os_ptr));
int zend(P1(os_ptr));
int zclosefile(P1(os_ptr));	/* for runexec_cleanup */
int zsetfont(P1(os_ptr));	/* for cshow_continue */

/* Operators exported for special customer needs. */
int zcurrentdevice(P1(os_ptr));
int ztoken(P1(os_ptr));
int ztokenexec(P1(os_ptr));
int zwrite(P1(os_ptr));
