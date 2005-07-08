/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* metro parser module */

#include <expat.h>
#include "metparse.h"
/* have expat use the gs memory manager. */

/* NB global - needed for expat callbacks */
gs_memory_t *gs_mem_ptr = NULL;

/* memory callbacks */
private void *
met_expat_alloc(size_t size)
{
    return gs_alloc_bytes(gs_mem_ptr, size, "met_expat_alloc");
}

private void
met_expat_free(void *ptr)
{
    return gs_free_object(gs_mem_ptr, ptr, "met_expat_free");
}

private void *
met_expat_realloc(void *ptr, size_t size)
{
    return gs_resize_object(gs_mem_ptr, ptr, size, "met_expat_free");
}

/* start and end callbacks NB DOC */
private void
met_start(void *data, const char *el, const char **attr)
{
    int i;
    met_parser_state_t *st = data;
    gs_memory_t *mem = st->memory;

    for (i = 0; i < st->depth; i++)
        dprintf(mem, "  ");

    dprintf1(mem, "%s", el);

    for (i = 0; attr[i]; i += 2) {
        dprintf2(mem, " %s='%s'", attr[i], attr[i + 1]);
    }

    dprintf(mem, "\n");
    st->depth++;

}

private void
met_end(void *data, const char *el)
{
    met_parser_state_t *st = data;
    st->depth--;
}

/* allocate the parser, and set the global memory pointer see above */
met_parser_state_t *
met_process_alloc(gs_memory_t *memory)
{
    /* memory procedures */
    const XML_Memory_Handling_Suite memprocs = {
        met_expat_alloc, met_expat_realloc, met_expat_free
    };
        
    /* NB should have a structure descriptor */
    met_parser_state_t *stp = gs_alloc_bytes(memory,
                                             sizeof(met_parser_state_t),
                                             "met_process_alloc");
    XML_Parser p; 
    /* NB set the static mem ptr used by expat callbacks */
    gs_mem_ptr = memory;

    if (!stp)
        return NULL;

    p = XML_ParserCreate_MM(NULL /* encoding */,
                            &memprocs,
                            NULL /* name space separator */);
    if (!p)
        return NULL;

    stp->memory = memory;
    stp->parser = p;
    stp->depth = 0;
    /* set up the start end callbacks */
    XML_SetElementHandler(p, met_start, met_end);
    XML_SetUserData(p, stp);
    return stp;
}

/* free the parser and corresponding expat parser */
void
met_process_release(met_parser_state_t *st)
{
    gs_free_object(st->memory, st, "met_process_release");
    XML_ParserFree(st->parser);
}

int
met_process(met_parser_state_t *st, met_state_t *mets, stream_cursor_read *pr)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    uint avail = rlimit - p;
    XML_Parser parser = st->parser;

    if (XML_Parse(parser, p + 1, avail, 0 /* done? */) == XML_STATUS_ERROR) {
        dprintf2(st->memory, "Parse error at line %d:\n%s\n",
              XML_GetCurrentLineNumber(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
        return -1;
    }

    return 0;
}

/* expat need explicit shutdown */
int
met_process_shutdown(met_parser_state_t *st)
{
    XML_Parser parser = st->parser;
    if (XML_Parse(parser, 0 /* data buffer */, 
                  0 /* buffer length */,
                  1 /* done? */ ) == XML_STATUS_ERROR) {
        dprintf2(st->memory, "Parse error at line %d:\n%s\n",
                 XML_GetCurrentLineNumber(parser),
                 XML_ErrorString(XML_GetErrorCode(parser)));
        return -1;
    }
    return 0;
}
