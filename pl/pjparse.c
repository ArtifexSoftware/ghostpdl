/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  
*/

/* pjparse.c */
/* PJL parser */
#include "memory_.h"
#include "scommon.h"
#include "gdebug.h"
#include "gp.h"
#include "pjparse.h"
#include <ctype.h> 		/* for toupper() */
#include <stdlib.h>             /* for atoi() */

/* ------ pjl state definitions ------ */

#define PJL_STRING_LENGTH (15)
#define PJL_PATH_NAME_LENGTH (150)

/* definitions for fontsource and font number table entries */
typedef struct pjl_fontsource {
    char designator[2];  
    char pathname[PJL_PATH_NAME_LENGTH+1];
    char fontnumber[PJL_STRING_LENGTH+1];
} pjl_fontsource_t;

/* definitions for variable names and values */
typedef struct pjl_envir_var_s {
    char var[PJL_STRING_LENGTH+1];
    char value[PJL_STRING_LENGTH+1];
} pjl_envir_var_t;

/* the pjl current environment and the default user environment.  Note
   the default environment should be stored in NVRAM on embedded print
   systems. */
typedef struct pjl_parser_state_s {
    char line[81];		/* buffered command line */
    int pos;			/* current position in line */
    pjl_envir_var_t *defaults;  /* the default environment (i.e. set default) */
    pjl_envir_var_t *envir;     /* the pjl environment */
    /* these are seperated out from the default and environmnet for no good reason */
    pjl_fontsource_t *font_defaults;
    pjl_fontsource_t *font_envir;
    gs_memory_t *mem;
} pjl_parser_state_t;

/* provide factory defaults for pjl commands.  Note these are not pjl
   defaults but initial values set in the printer when shipped.  In an
   embedded system these would be defined in ROM */
private const pjl_envir_var_t pjl_factory_defaults[] = {
    {"formlines", "60"},
    {"widea4", "no"},
    {"fontsource", "I"},
    {"fontnumber", "0"},
    {"pitch", "10.00"},
    {"ptsize", "12.00"},
    /* NB this is for the 6mp roman8 is used on most other HP devices
       other devices */
    {"symset", "pc8"},
    {"copies", "1"},
    {"paper", "letter"},
    {"orientation", "portrait"},
    {"duplex", "off"},
    {"binding", "longedge"},
    {"manualfeed", "off"},
    {"fontsource", "I"},
    {"fontnumber", "0"},
    {"personality", "pcl5c"},
    {"language", "auto"},
    /*    {"personality", "rtl"}, */
    {"", ""}
};

/* FONTS I (Internal Fonts) C, C1, C2 (Cartridge Fonts) S (Permanent
   Soft Fonts) M1, M2, M3, M4 (fonts stored in one of the printer's
   ROM SIMM slots).  Simulate cartridge, permanent soft fonts, and
   printer ROM SIMMS with sub directories.  See table below.  Also
   resources can be set up as lists of resources which is useful for
   host based systems.  Entries are seperated with a colon.  Note
   there is some unnecessary overlap in the factory default and font
   source table. */
private const pjl_fontsource_t pjl_fontsource_table[] = {
    { "I", "fonts/:/windows/system/:/windows/fonts/:/win95/fonts/:/winnt/fonts/" },
    { "C", "CART0/", "" },
    { "C1", "CART1/", "" },
    { "C2", "CART2/", "" },
    { "S", "MEM0/", ""  },
    { "M1", "MEM1/", ""  },
    { "M2", "MEM2/", ""  },
    { "M3", "MEM3/", ""  },
    { "M4", "MEM4/", ""  },
    { "", "", "" }
};

/* pjl tokens parsed */
typedef enum {
    DONE,
    SET,
    DEFAULT,
    EQUAL,
    VARIABLE,
    SETTING,
    UNIDENTIFIED,  /* NB not used */
    LPARM,   /* NB not used */
    PREFIX, /* @PJL */
    INITIALIZE, 
    RESET,
    INQUIRE,
    DINQUIRE,
    ENTER,
    LANGUAGE
} pjl_token_type_t;

