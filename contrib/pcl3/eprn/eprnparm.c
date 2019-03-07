/******************************************************************************
  File:     $Id: eprnparm.c,v 1.24 2001/08/18 17:42:34 Martin Rel $
  Contents: Device parameter handling for the ghostscript device 'eprn'
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*                                                                             *
*       Copyright (C) 2000, 2001 by Martin Lottermoser                        *
*       All rights reserved                                                   *
*                                                                             *
*******************************************************************************

  Preprocessor symbols:

    EPRN_GS_HAS_MEDIAPOSITION
        Define this if ghostscript should in the future implement the standard
        PostScript page device parameter "MediaPosition" as a device parameter.
        Otherwise it will be stored in the eprn device. Note that
        ghostscript's input media selection algorithm *does* react to the
        parameter, and you could also specify it from PostScript. This
        implementation is only needed to make the parameter available as a
        command line option.

    EPRN_NO_PAGECOUNTFILE
        Define this if you do not want to use eprn's pagecount-file feature.
        You very likely must define this on Microsoft Windows. This is
        automatically defined under Visual Studio builds.

    EPRN_TRACE
        Define this to enable tracing. Only useful for development.

******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE   500
#endif

/* Special Aladdin header, must be included before <sys/types.h> on some
   platforms (e.g., FreeBSD). */
#include "std.h"

/* Standard headers */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ghostscript headers */
#ifdef EPRN_TRACE
#include "gdebug.h"
#endif  /* EPRN_TRACE */

/* Special headers */
#include "gdeveprn.h"
#include "gp.h"

#include "gscoord.h"    /* for gs_setdefaultmatrix() */

/*****************************************************************************/

#define ERRPREF         "? eprn: "
#define WARNPREF        "?-W eprn: "

/*****************************************************************************/

/*  Data structures for string arguments to parameters */

const eprn_StringAndInt
  /* Colour models */
  eprn_colour_model_list[] = {
    /* Values of type 'eprn_ColourModel' are assumed to be usable as indices
       into this array in order to find string representations for them. */
    { "Gray",   eprn_DeviceGray },
    { "RGB",    eprn_DeviceRGB },
    { "CMY",    eprn_DeviceCMY },
    { "CMY+K",  eprn_DeviceCMY_plus_K },
    { "CMYK",   eprn_DeviceCMYK },
    { NULL,     0 }
  };

static const eprn_StringAndInt
  /* Intensity rendering methods */
  intensity_rendering_list[] = {
    { "printer",        eprn_IR_printer },
    { "halftones",      eprn_IR_halftones },
    { "Floyd-Steinberg", eprn_IR_FloydSteinberg },
    { NULL, 0}
  };

/******************************************************************************

  Function: eprn_get_string

  This function returns a string representation of 'in_value' in '*out_value',
  based on 'table'. 'table' must be an array terminated with an entry having
  NULL as the 'name' value and must be permanently allocated and constant.
  If 'in_value' cannot be found in 'table', the function returns a non-zero
  value, otherwise zero.

  The string buffer in '*out_value' will be a statically allocated area which
  must not be modified.

******************************************************************************/

int eprn_get_string(int in_value, const eprn_StringAndInt *table,
  gs_param_string *out_value)
{
  while (table->name != NULL && table->value != in_value) table++;
  if (table->name == NULL) return -1;

  out_value->data = (const byte *)table->name;
  out_value->size = strlen(table->name);
  out_value->persistent = true;

  return 0;
}

/******************************************************************************

  Function: eprn_get_int

  This function parses 'in_value' based on 'table' and returns the result in
  '*out_value'. 'table' must be an array, terminated with an entry having NULL
  as the value for 'name'.

  'in_value' must be a string present in 'table'. If it is, the function
  returns 0, otherwise a non-zero ghostscript error value.

  On returning 'gs_error_VMerror', the function will have issued an error
  message.

******************************************************************************/

int eprn_get_int(const gs_param_string *in_value,
  const eprn_StringAndInt *table, int *out_value)
{
  char *s;

  /* First we construct a properly NUL-terminated string */
  s = (char *) malloc(in_value->size + 1);
  if (s == NULL) {
    eprintf1(ERRPREF
      "Memory allocation failure in eprn_get_int(): %s.\n",
      strerror(errno));
    return_error(gs_error_VMerror);
  }
  strncpy(s, (const char *)in_value->data, in_value->size);
  s[in_value->size] = '\0';

  /* Loop over table */
  while (table->name != NULL && strcmp(table->name, s) != 0) table++;
  if (table->name != NULL) *out_value = table->value;
  else {
    free(s); s = NULL;
    return_error(gs_error_rangecheck);
  }

  free(s); s = NULL;

  return 0;
}

/******************************************************************************

  Function: eprn_dump_parameter_list

  This function is only used for debugging. It dumps the names of the
  parameters in the parameter list 'plist' on the debugging stream.

******************************************************************************/

#ifdef EPRN_TRACE

void eprn_dump_parameter_list(gs_param_list *plist)
{
  gs_param_enumerator_t iterator;
  gs_param_key_t key;
  int count = 0;

  param_init_enumerator(&iterator);
  while (param_get_next_key(plist, &iterator, &key) == 0) {
    int j;

    count++;
    dmlprintf(plist->memory, "  `");
    for (j = 0; j < key.size; j++) dmputc(plist->memory, key.data[j]);
    dmprintf(plist->memory, "'\n");
  }
  dmlprintf1(plist->memory, "  Number of parameters: %d.\n", count);

  return;
}

