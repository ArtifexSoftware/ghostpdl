/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgmand.h */
/* Definitions for HP-GL/2 commands and parser */

#ifndef pgmand_INCLUDED
#  define pgmand_INCLUDED
#include <setjmp.h>
#include "stdio_.h"		/* for gdebug.h */
#include "gdebug.h"
#include "pcommand.h"
#include "pcstate.h"
/* Define the type for the HP-GL state. */
typedef struct pcl_state_s hpgl_state_t;

/* Define the type for HP-GL/2 command arguments. */
typedef struct hpgl_args_s hpgl_args_t;

/* Define a command processing procedure. */
#define hpgl_command_proc(proc)\
  int proc(P2(hpgl_args_t *, hpgl_state_t *))
typedef hpgl_command_proc((*hpgl_command_proc_t));

/*
 * Because HP-GL/2 commands have irregular syntax, we have to let them parse
 * their arguments themselves.  But parsing can be interrupted at any point
 * by reaching an input buffer boundary, so we can't just let the commands
 * call procedures that get the next argument of a given type out of the
 * input stream.  At the same time, we would like to hide buffer boundaries
 * from the individual command implementations to the greatest extent
 * possible.
 *
 * For simple commands that take a maximum number of numeric arguments, we
 * do indeed let the commands call get-argument procedures.  In addition to
 * parsing the input, these procedures save the scanned arguments in an
 * array.  If one of these procedures runs out of input data, it does a
 * longjmp back to the main parser, which exits to its client to request
 * more input.  When the parser regains control, it simply restarts the
 * command procedure from the beginning.  This time, the get-argument
 * procedures return the saved values from the array before continuing to
 * scan the (new) input.  In this way, buffer boundaries cause some extra
 * work, but the command implementations don't notice them.  The only
 * requirement is that a command must not change the permanent state until
 * it has read all of its arguments.
 *
 * For commands that take other kinds of arguments, or a variable
 * (potentially large) number of numeric arguments, the situation is more
 * complicated.  The command is still expected to detect running out of
 * input data, and is still restarted, but it must keep track of its
 * progress itself.  We provide a variable called 'phase' for this
 * purpose: it is set to 0 just after the parser recognizes a command,
 * and is free for the command to use thereafter.
 */
typedef struct hpgl_command_s {
  hpgl_command_proc_t proc;
  byte flags;
#define hpgl_cdf_polygon 1	/* execute command even in polygon mode */
} hpgl_command_definition_t;

/* Define a HP-GL/2 command argument value. */
typedef struct hpgl_value_s {
  union v_ {
    int32 i;
    hpgl_real_t r;
  } v;
  bool is_real;
} hpgl_value_t;

/* Define the structure passed to HP-GL/2 commands. */
struct hpgl_args_s {
	/* Parsing state */
  stream_cursor_read source;
  int first_letter;		/* -1 if we haven't seen the first letter of */
				/* a command, the letter (0-25) if we have */
  bool done;			/* true if we've seen an argument */
				/* terminator */
  const hpgl_command_definition_t *command; /* command being executed, */
				/* 0 if none */
  jmp_buf exit_to_parser;	/* longjmp here if we ran out of data */
				/* while scanning an argument, or we */
				/* found a syntax error */
  struct arg_ {			/* argument scanning state */
	/* State within the current argument */
    int have_value;		/* 0 = no value, 1 = int, 2 = real */
    double frac_scale;		/* 10 ^ # of digits after dot */
    int sign;			/* 0 = none, +/-1 = sign */
	/* State of argument list collection */
    int count;			/* # of fully scanned arguments */
    int next;			/* # of next scanned arg to return */
    hpgl_value_t scanned[21];	/* args already scanned */
  } arg;
	/* Command execution state */
  int phase;			/* phase within command, see above */
};
/* Note that this is synonymous with the HP-GL/2 parser state. */
typedef hpgl_args_t hpgl_parser_state_t;

/* Register HP-GL/2 commands. */
typedef struct {
  char char1, char2;
  hpgl_command_definition_t defn;
} hpgl_named_command_t;
void hpgl_define_commands(P1(const hpgl_named_command_t *));
#define DEFINE_HPGL_COMMANDS \
{ static const hpgl_named_command_t defs_[] = {
#define HPGL_COMMAND(c1, c2, proc)\
  {c1, c2, {proc, 0}}
#define HPGL_POLY_COMMAND(c1, c2, proc)\
  {c1, c2, {proc, hpgl_cdf_polygon}}
#define END_HPGL_COMMANDS\
    {0, 0}\
  };\
  hpgl_define_commands(defs_);\
}

/* Define a return code asking for more data. */
#define e_NeedData (-200)

/* Initialize the HP-GL/2 parser. */
void hpgl_process_init(P1(hpgl_parser_state_t *));

/* Process a buffer of HP-GL/2 commands. */
/* Return 0 if more input needed, 1 if ESC seen, or an error code. */
int hpgl_process(P3(hpgl_parser_state_t *pst, hpgl_state_t *pgls,
		    stream_cursor_read *pr));

/* Prepare to scan the next (numeric) argument. */
#define hpgl_arg_init(pargs)\
  ((pargs)->arg.have_value = 0, (pargs)->arg.sign = 0)

/* Prepare to scan a sequence of numeric arguments.  Execute this */
/* once per sequence, when incrementing the phase. */
#define hpgl_args_init(pargs)\
  (hpgl_arg_init(pargs), (pargs)->arg.count = 0)
