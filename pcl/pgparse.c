/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pgparse.c */
/* HP-GL/2 parser */
#include "math_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gstypes.h"
#include "scommon.h"
#include "pgmand.h"

/*
 * We register all the HP-GL/2 commands dynamically, for maximum
 * configuration flexibility.  hpgl_command_list points to the individual
 * command definitions; as each command is registered, we enter it in the
 * list, and then store its index in the actual dispatch table
 * (hpgl_command_indices).
 */
private const hpgl_command_definition_t *hpgl_command_list[100];
private int hpgl_command_next_index;
/* Dispatch tables */
private byte hpgl_command_indices[26][26];

/* ---------------- Command definition ---------------- */

/* Register a command.  Return true if this is a redefinition. */
private bool
hpgl_register_command(byte *pindex, const hpgl_command_definition_t *pcmd)
{	int index = hpgl_command_next_index;
	byte prev = *pindex;

	if ( prev != 0 && prev <= index && hpgl_command_list[prev] == pcmd )
	  index = prev;
	else if ( index != 0 && hpgl_command_list[index] == pcmd )
	  ;
	else
	  hpgl_command_list[hpgl_command_next_index = ++index] = pcmd;
	*pindex = index;
	return (prev != 0 && prev != index);
}

/* Define a list of commands. */
void
hpgl_define_commands(const hpgl_named_command_t *pcmds)
{	const hpgl_named_command_t *pcmd = pcmds;

	for ( ; pcmd->char1; ++pcmd )
#ifdef DEBUG
	  if (
#endif
	  hpgl_register_command(&hpgl_command_indices
				[pcmd->char1 - 'A'][pcmd->char2 - 'A'],
				&pcmd->defn)
#ifdef DEBUG
	  )
	    dprintf2("Redefining command %c%c\n", pcmd->char1, pcmd->char2);
#endif
	  ;
}

/* ---------------- Parsing ---------------- */

/* Initialize the HP-GL/2 parser state. */
void
hpgl_process_init(hpgl_parser_state_t *pst)
{	pst->first_letter = -1;
	pst->command = 0;
}

/* Process a buffer of HP-GL/2 commands. */
/* Return 0 if more input needed, 1 if ESC seen, or an error code. */
int
hpgl_process(hpgl_parser_state_t *pst, hpgl_state_t *pgls,
  stream_cursor_read *pr)
{	const byte *p = pr->ptr;
	const byte *rlimit = pr->limit;
	int code = 0;

	pst->source.limit = rlimit;
	/* Prepare to catch a longjmp indicating the argument scanner */
	/* needs more data, or encountered an error. */
	code = setjmp(pst->exit_to_parser);
	if ( code )
	  { /* The command detected an error, or we need to ask */
	    /* the caller for more data.  pst->command != 0. */
	    pr->ptr = pst->source.ptr;
	    if ( code < 0 && code != e_NeedData )
	      { pst->command = 0; /* cancel command */
	        if_debug0('i', "\n");
	        return code;
	      }
	    return 0;
	  }
	/* Check whether we're still feeding data to a command. */
call:	if ( pst->command )
	  { pst->source.ptr = p;
	    pst->arg.next = 0;
	    code = (*pst->command->proc)(pst, pgls);
	    p = pst->source.ptr;
	    if ( code < 0 )
	      goto x;
	    pst->command = 0;
	    if_debug0('i', "\n");
	  }
	while ( p < rlimit )
	  {	byte next = *++p;

		if ( next >= 'A' && next <= 'Z' )
		  next -= 'A';
		else if ( next >= 'a' && next <= 'z' )
		  next -= 'a';
		else if ( next == ESC )
		  { --p;
		    pst->first_letter = -1;
		    code = 1;
		    break;
		  }
		else		/* ignore everything else */
		  { /* Apparently this is what H-P plotters do.... */
		    if ( next > ' ' && next != ',' )
		      pst->first_letter = -1;
		    continue;
		  }
		if ( pst->first_letter < 0 )
		  { pst->first_letter = next;
		    continue;
		  }
		{ int index = hpgl_command_indices[pst->first_letter][next];

#ifdef DEBUG
		  if ( gs_debug_c('i') )
		    { char c = (index ? '-' : '?');
		      dprintf4("--%c%c%c%c", pst->first_letter + 'A',
			       next + 'A', c, c);
		    }
#endif
		  if ( index == 0 )	/* anomalous, drop 1st letter */
		    { pst->first_letter = next;
		      continue;
		    }
		  pst->first_letter = -1;
		  pst->command = hpgl_command_list[index];
		  pst->phase = 0;
		  pst->done = false;
		  hpgl_args_init(pst);
		  /*
		   * Only a few commands should be executed while we're in
		   * polygon mode: check for this here.  Note that we rely
		   * on the garbage-skipping property of the parser to skip
		   * over any following arguments.  This doesn't work for
		   * the few commands with special syntax that should be
		   * ignored in polygon mode (CO, DT, LB, SM); they must be
		   * flagged as executable even in polygon mode, and check
		   * the render_mode themselves.
		   */
		  if (( pgls->g.polygon_mode ) &&
		       !(pst->command->flags & hpgl_cdf_polygon)
		      )
		    pst->command = 0;
		  else
		    { /* similarly if we are in lost mode we do not
			 execute the commands that are only defined to
			 be used when lost mode is cleared. */
		      if (( pgls->g.lost_mode == hpgl_lost_mode_entered ) &&
			  (pst->command->flags & hpgl_cdf_lost_mode_cleared)
			  )
			pst->command = 0;
		    }
		  goto call;
		}
	  }
x:	pr->ptr = p;
	return (code == e_NeedData ? 0 : code);
}