/* lookup table to map strings to pjl tokens */
typedef struct pjl_lookup_table_s {
    char pjl_string[PJL_STRING_LENGTH+1];
    pjl_token_type_t pjl_token;
} pjl_lookup_table_t;

private const pjl_lookup_table_t pjl_table[] = {
    { "@PJL", PREFIX },
    { "SET", SET },
    { "DEFAULT", DEFAULT },
    { "INITIALIZE", INITIALIZE },
    { "=", EQUAL },
    { "DINQUIRE", DINQUIRE },
    { "INQUIRE", INQUIRE },
    { "ENTER", ENTER },
    { "", (pjl_token_type_t)0 /* don't care */ }
};

/* permenant soft font slots - bit n is the n'th font number. */
#define MAX_PERMANENT_FONTS 256  /* multiple of 8 */
unsigned char pjl_permanent_soft_fonts[MAX_PERMANENT_FONTS / 8];

/* ----- private functions and definitions ------------ */

/* forward declaration */
private int pjl_set(P4(pjl_parser_state_t *pst, char *variable, char *value, bool defaults));

/* handle pjl variables which affect the state of other variables - we
   don't handle all of these yet.  NB not complete. */

 void
pjl_side_effects(pjl_parser_state_t *pst, char *variable, char *value, bool defaults)
{
    /* default formlines to 45 if the orientation is set to landscape.
       We assume the side effect will not affect itself so we can call
       the caller. */
    if ( !pjl_compare(variable, "ORIENTATION") &&
	 !pjl_compare(value, "LANDSCAPE") )
	pjl_set(pst, "FORMLINES", "45", defaults);
    /* fill in other side effects here */
    return;
}

/* set a pjl environment or default variable. */
 private int
pjl_set(pjl_parser_state_t *pst, char *variable, char *value, bool defaults)
{
    pjl_envir_var_t *table = (defaults ? pst->defaults : pst->envir);
    int i;

    for (i = 0; table[i].var[0]; i++)
	if (!pjl_compare(table[i].var, variable)) {
	    /* set the value */
	    strcpy(table[i].value, value);
	    /* set any side effects of setting the value */
	    pjl_side_effects(pst, variable, value, defaults);
	    return 1;
	}
    /* didn't find variable */
    return 0;
}
    
/* get next token from the command line buffer */
 private pjl_token_type_t
pjl_get_token(pjl_parser_state_t *pst, char token[])
{
    int c;
    int start_pos;

    /* skip any whitespace if we need to.  */
    while((c = pst->line[pst->pos]) == ' ' || c == '\t') pst->pos++;

    /* special case to allow = with no intevervening spaces between
       lhs and rhs */
    if ( c == '=' ) {
	pst->pos++;
	return EQUAL;
    }

    /* set the starting position */
    start_pos = pst->pos;

    /* end of line reached; null shouldn't happen but we check anyway */
    if ( c == '\0' || c == '\n' )
	return DONE;

    /* set the ptr to the next delimeter. */
    while((c = pst->line[pst->pos]) != ' ' &&
	  c != '\t' &&
	  c != '\r' &&
	  c != '\n' &&
	  c != '=' &&
	  c != '\0')
	pst->pos++;

    /* build the token */
    {
	int slength = pst->pos - start_pos;
	int i;

	/* we allow = to special case for allowing 
	/* token doesn't fit or is empty */
	if (( slength > PJL_STRING_LENGTH) || slength == 0)
	    return DONE;
	/* now the string can be safely copied */
	strncpy(token, &pst->line[start_pos], slength);
	token[slength] = '\0';

	/* for known tokens */
	for (i = 0; pjl_table[i].pjl_string[0]; i++)
	    if (!pjl_compare(pjl_table[i].pjl_string, token))
	       return pjl_table[i].pjl_token;

	/* NB add other cases here */
	/* check for variables that we support */
	for (i = 0; pst->envir[i].var[0]; i++)
	    if (!pjl_compare(pst->envir[i].var, token))
	       return VARIABLE;
	
	/* NB assume this is a setting yuck */
	return SETTING;
    }
    /* shouldn't happen */
    return DONE;
}

