/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
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

/* genconf.c */
/* Generate configuration files */
#include "stdpre.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>		/* for calloc */
#include <string.h>

/* We would like to use the real realloc, but it doesn't work on all systems */
/* (e.g., some Linux versions). */
void *
mrealloc(void *old_ptr, size_t old_size, size_t new_size)
{	void *new_ptr = malloc(new_size);
	if ( new_ptr == NULL )
	  return NULL;
	/* We have to pass in the old size, since we have no way to */
	/* determine it otherwise. */
	memcpy(new_ptr, old_ptr, min(old_size, new_size));
	return new_ptr;
}

/*
 * This program generates a set of configuration files.
 * Almost everything it does could be done by a shell program, except that
 * (1) Unix shells are not compatible from one system to another,
 * (2) the DOS shell is not equal to the task,
 * (3) the VMS shell is radically different from all others.
 *
 * Usage:
 *	genconf [-Z] [-n [name_prefix | -]] [@]xxx.dev*
 *	  [-f gconfigf.h] [-h gconfig.h]
 *	  [-p[l|L][u][e] pattern] [-l|o|lo|ol out.tr]
 * & in the pattern acts as an escape character as follows:
 *	&p produces a %;
 *	&s produces a space;
 *	&& produces a \;
 *	&- produces a -;
 *	&x, for any other character x, is an error.
 */

/* DEFAULT_PREFIX should be DEFAULT_NAME_PREFIX. */
#define DEFAULT_PREFIX "gs_"

/* Structures for accumulating information. */
typedef struct string_item_s {
	const char *str;
	int index;
} string_item;
/* The values of uniq_mode are bit masks. */
typedef enum {
	uniq_all = 1,	/* keep all occurrences (default) */
	uniq_first = 2,	/* keep only first occurrence */
	uniq_last = 4	/* keep only last occurrence */
} uniq_mode;
typedef struct string_list_s {
		/* The following are set at creation time. */
	int max_count;
	uniq_mode mode;
		/* The following are updated dynamically. */
	int count;
	string_item *items;
} string_list;
#define max_pattern 60
typedef struct string_pattern_s {
	bool upper_case;
	bool drop_extn;
	char pattern[max_pattern + 1];
} string_pattern;
typedef struct config_s {
	int debug;
	const char *name_prefix;
	const char *file_prefix;
	/* file_names and file_contents are special.... */
	string_list file_names;
	string_list file_contents;
	string_list resources;
	string_list devs;
	string_list fonts;
	string_list headers;
	string_list libs;
	string_list libpaths;
	string_list objs;
	string_pattern lib_p;
	string_pattern libpath_p;
	string_pattern obj_p;
} config;
/* These lists grow automatically if needed, so we could start out with */
/* small allocations. */
static const config init_config = {
	0,			/* debug */
	DEFAULT_PREFIX,		/* name_prefix */
	"",			/* file_prefix */
	{ 200 },		/* file_names */
	{ 200 },		/* file_contents */
	{ 100, uniq_first },	/* resources */
	{ 100, uniq_first },	/* devs */
	{ 50, uniq_first },	/* fonts */
	{ 20, uniq_first },	/* headers */
	{ 20, uniq_last },	/* libs */
	{ 10, uniq_first },	/* libpaths */
	{ 400, uniq_first }	/* objs */
};

/* Forward definitions */
int alloc_list(P1(string_list *));
void parse_affix(P2(char *, const char *));
int read_dev(P2(config *, const char *));
int read_token(P3(char *, int, const char **));
int add_entry(P3(config *, char *, const char *));
string_item *add_item(P2(string_list *, const char *));
void sort_uniq(P1(string_list *));
void write_list(P3(FILE *, const string_list *, const char *));
void write_list_pattern(P3(FILE *, const string_list *, const string_pattern *));

