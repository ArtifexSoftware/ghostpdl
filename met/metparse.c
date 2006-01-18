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
#include "memory_.h"
#include "metparse.h"
#include "metelement.h"
#include "metcomplex.h"
#include "mt_error.h"
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

/* used to set an error in the midst of a callback */
private void
met_set_error(met_parser_state_t *st, int error_code)
{
    st->error_code = error_code;
    /* this effectively will stop the parsing even if the buffer is
       not complete */
    XML_SetElementHandler(st->parser, 0, 0);
    st->error_code = error_code;
    return;
}
#define INDENT (st->depth * 4)

/* start and end callbacks NB DOC */
private void
met_start(void *data, const char *el, const char **attr)
{
    int i;
    met_parser_state_t *st = data;
    gs_memory_t *mem = st->memory;
    int code;
    if ( gs_debug_c('i') ) {
        dprintf2(mem, "%*s\n", INDENT + strlen(el), el);
        for (i = 0; attr[i]; i += 2)
            dprintf3(mem, "%*s='%.256s'\n", 
                     INDENT + strlen(attr[i]),
                     attr[i], attr[i + 1]);
    }
    {
        met_element_t *metp = met_get_element_definition(el);
        if (!metp) {
            if ( gs_debug_c('i') ) {
                const char *msg = "element is not in definition table\n";
                dprintf2(mem, "%*s\n", INDENT + strlen(msg), msg);
            }
        } else if (!metp->init || !metp->action || !metp->done) {
            if ( gs_debug_c('i') ) {
                dprintf2(mem, "%*s has no implementation\n", 
                         INDENT + strlen(metp->element),
                         metp->element);
            }
        } else {
            code = (*metp->init)(&st->data_stack[st->depth], st->mets, el, attr);
            if (code < 0) {
		mt_rethrow(code, "met_start init");
                met_set_error(st, code);
            }
            code = (*metp->action)(st->data_stack[st->depth], st->mets);
            if (code < 0) {
		mt_rethrow(code, "met_start action");
                met_set_error(st, code);
            }
        }
    }
    st->depth++;

}

private void
met_end(void *data, const char *el)
{
    /* nb annoyingly we look this up again just as we did when
       starting to process the element */
    met_element_t *metp = met_get_element_definition(el);
    met_parser_state_t *st = data;
    gs_memory_t *mem = st->memory;
    int code = 0;

    st->depth--;
    if ( gs_debug_c('i') ) {
        /* nb gcc'ism */
        const char *term = "/";
        char str[strlen(term) + strlen(el) + 1];
        strcpy(str, term);
        strcat(str, el);
        dprintf2(mem, "%*s\n", INDENT + strlen(str), str);
    }

    if (metp && metp->done)
        code = (*metp->done)(st->data_stack[st->depth], st->mets);
    if (code < 0) {
	mt_rethrow(code, "met_end done");
        met_set_error(st, code);
    }
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
    met_parser_state_t *stp =
        (met_parser_state_t *)gs_alloc_bytes(memory,
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
    stp->error_code = 0;
    /* set up the start end callbacks */
    XML_SetElementHandler(p, met_start, met_end);
    XML_SetUserData(p, stp);
    return stp;
}

/* free the parser and corresponding expat parser */
void
met_process_release(met_parser_state_t *st)
{
    XML_ParserFree(st->parser);
    gs_free_object(st->memory, st, "met_process_release");
}

private void
met_unset_error_code(met_parser_state_t *st)
{
    /* restore handlers */
    XML_SetElementHandler(st->parser, met_start, met_end);
    st->error_code = 0;
}

int
met_process(met_parser_state_t *st, met_state_t *mets,  void *pzip, stream_cursor_read *pr)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    uint avail = rlimit - p;
    XML_Parser parser = st->parser;
    static bool started = false;
    /* expat hack - it is not possible to parse multiple xml documents
       without restarting the parser.  For expat a document ends with
       the closing of the opening element.  We create an artificial
       opening and closing (see met_process_shutdown for close) to
       parse multiple xml documents.  NB This should be handled with
       additional calls from the language switching api not with a
       static. */
    /* NB metro state within parser state see metparse.h */
    st->mets = mets;

    if (!started) {
        const char *start = "<JOB>";
        if (XML_Parse(parser, start, strlen(start), 0) == XML_STATUS_ERROR) {
            dprintf2(st->memory, "Parse error at line %d:\n%s\n",
                     XML_GetCurrentLineNumber(parser),
                     XML_ErrorString(XML_GetErrorCode(parser)));
            return -1;
        }
        started = true;
    }
            

    if (XML_Parse(parser, p + 1, avail, 0 /* done? */) == XML_STATUS_ERROR) {
        dprintf2(st->memory, "Parse error at line %d:\n%s\n",
              XML_GetCurrentLineNumber(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
        return mt_throw(-1, "xml parse error");
    } else if (st->error_code < 0) {
        int code = st->error_code;
        met_unset_error_code(st);
        return mt_throw(code, "xml parse error st error code");
    }
    /* nb for now we assume the parser has consumed exactly what we gave it. */
    pr->ptr = p + avail;
    return 0;
}

/* expat need explicit shutdown */
int
met_process_shutdown(met_parser_state_t *st)
{
    XML_Parser parser = st->parser;
    /* end the artificial bracketing */
    
    const char *end = "</JOB>";
    if (XML_Parse(parser, end, strlen(end), 0) == XML_STATUS_ERROR) {
        dprintf2(st->memory, "Parse error at line %d:\n%s\n",
                 XML_GetCurrentLineNumber(parser),
                 XML_ErrorString(XML_GetErrorCode(parser)));
        return -1;
    }

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