#endif  /* EPRN_TRACE */

/******************************************************************************

  Function: eprn_fillpage
  This is just a "call-through" to the default, so we can grab the gs_gstate

******************************************************************************/
int
eprn_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
  eprn_Eprn *eprn = &((eprn_Device *)dev)->eprn;

  eprn->pgs = pgs;

  return (*eprn->orig_fillpage)(dev, pgs, pdevc);
}


static void eprn_replace_fillpage(gx_device *dev)
{
    eprn_Eprn *eprn = &((eprn_Device *)dev)->eprn;

    if (dev->procs.fillpage != eprn_fillpage) {
        eprn->orig_fillpage = dev->procs.fillpage;
        dev->procs.fillpage = eprn_fillpage;
    }
}


/******************************************************************************

  Function: eprn_get_params

  This function returns to the caller information about the values of
  parameters defined for the device in the 'eprn' part and its base devices.

  The function returns zero on success and a negative value on error.

******************************************************************************/

int eprn_get_params(gx_device *device, gs_param_list *plist)
{
  gs_param_string string_value;
  const eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;
  int rc;

#ifdef EPRN_TRACE
  if_debug0(EPRN_TRACE_CHAR, "! eprn_get_params()...\n");
#endif

  eprn_replace_fillpage(device);

  /* Base class parameters */
  rc = gdev_prn_get_params(device, plist);
  if (rc < 0) return rc;

  /* Number of intensity levels. The casts are used to get rid of compiler
     warnings because the *_levels are unsigned. */
  if ((rc = param_write_int(plist, "BlackLevels",
      (const int *)&eprn->black_levels)) < 0) return rc;
  if ((rc = param_write_int(plist, "CMYLevels",
      (const int *)&eprn->non_black_levels)) < 0) return rc;
  if ((rc = param_write_int(plist, "RGBLevels",
      (const int *)&eprn->non_black_levels)) < 0) return rc;

  /* Colour model */
  eprn_get_string(eprn->colour_model, eprn_colour_model_list, &string_value);
  if ((rc = param_write_string(plist, "ColourModel", &string_value)) < 0 ||
      (rc = param_write_string(plist, "ColorModel", &string_value)) < 0)
    return rc;

  /* CUPS page accounting */
  if ((rc = param_write_bool(plist, "CUPSAccounting", &eprn->CUPS_accounting))
    < 0) return rc;

  /* CUPS message format */
  if ((rc = param_write_bool(plist, "CUPSMessages", &eprn->CUPS_messages)) < 0)
    return rc;

  /* Intensity rendering */
  eprn_get_string(eprn->intensity_rendering, intensity_rendering_list,
    &string_value);
  if ((rc = param_write_string(plist, "IntensityRendering", &string_value)) < 0)
    return rc;

  /* Leading edge */
  if (eprn->leading_edge_set) {
    if ((rc = param_write_int(plist, "LeadingEdge", &eprn->default_orientation))
      < 0) return rc;
  }
  else
    if ((rc = param_write_null(plist, "LeadingEdge")) < 0) return rc;

  /* Media configuration file */
  if (eprn->media_file == NULL) {
    if ((rc = param_write_null(plist, "MediaConfigurationFile")) < 0)
      return rc;
  }
  else {
    string_value.data = (const byte *)eprn->media_file;
    string_value.size = strlen((const char *)string_value.data);
    string_value.persistent = false;
    if ((rc =
        param_write_string(plist, "MediaConfigurationFile", &string_value)) < 0)
      return rc;
  }

#ifndef EPRN_GS_HAS_MEDIAPOSITION
  /* Requested input media position */
  if (eprn->media_position_set) {
    if ((rc = param_write_int(plist, "MediaPosition", &eprn->media_position))
      < 0) return rc;
  }
  else
    if ((rc = param_write_null(plist, "MediaPosition")) < 0) return rc;
#endif  /* EPRN_GS_HAS_MEDIAPOSITION */

#ifndef EPRN_NO_PAGECOUNTFILE
  /* Page count file */
  if (eprn->pagecount_file == NULL) {
    if ((rc = param_write_null(plist, "PageCountFile")) < 0) return rc;
  }
  else {
    string_value.data = (const byte *)eprn->pagecount_file;
    string_value.size = strlen((const char *)string_value.data);
    string_value.persistent = false;
    if ((rc = param_write_string(plist, "PageCountFile", &string_value)) < 0)
      return rc;
  }
#endif  /* EPRN_NO_PAGECOUNTFILE */

  return 0;
}

/******************************************************************************

  Function: is_word

  This function returns a non-zero value iff the string beginning at 's' is
  identical with the string pointed to by 'word' and is followed either by a
  blank character or '\0'.

******************************************************************************/

static int is_word(const char *s, const char *word)
{
  size_t l = strlen(word);
  if (strncmp(s, word, l) != 0) return 0;
  return s[l] == '\0' || isspace(s[l]);
}

/******************************************************************************

  Function: next_word

  This function returns a pointer to the beginning of the next blank-separated
  word in the string pointed to by 's'. If s[0] is not blank, the character is
  considered to be part of the current word, i.e. the word to be returned is
  the one following.

  If there is no next word in this sense, the function returns NULL.

******************************************************************************/