/* Move to the next phase of a command. */
#define hpgl_next_phase(pargs)\
  ((pargs)->phase++, hpgl_args_init(pargs))

/*
 * Scan a numeric argument, returning true if there was one, or false
 * if we have reached the end of the arguments.  Note that these
 * procedures longjmp back to the parser if they run out of input data.
 */
bool hpgl_arg_real(P2(hpgl_args_t *pargs, hpgl_real_t *pr));
bool hpgl_arg_c_real(P2(hpgl_args_t *pargs, hpgl_real_t *pr));
bool hpgl_arg_int(P2(hpgl_args_t *pargs, int32 *pi));
bool hpgl_arg_c_int(P2(hpgl_args_t *pargs, int *pi));
/*
 * Many vector commands are defined as taking parameters whose format is
 * "current units".  This is nonsensical: whether scaling is on or off
 * has no bearing on whether coordinates are represented as integers or
 * reals.  Nevertheless, in deference to this definition (and just in case
 * it turns out it actually matters), we define a separate procedure for
 * this.
 */
bool hpgl_arg_units(P2(hpgl_args_t *pargs, hpgl_real_t *pu));

/* In some cases, it is convenient for the implementation of command A to
 * call another command B.  While we don't particularly like this approach
 * in general, we do support it, as long as command B doesn't do its own
 * argument parsing (e.g., PE, LB).  The framework for doing this is the
 * following:
 *	hpgl_args_t args;
 *	...
 *	hpgl_args_setup(&args);
 *	<< As many times as desired: >>
 *		hpgl_args_add_int/real(&args, value);
 *	hpgl_B(&args, pgls);
 */
#define hpgl_args_setup(pargs)\
  ((void)((pargs)->done = true, (pargs)->arg.count = (pargs)->arg.next =\
	  (pargs)->phase = 0))
#define hpgl_args_add_int(pargs, ival)\
  ((void)((pargs)->arg.scanned[(pargs)->arg.count].v.i = (ival),\
	  (pargs)->arg.scanned[(pargs)->arg.count++].is_real = false))
#define hpgl_args_add_real(pargs, rval)\
  ((void)((pargs)->arg.scanned[(pargs)->arg.count].v.r = (rval),\
	  (pargs)->arg.scanned[(pargs)->arg.count++].is_real = true))
/*
 * We provide shortcuts for commands that take just 1 argument.
 */
#define hpgl_args_set_int(pargs, ival)\
  (hpgl_args_setup(pargs), hpgl_args_add_int(pargs, ival))
#define hpgl_args_set_real(pargs, rval)\
  (hpgl_args_setup(pargs), hpgl_args_add_real(pargs, rval))

/* a macro that calls a function and returns an error code if the code
   returned is less than 0.  Most of the hpgl and gs functions return
   if the calling function is less than 0 so this avoids cluttering up
   the code with the if statement and debug code.  HAS The current
   function name is gcc specific. */

#ifdef DEBUG
extern void hpgl_error(void);

#define hpgl_call(call)				\
{ 						\
  int code; 					\
  if ((code = (call)) != 0)			\
    {						\
      dprintf4("hpgl call failed\n\tcalled from: %s\n\tfile: %s\n\tline: %d\n\terror code: %d\n", \
	       __FUNCTION__, __FILE__, __LINE__, code);	\
      hpgl_error();                             \
      return(code);				\
    }						\
}
#else
#define hpgl_call(call) 			\
{						\
  int code;					\
  if ((code = (call)) < 0)			\
    {						\
      return(code);				\
    }						\
}
#endif

/*
 * HPGL mnemonics
 */

/* commands from pgchar.c -- HP-GL/2 character commands */
int hpgl_AD(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_CF(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_CP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_DI(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_DR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_DT(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_DV(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_ES(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_FI(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_FN(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_LB(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_LO(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SB(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SD(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SI(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SL(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SS(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_TD(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pgcolor.c - HP-GL/2 color vector graphics commands */
int hpgl_MC(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PC(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_NP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_CR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PP(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pgconfig.c - HP-GL/2 configuration and status commands */
int hpgl_CO(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_DF(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_IN(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_IP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_IR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_IW(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PG(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RO(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SC(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pglfill.c - HP-GL/2 line and fill attributes commands */
int hpgl_AC(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_FT(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_LA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_LT(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PW(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RF(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SM(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_SV(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_TR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_UL(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_WU(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pgpoly.c -- HP-GL/2 polygon commands */
int hpgl_EA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_EP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_ER(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_EW(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_FP(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PM(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_WG(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pgvector.c - HP-GL/2 vector commands */
int hpgl_AA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_AR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_AT(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_BR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_BZ(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_CI(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PA(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PD(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PE(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PR(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_PU(hpgl_args_t *pargs, hpgl_state_t *pgls);
int hpgl_RT(hpgl_args_t *pargs, hpgl_state_t *pgls);

/* commands from pgframe.c -- PCL5/HP-GL/2 picture frame commands */
int pcl_horiz_pic_frame_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls);
int pcl_vert_pic_frame_size_decipoints(pcl_args_t *pargs, pcl_state_t *pcls);
int pcl_set_pic_frame_anchor_point(pcl_args_t *pargs, pcl_state_t *pcls);
int pcl_hpgl_plot_horiz_size(pcl_args_t *pargs, pcl_state_t *pcls);
int pcl_hpgl_plot_vert_size(pcl_args_t *pargs, pcl_state_t *pcls);

#endif                                         /* pgmand_INCLUDED */
