/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pcparse.c */
/* PCL5 parser */
#include "stdio_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "scommon.h"
#include "pcparse.h"
#include "pcstate.h"            /* for display_functions */
#include "pcursor.h"
#include "rtmisc.h"
#include "rtgmode.h"

/* We don't know whether an Enable Display Functions takes effect while */
/* defining a macro.  To play it safe, we're providing both options. */
/* If the following #define is uncommented, E.D.F. *does* take effect */
/* while scanning a macro definition. */
/*#define DISPLAY_FUNCTIONS_IN_MACRO*/

/* ---------------- Command definition ---------------- */

/* Register a command.  Return true if this is a redefinition. */
static bool
pcl_register_command(byte * pindex, const pcl_command_definition_t * pcmd,
                     pcl_parser_state_t * pcl_parser_state)
{
    int index = pcl_parser_state->definitions->pcl_command_next_index;

    byte prev = *pindex;

    if (prev != 0 && prev <= index
        && pcl_parser_state->definitions->pcl_command_list[prev] == pcmd)
        index = prev;
    else if (index != 0
             && pcl_parser_state->definitions->pcl_command_list[index] ==
             pcmd);
    else
        pcl_parser_state->definitions->
            pcl_command_list
            [pcl_parser_state->definitions->pcl_command_next_index =
             ++index] = (pcl_command_definition_t *) pcmd;
    *pindex = index;
    return (prev != 0 && prev != index);
}

/* Define a command or list of commands. */
void
pcl_define_control_command(int /*char */ chr,
                           const pcl_command_definition_t * pcmd,
                           pcl_parser_state_t * pcl_parser_state)
{
#ifdef DEBUG
    if (chr < 0
        || chr >=
        countof(pcl_parser_state->definitions->pcl_control_command_indices))
        if_debug1('I', "Invalid control character %d\n", chr);
    else if (
#endif
                pcl_register_command(&pcl_parser_state->
                                     definitions->pcl_control_command_indices
                                     [chr], pcmd, pcl_parser_state)
#ifdef DEBUG
        )
        if_debug1('I', "Redefining control character %d\n", chr);
#endif
    ;
}
void
pcl_define_escape_command(int /*char */ chr,
                          const pcl_command_definition_t * pcmd,
                          pcl_parser_state_t * pcl_parser_state)
{
#ifdef DEBUG
    if (chr < min_escape_2char || chr > max_escape_2char)
        if_debug1('I', "Invalid escape character %c\n", chr);
    else if (
#endif
                pcl_register_command(&pcl_parser_state->
                                     definitions->pcl_escape_command_indices
                                     [chr - min_escape_2char], pcmd,
                                     pcl_parser_state)
#ifdef DEBUG
        )
        if_debug1('I', "Redefining ESC %c\n", chr)
#endif
            ;
}

/*
 * Convert escape classes to second level dispatch indices.
 */
static const byte pcl_escape_class_indices[max_escape_class -
                                           min_escape_class + 1] = {
    0, 0, 0, 0, 1 /*% */ , 2 /*& */ , 0, 3 /*( */ , 4 /*) */ , 5 /***/ , 0, 0,
    0, 0, 0
};

void
pcl_define_class_command(int /*char */ class, int /*char */ group,
                         int /*char */ command,
                         const pcl_command_definition_t * pcmd,
                         pcl_parser_state_t * pcl_parser_state)
{
#ifdef DEBUG
    if (class < min_escape_class || class > max_escape_class ||
        pcl_escape_class_indices[class - min_escape_class] == 0 ||
        (group != 0 && (group < min_escape_group || group > max_escape_group))
        || command < min_escape_command || command > max_escape_command)
        if_debug3('I', "Invalid command %c %c %c\n", class, group, command);
    else if (
#endif
                pcl_register_command(&pcl_parser_state->
                                     definitions->pcl_grouped_command_indices
                                     [pcl_escape_class_indices
                                      [class - min_escape_class] - 1]
                                     [group ==
                                      0 ? 0 : group - min_escape_group + 1]
                                     [command - min_escape_command], pcmd,
                                     pcl_parser_state)
#ifdef DEBUG
        )
        if_debug3('I', "Redefining ESC %c %c %c\n", class,
                  (group == 0 ? ' ' : group), command)
#endif
            ;
}
void
pcl_define_class_commands(int /*char */ class,
                          const pcl_grouped_command_definition_t * pgroup,
                          pcl_parser_state_t * pcl_parser_state)
{
    const pcl_grouped_command_definition_t *pgc = pgroup;

    for (; pgc->command != 0; ++pgc)
        pcl_define_class_command(class, pgc->group, pgc->command,
                                 &pgc->defn, pcl_parser_state);
}