main(int argc, char *argv[])
{	config conf;
	int i;

	/* Allocate string lists. */
	conf = init_config;
	alloc_list(&conf.file_names);
	alloc_list(&conf.file_contents);
	alloc_list(&conf.resources);
	alloc_list(&conf.devs);
	alloc_list(&conf.fonts);
	alloc_list(&conf.headers);
	alloc_list(&conf.libs);
	alloc_list(&conf.libpaths);
	alloc_list(&conf.objs);

	/* Initialize patterns. */
	conf.lib_p.upper_case = false;
	conf.lib_p.drop_extn = false;
	strcpy(conf.lib_p.pattern, "%s\n");
	conf.libpath_p = conf.lib_p;
	conf.obj_p = conf.lib_p;

	/* Process command line arguments. */
	for ( i = 1; i < argc; i++ )
	{	const char *arg = argv[i];
		FILE *out;
		int lib = 0, obj = 0;
		if ( *arg != '-' )
		{	read_dev(&conf, arg);
			continue;
		}
		if ( i == argc - 1 )
		{	fprintf(stderr, "Missing argument after %s.\n",
				arg);
			exit(1);
		}
		switch ( arg[1] )
		{
		case 'C':	/* change directory, by analogy with make */
			conf.file_prefix =
			  (argv[i + 1][0] == '-' ? "" : argv[i + 1]);
			++i;
			continue;
		case 'n':
			conf.name_prefix =
			  (argv[i + 1][0] == '-' ? "" : argv[i + 1]);
			++i;
			continue;
		case 'p':
		{	string_pattern *pat;
			switch ( *(arg += 2) )
			{
			case 'l': pat = &conf.lib_p; break;
			case 'L': pat = &conf.libpath_p; break;
			default: pat = &conf.obj_p; arg--;
			}
			pat->upper_case = false;
			pat->drop_extn = false;
			if ( argv[i + 1][0] == '-' )
				strcpy(pat->pattern, "%s\n");
			else
			{	char *p, *q;
				for ( p = pat->pattern, q = argv[++i];
				      (*p++ = *q++) != 0;
				    )
				 if ( p[-1] == '&' )
				  switch ( *q )
				{
				case 'p': p[-1] = '%'; q++; break;
				case 's': p[-1] = ' '; q++; break;
				case '&': p[-1] = '\\'; q++; break;
				case '-': p[-1] = '-'; q++; break;
				default:
					fprintf(stderr,
						"& not followed by ps&-: &%c\n",
						*q);
					exit(1);
				}
				p[-1] = '\n';
				*p = 0;
			}
			for ( ; ; )
			  switch ( *++arg )
			{
			case 'u': pat->upper_case = true; break;
			case 'e': pat->drop_extn = true; break;
			case 0: goto pbreak;
			default:
				fprintf(stderr, "Unknown switch %s.\n", arg);
				exit(1);
			}
pbreak:			if ( pat == &conf.obj_p )
			{	conf.lib_p = *pat;
				conf.libpath_p = *pat;
			}
			continue;
		case 'Z':
			conf.debug = 1;
			continue;
		}
		}
		/* Must be an output file. */
		out = fopen(argv[++i], "w");
		if ( out == 0 )
		{	fprintf(stderr, "Can't open %s for output.\n",
				argv[i]);
			exit(1);
		}
		switch ( arg[1] )
		{
		case 'f':
			fputs("/* This file was generated automatically by genconf.c. */\n", out);
			fputs("/* For documentation, see gsconfig.c. */\n", out);
			{ char template[80];
			  sprintf(template,
				  "font_(\"0.font_%%s\",%sf_%%s,zf_%%s)\n",
				  conf.name_prefix);
			  write_list(out, &conf.fonts, template);
			}
			break;
		case 'h':
			fputs("/* This file was generated automatically by genconf.c. */\n", out);
			{ char template[80];
			  sprintf(template, "device_(%s%%s_device)\n",
				  conf.name_prefix);
			  write_list(out, &conf.devs, template);
			}
			sort_uniq(&conf.resources);
			write_list(out, &conf.resources, "%s\n");
			write_list(out, &conf.headers, "#include \"%s\"\n");
			break;
		case 'l':
			lib = 1;
			obj = arg[2] == 'o';
			goto lo;
		case 'o':
			obj = 1;
			lib = arg[2] == 'l';
lo:			if ( obj )
			{	sort_uniq(&conf.objs);
				write_list_pattern(out, &conf.objs, &conf.obj_p);
			}
			if ( lib )
			{	sort_uniq(&conf.libs);
				write_list_pattern(out, &conf.libpaths, &conf.libpath_p);
				write_list_pattern(out, &conf.libs, &conf.lib_p);
			}
			break;
		default:
			fclose(out);
			fprintf(stderr, "Unknown switch %s.\n", argv[i]);
			exit(1);
		}
		fclose(out);
	}

	exit(0);
}

/* Allocate and initialize a string list. */
int
alloc_list(string_list *list)
{	list->count = 0;
	list->items =
	  (string_item *)calloc(list->max_count, sizeof(string_item));
	assert(list->items != NULL);
	return 0;
}