/* check if fonts exist in the current font source path */
 private char *
pjl_check_font_path(char *path_list, gs_memory_t *mem)
{
    /* lookup a font path and check if any files (presumably fonts are
       present) */
    char tmp_path[PJL_PATH_NAME_LENGTH+1];
    char *tmp_pathp = tmp_path;
    const char pattern[] = "*";
    char tmp_path_and_pattern[PJL_PATH_NAME_LENGTH+1+1]; /* pattern + null */
    char *dirname;
#define MAXPATHLEN 1024
	    char fontfilename[MAXPATHLEN+1];
#undef MAXPATHLEN

    /* make a tmp copy of the colon delimited path */
    strcpy(tmp_path, path_list);
    /* for each path search for fonts.  If we find them return we only
       check if the directory resource has files without checking if
       the files are indeed fonts. */
    while ( (dirname = strtok(tmp_pathp, ":")) != NULL ) {
	file_enum *fe;
	strcpy(tmp_path_and_pattern, dirname);
	strcat(tmp_path_and_pattern, pattern);
	fe = gp_enumerate_files_init(tmp_path_and_pattern, strlen(tmp_path_and_pattern), mem);
	if ( (gp_enumerate_files_next(fe, fontfilename, 0 /*??*/) ) == -1 ) {
	    tmp_pathp = NULL;
	} else {
	    /* wind through the rest of the files.  This should close
               things up as well.  All we need to do is clean up but
               gp_enumerate_files_close() does not close the current
               directory */
	    while ( 1 ) {
		int fstatus = (int)gp_enumerate_files_next(fe, fontfilename, 0);
		/* we don't care if the file does not fit (return +1) */
		if ( fstatus == -1 )
		    break;
	    }
	    /* NB fix me - replace : separated path with real path.
               We should do this elsewhere */
	    strcpy(path_list, dirname);
	    return path_list;
	}
    }
    return NULL;
}

/* initilize both pjl state and default environment to default font
   number if font resources are present.  Note we depend on the PDL
   (pcl) to provide information about 'S' permanent soft fonts.  For
   all other resources we check for filenames which should correspond
   to fonts. */
 private void
pjl_reset_fontsource_fontnumbers(pjl_parser_state_t* pst)
{
    char default_font_number[] = "0"; /* default number if resources are present */
    gs_memory_t *mem = pst->mem;

    int i;
    for (i = 0; pst->font_defaults[i].designator[0]; i++) {
	if ( pjl_check_font_path(pst->font_defaults[i].pathname, mem) )
	    strcpy(pst->font_defaults[i].fontnumber, default_font_number);
	if ( pjl_check_font_path(pst->font_envir[i].pathname, mem) )
	    strcpy(pst->font_envir[i].fontnumber, default_font_number);
    }
}

/* parse and set up state for one line of pjl commands */
 private int