static char *next_word(char *s)
{
  /* Skip current word */
  while (*s != '\0' && !isspace(*s)) s++;

  /* Skip intermediate blanks */
  while (*s != '\0' && isspace(*s)) s++;

  return *s == '\0'? NULL: s;
}

/******************************************************************************

  Function: eprn_read_media_data

  This function reads a media configuration file and stores the result in
  '*eprn'.  The file name must already have been stored in 'eprn->media_file',
  'eprn->media_overrides' should be NULL.

  The function returns zero on success and a non-zero ghostscript error value
  otherwise. In the latter case, an error message will have been issued.

******************************************************************************/

#define BUFFER_SIZE     200
  /* should be large enough for a single line */

#define cleanup()       (free(list), gp_fclose(f))

static int eprn_read_media_data(eprn_Eprn *eprn, gs_memory_t *memory)
{
  char buffer[BUFFER_SIZE];
  const char
    *epref = eprn->CUPS_messages? CUPS_ERRPREF: "",
    *wpref = eprn->CUPS_messages? CUPS_WARNPREF: "";
  gp_file *f;
  float conversion_factor = BP_PER_IN;
    /* values read have to be multiplied by this value to obtain bp */
  int
    line = 0,   /* line number */
    read = 0;   /* number of entries read so far */
  eprn_PageDescription *list = NULL;

  /* Open the file */
  if ((f = gp_fopen(memory, eprn->media_file, "r")) == NULL) {
    eprintf5("%s" ERRPREF "Error opening the media configuration file\n"
      "%s    `%s'\n%s  for reading: %s.\n",
      epref, epref, eprn->media_file, epref, strerror(errno));
    return_error(gs_error_invalidfileaccess);
  }

  /* Loop over input lines */
  while (gp_fgets(buffer, BUFFER_SIZE, f) != NULL) {
    char *s, *t;
    eprn_PageDescription *current;
    int chars_read;

    line++;

    /* Check for buffer overflow */
    if ((s = strchr(buffer, '\n')) == NULL && gp_fgetc(f) != EOF) {
      eprintf5("%s" ERRPREF "Exceeding line length %d in "
          "media configuration file\n%s  %s, line %d.\n",
        epref, BUFFER_SIZE - 2 /* '\n'+'\0' */, epref, eprn->media_file, line);
      cleanup();
      return_error(gs_error_limitcheck);
    }

    /* Eliminate the newline character */
    if (s != NULL) *s = '\0';

    /*  Originally, I did nothing further at this point and used a
        "%g %g %g %g %n" format in the sscanf() call below to skip trailing
        blanks. This does not work with Microsoft Visual C up to at least
        version 6 (_MSC_VER is 1200) because the variable for %n will never be
        set. If one drops the blank, it will be set, also if there are
        additional directives after %n. In addition, Cygwin does not (as of
        early 2001) set the %n variable if there is trailing white space in the
        string scanned. I don't want to know what's going on there, I just
        foil these bugs by removing all trailing white space from the input
        line which means I don't have to scan it afterwards.
    */
    if (s == NULL) s = strchr(buffer, '\0');
    while (buffer < s && isspace(*(s-1))) s--;
    *s = '\0';

    /* Ignore blank and comment lines */
    s = buffer;
    while (isspace(*s)) s++;
    if (*s == '\0' || *s == '#') continue;

    /* Check for unit specification */
    if (is_word(s, "unit")) {
      char *unit_name = next_word(s);
      if (unit_name != NULL) {
        s = next_word(unit_name);
        if (s == NULL) {
          if (is_word(unit_name, "in")) {
            conversion_factor = BP_PER_IN;
            continue;
          }
          if (is_word(unit_name, "mm")) {
            conversion_factor = BP_PER_MM;
            continue;
          }
        }
        /* If 's' is not NULL or the unit is not recognized, the error message
           will be generated when the attempt to read the whole line as a media
           specification will fail because there is no media size called
           "unit". */
      }
    }

    /* Extend the list */
    {
      eprn_PageDescription *new_list;
      new_list = (eprn_PageDescription *)
        realloc(list, (read+1)*sizeof(eprn_PageDescription));
      if (new_list == NULL) {
        eprintf2("%s" ERRPREF
          "Memory allocation failure in eprn_read_media_data(): %s.\n",
          epref, strerror(errno));
        cleanup();
        return_error(gs_error_VMerror);
      }
      list = new_list;
    }

    /* Set 'current' on the new entry */
    current = list + read;

    /* Isolate and identify the media size name */
    s = buffer;
    while (isspace(*s)) s++;
    t = s + 1;  /* we checked above that the line is not empty */
    while (*t != '\0' && !isspace(*t)) t++;
    if (*t != '\0') {
      *t = '\0';
      t++;
    }
    {
      ms_MediaCode code = ms_find_code_from_name(s, eprn->flag_desc);
      if (code == ms_none) {
        eprintf5("%s" ERRPREF "Unknown media name (%s) in "
            "media configuration file\n%s  %s, line %d.\n",
          epref, s, epref, eprn->media_file, line);
        cleanup();
        return_error(gs_error_rangecheck);
      }
      if (code & MS_ROTATED_FLAG) {
        eprintf5("%s" ERRPREF "Invalid substring \"" MS_ROTATED_STRING
            "\" in media name (%s)\n"
          "%s  in media configuration file %s, line %d.\n",
          epref, s, epref, eprn->media_file, line);
        cleanup();
        return_error(gs_error_rangecheck);
      }
      current->code = code;
    }

    /* Look for margins */
    if (sscanf(t, "%g %g %g %g%n", &current->left,
          &current->bottom, &current->right, &current->top, &chars_read) != 4 ||
        t[chars_read] != '\0') {
      if (*t != '\0') *(t-1) = ' ';     /* remove NUL after media name */
      eprintf5("%s" ERRPREF
        "Syntax error in media configuration file %s, line %d:\n%s    %s\n",
        epref, eprn->media_file, line, epref, buffer);
      cleanup();
      return_error(gs_error_rangecheck);
    }

    /* Check for sign */
    if (current->left < 0 || current->bottom < 0 || current->right < 0 ||
        current->top < 0) {
      eprintf4("%s" ERRPREF
        "Ghostscript does not support negative margins (line %d in the\n"
        "%s  media configuration file %s).\n",
        epref, line, epref, eprn->media_file);
      cleanup();
      return_error(gs_error_rangecheck);
    }

    read++;

    /* Convert to bp */
    current->left   *= conversion_factor;
    current->bottom *= conversion_factor;
    current->right  *= conversion_factor;
    current->top    *= conversion_factor;

    /* A margin for custom page sizes without the corresponding capability in
       the printer is useless although it would not lead to a failure of eprn.
       The user might not notice the reason without help, hence we check. */
    if (ms_without_flags(current->code) == ms_CustomPageSize &&
        eprn->cap->custom == NULL)
      eprintf6("%s" WARNPREF "The media configuration file %s\n"
        "%s    contains a custom page size entry in line %d, "
          "but custom page sizes\n"
        "%s    are not supported by the %s.\n",
        wpref, eprn->media_file, wpref, line, wpref, eprn->cap->name);
  }
  if (gp_ferror(f)) {
    eprintf2("%s" ERRPREF
      "Unidentified system error while reading `%s'.\n",
      epref, eprn->media_file);
    cleanup();
    return_error(gs_error_invalidfileaccess);
  }
  gp_fclose(f);

  /* Was the file empty? */
  if (read == 0) {
    eprintf3("%s" ERRPREF "The media configuration file %s\n"
      "%s  does not contain any media information.\n",
      epref, eprn->media_file, epref);
    return_error(gs_error_rangecheck);
  }

  /* Create a list in the device structure */
  eprn->media_overrides = (eprn_PageDescription *) gs_malloc(memory, read + 1,
    sizeof(eprn_PageDescription), "eprn_read_media_data");
  if (eprn->media_overrides == NULL) {
    eprintf1("%s" ERRPREF
      "Memory allocation failure from gs_malloc() in eprn_read_media_data().\n",
      epref);
    free(list);
    return_error(gs_error_VMerror);
  }

  /* Copy the list and set the sentinel entry */
  memcpy(eprn->media_overrides, list, read*sizeof(eprn_PageDescription));
  eprn->media_overrides[read].code = ms_none;

  /* Cleanup */
  free(list);

  return 0;
}