/* Read an entire file into memory. */
/* We use the 'index' of the file_contents string_item to record the union */
/* of the uniq_modes of all (direct and indirect) items in the file. */
string_item *
read_file(config *pconf, const char *fname)
{	char *cname = malloc(strlen(fname) + strlen(pconf->file_prefix) + 1);
	int i;
	FILE *in;
	int end, nread;
	char *cont;
	string_item *item;

	if ( cname == 0 )
	  { fprintf(stderr, "Can't allocate space for file name %s%s.\n",
		    pconf->file_prefix, fname);
	    exit(1);
	  }
	strcpy(cname, pconf->file_prefix);
	strcat(cname, fname);
	for ( i = 0; i < pconf->file_names.count; ++i )
	  if ( !strcmp(pconf->file_names.items[i].str, cname) )
	    { free(cname);
	      return &pconf->file_contents.items[i];
	    }
	/* Try to open the file in binary mode, to avoid the overhead */
	/* of unnecessary EOL conversion in the C library. */
	in = fopen(cname, "rb");
	if ( in == 0 )
	{	in = fopen(cname, "r");
		if ( in == 0 )
		{	fprintf(stderr, "Can't read %s.\n", cname);
			exit(1);
		}
	}
	fseek(in, 0L, 2 /*SEEK_END*/);
	end = ftell(in);
	cont = malloc(end + 1);
	if ( cont == 0 )
	{	fprintf(stderr, "Can't allocate %d bytes to read %s.\n",
			end + 1, cname);
		exit(1);
	}
	rewind(in);
	nread = fread(cont, 1, end, in);
	fclose(in);
	cont[nread] = 0;
	if ( pconf->debug )
	  printf("File %s = %d bytes.\n", cname, nread);
	add_item(&pconf->file_names, cname);
	item = add_item(&pconf->file_contents, cont);
	item->index = 0;		/* union of uniq_modes */
	return item;
}

/* Read and parse a .dev file. */
/* Return the union of all its uniq_modes. */
int
read_dev(config *pconf, const char *arg)
{	string_item *item;
	const char *in;
#define max_token 256
	char *token = malloc(max_token + 1);
	char *category = malloc(max_token + 1);
	int len;

	if ( pconf->debug )
	  printf("Reading %s;\n", arg);
	item = read_file(pconf, arg);
	if ( item->index == uniq_first )
	{	/* Don't need to read the file again. */
		if ( pconf->debug )
		  printf("Skipping duplicate file.\n");
		return uniq_first;
	}
	in = item->str;
	strcpy(category, "obj");
	while ( (len = read_token(token, max_token, &in)) > 0 )
	  item->index |= add_entry(pconf, category, token);
	free(category);
#undef max_token
	if ( len < 0 )
	{	fprintf(stderr, "Token too long: %s.\n", token);
		exit(1);
	}
	if ( pconf->debug )
	  printf("Finished %s.\n", arg);
	free(token);
	return item->index;
}

/* Read a token from a string that contains the contents of a file. */
int
read_token(char *token, int max_len, const char **pin)
{	const char *in = *pin;
	int len = 0;

	while ( len < max_len )
	{	char ch = *in;
		if ( ch == 0 )
		  break;
		++in;
		if ( isspace(ch) )
		{	if ( len > 0 )
			  break;
			continue;
		}
		token[len++] = ch;
	}
	token[len] = 0;
	*pin = in;
	return (len >= max_len ? -1 /* token too long */ : len);
}

/* Add an entry to a configuration. */
/* Return its uniq_mode. */
int
add_entry(config *pconf, char *category, const char *item)
{	if ( item[0] == '-' )		/* set category */
	{	strcpy(category, item + 1);
		return 0;
	}
	else				/* add to current category */
	{
#define max_str 120
		char str[max_str];
		char template[80];
		const char *pat = 0;
		string_list *list = &pconf->resources;

		if ( pconf->debug )
		  printf("Adding %s %s;\n", category, item);
		/* Handle a few resources specially; just queue the rest. */
		switch ( category[0] )
		{
#define is_cat(str) !strcmp(category, str)
		case 'd':
			if ( is_cat("dev") )
			  { list = &pconf->devs; break; }
			goto err;
		case 'e':
			if ( is_cat("emulator") )
			  { pat = "emulator_(\"%s\")"; break; }
			goto err;
		case 'f':
			if ( is_cat("font") )
			  { list = &pconf->fonts; break; }
			goto err;
		case 'h':
			if ( is_cat("header") )
			  { list = &pconf->headers; break; }
			goto err;
		case 'i':
			if ( is_cat("include") )
			{	int len = strlen(item);
				strcpy(str, item);
				if ( len < 5 || strcmp(str + len - 4, ".dev") )
				  strcat(str, ".dev");
				return read_dev(pconf, str);
			}
			if ( is_cat("includef") )
			{	strcpy(str, item);
				strcat(str, ".dvc");
				return read_dev(pconf, str);
			}
			if ( is_cat("init") )
			  { pat = "init_(%s%%s_init)"; }
			else if ( is_cat("iodev") )
			  { pat = "io_device_(%siodev_%%s)"; }
			else
			  goto err;
			sprintf(template, pat, pconf->name_prefix);
			pat = template;
			break;
		case 'l':
			if ( is_cat("lib") )
			  { list = &pconf->libs; break; }
			if ( is_cat("libpath") )
			  { list = &pconf->libpaths; break; }
			goto err;
		case 'o':
			if ( is_cat("obj") )
			  { list = &pconf->objs;
			    strcpy(template, pconf->file_prefix);
			    strcat(template, "%s");
			    pat = template;
			    break;
			  }
			if ( is_cat("oper") )
			  { pat = "oper_(%s_op_defs)"; break; }
			goto err;
		case 'p':
			if ( is_cat("ps") )
			  { pat = "psfile_(\"%s.ps\")"; break; }
			goto err;
#undef is_cat
		default:
err:			fprintf(stderr, "Unknown category %s.\n", category);
			exit(1);
		}
		if ( pat )
		{	sprintf(str, pat, item);
			assert(strlen(str) < max_str);
			add_item(list, str);
		}
		else
		  add_item(list, item);
		return list->mode;
	}
}