/*
 * Look up an escape command.  The arguments are as follows:
 *	ESC x		0, 0, 'x'
 *	ESC ? <arg> c	'?', 0, 'c'
 *	ESC ? b <arg> c	'?', 'b', 'c'
 * The caller is responsible for providing valid arguments.
 */
static const pcl_command_definition_t *
pcl_get_command_definition(pcl_parser_state_t * pcl_parser_state,
                           int /*char */ class,
                           int /*char */ group,
                           int /*char */ command)
{
    const pcl_command_definition_t *cdefn = 0;

    if (class == 0) {
        if (command >= min_escape_2char && command <= max_escape_2char)
            cdefn = pcl_parser_state->definitions->pcl_command_list
                [pcl_parser_state->
                 definitions->pcl_escape_command_indices[command -
                                                         min_escape_2char]];
    } else {
        int class_index = pcl_escape_class_indices[class - min_escape_class];

        if (class_index)
            cdefn = pcl_parser_state->definitions->pcl_command_list
                [pcl_parser_state->
                 definitions->pcl_grouped_command_indices[class_index - 1]
                 [group ? group - min_escape_group + 1 : 0]
                 [command - min_escape_command]
                ];
    }
#ifdef DEBUG
    if (cdefn == 0) {
        if (class == 0)
            if_debug1('I', "ESC %c undefined\n", command);
        else if (group == 0)
            if_debug2('I', "ESC %c %c undefined\n", class, command);
        else
            if_debug3('I', "ESC %c %c %c undefined\n", class, group, command);
    }
#endif
    return cdefn;
}

/* ---------------- Parsing ---------------- */

/* Initialize the parser state. */
int
pcl_process_init(pcl_parser_state_t * pst, pcl_state_t * pcs)
{
    pcl_args_t args;
    int code = 0;
    pcl_parser_init_inline(pst);

    /*
     * RTL Files should start with a PCL escape sequence to enter the
     * GL/2 parser but many do not, so we start in HPGL.  If an escape
     * sequence is detected the parse mode returns to PCL.
     */

    if (pcs->personality == rtl) {
        arg_set_uint(&args, 0);
        code = rtl_enter_hpgl_mode(&args, pcs);
    }
    return code;
}

/* Adjust the argument value according to the command's argument type. */
/* Return 1 if the command should be ignored. */
static int
pcl_adjust_arg(pcl_args_t * pargs, const pcl_command_definition_t * pdefn)
{
    uint acts = pdefn->actions;

    if (value_is_neg(&pargs->value)) {
        switch (acts & pca_neg_action) {
            case pca_neg_clamp:
                arg_set_uint(pargs, 0);
                break;
            case pca_neg_error:
                return e_Range;
            case pca_neg_ignore:
                return 1;
            default:           /* case pca_neg_ok */
                ;
        }
    } else if (pargs->value.i > 32767)  /* overflowed int range */
        switch (acts & pca_big_action) {
            case pca_big_clamp:
                arg_set_uint(pargs, 32767);
                break;
            case pca_big_error:
                return e_Range;
            case pca_big_ignore:
                return 1;
            default:           /* case pca_big_ok */
                ;
        }
    return 0;
}