#undef BUFFER_SIZE
#undef cleanup

/******************************************************************************

  Function: eprn_set_media_data

  This function sets the media size and margin information in an 'eprn' device
  from the specified media configuration file.

  The return code will be zero an success and a ghostscript error code
  otherwise. In the latter case, an error message will have been issued.

  The 'length' may be positive in which case it denotes the length of the
  string 'media_file' or zero in which case the string is assumed to be
  NUL-terminated.

  A NULL value or an empty string for 'media_file' is permitted and removes
  all previous media descriptions read from a media configuration file.

******************************************************************************/

int eprn_set_media_data(eprn_Device *dev, const char *media_file, size_t length)
{
  eprn_Eprn *eprn = &dev->eprn;
  const char *epref = eprn->CUPS_messages? CUPS_ERRPREF: "";
  int rc = 0;

  /* Any previous size determination is obsolete now */
  eprn->code = ms_none;

  /* Free old storage */
  if (eprn->media_file != NULL) {
    gs_free(dev->memory->non_gc_memory, eprn->media_file, strlen(eprn->media_file) + 1,
      sizeof(char), "eprn_set_media_data");
    eprn->media_file = NULL;
  }
  if (eprn->media_overrides != NULL) {
    int n = 0;
    while (eprn->media_overrides[n].code != ms_none) n++;
    gs_free(dev->memory->non_gc_memory, eprn->media_overrides, n+1, sizeof(eprn_PageDescription),
      "eprn_set_media_data");
    eprn->media_overrides = NULL;
  }

  /* Set the file name length if not given */
  if (media_file != NULL && length == 0) length = strlen(media_file);

  /* Read media configuration file, unless the name is NULL or the empty
     string */
  if (media_file != NULL && length > 0) {
    eprn->media_file = (char *)gs_malloc(dev->memory->non_gc_memory, length + 1, sizeof(char),
      "eprn_set_media_data");
    if (eprn->media_file == NULL) {
      eprintf1("%s" ERRPREF
        "Memory allocation failure from gs_malloc() in "
        "eprn_set_media_data().\n",
        epref);
      rc = gs_error_VMerror;
    }
    else {
      strncpy(eprn->media_file, media_file, length);
      eprn->media_file[length] = '\0';
      if ((rc = eprn_read_media_data(eprn, dev->memory->non_gc_memory)) != 0) {
        gs_free(dev->memory->non_gc_memory, eprn->media_file, length + 1, sizeof(char),
          "eprn_set_media_data");
        eprn->media_file = NULL;
      }
    }
  }

  return rc;
}