pjl_parse_and_process_line(pjl_parser_state_t *pst)
{
    pjl_token_type_t tok;
    char token[PJL_STRING_LENGTH+1] = {0};

    /* reset the line position to the beginning of the line */
    pst->pos = 0;
    /* all pjl commands start with the pjl prefix @PJL */
    if ( (tok = pjl_get_token(pst, token)) != PREFIX )
	return -1;
    /* NB we should check for required and optional used of whitespace
       but we don't see PJLTRM 2-6 PJL Command Syntax and Format. */
    while( (tok = pjl_get_token(pst, token)) != DONE ) {
	switch( tok ) {
	case SET:
	case DEFAULT:
	    {
		bool defaults;
var:            defaults = (tok == DEFAULT); 
		/* NB we skip over lparm and search for the variable */
		while( (tok = pjl_get_token(pst, token)) != DONE )
		    if ( tok == VARIABLE ) {
			char variable[PJL_STRING_LENGTH+1];
			strcpy(variable, token);
			if (((tok = pjl_get_token(pst, token)) == EQUAL) &&
			    (tok = pjl_get_token(pst, token)) == SETTING) {
			    return pjl_set(pst, variable, token, defaults);
			} else
			    return -1; /* syntax error */
		    } else
			continue;
		return 0;
	    }
	case INITIALIZE:
	    /* set the user default environment to the factory default environment */
	    memcpy(pst->defaults, &pjl_factory_defaults, sizeof(pjl_factory_defaults));
	    memcpy(pst->font_defaults, &pjl_fontsource_table, sizeof(pjl_fontsource_table));
	    pjl_reset_fontsource_fontnumbers(pst);
	    return 0;
	    /* set the current environment to the user default environment */
	case RESET:
	    memcpy(pst->envir, pst->defaults, sizeof(pjl_factory_defaults));
	    memcpy(pst->font_envir, pst->font_defaults, sizeof(pjl_fontsource_table));
	    return 0;
	case ENTER:
	    /* there is no setting for the default language */
	    tok = SET;
	    goto var;
	default:
	    return -1;
	}
    }
    return (tok == DONE ? 0 : -1);
}
/* set the initial environment to the default environment, this should
   be done at the beginning of each job */
 void
pjl_set_init_from_defaults(pjl_parser_state_t *pst)
{
    memcpy(pst->envir, pst->defaults, sizeof(pjl_factory_defaults));
    memcpy(pst->font_envir, pst->font_defaults, sizeof(pjl_fontsource_table));
}

/* sets fontsource to the next priority resource containing fonts */
 void
pjl_set_next_fontsource(pjl_parser_state_t* pst)
{
    int current_source;
    pjl_envvar_t *current_font_source = pjl_get_envvar(pst, "fontsource");

    /* find the index of the current resource then work backwards
       until we find font resources.  We assume the internal source
       will have fonts */
    for (current_source = 0; pst->font_envir[current_source].designator[0]; current_source++ )
	if (!pjl_compare(pst->font_envir[current_source].designator, current_font_source))
	    break;

    /* next resource is not internal 'I' */
    if ( current_source != 0 ) {
	while( current_source > 0 ) {
	    /* valid font number found */
	    if ( pst->font_envir[current_source].fontnumber[0] )
		break;
	    current_source--;
	}
    }
    /* set both default and environment font source, the spec is not clear about this */
    pjl_set(pst, "fontsource", pst->font_envir[current_source].designator, true);
    pjl_set(pst, "fontsource", pst->font_defaults[current_source].designator, false);
}

/* get a pjl environment variable from the current environment - not
   the user default environment */
 pjl_envvar_t *
pjl_get_envvar(pjl_parser_state *pst, const char *pjl_var)
{
    int i;
    pjl_envir_var_t *env = pst->envir;
    /* lookup up the value */
    for (i = 0; env[i].var[0]; i++)
	if (!pjl_compare(env[i].var, pjl_var)) {
	    return env[i].value;
	}
    return NULL;
}

/* -- public functions - see pjparse.h for interface documentation -- */

/* Process a buffer of PJL commands. */
 int
pjl_process(pjl_parser_state* pst, void *pstate, stream_cursor_read * pr)
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
	    /* null terminate, parse and set the pjl state */
	    pst->line[pst->pos] = '\0';
	    pjl_parse_and_process_line(pst);
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

/* PJL symbol set environment variable -> pcl symbol set numbers.
   This probably should not be defined here :-( */