/* Append some just-scanned input data to the macro being defined. */
static int
append_macro(const byte * from, const byte * to, pcl_state_t * pcs)
{
    if (pcs->macro_definition != NULL) {
        uint count = to - from;
        uint size = gs_object_size(pcs->memory, pcs->macro_definition);
        byte *new_defn =
            gs_resize_object(pcs->memory, pcs->macro_definition, size + count,
                             "append_macro");

        if (new_defn == 0)
            return_error(e_Memory);
        memcpy(new_defn + size, from + 1, count);
        pcs->macro_definition = new_defn;
    }
    return 0;
}

#ifdef DEBUG
static void
debug_dump_char(gs_memory_t *mem, char chr)
{
    dmprintf1(mem, (chr >= 33 && chr <= 126 ?
                    "%c" : "\\%03o"), (unsigned char)chr);
    if (chr == '\\')
        dmprintf1(mem, "%c", '\\');
    return;
}
#endif

/* Process a buffer of PCL commands. */
int
pcl_process(pcl_parser_state_t * pst, pcl_state_t * pcs,
            stream_cursor_read * pr)
{
    const byte *p = pr->ptr;
    const byte *rlimit = pr->limit;
    int code = 0;
    int bytelen = 0;
    bool in_macro = pcs->defining_macro;
    /* Record how much of the input we've copied into a macro */
    /* in the process of being defined. */
    const byte *macro_p = p;

/* Reset the parameter scanner */
#define avalue pst->args.value
#define param_init()\
  (avalue.type = pcv_none, avalue.i = 0)

#ifdef DISPLAY_FUNCTIONS_IN_MACRO
#  define do_display_functions() 1
#else
#  define do_display_functions() (!in_macro)
#endif

    if (rlimit - p < pst->min_bytes_needed) {
        pst->min_bytes_needed = 0;
        p = rlimit;
        goto x;
    } else {
        pst->min_bytes_needed = 0;
    }

    while (p < rlimit) {
        byte chr = 0x00; /* silence a compiler warning */
        const pcl_command_definition_t *cdefn = NULL;
        switch (pst->scan_type) {
            case scanning_data:
                {               /* Accumulate command data in a buffer. */
                    uint count = uint_arg(&pst->args);
                    uint pos = pst->data_pos;

                    if (pos < count) {
                        uint avail = rlimit - p;
                        uint copy = min(count - pos, avail);

                        memcpy(pst->args.data + pos, p + 1, copy);
                        pst->data_pos += copy;
                        p += copy;
                        continue;
                    }
                    /* Invoke the command. */
                    cdefn = pcl_get_command_definition(pst,
                                                       pst->param_class,
                                                       pst->param_group,
                                                       pst->args.command);
                    if (pst->short_hand) {
                        pst->scan_type = scanning_parameter;
                        continue;
                    } else
                        pst->scan_type = scanning_none;
                    break;
                }
            case scanning_display:
                {
                    /* Display, don't execute, all characters. */
                    chr = *++p;
                    if (chr == ESC) {
                        int index;

                        if (p >= rlimit)
                            goto x;
                        if (p[1] >= min_escape_2char
                            && p[1] <= max_escape_2char
                            && (index =
                                pst->
                                definitions->pcl_escape_command_indices[p[1] -
                                                                        min_escape_2char])
                            != 0
                            && pst->definitions->
                            pcl_command_list[index]->proc ==
                            pcl_disable_display_functions) {
                            if (do_display_functions()) {
                                pst->args.command = chr;
                                code = pcl_plain_char(&pst->args, pcs);
                                if (code < 0)
                                    goto x;
                            }
                            pst->args.command = chr = *++p;
                            pcl_disable_display_functions(&pst->args, pcs);
                            pst->scan_type = scanning_none;
                        }
                    }
                    if (do_display_functions()) {
                        if (chr == CR) {
                            code = pcl_do_CR(pcs);
                            if (code >= 0)
                                code = pcl_do_LF(pcs);
                        } else {
                            pst->args.command = chr;
                            code = pcl_plain_char(&pst->args, pcs);
                        }
                        if (code < 0)
                            goto x;
                    }
                    continue;
                }
            case scanning_parameter:
                for (;;) {
                    if (p >= rlimit)
                        goto x;
                    chr = *++p;
                    /*
                     * The parser for numbers is very lenient, and accepts
                     * many strings that aren't valid numbers.  If this
                     * ever becomes a problem, we can tighten it up....
                     */
                    if (pst->garbage_in_parameter) {
                        if (chr < ' ' || chr > '?') {
                            pst->garbage_in_parameter = false; /* no garbage anymore */
                            break;
                        }
                    } else {
                        if (chr >= '0' && chr <= '9') {
                            chr -= '0';
                            if (value_is_float(&avalue))
                                avalue.fraction += (chr / (pst->scale *= 10));
                            else
                                avalue.type |= pcv_int,
                                    avalue.i = avalue.i * 10 + chr;
                        } else if (chr == '-')
                            avalue.type |= pcv_neg;
                        else if (chr == '+')
                            avalue.type |= pcv_pos;
                        else if (chr == '.')
                            avalue.type |= pcv_float,
                                avalue.fraction = 0, pst->scale = 1.0;
                        else if (chr == ' ')
                            continue;
                        else if (chr >= ' ' && chr <= '?') {        /* Ignore garbage nearby in the code space. */
                            /* skip the remainder until a chr outside this range is found */
                            if (avalue.type != pcv_none)
                                pst->garbage_in_parameter = true;
                            continue;
                        } else
                            break;
                    }
                }
#ifdef DEBUG
                if (gs_debug_c('i')) {
                    dmprintf(pcs->memory, "ESC ");
                    debug_dump_char(pcs->memory, pst->param_class);
                    dmprintf(pcs->memory, " ");
                    debug_dump_char(pcs->memory, pst->param_group);
                    if (value_is_present(&avalue)) {
                        dmputc(pcs->memory, ' ');
                        if (value_is_signed(&avalue))
                            dmputc(pcs->memory,
                                   (value_is_neg(&avalue) ? '-' : '+'));
                        if (value_is_float(&avalue))
                            dmprintf1(pcs->memory, "%g",
                                      avalue.i + avalue.fraction);
                        else
                            dmprintf1(pcs->memory, "%u", avalue.i);
                    }
                    dmprintf1(pcs->memory, " %c\n", chr);
                }
#endif
                pst->short_hand = false;
                if (chr >= min_escape_command + 32 &&
                    chr <= max_escape_command + 32) {
                    pst->short_hand = true;
                    chr -= 32;
                } else if (chr >= min_escape_command &&
                           chr <= max_escape_command)
                    pst->scan_type = scanning_none;
                else {
                    pst->scan_type = scanning_none;
                    /* Rescan the out-of-place character. */
                    --p;
                    continue;
                }
                /* Dispatch on param_class, param_group, and chr. */
                cdefn = pcl_get_command_definition(pst,
                                                   pst->param_class,
                                                   pst->param_group, chr);
                if (cdefn) {
                    if_debug1m('i', pcs->memory, "   [%s]\n", cdefn->cname);
                    code = pcl_adjust_arg(&pst->args, cdefn);
                    if (code < 0)
                        goto x;
                    if (cdefn->actions & pca_byte_data) {
                        uint count = uint_arg(&pst->args);

                        if ((count > 0) && (rlimit - p < count)) {
                            pst->args.data =
                                gs_alloc_bytes(pcs->memory, count,
                                               "command data");
                            if (pst->args.data == 0) {
                                --p;
                                code = gs_note_error(e_Memory);
                                (void)pcl_grestore(pcs);
                                goto x;
                            }
                            pst->args.data_on_heap = true;
                            pst->args.command = chr;
                            pst->data_pos = 0;
                            pst->scan_type = scanning_data;
                            continue;
                        }
                        pst->args.data = (byte *) (p + 1);
                        pst->args.data_on_heap = false;
                        p += count;
                    }
                    break;
                }
                param_init();
                continue;
            case scanning_none:
                if (pcs->parse_other) { /* HPGL/2 mode */
                    pr->ptr = p;
                    code = (*pcs->parse_other)
                        (pcs->parse_data, pcs, pr);
                    p = pr->ptr;
                    if (code < 0 || (code == 0 && pcs->parse_other))
                        goto x;
                }

                chr = *++p;
                /* check for multi-byte scanning */
                bytelen = pcl_char_bytelen(chr, pcs->text_parsing_method);
                if (bytelen == 0) {
                    bytelen = 1;        /* invalid utf-8 leading char */
                }
                if (bytelen > 1) {
                    /* check if we need more data */
                    if ((p + bytelen - 1) > rlimit) {
                        pst->min_bytes_needed = 2;
                        --p;
                        goto x;
                    }
                    if_debug2m('i', pcs->memory, "%x%x\n", p[0], p[1]);
                    code = pcl_text(p, bytelen, pcs, false);
                    if (code < 0)
                        goto x;
                    /* now pass over the remaining bytes */
                    p += (bytelen - 1);
                    cdefn = NULL;
                } else if (chr != ESC) {
#ifdef DEBUG
                    if (gs_debug_c('i')) {
                        if_debug1m('i', pcs->memory, "[i%d] ", bytelen);
                        debug_dump_char(pcs->memory, chr);
                        if_debug0m('i', pcs->memory, "\n");
                    }
#endif
                    cdefn = pst->definitions->pcl_command_list
                        [chr < 33 ?
                         pst->definitions->pcl_control_command_indices[chr] :
                         pst->definitions->pcl_control_command_indices[1]];
                    if ((cdefn == 0 || cdefn->proc == pcl_plain_char) &&
                        !in_macro && !pcs->parse_other
                        && !pcs->raster_state.graphics_mode) {
                        /* Look ahead for a run of plain text.  We can
                           be very conservative about this, because
                           this is only a performance enhancement. */
                        const byte *str = p;

                        while (p < rlimit && p[1] >= 32 && p[1] <= 127) {
#ifdef DEBUG
                            if (gs_debug_c('i')) {
                                if (p == str)
                                    if_debug1m('i', pcs->memory, "[i%d] ", bytelen);
                                debug_dump_char(pcs->memory, p[1]);
                            }
#endif
                            ++p;
                        }

                        if_debug0m('i', pcs->memory, "\n");
                        code = pcl_text(str, (uint) (p + 1 - str),
                                        pcs, false);
                        if (code < 0)
                            goto x;
                        cdefn = NULL;
                    }
                } else {
                    if (p >= rlimit) {
                        pst->min_bytes_needed = 2;
                        --p;
                        goto x;
                    }
                    chr = *++p;
                    if (chr < min_escape_class || chr > max_escape_class) {
                        cdefn = pcl_get_command_definition(pst, 0, 0, chr);
                        if (!cdefn) {
                            /* Skip the ESC, back up
                               to the char following the
                               ESC. */
                            --p;
#ifdef DEBUG
                            if (gs_debug_c('i')) {
                                debug_dump_char(pcs->memory, '\033');
                            }
#endif
                            continue;
                        }
#ifdef DEBUG
                        if (gs_debug_c('i')) {
                            dmprintf(pcs->memory, "ESC ");
                            debug_dump_char(pcs->memory, chr);
                            dmprintf(pcs->memory, "\n");
                            dmprintf1(pcs->memory, "   [%s]\n", cdefn->cname);
                        }
#endif
                    } else {
                        if (p >= rlimit) {
                            p -= 2;
                            pst->min_bytes_needed = 3;
                            goto x;
                        }
                        pst->param_class = chr;
                        chr = *++p;
                        if (chr < min_escape_group || chr > max_escape_group) { /* Class but no group */
                            --p;
                            chr = 0;
                        }
                        pst->param_group = chr;
                        pst->scan_type = scanning_parameter;
                        pst->garbage_in_parameter = false;
                        param_init();
                        continue;
                    }
                }
                break;
        }
        if (cdefn == NULL) {
            param_init();
            continue;
        }
        if (!in_macro || (cdefn->actions & pca_in_macro)) {
            /* This might be the end-of-macro command. */
            /* Make sure all the data through this point */
            /* has been captured in the macro definition. */
            if (in_macro) {
                code = append_macro(macro_p, p, pcs);
                macro_p = p;
                if (code < 0)
                    goto x;
            }
            pst->args.command = chr;
            if (!pcs->raster_state.graphics_mode ||
                (cdefn->actions & pca_raster_graphics) ||
                (code = pcl_end_graphics_mode_implicit(pcs,
                                                       cdefn->
                                                       actions &
                                                       pca_dont_lockout_in_rtl))
                >= 0) {
                if ((pcs->personality != rtl) || ((pcs->personality == rtl)
                                                  && (cdefn->
                                                      actions & pca_in_rtl)))
                    code = (*cdefn->proc) (&pst->args, pcs);
            }
            /*
             * If we allocated a buffer for command data,
             * and the command didn't take possession of it,
             * free it now.
             */
            if (pst->args.data_on_heap && pst->args.data) {
                gs_free_object(pcs->memory, pst->args.data, "command data");
                pst->args.data = 0;
            }
            if (code == e_Unimplemented) {
#if e_Unimplemented != 0
                if_debug0m('i', pcs->memory, "Unimplemented\n");
#endif
            } else if (code < 0)
                break;
            if (pcs->display_functions) {
                /* This calls for a special parsing state. */
                pst->scan_type = scanning_display;
            }
            if (pcs->defining_macro && !in_macro) {
                /* We just started a macro definition. */
                if (pst->scan_type != scanning_none) {
                    /* macro was defined with an incomple combined command */
                    /* start definition of macro with esc&f preloaded */
                    static const byte macro_prefix[4] = " \033&f";

                    code = append_macro(&macro_prefix[0], &macro_prefix[3], pcs);
                    if (code < 0)
                        goto x;
                }
                macro_p = p;
            }
            in_macro = pcs->defining_macro;
        } else {
            /*
             * If we allocated a buffer for command data,
             * free it now.
             */
            if (pst->args.data_on_heap && pst->args.data) {
                gs_free_object(pcs->memory, pst->args.data, "command data");
                pst->args.data = 0;
            }
        }
        param_init();
    }
  x:pr->ptr = p;
    /* Append the last bit of data to the macro, if defining. */
    if (in_macro) {
        int mcode = append_macro(macro_p, p, pcs);

        if (mcode < 0 && code >= 0)
            code = mcode;
    }
    return code;
}