/******************************************************************************

  Function: eprn_bits_for_levels

  This function returns the number of bits used to represent 'levels' intensity
  levels. 'levels' must be <= (ULONG_MAX+1)/2.

******************************************************************************/

unsigned int eprn_bits_for_levels(unsigned int levels)
{
  unsigned int bits = 0;
  unsigned long n;

  for (n = 1; n < levels; n *= 2) bits++;

  return bits;
}

/******************************************************************************

  Function: res_supported

  'list' must either bei NULL (all resolutions are accepted) or point to a
  list of resolutions terminated with a {0.0, 0.0} entry.

******************************************************************************/

static bool res_supported(const eprn_Resolution *list, float hres, float vres)
{
  if (list == NULL) return true;

  while (list->h > 0.0 && (list->h != hres || list->v != vres)) list++;

  return list->h > 0.0;
}

/******************************************************************************

  Function: levels_supported

  'list' may not be NULL and must point to a {0,0}-terminated list of
  supported ranges.

******************************************************************************/

static bool levels_supported(const eprn_IntensityLevels *list,
  unsigned int levels)
{
  while (list->from > 0 && (levels < list->from || list->to < levels)) list++;

  return list->from > 0;
}

/******************************************************************************

  Function: reslev_supported

******************************************************************************/

static int reslev_supported(const eprn_ResLev *entry, float hres, float vres,
  unsigned int levels)
{
  return res_supported(entry->resolutions, hres, vres) &&
    levels_supported(entry->levels, levels);
}

/******************************************************************************

  Function: eprn_check_colour_info

  This function checks the arguments starting at 'model' whether they are
  supported according to 'list'. This list must satisfy the constraints for
  'colour_info' in 'eprn_PrinterDescription'.

  The function returns zero if the values are supported and a non-zero value
  if they are not.

******************************************************************************/

int eprn_check_colour_info(const eprn_ColourInfo *list,
  eprn_ColourModel *model, float *hres, float *vres,
  unsigned int *black_levels, unsigned int *non_black_levels)
{
  const eprn_ColourInfo *entry;

  /* Search for a match. Successful exits are in the middle of the loop. */
  for (entry = list; entry->info[0] != NULL; entry++)
    if (entry->colour_model == *model ||
        (entry->colour_model == eprn_DeviceCMYK &&
         *model == eprn_DeviceCMY_plus_K)) {
      const eprn_ResLev *rl;
      unsigned int levels = (entry->colour_model == eprn_DeviceRGB ||
          entry->colour_model == eprn_DeviceCMY? *non_black_levels:
        *black_levels);

      for (rl = entry->info[0]; rl->levels != NULL; rl++)
        if (reslev_supported(rl, *hres, *vres, levels)) {
          const eprn_ResLev *rl2 = NULL;

          /* Check on info[1] needed? */
          if (entry->colour_model == eprn_DeviceGray ||
              entry->colour_model == eprn_DeviceRGB ||
              entry->colour_model == eprn_DeviceCMY)
            return 0;

          /* CMY+K or CMYK process colour models */
          if (entry->info[1] != NULL) {
            for (rl2 = entry->info[1]; rl2->levels != NULL; rl2++)
              if (reslev_supported(rl2, *hres, *vres, *non_black_levels)) break;
          }
          if ((entry->info[1] == NULL && *black_levels == *non_black_levels) ||
              (entry->info[1] != NULL && rl2->levels != NULL))
            return 0;
        }
    }

  return -1;
}

/******************************************************************************

  Function: set_derived_colour_data

  This routine determines and sets various derived values in the device
  structure based on the number of black and non-black levels requested.

  The values to be set are 'eprn.bits_per_colorant' and the fields 'depth',
  'max_gray', 'max_color', 'dither_grays' and 'dither_colors' in the
  'color_info' structure.

  The parameters 'black_levels' and 'non_black_levels' must be in the range
  0 to 256 without 1. At least one of the two must be positive.

******************************************************************************/

