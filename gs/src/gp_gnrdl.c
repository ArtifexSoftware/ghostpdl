/* Copyright (C) 1999 Aladdin Enterprises.  All rights reserved.

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


/* GNU readline implementation */
#include "ctype_.h"
#include "string_.h"
#include "malloc_.h"
#include "memory_.h"
#include <readline/readline.h>
#include <readline/history.h>
#include "ghost.h"
#include "errors.h"
#include "gp.h"
#include "gsmalloc.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "stream.h"
#include "gxiodev.h"
#include "idict.h"
#include "iname.h"
#include "iutil.h"
#include "dstack.h"
#include "ostack.h"

/*
 * Note: This code was contributed by a user: please contact
 * Alexey Subbotin <A.Subbotin@lpi.ru> if you have questions.
 */

#define DEFAULT_BUFFER_SIZE 256
#define BUFSIZE_INCR 128

/* The file names can be set in the makefile if desired. */
/*
 * The prototypes for read_history and write_history are broken (they don't
 * specify the file name strings as const), so we have to omit the const
 * here too, or else add casts that will cause some compilers to complain.
 */
#ifndef GS_HISTFILE
#  define GS_HISTFILE ".gs_history"
#endif
private /*const*/ char *const GS_histfile = GS_HISTFILE;
#ifndef RL_INITFILE
#  define RL_INITFILE "~/.inputrc"
#endif
private /*const*/ char *const RL_initfile = RL_INITFILE;

/* initial key codes to make dictionary current_completion_dict
   (may be changed to any key sequences by settings in init file) */
#define RL_systemdict_keycode	('s' | 0x80 )	/* Alt-s */
#define RL_userdict_keycode	('u' | 0x80 )	/* Alt-u */
#define RL_currentdict_keycode	('c' | 0x80 )	/* Alt-c */
#define RL_filenames_keycode	('f' | 0x80 )	/* Alt-f */
#define RL_show_value_keycode	('i' | 0x80 )	/* Alt-i ( = info) */
/* e.g.:   "\C-x\C-u": complete-from-userdict  */
#define RL_systemdict_func	"gs-complete-from-systemdict"
#define RL_userdict_func	"gs-complete-from-userdict"
#define RL_currentdict_func	"gs-complete-from-currentdict"
#define RL_filenames_func	"gs-complete-filenames"
#define RL_show_value_func	"gs-show-name-value"

#define RL_init_buffsz  DEFAULT_BUFFER_SIZE

private const char *const ps_delimiters = " \n\t{}[]()/";

#define is_regular(A)  (strchr(ps_delimiters,(A)) == NULL)

typedef enum {
    compl_systemdict,
    compl_userdict,
    compl_currentdict,
    compl_filenames
} compl_t;

typedef struct readline_data_s {
    compl_t completion_type;
    gs_memory_t *mem;
    i_ctx_t *i_ctx_p;
    /* current completion state */
    int c_idx, c_len;
} readline_data_t;
gs_private_st_ptrs1(st_readline_data, readline_data_t, "readline_data_t",
  readline_data_enum_ptrs, readline_data_reloc_ptrs, i_ctx_p);

private readline_data_t *the_readline_data;

private char *
gs_readline_complete(char *text, int state)
{
    readline_data_t *const p = the_readline_data;
    i_ctx_t *i_ctx_p = p->i_ctx_p;
    ref *cdict;
    ref eltp[2];

    switch (p->completion_type) {
    case compl_systemdict:
	cdict = systemdict;
	break;
    case compl_userdict:
	cdict = idict_stack.stack.bot + dstack_userdict_index;
	break;
    case compl_currentdict:
	cdict = idict_stack.stack.p;
	break;
    case compl_filenames:
	return (*filename_completion_function) (text, state);
    default:
	return NULL;
    }
    if (!state) {
	p->c_len = strlen(text);
	if (!(p->c_idx = dict_first(cdict)))
	    return NULL;
    }
    while ((p->c_idx = dict_next(cdict, p->c_idx, eltp)) >= 0) {
	const byte *rname = eltp->value.refs->value.const_bytes;
	uint rlen = eltp->value.refs->tas.rsize >> 2;

	if (rname && !strncmp((const char *)rname, text, p->c_len)) {
	    char *name = (char *)malloc(rlen + 1);

	    memcpy(name, rname, rlen);
	    name[rlen] = 0;
	    return name;
	}
    }
    return NULL;
}

private int
rl_systemdict_compl(int count, int key)
{
    readline_data_t *const p = the_readline_data;

    p->completion_type = compl_systemdict;
    return 0;
}
private int
rl_userdict_compl(int count, int key)
{
    readline_data_t *const p = the_readline_data;

    p->completion_type = compl_userdict;
    return 0;
}
private int
rl_currentdict_compl(int count, int key)
{
    readline_data_t *const p = the_readline_data;

    p->completion_type = compl_currentdict;
    return 0;
}
private int
rl_filenames_compl(int count, int key)
{
    readline_data_t *const p = the_readline_data;

    p->completion_type = compl_filenames;
    return 0;
}