/* inialize the pcl command counter */
int
pcl_init_command_index(pcl_parser_state_t * pcl_parser_state,
                       pcl_state_t * pcs)
{
    pcl_command_definitions_t *definitions =
        (pcl_command_definitions_t *) gs_alloc_bytes(pcs->memory,
                                                     sizeof
                                                     (pcl_command_definitions_t),
                                                     "pcl_init_command_index");

    /* fatal */
    if (definitions == 0)
        return -1;
    /* we should set these individually but each field is properly
       initialized to zero */
    memset(definitions, 0, sizeof(pcl_command_definitions_t));
    /* plug command definitions into the parser state and a pointer
       for the command definitions into pcl's state for use by macros,
       I don't like this but the alternative is getting the parser
       state to the code that executer macros which is inconvenient at
       this time */
    pcs->pcl_commands = pcl_parser_state->definitions = definitions;
    return 0;
}

/* for now deallocates the memory associated with the command definitions */
int
pcl_parser_shutdown(pcl_parser_state_t * pcl_parser_state, gs_memory_t * mem)
{
    gs_free_object(mem, pcl_parser_state->definitions, "pcl_parser_shutdown");
    return 0;
}
/* ---------------- Initialization ---------------- */

int
pcparse_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & (pcl_reset_initial | pcl_reset_printer))
        pcs->parse_other = 0;

    return 0;
}

const pcl_init_t pcparse_init = {
    0, pcparse_do_reset
};