/* Add an item to a list. */
string_item *
add_item(string_list *list, const char *str)
{	char *rstr = malloc(strlen(str) + 1);
	int count = list->count;
	string_item *item;

	strcpy(rstr, str);
	if ( count >= list->max_count )
	  {	list->max_count <<= 1;
		list->items =
		  (string_item *)mrealloc(list->items,
					  (list->max_count >> 1) *
					  sizeof(string_item),
					  list->max_count *
					  sizeof(string_item));
		assert(list->items != NULL);
	  }
	item = &list->items[count];
	item->index = count;
	item->str = rstr;
	list->count++;
	return item;
}

/* Remove duplicates from a list of string_items. */
/* In case of duplicates, remove all but the earliest (if last = false) */
/* or the latest (if last = true). */
#define psi1 ((const string_item *)p1)
#define psi2 ((const string_item *)p2)
int
cmp_index(const void *p1, const void *p2)
{	int cmp = psi1->index - psi2->index;
	return (cmp < 0 ? -1 : cmp > 0 ? 1 : 0);
}
int
cmp_str(const void *p1, const void *p2)
{	return strcmp(psi1->str, psi2->str);
}
#undef psi1
#undef psi2
void
sort_uniq(string_list *list)
{	string_item *strlist = list->items;
	int count = list->count;
	const string_item *from;
	string_item *to;
	int i;
	bool last = list->mode == uniq_last;

	if ( count == 0 )
	 return;
	qsort((char *)strlist, count, sizeof(string_item), cmp_str);
	for ( from = to = strlist + 1, i = 1; i < count; from++, i++ )
	  if ( strcmp(from->str, to[-1].str) )
	    *to++ = *from;
	  else if ( (last ? from->index > to[-1].index :
		     from->index < to[-1].index)
		  )
	    to[-1] = *from;
	count = to - strlist;
	list->count = count;
	qsort((char *)strlist, count, sizeof(string_item), cmp_index);
}

/* Write a list of strings using a template. */
void
write_list(FILE *out, const string_list *list, const char *pstr)
{	string_pattern pat;
	pat.upper_case = false;
	pat.drop_extn = false;
	strcpy(pat.pattern, pstr);
	write_list_pattern(out, list, &pat);
}
void
write_list_pattern(FILE *out, const string_list *list, const string_pattern *pat)
{	int i;
	char macname[40];
	int plen = strlen(pat->pattern);
	*macname = 0;
	for ( i = 0; i < list->count; i++ )
	{	const char *lstr = list->items[i].str;
		int len = strlen(lstr);
		char *str = malloc(len + 1);
		int xlen = plen + len * 3;
		char *xstr = malloc(xlen + 1);
		char *alist;
		strcpy(str, lstr);
		if ( pat->drop_extn )
		{	char *dot = str + len;
			while ( dot > str && *dot != '.' ) dot--;
			if ( dot > str ) *dot = 0, len = dot - str;
		}
		if ( pat->upper_case )
		{	char *ptr = str;
			for ( ; *ptr; ptr++ )
			  if ( islower(*ptr) ) *ptr = toupper(*ptr);
		}
		/* We repeat str for the benefit of patterns that */
		/* need the argument substituted in more than one place. */
		sprintf(xstr, pat->pattern, str, str, str);
		/* Check to make sure the item is within the scope of */
		/* an appropriate #ifdef, if necessary. */
		alist = strchr(xstr, '(');
		if ( alist != 0 && alist != xstr && alist[-1] == '_' )
		{	*alist = 0;
			if ( strcmp(xstr, macname) )
			{	if (*macname )
				  fputs("#endif\n", out);
				fprintf(out, "#ifdef %s\n", xstr);
				strcpy(macname, xstr);
			}
			*alist = '(';
		}
		else
		{	if ( *macname )
			{	fputs("#endif\n", out);
				*macname = 0;
			}
		}
		fputs(xstr, out);
		free(xstr);
		free(str);
	}
	if ( *macname )
	  fputs("#endif\n", out);
}