private const struct {
    const char *symname;
    const char *pcl_selectcode;
} symbol_sets[] = {
    { "ROMAN8", "8U"    },
    { "ISOL1", "0N"     },
    { "ISOL2", "2N"     },
    { "ISOL5", "5N"     },
    { "PC8", "10U"      },
    { "PC8DN", "11U"    },
    { "PC850", "12U"    },
    { "PC852", "17U"    },
    { "PC8TK", "9T"     }, /* pc-8 turkish ?? not sure */
    { "WINL1", "9U"     },
    { "WINL2", "9E"     },
    { "WINL5", "5T"     },
    { "DESKTOP", "7J"   },
    { "PSTEXT", "10J"   },
    { "VNINTL", "13J"   },
    { "VNUS", "14J"     },
    { "MSPUBL", "6J"    },
    { "MATH8", "8M"     },
    { "PSMATH", "5M"    },
    { "VNMATH", "6M"    },
    { "PIFONT", "15U"   },
    { "LEGAL", "1U"     },
    { "ISO4", "1E"      },
    { "ISO6", "0U"      },
    { "ISO11", "0S"     },
    { "ISO15", "0I"     },
    { "ISO17", "2S"     },
    { "ISO21", "1G"     },
    { "ISO60", "OD"     },
    { "ISO69", "1F"     },
    { "WIN30", NULL     }, /* don't know */
    { "WIN31J", NULL    }, /* don't know */
    { "GB2312", NULL    }, /* don't know */
    { NULL, NULL        }
};

/* map a pjl symbol table name to a pcl symbol table name */
 int
pjl_map_pjl_sym_to_pcl_sym(const char *symname)
{
    int i;
    for (i = 0; symbol_sets[i].symname; i++)
	if (!pjl_compare(symname, symbol_sets[i].symname)) {
	    /* convert the character code to it's integer
               representation.  NB peculiar code! */
	    char pcl_symbol[4], chr;
	    int char_pos;
	    strcpy(pcl_symbol, symbol_sets[i].pcl_selectcode);
	    char_pos = strlen(pcl_symbol) - 1;
	    chr = pcl_symbol[char_pos];
	    pcl_symbol[char_pos] = (char)NULL;
	    return (atoi(pcl_symbol) << 5) + chr - 64;
	}
    return -1;
}

/* environment variable to integer */
 int 
pjl_vartoi(const pjl_envvar_t *s)
{
    return atoi(s);
}

/* environment variable to float */
 floatp 
pjl_vartof(const pjl_envvar_t *s)
{
    return atof(s);
}

/* convert a pjl font source to a pathname */
 char *
pjl_fontsource_to_path(const pjl_parser_state *pjls, const pjl_envvar_t *fontsource)
{
    int i;
    for (i = 0; pjls->font_envir[i].designator[0]; i++)
	if (!pjl_compare(pjls->font_envir[i].designator, fontsource))
	    return pjl_check_font_path(pjls->font_envir[i].pathname, pjls->mem);
    return NULL;
}

/* Create and initialize a new state. */
 pjl_parser_state *