static void set_derived_colour_data(eprn_Device *dev)
{
  eprn_Eprn *eprn = &dev->eprn;
  unsigned int levels;

  /* Choose equal number of bits in 'gx_color_index' for all components present
   */
  if (eprn->intensity_rendering == eprn_IR_FloydSteinberg) levels = 256;
  else if (eprn->black_levels >= eprn->non_black_levels)
    levels = eprn->black_levels;
  else levels = eprn->non_black_levels;
  eprn->bits_per_colorant = eprn_bits_for_levels(levels);

  /* For the depth, consider all components and adjust to possible values.
     Ghostscript permits pixel depths 1, 2, 4, 8, 16, 24 and 32. */
  dev->color_info.depth =
    (eprn->non_black_levels == 0? 1: 4) * eprn->bits_per_colorant;
      /* No distinction between non-Gray colour models */
  if (dev->color_info.depth > 2) {
    if (dev->color_info.depth <= 4) dev->color_info.depth = 4;
    else if (dev->color_info.depth <= 8) dev->color_info.depth = 8;
    else dev->color_info.depth = ((dev->color_info.depth + 7)/8)*8;
      /* Next multiple of 8 */
  }

  /*  Set ghostscript's color_info data. This is an area where ghostscript's
      documentation (Drivers.htm) is not particularly intelligible. For
      example: can there be situations where the dither_* parameters are
      different from their corresponding max_* parameter plus one?
  */
  if (eprn->intensity_rendering != eprn_IR_halftones) {
    /*  Here we cover two cases: retaining as much colour information as
        possible and effectively setting up 1-pixel halftone cells. Both
        demand that ghostscript is prevented from doing halftoning; the
        remaining difference (which is the essential part) is then handled in
        our colour mapping functions.
        According to Drivers.htm, a value of at least 31 for max_gray or
        max_color (actually, the documentation says "max_rgb") leads to
        ghostscript using the colour returned by the device if valid
        instead of using the dither parameters for halftoning. The actual
        values for the max_* parameters should then be irrelevant.
    */
    if (eprn->non_black_levels > 0) dev->color_info.max_color = 255;
    else dev->color_info.max_color = 0;
    dev->color_info.max_gray = 255;

    /* As long as our colour mapping functions return valid color indices, the
       following parameters should be irrelevant. */
    dev->color_info.dither_grays = 256;
    if (dev->color_info.num_components == 1) dev->color_info.dither_colors = 0;
    else dev->color_info.dither_colors = dev->color_info.max_color + 1;
  }
  else {
    /* Let ghostscript do halftoning */
    if (eprn->non_black_levels > 0)
      dev->color_info.max_color = eprn->non_black_levels - 1;
    else dev->color_info.max_color = 0;
    if (eprn->black_levels > 0)
      dev->color_info.max_gray = eprn->black_levels - 1;
    else dev->color_info.max_gray = dev->color_info.max_color;

    /* The dither parameters */
    if (eprn->black_levels > 0)
      dev->color_info.dither_grays = eprn->black_levels;
    else dev->color_info.dither_grays = eprn->non_black_levels;
    if (eprn->non_black_levels > 0)
      dev->color_info.dither_colors = eprn->non_black_levels;
    else dev->color_info.dither_colors = 0;
  }

  return;
}

/******************************************************************************

  Function: eprn_put_params

  This function reads a parameter list, extracts the parameters known to the
  'eprn' device, and configures the device appropriately. This includes
  parameters defined by base devices.

  If an error occurs in the processing of parameters, the function will
  return a negative value, otherwise zero.

  This function does *not* exhibit transactional behaviour as requested in
  gsparam.h, i.e. on error the parameter values in the device structure
  might have changed. However, all values will be individually valid.

  For most of the parameters an attempt to set them closes the device if it is
  open.

******************************************************************************/

