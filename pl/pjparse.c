/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pjparse.c */
/* PJL parser */
#include "memory_.h"
#include "scommon.h"
#include "pjparse.h"

/* Initialize the parser state. */
void
pjl_process_init(pjl_parser_state_t * pst)
{
    pjl_parser_init_inline(pst);
}

/* Process a buffer of PJL commands. */
int
pjl_process(pjl_parser_state_t * pst, void *pstate, stream_cursor_read * pr)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    int code = 0;

    while (p < rlimit) {
	if (pst->pos == 0) {	/* Look ahead for the @PJL prefix or a UEL. */
	    uint avail = rlimit - p;

	    if (!memcmp(p + 1, "\033%-12345X", min(avail, 9))) {	/* Might be a UEL. */
		if (avail < 9) {	/* Not enough data to know yet. */
		    break;
		}
		/* Skip the UEL and continue. */
		p += 9;
		continue;
	    } else if (!memcmp(p + 1, "@PJL", min(avail, 4))) {		/* Might be PJL. */
		if (avail < 4) {	/* Not enough data to know yet. */
		    break;
		}
		/* Definitely a PJL command. */
	    } else {		/* Definitely not PJL. */
		code = 1;
		break;
	    }
	}
	if (p[1] == '\n') {
	    ++p;
	    /* Parse the command.  Right now this is a no-op. */
	    pst->pos = 0;
	    continue;
	}
	/* Copy the PJL line into the parser's line buffer. */
	/* Always leave room for a terminator. */
	if (pst->pos < countof(pst->line) - 1)
	    pst->line[pst->pos] = p[1], pst->pos++;
	++p;
    }
    pr->ptr = p;
    return code;
}

/* Discard the remainder of a job.  Return true when we reach a UEL. */
/* The input buffer must be at least large enough to hold an entire UEL. */
bool
pjl_skip_to_uel(stream_cursor_read * pr)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;

    for (; p < rlimit; ++p)
	if (p[1] == '\033') {
	    uint avail = rlimit - p;

	    if (memcmp(p + 1, "\033%-12345X", min(avail, 9)))
		continue;
	    if (avail < 9)
		break;
	    pr->ptr = p + 9;
	    return true;
	}
    pr->ptr = p;
    return false;
}