private int
rl_show_name_value(int count, int key)
{
    readline_data_t *const p = the_readline_data;
    i_ctx_t *i_ctx_p = p->i_ctx_p;
    int i = rl_point;
    char *c = rl_line_buffer + rl_point;
    ref nref;
    ref *pvref;

    while (is_regular(*c) && i < rl_end) {
	c++;
	i++;
    }
    while (!is_regular(*c) && i) {
	c--;
	i--;
    }
    if (!is_regular(*c))
	return 0;
    while (is_regular(*c) && c > rl_line_buffer)
	c--;
    if (!is_regular(*c))
	c++;
    i += ((rl_line_buffer - c) + 1);
    /*
     * Now the name to be looked up extends from c through c + i - 1.
     */
    if (name_ref((const byte *)c, (uint)i, &nref, -1) < 0 ||
	(pvref = dict_find_name(&nref)) == 0
	)
	puts("\nnot found");
    else {
#define MAX_CVS 128
	char str[MAX_CVS];
	const byte *pchars = (const byte *)str;
	uint len;
	int code = obj_cvp(pvref, (byte *)str, MAX_CVS, &len, 1, 0);

	putchar('\n');
	if (code < 0) {
	    code = obj_string_data(pvref, &pchars, &len);
	    if (code >= 0)
		switch (r_type(pvref)) {
		case t_string:
		    putchar('(');
		    fwrite(pchars, 1, len, stdout);
		    pchars = (const byte *)")", len = 1;
		    break;
		case t_name:
		    if (!r_has_attr(pvref, a_executable))
			putchar('/');
		    break;
		default:
		    code = obj_cvs(pvref, (byte *)str, MAX_CVS, &len, &pchars);
	    }
	}
	if (code < 0)
	    puts("-error-");
	else {
	    fwrite(pchars, 1, len, stdout);
	    putchar('\n');
	}
    }
   rl_on_new_line();
   rl_forced_update_display(); 
   return 0;
}


int
gp_readline_init(void **preadline_data, gs_memory_t * mem)
{
    readline_data_t *p;

    using_history();
    read_history(GS_histfile);

    p = (readline_data_t *)
	gs_alloc_struct_immovable(mem, readline_data_t, &st_readline_data,
				  "gp_readline_init(readline structure)");
    if (!p)
	return_error(e_VMerror);
    gs_register_struct_root(mem, NULL, (void **)&the_readline_data,
			    "the_readline_data");

    p->mem = mem;
    p->i_ctx_p = 0;		/* only meaningful when reading a line */
    p->c_idx = p->c_len = 0;
    p->completion_type = compl_systemdict;

    rl_add_defun(RL_systemdict_func, rl_systemdict_compl, RL_systemdict_keycode);
    rl_add_defun(RL_userdict_func, rl_userdict_compl, RL_userdict_keycode);
    rl_add_defun(RL_currentdict_func, rl_currentdict_compl, RL_currentdict_keycode);
    rl_add_defun(RL_filenames_func, rl_filenames_compl, RL_filenames_keycode);
    rl_add_defun(RL_show_value_func, rl_show_name_value, RL_show_value_keycode);

    rl_read_init_file(RL_initfile);

    /*
     * The GNU readline API is pretty badly broken, as indicated in the
     * following comments about the following statics that it declares.
     * (Just to begin with, using statics at all is broken design.)
     */
    /*
     * rl_completion_entry_function should be declared with the same
     * prototype as gs_readline_complete; however, it's declared as
     * Function *, where Function is int ()();
     */
    rl_completion_entry_function = (Function *)gs_readline_complete;
    /*
     * rl_basic_word_break_characters should declared as const char *;
     * however, it's declared as char *.
     */ 
    rl_basic_word_break_characters = (char *)ps_delimiters;

    *preadline_data = p;
    the_readline_data = p;	/* HACK */
    return 0;
}

void
gp_readline_finit(void *data)
{
    readline_data_t *dp = (readline_data_t *)data;

    if (dp)
	gs_free_object(dp->mem, dp, "gp_readline_finit(readline structure)");
    write_history(GS_histfile);
    clear_history();
}

int
gp_readline(stream *ignore_s_in, stream *ignore_s_out,
	    void *readline_data,
	    gs_const_string *ignore_prompt, gs_string * buf,
	    gs_memory_t * bufmem, uint * pcount, bool *pin_eol,
	    bool (*is_stdin)(P1(const stream *)))
{
    /* HACK: ignore readline_data, which is currently not supplied. */
    readline_data_t *p = the_readline_data;
    char *c;
    char prompt[64];
    uint count;
    gx_io_device *indev = gs_findiodevice((const byte *)"%stdin", 6);
    /* HACK: get the context pointer from the IODevice.  See ziodev.c. */
    i_ctx_t *i_ctx_p = (i_ctx_t *)indev->state;

    p->i_ctx_p = i_ctx_p;
    count = ref_stack_count(&o_stack);
    if (count > 2)
	sprintf(prompt, "GS<%d>", count - 2);
    else
	strcpy(prompt, "GS>");

    if ((c = readline(prompt)) == NULL) {
	*pcount = 0;
	*pin_eol = false;
	return EOFC;
    } else {
	count = strlen(c) + 1;
	if (*c)
	    add_history(c);
	if (count >= buf->size) {
	    if (!bufmem)
		return ERRC;	/* no better choice */
	    {
		uint nsize = count + BUFSIZE_INCR;
		byte *nbuf = gs_resize_string(bufmem, buf->data, buf->size,
					      nsize,
					      "gp_readline(resize buffer)");

		if (nbuf == 0)
		    return ERRC; /* no better choice */
		buf->data = nbuf;
		buf->size = nsize;
	    }
	}
	memcpy(buf->data, c, count);
	free(c);
	*pin_eol = true;
	*pcount = count;
	return 0;
    }
}