pjl_process_init(gs_memory_t *mem)
{
    pjl_parser_state_t *pjlstate =
	(pjl_parser_state *)gs_alloc_bytes(mem, 
	     sizeof(pjl_parser_state_t), "pjl_state" );
    pjl_envir_var_t *pjl_env =
	(pjl_envir_var_t *)gs_alloc_bytes(mem, 
             sizeof(pjl_factory_defaults), "pjl_envir" );
    pjl_envir_var_t *pjl_def =
    	(pjl_envir_var_t *)gs_alloc_bytes(mem, 
             sizeof(pjl_factory_defaults), "pjl_defaults" );
    pjl_fontsource_t *pjl_fontenv =
	(pjl_fontsource_t *)gs_alloc_bytes(mem,
             sizeof(pjl_fontsource_table), "pjl_font_envir" );
    pjl_fontsource_t *pjl_fontdef =
	(pjl_fontsource_t *)gs_alloc_bytes(mem,
             sizeof(pjl_fontsource_table), "pjl_font_defaults" );

    if ( pjlstate == NULL || pjl_env == NULL || pjl_def == NULL )
	return NULL; /* should be fatal so we don't bother piecemeal frees */

    pjlstate->defaults = pjl_def;
    pjlstate->envir = pjl_env;
    pjlstate->font_envir = pjl_fontenv;
    pjlstate->font_defaults = pjl_fontdef;
    /* initialize the default and initial pjl environment.  We assume
       that these are the same layout as the factory defaults. */
    memcpy(pjlstate->defaults, pjl_factory_defaults, sizeof(pjl_factory_defaults));
    memcpy(pjlstate->envir, pjlstate->defaults, sizeof(pjl_factory_defaults));
    /* initialize the font repository data as well */
    memcpy(pjlstate->font_defaults, pjl_fontsource_table, sizeof(pjl_fontsource_table));
    memcpy(pjlstate->font_envir, pjl_fontsource_table, sizeof(pjl_fontsource_table));
    /* initialize the current position in the line array */
    pjlstate->pos = 0;
    pjlstate->mem = mem;
    /* initialize available font sources */
    pjl_reset_fontsource_fontnumbers(pjlstate);
    {
	int i;
	for (i = 0; i < countof(pjl_permanent_soft_fonts); i++)
	    pjl_permanent_soft_fonts[i] = 0;
    }
     return (pjl_parser_state *)pjlstate;
}

/* case insensitive comparison of two null terminated strings. */
 int
pjl_compare(const pjl_envvar_t *s1, const char *s2)
{
    for (; toupper(*s1) == toupper(*s2); ++s1, ++s2)
	if (*s1 == '\0')
	    return(0);
    return 1;
}

/* free all memory associated with the PJL state */
 void
pjl_process_destroy(pjl_parser_state *pst, gs_memory_t *mem)
{
    gs_free_object(mem, pst->font_envir, "pjl_font_envir");
    gs_free_object(mem, pst->font_defaults, "pjl_font_defaults");
    gs_free_object(mem, pst->defaults, "pjl_defaults");
    gs_free_object(mem, pst->envir, "pjl_envir");
    gs_free_object(mem, pst, "pjl_state");
}

/* Convert font file name to fontnumber.  This table is hardwired here
   so that PJL can have a numbering for the fonts.  Presumably these
   font files would be burned into ROM in PJL font number order, PJL
   would enumerate the fonts and hand them to pcl.  The table entries
   and their order are device dependant and vary on different HP
   devices.  This is the setup on an LJ4 modulo a few new fonts that
   are not supported on that device. */
 int
pjl_get_pcl_internal_font_number(const char *filename)
{
    /* font file names - indices are the pjl font numbers */
    static const char *font_table[] = {
	"cour",                             /* 0 */
	"cgti",                   	    /* 1 */
	"cgtib",			    /* 2 */
	"cgtii",			    /* 3 */
	"cgtibi",			    /* 4 */
	"cgom",                   	    /* 5 */
	"cgomb",                  	    /* 6 */
	"cgomi",                  	    /* 7 */
	"cgombi",                 	    /* 8 */
	"coro",                   	    /* 9 */
	"clarbc",                 	    /* 10 */
	"univm",                  	    /* 11 */
	"univb",                  	    /* 12 */
	"univmi",                 	    /* 13 */
	"univbi",                 	    /* 14 */
	"univmc",                 	    /* 15 */
	"univcb",                 	    /* 16 */
	"univmci",                	    /* 17 */
	"univcbi",                	    /* 18 */
	"anto",                   	    /* 19 */
	"antob",                  	    /* 20 */
	"antoi",                  	    /* 21 */
	"garaa",                  	    /* 22 */
	"garrah",                 	    /* 23 */
	"garak",                  	    /* 24 */
	"garrkh",                 	    /* 25 */
	"mari",                   	    /* 26 */
	"albrmd",                 	    /* 27 */
	"albrxb",                 	    /* 28 */
	"albrmdi",                	    /* 29 */
	"arial",                  	    /* 30 */
	"arialbd",                	    /* 31 */
	"ariali",                 	    /* 32 */
	"arialbi",                	    /* 33 */
	"times",                  	    /* 34 */
	"timesbd",                	    /* 35 */
	"timesi",                 	    /* 36 */
	"timesbi",                	    /* 37 */
	"symbol",                 	    /* 38 */
	"wingding",               	    /* 39 */
	"courbd",                 	    /* 40 */
	"couri",                  	    /* 41 */
	"courbi",                 	    /* 42 */
	"letr",                   	    /* 43 */
	"letrb",                  	    /* 44 */
	"letri",                  	    /* 45 */
	"letrbi",                 	    /* 46 */
	""
    };
    int i;
    for ( i = 0; font_table[i]; i++ )
	if ( !strcmp(font_table[i], filename) )
	    return i;

    dprintf1("pjparse.c:pjl_get_pcl_internal_font_number() font not found %s\n", filename);
    return 0;
}