/*
 * Get a numeric HP-GL/2 argument from the input stream.  Return 0 if no
 * argument, a pointer to the value if an argument is present, or longjmp if
 * need more data.  Note that no errors are possible.
 */
private const hpgl_value_t *
hpgl_arg(hpgl_parser_state_t *pst)
{	const byte *p = pst->source.ptr;
	const byte *rlimit = pst->source.limit;
	hpgl_value_t *pvalue;

#define parg (&pst->arg)
	if ( parg->next < parg->count )
	  { /* We're still replaying already-scanned arguments. */
	    return &parg->scanned[parg->next++];
	  }
	if ( pst->done )
	  return 0;
	pvalue = &parg->scanned[parg->count];
#define check_value()\
  if ( parg->have_value ) goto done

	for ( ; p < rlimit; ++p )
	  { byte ch = p[1];
	    switch ( ch )
	      {
	      case '+':
		check_value();
		parg->have_value = 1;
		parg->sign = 1;
		pvalue->v.i = 0;
		break;
	      case '-':
		check_value();
		parg->have_value = 1;
		parg->sign = -1;
		pvalue->v.i = 0;
		break;
	      case '.':
		switch ( parg->have_value )
		  {
		  default:	/* > 1 */
		    goto out;
		  case 0:
		    pvalue->v.r = 0;
		    break;
		  case 1:
		    pvalue->v.r = pvalue->v.i;
		  }
		parg->have_value = 2;
		parg->frac_scale = 1.0;
		break;
	      case HT: case LF: case FF: case CR: case ';':
		++p;
		pst->done = true;
		check_value();
		goto out;
	      case SP: case ',':
		/*
		 * The specification doesn't say what to do with extra
		 * separators; we just ignore them.
		 */
		if ( !parg->have_value )
		  break;
		++p;
done:		if ( parg->sign < 0 )
		  { if ( parg->have_value > 1 )
		      pvalue->v.r = -pvalue->v.r;
		    else
		      pvalue->v.i = -pvalue->v.i;
		  }
		goto out;
	      case '0': case '1': case '2': case '3': case '4':
	      case '5': case '6': case '7': case '8': case '9':
		ch -= '0';
#define max_i 0x3fffffff
		switch ( parg->have_value )
		  {
		  default:	/* case 2 */
		    pvalue->v.r += ch / (parg->frac_scale *= 10);
		    break;
		  case 0:
		    parg->have_value = 1;
		    pvalue->v.i = ch;
		    break;
		  case 1:
		    if ( pvalue->v.i >= max_i/10 &&
			 (pvalue->v.i > max_i/10 || ch > max_i%10)
		       )
		      pvalue->v.i = max_i;
		    else
		      pvalue->v.i = pvalue->v.i * 10 + ch;
		  }
		break;
	      default:
		pst->done = true;
		check_value();
		goto out;
	      }
	  }
	/* We ran out of data before reaching a terminator. */
	pst->source.ptr = p;
	longjmp(pst->exit_to_parser, e_NeedData);
	/* NOTREACHED */
out:	pst->source.ptr = p;
	switch ( parg->have_value )
	  {
	  case 0:		/* no argument */
	    return false;
	  case 1:		/* integer */
	    if_debug1('I', "  %ld", (long)pvalue->v.i);
	    pvalue->is_real = false;
	    break;
	  default /* case 2 */:	/* real */
	    if_debug1('I', "  %g", pvalue->v.r);
	    pvalue->is_real = true;
	  }
	hpgl_arg_init(pst);
	parg->next = ++(parg->count);
	return pvalue;
#undef parg
}

/* Get a real argument. */
bool
hpgl_arg_real(hpgl_args_t *pargs, hpgl_real_t *pr)
{	const hpgl_value_t *pvalue = hpgl_arg(pargs);

	if ( !pvalue )
	  return false;
	*pr = (pvalue->is_real ? pvalue->v.r : pvalue->v.i);
	return true;
}

/* Get a clamped real argument. */
bool
hpgl_arg_c_real(hpgl_args_t *pargs, hpgl_real_t *pr)
{	const hpgl_value_t *pvalue = hpgl_arg(pargs);
	hpgl_real_t r;

	if ( !pvalue )
	  return false;
	r = (pvalue->is_real ? pvalue->v.r : pvalue->v.i);
	*pr = (r < -32768 ? -32768 : r > 32767 ? 32767 : r);
	return true;

}

/* Get an integer argument. */
bool
hpgl_arg_int(hpgl_args_t *pargs, int32 *pi)
{	const hpgl_value_t *pvalue = hpgl_arg(pargs);

	if ( !pvalue )
	  return false;
	*pi = (pvalue->is_real ? (int32)pvalue->v.r : pvalue->v.i);
	return true;
}

/* Get a clamped integer argument. */
bool
hpgl_arg_c_int(hpgl_args_t *pargs, int *pi)
{	const hpgl_value_t *pvalue = hpgl_arg(pargs);
	int32 i;

	if ( !pvalue )
	  return false;
	i = (pvalue->is_real ? (int32)pvalue->v.r : pvalue->v.i);
	*pi = (i < -32768 ? -32768 : i > 32767 ? 32767 : i);
	return true;
}

/* Get a "current units" argument. */
bool
hpgl_arg_units(hpgl_args_t *pargs, hpgl_real_t *pu)
{	/****** PROBABLY WRONG ******/
	return hpgl_arg_real(pargs, pu);
}

/* initialize the HPGL command counter (for uninitialized BSS) */
void
hpgl_init_command_index(void)
{
    hpgl_command_next_index = 0;
}