int eprn_put_params(gx_device *dev, gs_param_list *plist)
{
  bool colour_mode_given_and_valid = false;
  gs_param_name pname;
  gs_param_string string_value;
  eprn_Eprn *eprn = &((eprn_Device *)dev)->eprn;
  const char
    *epref = eprn->CUPS_messages? CUPS_ERRPREF: "",
    *wpref = eprn->CUPS_messages? CUPS_WARNPREF: "";
  float mediasize[2];
  int
    height = dev->height,
    last_error = 0,
    temp,
    rc,
    width = dev->width;

#ifdef EPRN_TRACE
  if (gs_debug_c(EPRN_TRACE_CHAR)) {
    dmlprintf(dev->memory,
      "! eprn_put_params() called with the following device parameters:\n");
    eprn_dump_parameter_list(plist);
  }
#endif  /* EPRN_TRACE */

  eprn_replace_fillpage(dev);

  /* Remember initial page size */
  for (temp = 0; temp < 2; temp++) mediasize[temp] = dev->MediaSize[temp];

  /* CUPS message format. This should be the first parameter to be set which is
     a problem because it should happen even before the put_params methods of
     derived devices are called. However, note that in a CUPS filter
     "CUPSMessages" will usually be set from the command line while most
     remaining parameters will be set via setpagedevice. Hence this will come
     first anyway except for problems for which the print administrator is
     responsible, not the ordinary user. */
  if ((rc = param_read_bool(plist, "CUPSMessages", &eprn->CUPS_messages)) == 0)
  {
    epref = eprn->CUPS_messages? CUPS_ERRPREF: "";
    wpref = eprn->CUPS_messages? CUPS_WARNPREF: "";
  }
  else if (rc < 0) last_error = rc;

  /* Read colour model into 'temp'. For those colonials across the pond we also
     accept the barbarized spelling variant.
  */
#define colour_model(option)                                            \
  if ((rc = param_read_string(plist, (pname = option), &string_value)) == 0) { \
    rc = eprn_get_int(&string_value, eprn_colour_model_list, &temp);    \
    if (rc != 0) {                                                      \
      if (rc != gs_error_VMerror) {                                     \
        eprintf1("%s" ERRPREF "Unknown colour model: `", epref);        \
        errwrite(dev->memory, (const char *)string_value.data, sizeof(char)*string_value.size); \
        eprintf("'.\n");                                                \
      }                                                                 \
      last_error = rc;                                                  \
      param_signal_error(plist, pname, last_error);                     \
    }                                                                   \
    else colour_mode_given_and_valid = true;                            \
  }                                                                     \
  else if (rc < 0) last_error = rc;

  colour_model("ColorModel")
  colour_model("ColourModel")   /* overrides if both are given */

#undef colour_model

  if (colour_mode_given_and_valid) {
    if (eprn->colour_model != temp && dev->is_open) gs_closedevice(dev);
      /* The close_device method can fail, but what should I do then? */
    eprn->colour_model = temp;

    /* Set the native colour space */
    switch(eprn->colour_model) {
    case eprn_DeviceGray:
      dev->color_info.num_components = 1; break;
    case eprn_DeviceRGB:
      /*FALLTHROUGH*/
    case eprn_DeviceCMY:
      /*FALLTHROUGH*/
    case eprn_DeviceCMY_plus_K:
      dev->color_info.num_components = 3; break;
    case eprn_DeviceCMYK:
      dev->color_info.num_components = 4; break;
    default:
      assert(0);
    }

    dev->color_info.polarity =
        dci_std_polarity(dev->color_info.num_components);

    /* Adjust black levels */
    if (eprn->colour_model == eprn_DeviceCMY ||
        eprn->colour_model == eprn_DeviceRGB) {
      if (eprn->black_levels != 0) eprn->black_levels = 0;
    }
    else
      if (eprn->black_levels == 0) eprn->black_levels = 2;
    /* Adjust non-black levels if they are too small for colour */
    if (dev->color_info.num_components > 1 && eprn->non_black_levels <= 0)
      eprn->non_black_levels = 2;

    /* Adjustments to pixel depth, 'max_gray', 'max_color' and dither levels
       will occur near the end of this routine */
  }

  /* BlackLevels. Various depending values will be adjusted below. */
  if ((rc = param_read_int(plist, (pname = "BlackLevels"), &temp)) == 0) {
    if ((temp == 0 && (eprn->colour_model == eprn_DeviceRGB ||
                       eprn->colour_model == eprn_DeviceCMY)) ||
        (2 <= temp && temp <= 256 &&
         eprn->colour_model != eprn_DeviceRGB &&
         eprn->colour_model != eprn_DeviceCMY)) {
      if (eprn->black_levels != temp && dev->is_open) gs_closedevice(dev);
      eprn->black_levels = temp;
    }
    else {
      eprintf2("%s" ERRPREF
        "The value for BlackLevels is outside the range permitted: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* CMYLevels */
  if ((rc = param_read_int(plist, (pname = "CMYLevels"), &temp)) == 0) {
    if ((temp == 0 && eprn->colour_model == eprn_DeviceGray) ||
        (2 <= temp && temp <= 256 && eprn->colour_model != eprn_DeviceGray)) {
      if (eprn->non_black_levels != temp && dev->is_open) gs_closedevice(dev);
      eprn->non_black_levels = temp;
    }
    else {
      eprintf2("%s" ERRPREF
        "The value for CMYLevels is outside the range permitted: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* CUPS page accounting messages */
  {
    bool temp;
    if ((rc = param_read_bool(plist, "CUPSAccounting", &temp)) == 0) {
      if (eprn->CUPS_accounting && !temp)
        eprintf(CUPS_WARNPREF WARNPREF
          "Attempt to set CUPSAccounting from true to false.\n");
      else eprn->CUPS_accounting = temp;
    }
    else if (rc < 0) last_error = rc;
  }

  /* Intensity rendering */
  if ((rc = param_read_string(plist, (pname = "IntensityRendering"),
      &string_value)) == 0) {
    rc = eprn_get_int(&string_value, intensity_rendering_list, &temp);
    if (rc == 0) {
      if (temp != eprn->intensity_rendering && dev->is_open)
        gs_closedevice(dev);
      eprn->intensity_rendering = temp;
    }
    else {
      eprintf1("%s" ERRPREF "Invalid method for IntensityRendering: `",
        epref);
      errwrite(dev->memory, (const char *)string_value.data, sizeof(char)*string_value.size);
      eprintf("'.\n");
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Leading edge */
  if ((rc = param_read_null(plist, (pname = "LeadingEdge"))) == 0) {
    if (eprn->leading_edge_set && dev->is_open) gs_closedevice(dev);
    eprn->leading_edge_set = false;
  }
  else if (rc < 0 && rc != gs_error_typecheck) last_error = rc;
  else if ((rc = param_read_int(plist, (pname = "LeadingEdge"), &temp)) == 0) {
    if (0 <= temp && temp <= 3) {
      if ((!eprn->leading_edge_set || eprn->default_orientation != temp) &&
        dev->is_open) gs_closedevice(dev);
      eprn->leading_edge_set = true;
      eprn->default_orientation = temp;
    }
    else {
      eprintf2(
        "%s" ERRPREF "LeadingEdge may only have values 0 to 3, not %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Media configuration file */
  if ((rc = param_read_null(plist, (pname = "MediaConfigurationFile"))) == 0) {
    if (eprn->media_file != NULL && dev->is_open) gs_closedevice(dev);
    rc = eprn_set_media_data((eprn_Device *)dev, NULL, 0);
  }
  else if (rc < 0 && rc != gs_error_typecheck) last_error = rc;
  else if ((rc = param_read_string(plist, pname, &string_value)) == 0) {
    if (string_value.size > 0) {
      if ((eprn->media_file == NULL ||
          strncmp(eprn->media_file, (const char *)string_value.data,
            string_value.size) != 0 ||
          eprn->media_file[string_value.size] != '\0') && dev->is_open)
        gs_closedevice(dev);
      rc = eprn_set_media_data((eprn_Device *)dev,
        (const char *)string_value.data, string_value.size);
    }
    else {
      if (eprn->media_file != NULL && dev->is_open) gs_closedevice(dev);
      rc = eprn_set_media_data((eprn_Device *)dev, NULL, 0);
    }

    if (rc != 0) {
      last_error = rc;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

#ifndef EPRN_GS_HAS_MEDIAPOSITION
  if ((rc = param_read_null(plist, (pname = "MediaPosition"))) == 0)
    eprn->media_position_set = false;
  else if (rc < 0 && (rc = param_read_int(plist, pname, &eprn->media_position))
      == 0) {
    /* Current (up to at least gs 6.50) ghostscript versions do not accept
       negative MediaPosition values. */
    if (eprn->media_position < 0)
      eprintf3("%s" WARNPREF
        "Ghostscript does not accept negative values (%d) for the\n"
          "%s    `MediaPosition' parameter.\n",
        wpref, eprn->media_position, wpref);
      /* The error message is left for ghostscript to generate during input
         media selection, should such an entry be a match. */
    eprn->media_position_set = true;
  }
  else if (rc < 0) last_error = rc;
#endif  /* EPRN_GS_HAS_MEDIAPOSITION */

#ifndef EPRN_NO_PAGECOUNTFILE
  /* Page count file */
  if ((rc = param_read_null(plist, (pname = "PageCountFile"))) == 0) {
    if (eprn->pagecount_file != NULL) {
      gs_free(dev->memory->non_gc_memory, eprn->pagecount_file, strlen(eprn->pagecount_file) + 1,
        sizeof(char), "eprn_put_params");
      eprn->pagecount_file = NULL;
    }
  }
  else if (rc < 0 && rc != gs_error_typecheck) last_error = rc;
  else if ((rc = param_read_string(plist, pname, &string_value)) == 0) {
    /* Free old storage */
    if (eprn->pagecount_file != NULL) {
      gs_free(dev->memory->non_gc_memory, eprn->pagecount_file, strlen(eprn->pagecount_file) + 1,
        sizeof(char), "eprn_put_params");
      eprn->pagecount_file = NULL;
    }

    /* Store file name unless it is the empty string */
    if (string_value.size > 0) {
      eprn->pagecount_file = (char *)gs_malloc(dev->memory->non_gc_memory, string_value.size + 1,
        sizeof(char), "eprn_put_params");
      if (eprn->pagecount_file == NULL) {
        eprintf1( "%s" ERRPREF
          "Memory allocation failure from gs_malloc() in eprn_put_params().\n",
          epref);
        last_error = gs_error_VMerror;
        param_signal_error(plist, pname, last_error);
      }
      else {
        strncpy(eprn->pagecount_file, (const char *)string_value.data,
          string_value.size);
        eprn->pagecount_file[string_value.size] = '\0';
      }
    }
  }
#endif  /* EPRN_NO_PAGECOUNTFILE */

  /* RGBLevels */
  if ((rc = param_read_int(plist, (pname = "RGBLevels"), &temp)) == 0) {
    if (temp == 0 || (2 <= temp && temp <= 256)) {
      if (eprn->non_black_levels != temp && dev->is_open) gs_closedevice(dev);
      eprn->non_black_levels = temp;
    }
    else {
      eprintf2("%s" ERRPREF
        "The value for RGBLevels is outside the range permitted: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Determine various derived colour parameters */
  set_derived_colour_data((eprn_Device *)dev);

  /* Catch a specification of BitsPerPixel --- otherwise a wrong value gives
     just a rangecheck error from gs without any readily understandable
     information (see gx_default_put_params()). This confused one hpdj user
     so much that he wrote in a newsgroup that gs was dumping core in this
     case :-).
  */
  if ((rc = param_read_int(plist, (pname = "BitsPerPixel"), &temp)) == 0) {
    if (temp != dev->color_info.depth) {
      eprintf3("%s" ERRPREF
        "Attempt to set `BitsPerPixel' to a value (%d)\n"
        "%s  other than the one selected by the driver.\n",
        epref, temp, epref);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  } else if (rc < 0) last_error = rc;

  /* Check whether ".HWMargins" is specified and, if it is, refrain from
     overwriting it. */
  {
    gs_param_typed_value temp;
    if (param_read_typed(plist, ".HWMargins", &temp) == 0) {
      /* ".HWMargins" is specified */
#ifdef EPRN_TRACE
      if_debug1(EPRN_TRACE_CHAR, "! .HWMargins is specified: type is %d.\n",
        (int)temp.type);
#endif
      eprn->keep_margins = true;
    }
  }

  /* Process parameters defined by base classes (should occur after treating
     parameters defined for the derived class, see gsparam.h) */
  rc = gdev_prn_put_params(dev, plist);
  if (rc < 0 || (rc > 0 && last_error >= 0))
    last_error = rc;

  if (last_error < 0) return_error(last_error);

  /* If the page size was modified, close the device */
  if (dev->is_open && (dev->width != width || dev->height != height ||
      mediasize[0] != dev->MediaSize[0] || mediasize[1] != dev->MediaSize[1])) {
    gs_closedevice(dev);
#ifdef EPRN_TRACE
    if_debug0(EPRN_TRACE_CHAR,
      "! Closing device because of page size modification.\n");
#endif
  }

  return last_error;
}