/* delete a permanent soft font */
 int
pjl_register_permanent_soft_font_deletion(pjl_parser_state *pst, int font_number)
{
    if ( (font_number > MAX_PERMANENT_FONTS - 1) || (font_number < 0) ) {
	dprintf("pjparse.c:pjl_register_permanent_soft_font_deletion() bad font number\n");
	return 0;
    }
    /* if the font is present. */
    if ( (pjl_permanent_soft_fonts[font_number >> 3]) & (128 >> (font_number & 7)) ) {
	/* set the bit to zero to indicate the fontnumber has been deleted */
	pjl_permanent_soft_fonts[font_number >> 3] &= ~(128 >> (font_number & 7));
	/* if the current font source is 'S' and the current font number
	   is the highest number, and *any* soft font was deleted or if
	   the last font has been removed, set the stage for changing to
	   the next priority font source.  BLAME HP not me. */
	{
	    bool is_S = !pjl_compare(pjl_get_envvar(pst, "fontsource"), "S");
	    bool empty = true;
	    int highest_fontnumber = -1;
	    int current_fontnumber = pjl_vartoi(pjl_get_envvar(pst, "fontnumber"));
	    int i;
	    /* check for no more fonts and the highest font number.
	       NB should look at longs not bits in the loop */
	    for ( i = 0; i < MAX_PERMANENT_FONTS; i++ )
		if ( (pjl_permanent_soft_fonts[i >> 3]) & (128 >> (i & 7)) ) {
		    empty = false;
		    highest_fontnumber = i;
		}
	    if ( is_S && ((highest_fontnumber == current_fontnumber) || empty) ) {
#define SINDEX 4
		pst->font_defaults[SINDEX].fontnumber[0] = (char)NULL;
		pst->font_envir[SINDEX].fontnumber[0] = (char)NULL;
		return 1;
#undef SINDEX
	    }
	}
    }
    return 0;
}

/* request that pjl add a soft font and return a pjl font number for
   the font. */
 int
pjl_register_permanent_soft_font_addition(pjl_parser_state *pst)
{
    /* Find an empty slot in the table.  We have no HP documentation
       that says how a soft font gets associated with a font number */
    int font_num;
    bool slot_found;
    for ( font_num = 0; font_num < MAX_PERMANENT_FONTS; font_num++ )
	if ( !((pjl_permanent_soft_fonts[font_num >> 3]) & (128 >> (font_num & 7))) ) {
	    slot_found = true;
	    break;
	}
    /* yikes, shouldn't happen */
    if ( !slot_found ) {
	dprintf("pjparse.c:pjl_register_permanent_soft_font_addition()\
                 font table full recycling font number 0\n");
	font_num = 0;
    }
    /* set the bit to 1 to indicate the fontnumber has been added */
    pjl_permanent_soft_fonts[font_num >> 3] |= (128 >> (font_num & 7));
    return font_num;
}
