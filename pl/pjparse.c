/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  
*/

/* NB change this to pjl.c and pjl.h and remove the parsing stuff. */
/* NB procedures should be in the same order as the header file */
/* NB need error checking for NULLS */
/* NB should use dictionaries - this implementation was used for faster debugging */
/* NB should define a lookup procedure */

/* pjparse.c */
/* PJL parser */
#include "memory_.h"
#include "scommon.h"
#include "pjparse.h"
#include <ctype.h> 		/* for toupper() */
#include <stdlib.h>             /* for atoi() */

/* ------ Parser state ------ */

#define PJL_STRING_LENGTH (15)

/* for simplicity we define both variable names and values as strings.
   The client must do the appropriate conversions. */
typedef struct pjl_envir_var_s {
    char var[PJL_STRING_LENGTH+1];
    char value[PJL_STRING_LENGTH+1];
} pjl_envir_var_t;

typedef struct pjl_parser_state_s {
    char line[81];		/* buffered command line */
    int pos;			/* current position in line */
    pjl_envir_var_t *defaults;  /* the default environment (i.e. set default) */
    pjl_envir_var_t *envir;     /* the pjl environment */
} pjl_parser_state_t;

/* provide factory defaults for pjl commands.  Note these are not pjl
   defaults but initial values set in the printer when shipped */
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
    {"", ""}
};

/* case insensitive comparison of two null terminated strings. */
 int
pjl_compare(const char *s1, const char *s2)
{
    for (; toupper(*s1) == toupper(*s2); ++s1, ++s2)
	if (*s1 == '\0')
	    return(0);
    return 1;
}

/* set a pjl environment or default variable.  NB handle side effects
   on other variable of each command */
 private int
pjl_set(pjl_parser_state_t *pst, char *variable, char *value, bool defaults)
{
    pjl_envir_var_t *table = (defaults ? pst->defaults : pst->envir);
    int i;

    for (i = 0; table[i].var[0]; i++)
	if (!pjl_compare(table[i].var, variable)) {
	    strcpy(table[i].value, value);
	    return 1;
	}
    /* didn't find variable */
    return 0;
}
    
/* Create and initialize a new state. */
 pjl_parser_state *
pjl_process_init(gs_memory_t *mem)
{
    pjl_parser_state_t *pjlstate =
	(pjl_parser_state *)gs_alloc_bytes(mem, 
	     sizeof(pjl_parser_state_t), "pjl_process_init()" );
    pjl_envir_var_t *pjl_env =
	(pjl_envir_var_t *)gs_alloc_bytes(mem, 
             sizeof(pjl_factory_defaults), "pjl_process_init()" );
    pjl_envir_var_t *pjl_def =
    	(pjl_envir_var_t *)gs_alloc_bytes(mem, 
             sizeof(pjl_factory_defaults), "pjl_process_init()" );
    if ( pjlstate == NULL || pjl_env == NULL || pjl_def == NULL )
	return NULL; /* should be fatal so we don't bother piecemeal frees */

    pjlstate->defaults = pjl_def;
    pjlstate->envir = pjl_env;
    /* initialize the default and initial pjl environment.  We assume
       that these are the same layout as the factory defaults. */
    memcpy(pjlstate->defaults, pjl_factory_defaults, sizeof(pjl_factory_defaults));
    memcpy(pjlstate->envir, pjlstate->defaults, sizeof(pjl_factory_defaults));
    /* initialize the current position in the line array */
    pjlstate->pos = 0;
    return (pjl_parser_state *)pjlstate;
}

/* Create and initialize a new state. */
 void
pjl_process_destroy(pjl_parser_state *pst,gs_memory_t *mem)
{
    gs_free_object(mem, pst->defaults, "pjl_process_destroy()");
    gs_free_object(mem, pst->envir, "pjl_process_destroy()");
    gs_free_object(mem, pst, "pjl_process_destroy()");
}

/* a tokenizer to return what we are interested in - set, default, =,
   and our variables variable and possible values.  Returns null when
   finished with the line.  NB It will also skip over lparm : pcl etc. */

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
} pjl_token_type_t;

 private pjl_token_type_t
pjl_get_token(pjl_parser_state_t *pst, char token[])
{
    int c;
    int start_pos;

    /* skip any whitespace if we need to.  */
    while((c = pst->line[pst->pos]) == ' ' || c == '\t') pst->pos++;

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
	  c != '\0')
	pst->pos++;

    /* build the token */
    {
	int slength = pst->pos - start_pos;
	int i;

	/* token doesn't fit or is empty */
	if (( slength > PJL_STRING_LENGTH) || slength == 0)
	    return DONE;
	/* now the string can be safely copied */
	strncpy(token, &pst->line[start_pos], slength);
	token[slength] = '\0';

	/* check for known tokens NB should use a table here. */
	if (!pjl_compare(token, "@PJL")) return PREFIX;
	if (!pjl_compare(token, "SET")) return SET;
	if (!pjl_compare(token, "DEFAULT")) return DEFAULT;
	if (!pjl_compare(token, "INITIALIZE")) return INITIALIZE;
	if (!pjl_compare(token, "=")) return EQUAL;
	if (!pjl_compare(token, "DINQUIRE")) return DINQUIRE;
	if (!pjl_compare(token, "INQUIRE")) return INQUIRE;
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
		bool defaults = (tok == DEFAULT);
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
	    memcpy(pst->envir, &pjl_factory_defaults, sizeof(pjl_factory_defaults));
	    return 0;
	case RESET:
	    memcpy(pst->envir, &pjl_factory_defaults, sizeof(pjl_factory_defaults));
	    return 0;
	default:
	    return -1;
	}
    }
    return (tok == DONE ? 0 : -1);
}

 char *
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

/* PJL symbol set environment variable -> pcl symbol set numbers */
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

/* NB the nulls will crash the table */
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

/* utilities */

int pjl_vartoi(const char *s)
{
    return atoi(s);
}

/* envioronment variable to float */
floatp pjl_vartof(const char *s)
{
    return atof(s);
}

