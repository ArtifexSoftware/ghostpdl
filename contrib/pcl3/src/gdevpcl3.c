/******************************************************************************
  File:     $Id: gdevpcl3.c,v 1.32 2001/08/14 15:22:35 Martin Rel $
  Contents: Ghostscript device 'pcl3' for PCL-3+ printers
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*                                                                             *
*       Copyright (C) 2000, 2001 by Martin Lottermoser                        *
*       All rights reserved                                                   *
*                                                                             *
*******************************************************************************

  Preprocessor symbols:

    GS_REVISION (integer)
        If defined, this must be the ghostscript version number, e.g., 601 for
        ghostscript 6.01.

    PCL3_MEDIA_FILE (const char *)
        Define this to set a media configuration file for the "unspec" device
        unless the user overrides it.

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

/* Driver-specific headers */
#include "gdeveprn.h"
#include "pclcap.h"
#include "pclgen.h"
#include "pclsize.h"

/*****************************************************************************/

/* Does the argument point to an instance of the generic (pcl3) device? */
#define is_generic_device(dev)  (strcmp(dev->dname, "pcl3") == 0)

/*****************************************************************************/

/* Combined type with a range of bool plus null */
typedef enum {bn_null, bn_true, bn_false} bool_or_null;

/* Type for duplex capabilities */
typedef enum {Duplex_none, Duplex_sameLeadingEdge, Duplex_oppositeLeadingEdge,
  Duplex_both} DuplexCapabilities;

/* Device structure */
typedef struct {
  gx_eprn_device_common;        /* eprn part including base types */

  /* Printer selection and other data not directly mappable to PCL */
  pcl_Printer printer;
  bool_or_null use_card;
  DuplexCapabilities duplex_capability;
  bool tumble;  /* only relevant if 'Duplex' is 'true' */

  /* PCL generation */
  bool
    initialized,        /* Has init() been run on this device instance? */
    configured,         /* Has the output file been configured? */
    configure_every_page;  /* Repeat the configuration for every page? */
  pcl_FileData file_data;
  pcl3_sizetable table;
} pcl3_Device;

/*****************************************************************************/

/* Device procedures */
static dev_proc_open_device(pcl3_open_device);
static dev_proc_close_device(pcl3_close_device);
static dev_proc_get_params(pcl3_get_params);
static dev_proc_put_params(pcl3_put_params);

/* Device procedures */
static void
eprn_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs(dev);

    set_dev_proc(dev, open_device, pcl3_open_device);
    set_dev_proc(dev, get_initial_matrix, eprn_get_initial_matrix);
    set_dev_proc(dev, close_device, pcl3_close_device);
    set_dev_proc(dev, map_rgb_color, eprn_map_rgb_color_for_CMY_or_K);
    set_dev_proc(dev, map_color_rgb, eprn_map_color_rgb);
    set_dev_proc(dev, map_cmyk_color, eprn_map_cmyk_color_glob);
    set_dev_proc(dev, get_params, pcl3_get_params);
    set_dev_proc(dev, put_params, pcl3_put_params);
    set_dev_proc(dev, fillpage, eprn_fillpage);
};

/* prn procedure implementations */
#if !defined(GS_REVISION) || GS_REVISION >= 550
static prn_dev_proc_print_page(pcl3_print_page);
#else
static dev_proc_print_page(pcl3_print_page);
#endif

/*****************************************************************************/

/* Media flags known to this device in addition to the standard ones */
static const ms_Flag
  flag_description[] = {
    {PCL_CARD_FLAG, PCL_CARD_STRING},
    {ms_none, NULL}
  };

/* List of possible optional media flags */
static const ms_MediaCode
  card_is_optional[] = {PCL_CARD_FLAG, ms_none};

/*****************************************************************************/

/* Forward declaration */
static void pcl3_flag_mismatch_reporter(
  const struct s_eprn_Device *eprn, bool no_match);

/* Macro for creating device structure instances */
#define pcl3_device_instance(dname, printer)                    \
  pcl3_Device gs_##dname##_device = {                           \
    eprn_device_initdata(                                       \
      pcl3_Device,      /* device type */                       \
      eprn_initialize_device_procs, /* initialize dev_procs */  \
      #dname,           /* device name */                       \
      300.0, 300.0,     /* horizontal and vertical resolution */\
      pcl3_print_page,  /* print page routine */                \
      &pcl3_printers[printer].desc, /* printer capability description */ \
      flag_description, /* flag descriptions */                 \
      ms_none,          /* desired media flags */               \
      card_is_optional, /* list of optional flags */            \
      &pcl3_flag_mismatch_reporter),    /* reporting function */\
    printer,            /* printer */                           \
    bn_null,            /* use_card */                          \
    Duplex_none,        /* duplex_capability */                 \
    false,              /* tumble */                            \
    false               /* initialized */                       \
    /* The remaining fields will be set in init(). */           \
  }

/* Generic and flexible device structure instance */
pcl3_device_instance(pcl3, pcl3_generic_new);

/* Printer-specific and fixed device structure instances */
/* At present there is no entry for the HP DeskJet because its natural name
   collides with the hpdj driver. */
pcl3_device_instance(hpdjplus,  HPDeskJetPlus);
pcl3_device_instance(hpdjportable, HPDJPortable);
pcl3_device_instance(hpdj310,   HPDJ310);
pcl3_device_instance(hpdj320,   HPDJ320);
pcl3_device_instance(hpdj340,   HPDJ340);
pcl3_device_instance(hpdj400,   HPDJ400);
pcl3_device_instance(hpdj500,   HPDJ500);
pcl3_device_instance(hpdj500c,  HPDJ500C);
pcl3_device_instance(hpdj510,   HPDJ510);
pcl3_device_instance(hpdj520,   HPDJ520);
pcl3_device_instance(hpdj540,   HPDJ540);
pcl3_device_instance(hpdj550c,  HPDJ550C);
pcl3_device_instance(hpdj560c,  HPDJ560C);
pcl3_device_instance(hpdj600,   HPDJ600);
pcl3_device_instance(hpdj660c,  HPDJ660C);
pcl3_device_instance(hpdj670c,  HPDJ670C);
pcl3_device_instance(hpdj680c,  HPDJ680C);
pcl3_device_instance(hpdj690c,  HPDJ690C);
pcl3_device_instance(hpdj850c,  HPDJ850C);
pcl3_device_instance(hpdj855c,  HPDJ855C);
pcl3_device_instance(hpdj870c,  HPDJ870C);
pcl3_device_instance(hpdj890c,  HPDJ890C);
pcl3_device_instance(hpdj1120c, HPDJ1120C);

/*****************************************************************************/

#define ERRPREF         "? pcl3: "
#define WARNPREF        "?-W pcl3: "

#define array_size(a)   (sizeof(a)/sizeof(a[0]))

/*****************************************************************************/

static const eprn_StringAndInt
  /* Names for duplex capabilities */
  duplex_capabilities_list[] = {
    { "none",           Duplex_none },
    { "sameLeadingEdge", Duplex_sameLeadingEdge },
    { "oppositeLeadingEdge", Duplex_oppositeLeadingEdge },
    { "both",           Duplex_both },
    { NULL,             0 }
  },
  /* Names for PCL Media Type values */
  media_type_list[] = {
    /* Canonical names */
    { "plain paper",    0 },
    { "bond paper",     1 },
    { "HP Premium paper", 2 },
    { "glossy paper",   3 },
    { "transparency film", 4 },
    { "quick dry glossy", 5 },
    { "quick dry transparency", 6 },
    /* Shortened names */
    { "plain",          0 },
    { "bond",           1 },
    { "Premium",        2 },
    { "glossy",         3 },
    { "transparency",   4 },
    { NULL,             0 }
  },
  /* Print Quality */
  print_quality_list[] = {
    { "draft",         -1 },
    { "normal",         0 },
    { "presentation",   1 },
    /* Start of synonyms */
    { "econo",          -1 },
    { "best",           1 },
    { NULL,             0 }
  },
  /* Subdevice names. They must be ordered by 'value' except for the last
     (NULL) entry. At present, there are 26 non-NULL entries here. */
  subdevice_list[] = {
    { "hpdj",           HPDeskJet },
    { "hpdjplus",       HPDeskJetPlus },
    { "hpdjportable",   HPDJPortable },
    { "hpdj310",        HPDJ310 },
    { "hpdj320",        HPDJ320 },
    { "hpdj340",        HPDJ340 },
    { "hpdj400",        HPDJ400 },
    { "hpdj500",        HPDJ500 },
    { "hpdj500c",       HPDJ500C },
    { "hpdj510",        HPDJ510 },
    { "hpdj520",        HPDJ520 },
    { "hpdj540",        HPDJ540 },
    { "hpdj550c",       HPDJ550C },
    { "hpdj560c",       HPDJ560C },
    { "unspecold",      pcl3_generic_old },
    { "hpdj600",        HPDJ600 },
    { "hpdj660c",       HPDJ660C },
    { "hpdj670c",       HPDJ670C },
    { "hpdj680c",       HPDJ680C },
    { "hpdj690c",       HPDJ690C },
    { "hpdj850c",       HPDJ850C },
    { "hpdj855c",       HPDJ855C },
    { "hpdj870c",       HPDJ870C },
    { "hpdj890c",       HPDJ890C },
    { "hpdj1120c",      HPDJ1120C },
    { "unspec",         pcl3_generic_new },
    { NULL,             0 }
  };

/******************************************************************************

  Function: cmp_by_value

  This function compares two 'eprn_StringAndInt' instances by their 'value'
  fields.

******************************************************************************/

static int cmp_by_value(const void *a, const void *b)
{
  return ((const eprn_StringAndInt *)a)->value -
    ((const eprn_StringAndInt *)b)->value;
}

/******************************************************************************

  Function: get_string_for_int

  This function returns a string representation of 'in_value' in '*out_value',
  based on 'table'. 'table' must be an array terminated with an entry having
  NULL as the 'name' value and must be permanently allocated and constant.
  If 'in_value' cannot be found in 'table', the function returns a decimal
  representation of 'in_value'.

  The string buffer in '*out_value' will be a permanently allocated area which
  must not be modified.

******************************************************************************/

static void get_string_for_int(int in_value, const eprn_StringAndInt *table,
  gs_param_string *out_value)
{
  while (table->name != NULL && table->value != in_value) table++;
  if (table->name != NULL) {
    out_value->data = (const byte *)table->name;
    out_value->size = strlen(table->name);
    out_value->persistent = true;
  }
  else {
    char buffer[22];     /* Must be sufficient for an 'int' */

    gs_snprintf(buffer, sizeof(buffer), "%d", in_value);
    assert(strlen(buffer) < sizeof(buffer));
    out_value->data = (const byte *)buffer;
    out_value->size = strlen(buffer);
    out_value->persistent = false;
  }

  return;
}

/******************************************************************************

  Function: get_int_for_string

  This function parses 'in_value' based on 'table' and returns the result in
  '*out_value'. 'table' must be an array, terminated with an entry having NULL
  as the value for 'name'.

  'in_value' must either be a decimal representation of an integer or must be
  a string present in 'table'. In these cases, the function returns 0,
  otherwise a non-zero ghostscript error value.

  On returning 'gs_error_VMerror', the function will have issued an error
  message.

******************************************************************************/

static int get_int_for_string(const gs_param_string *in_value,
  const eprn_StringAndInt *table, int *out_value)
{
  char *s;
  int read;     /* counter */

  /* First we construct a properly NUL-terminated string */
  s = (char *) malloc(in_value->size + 1);
  if (s == NULL) {
    eprintf1(ERRPREF
      "Memory allocation failure in get_int_for_string(): %s.\n",
      strerror(errno));
    return_error(gs_error_VMerror);
  }
  strncpy(s, (const char *)in_value->data, in_value->size);
  s[in_value->size] = '\0';

  /* To foil bugs in sscanf() on Windows (see gdeveprn.c in eprn) I'm removing
     trailing white space here instead of skipping it with a format. */
  {
    char *t = strchr(s, '\0');

    while (s < t && isspace(*(t-1))) t--;
    *t = '\0';
  }

  /* Check for a numerical value */
  if (sscanf(s, "%d%n", out_value, &read) != 1 || s[read] != '\0') {
    /* What the user specified is not a valid numerical value */
    while (table->name != NULL && strcmp(table->name, s) != 0) table++;
    if (table->name == NULL) {
      free(s); s = NULL;
      return_error(gs_error_rangecheck);
    }
    *out_value = table->value;
  }

  free(s); s = NULL;

  return 0;
}

/******************************************************************************

  Function: init

  This function does that part of initialization which cannot be performed at
  compile time or which must be repeated whenever the subdevice is changed.
  It must be called if 'initialized' is false and after the subdevice is
  changed.

  When this function is called, 'dev->printer' must have been set correctly.

  This function must not and does not depend on the state of initialization of
  its base devices.

******************************************************************************/

static void init(pcl3_Device *dev)
{
#ifndef NDEBUG
  /* Check that 'subdevice_list' is sorted by 'value' */
  {
    int j;
    for (j = 1; j < array_size(subdevice_list) - 1; j++)
      assert(cmp_by_value(subdevice_list + j - 1, subdevice_list + j) <= 0);
  }
#endif  /* !NDEBUG */

  /* Base class fields */
  if (is_generic_device(dev)) dev->Duplex_set = 0;
   /* "Duplex" is null. See remarks on the "Duplex" page device parameter in
      pcl3_put_params(). */

  /* pcl3 fields */
  dev->use_card = bn_null;
  dev->duplex_capability = Duplex_none;
  dev->tumble = false;
  dev->configured = false;
  dev->configure_every_page = false;

  /* Initialize 'file_data' */
  pcl3_fill_defaults(dev->printer, &dev->file_data);

  dev->initialized = true;

  return;
}

/******************************************************************************

  Function: pcl3_flag_mismatch_reporter

  Flag mismatch reporting function for the pcl3 device.

  The 'desired_flags' field can only contain MS_BIG_FLAG and PCL_CARD_FLAG.

******************************************************************************/

static void pcl3_flag_mismatch_reporter(
  const struct s_eprn_Device *eprn, bool no_match)
{
  const char *epref = eprn->CUPS_messages? CUPS_ERRPREF: "";

  if (eprn->desired_flags == 0) {
    eprintf2(
      "%s" ERRPREF "The %s does not support the requested media properties.\n",
      epref, eprn->cap->name);
  }
  else if (eprn->desired_flags == MS_BIG_FLAG) {
    eprintf2("%s" ERRPREF "The %s does not support banner printing",
      epref, eprn->cap->name);
    if (!no_match) eprintf(" for this size");
    eprintf(".\n");
  }
  else if (eprn->desired_flags == PCL_CARD_FLAG) {
    eprintf2("%s" ERRPREF
      "The %s does not support a `Card' variant for ",
      epref, eprn->cap->name);
    if (no_match) eprintf("any"); else eprintf("this");
    eprintf(" size.\n");
  }
  else {
    eprintf1(
      "%s" ERRPREF "Banner printing on postcards?? You must be joking!\n",
      epref);
  }

  return;
}

/******************************************************************************

  Function: find_subdevice_name

  This function returns a pointer to a static storage location containing
  a NUL-terminated string with the name of the subdevice for 'subdev'.

  It must not be called for invalid 'subdev' values.

******************************************************************************/

static const char *find_subdevice_name(int subdev)
{
  eprn_StringAndInt
    key = {NULL, 0};
  const eprn_StringAndInt
    *found;

  key.value = subdev;

  found = (const eprn_StringAndInt *)bsearch(&key, subdevice_list,
    array_size(subdevice_list) - 1, sizeof(eprn_StringAndInt), cmp_by_value);
  assert(found != NULL);

  return found->name;
}

/******************************************************************************

  Function: pcl3_get_params

  This function returns to the caller information about the values of
  parameters defined for the device. This includes parameters defined in base
  classes.

  The function returns zero on success and a negative value on error.

******************************************************************************/

static int pcl3_get_params(gx_device *device, gs_param_list *plist)
{
  gs_param_string string_value;
  pcl3_Device *dev = (pcl3_Device *)device;
  const pcl_FileData *data = &dev->file_data;
  int
    temp,  /* Used as an intermediate for various reasons */
    rc;

  /* Constructor */
  if (!dev->initialized) init(dev);

  /* Base class parameters */
  rc = eprn_get_params(device, plist);
  if (rc < 0) return rc;

  /* Compression method */
  temp = data->compression;
  if ((rc = param_write_int(plist, "CompressionMethod", &temp)) < 0) return rc;

  /* Configure every page */
  if ((rc = param_write_bool(plist, "ConfigureEveryPage",
    &dev->configure_every_page)) < 0) return rc;

  /* Dry time */
  if (data->dry_time < 0) {
    if ((rc = param_write_null(plist, "DryTime")) < 0) return rc;
  }
  else if ((rc = param_write_int(plist, "DryTime", &data->dry_time)) < 0)
    return rc;

  /* Duplex capability */
  if (is_generic_device(dev)) {
    eprn_get_string(dev->duplex_capability, duplex_capabilities_list,
      &string_value);
    if ((rc = param_write_string(plist, "DuplexCapability", &string_value)) < 0)
      return rc;
  }

  /* Manual feed */
  {
    bool temp = dev->file_data.manual_feed;
    if ((rc = param_write_bool(plist, "ManualFeed", &temp)) < 0) return rc;
  }

  /* PCL media type */
  get_string_for_int(data->media_type, media_type_list, &string_value);
  if ((rc = param_write_string(plist, "Medium", &string_value)) < 0)
    return rc;

  /* Media destination */
  if ((rc = param_write_int(plist, "%MediaDestination",
    &data->media_destination)) < 0) return rc;

  /* Media source */
  if ((rc = param_write_int(plist, "%MediaSource", &data->media_source)) < 0)
    return rc;

  /* Use of Configure Raster Data */
  if (is_generic_device(dev) || pcl_has_CRD(data->level)) {
    bool temp = (data->level == pcl_level_3plus_CRD_only);
    if ((rc = param_write_bool(plist, "OnlyCRD", &temp)) < 0) return rc;
  }

  /* PCL initilization strings */
  if (data->init1.length == 0) {
    if ((rc = param_write_null(plist, "PCLInit1")) < 0) return rc;
  }
  else {
    string_value.data = (const byte *)data->init1.str;
    string_value.size = data->init1.length;
    string_value.persistent = false;
    if ((rc = param_write_string(plist, "PCLInit1", &string_value)) < 0)
      return rc;
  }
  if (data->init2.length == 0) {
    if ((rc = param_write_null(plist, "PCLInit2")) < 0) return rc;
  }
  else {
    string_value.data = (const byte *)data->init2.str;
    string_value.size = data->init2.length;
    string_value.persistent = false;
    if ((rc = param_write_string(plist, "PCLInit2", &string_value)) < 0)
      return rc;
  }

  /* PJL job name */
  if (data->PJL_job == NULL) {
    if ((rc = param_write_null(plist, "PJLJob")) < 0) return rc;
  }
  else {
    string_value.data = (const byte *)data->PJL_job;
    string_value.size = strlen(data->PJL_job);
    string_value.persistent = false;
    if ((rc = param_write_string(plist, "PJLJob", &string_value)) < 0)
      return rc;
  }

  /* PJL language */
  if (data->PJL_language == NULL) {
    if ((rc = param_write_null(plist, "PJLLanguage")) < 0) return rc;
  }
  else {
    string_value.data = (const byte *)data->PJL_language;
    string_value.size = strlen(data->PJL_language);
    string_value.persistent = false;
    if ((rc = param_write_string(plist, "PJLLanguage", &string_value)) < 0)
      return rc;
  }

  /* Print quality */
  get_string_for_int(data->print_quality, print_quality_list, &string_value);
  if ((rc = param_write_string(plist, "PrintQuality", &string_value)) < 0)
    return rc;

  /* Black bit planes first (standard PCL) or last */
  {
    bool temp = (data->order_CMYK == TRUE);
    if ((rc = param_write_bool(plist, "SendBlackLast", &temp)) < 0) return rc;
  }

  /* NUL header */
  if ((rc = param_write_int(plist, "SendNULs", &data->NULs_to_send)) < 0)
    return rc;

  /* Subdevice name, but only for the generic device */
  if (is_generic_device(dev)) {
    const char *name = find_subdevice_name(dev->printer);
    string_value.data = (const byte *)name;
    string_value.size = strlen(name);
    string_value.persistent = true;
    if ((rc = param_write_string(plist, "Subdevice", &string_value)) < 0)
      return rc;
  }

  /* Tumble */
  if (is_generic_device(dev))
    if ((rc = param_write_bool(plist, "Tumble", &dev->tumble)) < 0) return rc;

  /* UseCard */
  if (dev->use_card == bn_null) {
    if ((rc = param_write_null(plist, "UseCard")) < 0) return rc;
  }
  else {
    bool temp = (dev->use_card == bn_true);
    if ((rc = param_write_bool(plist, "UseCard", &temp)) < 0) return rc;
  }

  /* The old quality commands if they have meaning for this device */
  if (pcl_use_oldquality(data->level)) {
    /* Depletion */
    if (data->depletion == 0) {
      if ((rc = param_write_null(plist, "Depletion")) < 0) return rc;
    }
    else if ((rc = param_write_int(plist, "Depletion", &data->depletion)) < 0)
      return rc;

    /* Raster Graphics Quality */
    if ((rc = param_write_int(plist, "RasterGraphicsQuality",
      &data->raster_graphics_quality)) < 0) return rc;

    /* Shingling */
    if ((rc = param_write_int(plist, "Shingling", &data->shingling)) < 0)
      return rc;
  }
  else if (is_generic_device(dev)) {
    /* It is logically wrong for these parameters to be visible if we are
       dealing with a group-3 device, but I haven't yet found a way to get
       around ghostscript's undefined-parameter-update problem. */
    if ((rc = param_write_null(plist, "Depletion")) < 0) return rc;
    if ((rc = param_write_null(plist, "RasterGraphicsQuality")) < 0) return rc;
    if ((rc = param_write_null(plist, "Shingling")) < 0) return rc;
  }

#ifdef EPRN_TRACE
  if (gs_debug_c(EPRN_TRACE_CHAR)) {
    dmlprintf(dev->memory, "! pcl3_get_params() returns the following parameters:\n");
    eprn_dump_parameter_list(plist);
  }
#endif

  return 0;
}

/******************************************************************************

  Function: fetch_octets

  This function checks whether 'plist' contains a string entry for the parameter
  'pname' and returns the value via '*s' if it does.

  Neither 'plist' nor 'pname' may be NULL. On entry, '*s' must be a valid octet
  string; if it is non-zero, 's->str' must point to a gs_malloc()-allocated
  storage area of length 's->length'.

  If the parameter exists in 'plist', its type must be null or string. If it is
  null, '*s' will become zero. If the parameter type is string, the value will
  be copied to '*s'.

  The function returns a negative ghostscript error code on error and zero
  otherwise. In the former case an error message will have been issued,
  using 'epref' as a prefix for the message.

******************************************************************************/

static int fetch_octets(const char *epref,
  gs_param_list *plist, const char *pname, pcl_OctetString *s)
{
  gs_param_string string_value;
  int rc;

  if ((rc = param_read_null(plist, pname)) == 0) {
    if (s->length != 0)
      gs_free(plist->memory->non_gc_memory, s->str, s->length, sizeof(pcl_Octet), "fetch_octets");
    s->str = NULL;
    s->length = 0;
  }
  else if (rc < 0 &&
      (rc = param_read_string(plist, pname, &string_value)) == 0) {
    /* Free old storage */
    if (s->length != 0)
      gs_free(plist->memory->non_gc_memory, s->str, s->length, sizeof(pcl_Octet), "fetch_octets");

    /* Allocate new */
    s->str = (pcl_Octet *)gs_malloc(plist->memory->non_gc_memory, string_value.size, sizeof(pcl_Octet),
      "fetch_octets");

    if (s->str == NULL) {
      s->length = 0;
      eprintf1("%s" ERRPREF
        "Memory allocation failure from gs_malloc().\n", epref);
      rc = gs_error_VMerror;
      param_signal_error(plist, pname, rc);
    }
    else {
      memcpy(s->str, string_value.data, string_value.size);
      s->length = string_value.size;
    }
  }
  else if (rc > 0) rc = 0;

  return rc;
}

/******************************************************************************

  Function: fetch_cstring

  This function checks whether 'plist' contains a string entry for the parameter
  'pname' and returns the value via '*s' if it does.

  Neither 'plist' nor 'pname' may be NULL. On entry, '*s' must be NULL or point
  to a NUL-terminated string in a gs_malloc()-allocated storage area.

  If the parameter exists in 'plist', its type must be null or string. If it is
  null, '*s' will be gs_free()d and set to NULL. If it is a string, '*s' will
  be reallocated to contain the NUL-terminated value of the string. Should the
  string already contain a NUL value, only the part up to the NUL will be
  copied.

  The function returns a negative ghostscript error code on error and zero
  otherwise. In the former case an error message will have been issued.

******************************************************************************/

static int fetch_cstring(const char *epref,
  gs_param_list *plist, const char *pname, char **s)
{
  gs_param_string string_value;
  int rc;

  if ((rc = param_read_null(plist, pname)) == 0) {
    if (*s != NULL) gs_free(plist->memory->non_gc_memory, *s, strlen(*s) + 1, sizeof(char), "fetch_cstring");
    *s = NULL;
  }
  else if (rc < 0 &&
      (rc = param_read_string(plist, pname, &string_value)) == 0) {
    /* Free old storage */
    if (*s != NULL) gs_free(plist->memory->non_gc_memory, *s, strlen(*s) + 1, sizeof(char), "fetch_cstring");

    /* Allocate new */
    *s = (char *)gs_malloc(plist->memory->non_gc_memory, string_value.size + 1, sizeof(char),
      "fetch_cstring");

    if (*s == NULL) {
      eprintf1("%s" ERRPREF
        "Memory allocation failure from gs_malloc().\n", epref);
      rc = gs_error_VMerror;
      param_signal_error(plist, pname, rc);
    }
    else {
      strncpy(*s, (const char*)string_value.data, string_value.size);
      (*s)[string_value.size] = '\0';
    }
  }
  else if (rc > 0) rc = 0;

  return rc;
}

/******************************************************************************

  Function: set_palette

  This function sets 'dev->file_data.palette' and some other fields in
  agreement with 'dev->eprn.colour_model'.

******************************************************************************/

static void set_palette(pcl3_Device *dev)
{
  pcl_FileData *data = &dev->file_data;

  switch(dev->eprn.colour_model) {
  case eprn_DeviceGray:
    {
      const eprn_ColourInfo *ci = dev->eprn.cap->colour_info;

      /* Can this printer switch palettes? */
      while (ci->info[0] != NULL && ci->colour_model == eprn_DeviceGray) ci++;
      if (ci->info[0] != NULL) data->palette = pcl_black;
      else data->palette = pcl_no_palette;
    }
    data->number_of_colorants = 1;
    data->depletion = 0;        /* Depletion is only meaningful for colour. */
    break;
  case eprn_DeviceCMY:
    data->palette = pcl_CMY;
    data->number_of_colorants = 3;
    break;
  case eprn_DeviceRGB:
    data->palette = pcl_RGB;
    data->number_of_colorants = 3;
    break;
  case eprn_DeviceCMY_plus_K:
    /*FALLTHROUGH*/
  case eprn_DeviceCMYK:
    data->palette = pcl_CMYK;
    data->number_of_colorants = 4;
    break;
  default:
    assert(0);
  }

  return;
}

/******************************************************************************

  Function: pcl3_put_params

  This function reads a parameter list, extracts the parameters known to the
  device, and configures the device appropriately. This includes parameters
  defined by base classes.

  If an error occurs in the processing of parameters, the function will
  return a negative value, otherwise zero.

  This function does *not* exhibit transactional behaviour as requested in
  gsparam.h, i.e. on error the parameter values in the device structure
  might have changed. However, all values will be individually valid.

  Some of the parameters determine derived data in base classes or are relevant
  for device initialization. Setting any of these parameters closes the
  device if it is open.

******************************************************************************/

static int pcl3_put_params(gx_device *device, gs_param_list *plist)
{
  bool new_quality = false;     /* has someone requested the new variables? */
  gs_param_name pname;
  gs_param_string string_value;
  pcl3_Device *dev = (pcl3_Device *)device;
  const char
    *epref = dev->eprn.CUPS_messages? CUPS_ERRPREF: "",
    *wpref = dev->eprn.CUPS_messages? CUPS_WARNPREF: "";
  eprn_ColourModel previous_colour_model = dev->eprn.colour_model;
  pcl_FileData *data = &dev->file_data;
  int
    last_error = 0,
    temp,
    rc;
  struct {
    int depletion, quality, shingling;
  } requested = {-1, -1, -1};
    /* old quality parameters. -1 means "not specified". */

  /* Check for subdevice selection */
  if (is_generic_device(dev)) {
    if ((rc = param_read_string(plist, (pname = "Subdevice"), &string_value))
        == 0) {
      /* This property must be a known string. */
      int j = 0;
      while (subdevice_list[j].name != NULL &&
          (string_value.size != strlen(subdevice_list[j].name) ||
            strncmp((const char *)string_value.data, subdevice_list[j].name,
              string_value.size) != 0))
        j++;
        /* param_read_string() does not return NUL-terminated strings. */
      if (subdevice_list[j].name != NULL) {
        if (dev->is_open) gs_closedevice(device);
        dev->printer = subdevice_list[j].value;
        dev->initialized = false;
        rc = eprn_init_device((eprn_Device *)dev, &pcl3_printers[dev->printer].desc);
        if (rc < 0) {
            last_error = rc;
            param_signal_error(plist, pname, last_error);
        }
      }
      else {
        eprintf1("%s" ERRPREF "Unknown subdevice name: `", epref);
        errwrite(dev->memory,
                 (const char *)string_value.data,
                 sizeof(char)*string_value.size);
        eprintf("'.\n");
        last_error = gs_error_rangecheck;
        param_signal_error(plist, pname, last_error);
      }
    }
    else if (rc < 0) last_error = rc;
  }

  /* Constructor */
  if (!dev->initialized) init(dev);

  /* Compression method */
  if ((rc = param_read_int(plist, (pname = "CompressionMethod"), &temp))
      == 0) {
    if (temp != pcl_cm_none && temp != pcl_cm_rl && temp != pcl_cm_tiff &&
        temp != pcl_cm_delta && temp != pcl_cm_crdr) {
      eprintf2("%s" ERRPREF "Unsupported compression method: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
    else {
      if (temp == pcl_cm_crdr && (dev->printer == HPDeskJet ||
          dev->printer == HPDeskJetPlus || dev->printer == HPDJ500)) {
        /* This I know to be the case for the DJ 500. The others are guessed. */
        eprintf2(
          "%s" ERRPREF "The %s does not support compression method 9.\n",
          epref, dev->eprn.cap->name);
        last_error = gs_error_rangecheck;
        param_signal_error(plist, pname, last_error);
      }
      else data->compression= temp;
    }
  }
  else if (rc < 0) last_error = rc;

  /* Configure every page */
  if ((rc = param_read_bool(plist, "ConfigureEveryPage",
    &dev->configure_every_page)) < 0) last_error = rc;

  /* Depletion */
  if ((rc = param_read_null(plist, (pname = "Depletion"))) == 0)
    requested.depletion = 0;
  else if (rc < 0 && (rc = param_read_int(plist, pname, &temp)) == 0) {
    if (1 <= temp && temp <= 5 && (dev->printer != HPDJ500C || temp <= 3))
      requested.depletion = temp;
    else {
      eprintf2("%s" ERRPREF "Invalid value for depletion: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Dry time */
  if ((rc = param_read_null(plist, (pname = "DryTime"))) == 0)
    data->dry_time = -1;
  else if (rc < 0 &&
      (rc = param_read_int(plist, pname, &temp)) == 0) {
    if (0 <= temp && temp <= 1200) {
      if (dev->printer == HPDJ500 || dev->printer == HPDJ500C) {
         /* According to HP (DJ6/8 p. 18), only some of the series 600 and 800
            DeskJets respond to this command. I also suspect that the same is
            true for pre-DeskJet-500 printers. This should not matter, though,
            because the PCL interpreter should merely ignore the command.
            Hence I'm giving an error message only in those cases where HP
            explicitly states that the printer does not support the command.
          */
        eprintf2(
          "%s" ERRPREF "The %s does not support setting a dry time.\n",
          epref, dev->eprn.cap->name);
        last_error = gs_error_rangecheck;
        param_signal_error(plist, pname, last_error);
      }
      else data->dry_time = temp;
    }
    else {
      eprintf2("%s" ERRPREF "Invalid value for the dry time: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Duplex capability */
  if (is_generic_device(dev)) {
    if ((rc = param_read_string(plist, (pname = "DuplexCapability"),
        &string_value)) == 0) {
      rc = eprn_get_int(&string_value, duplex_capabilities_list, &temp);
      if (rc == 0) {
        if (dev->printer == pcl3_generic_new ||
            dev->printer == pcl3_generic_old || temp == Duplex_none) {
          dev->duplex_capability = temp;
          if (dev->duplex_capability == Duplex_none)
            dev->Duplex_set = 0;        /* force to "null" */
        }
        else {
          eprintf2("%s" ERRPREF
            "You can use a non-trivial value for DuplexCapability\n"
            "%s  only for unspec and unspecold.\n", epref, epref);
          last_error = gs_error_rangecheck;
          param_signal_error(plist, pname, last_error);
        }
      }
      else {
        eprintf1("%s" ERRPREF "Invalid duplex capability: `", epref);
        errwrite(dev->memory,
                 (const char *)string_value.data,
                 sizeof(char)*string_value.size);
        eprintf("'.\n");
        last_error = gs_error_rangecheck;
        param_signal_error(plist, pname, last_error);
      }
    }
    else if (rc < 0) last_error = rc;
  }

  /* Check on "Duplex". This parameter is really read in gdev_prn_put_params(),
     but we check here to prevent it being set unless the printer really
     supports duplex printing. I would prefer to use the 'Duplex_set' variable
     for controlling the appearance of this page device parameter, but if I do,
     I can set the parameter only from PostScript and not from the command line.
  */
  {
    bool temp;
    if ((rc = param_read_bool(plist, (pname = "Duplex"), &temp)) == 0 &&
        temp && dev->duplex_capability == Duplex_none) {
      if (dev->printer == pcl3_generic_new || dev->printer == pcl3_generic_old)
        eprintf3("%s" ERRPREF
          "The '%s' device does not support duplex printing unless\n"
          "%s  'DuplexCapability' is not 'none'.\n",
          epref, find_subdevice_name(dev->printer), epref);
      else
        eprintf2("%s" ERRPREF
          "The %s does not support duplex printing.\n",
          epref, dev->eprn.cap->name);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
    /* NO check of rc because "null" is legal. */
  }

  /* Manual feed */
  {
    bool temp;
    if ((rc = param_read_bool(plist, (pname = "ManualFeed"), &temp)) == 0)
      dev->file_data.manual_feed = temp;
    else if (rc < 0) last_error = rc;
  }

  /* PCL media type */
  if ((rc = param_read_string(plist, (pname = "Medium"), &string_value)) == 0) {
    /*  We accept numerical and string values. Numerical values at present
        officially defined are 0-6, but not all printers know all these values.
        We give the user the benefit of the doubt, though, because the
        value is simply passed through to the printer, except for the older
        DeskJets where we map illegal values to "plain paper".
        If the user specifies a string, however, it must be a known one.
    */
    rc = get_int_for_string(&string_value, media_type_list, &temp);
    if (rc != 0) {
      if (rc != gs_error_VMerror) {
        eprintf1("%s" ERRPREF "Unknown medium: `", epref);
        errwrite(dev->memory,
                 (const char *)string_value.data,
                 sizeof(char)*string_value.size);
        eprintf("'.\n");
      }
      last_error = rc;
      param_signal_error(plist, pname, last_error);
    }
    else {
      new_quality = true;
      if (temp < 0 || 6 < temp)
        eprintf2("%s" WARNPREF "Unknown media type code: %d.\n",
          wpref, temp);
      pcl3_set_mediatype(data, temp);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Media destination */
  if ((rc = param_read_int(plist, (pname = "%MediaDestination"),
    &data->media_destination)) < 0) last_error = rc;

  /* Media source */
  if ((rc = param_read_int(plist, (pname = "%MediaSource"),
    &data->media_source)) < 0) last_error = rc;
  else if (rc == 0 && dev->is_open) gs_closedevice(device);
    /* In pcl3, %MediaSource is relevant for banner printing and hence page
       layout which is determined in the open routine. */

  /* Use of Configure Raster Data */
  if (is_generic_device(dev) || pcl_has_CRD(data->level)) {
    bool temp;

    if ((rc = param_read_bool(plist, (pname = "OnlyCRD"), &temp)) == 0) {
      if (pcl_has_CRD(data->level))
        data->level = (temp? pcl_level_3plus_CRD_only: pcl_level_3plus_S68);
      else if (temp == true) {
        eprintf1("%s" ERRPREF
          "OnlyCRD may be set only for group-3 devices.\n", epref);
        last_error = gs_error_rangecheck;
        param_signal_error(plist, pname, last_error);
      }
    }
    else if (rc < 0) last_error = rc;
  }

  /* PCL initialization string 1 */
  rc = fetch_octets(epref, plist, "PCLInit1", &data->init1);
  if (rc < 0) last_error = rc;

  /* PCL initialization string 2 */
  rc = fetch_octets(epref, plist, "PCLInit2", &data->init2);
  if (rc < 0) last_error = rc;

  /* PJL job name */
  rc = fetch_cstring(epref, plist, "PJLJob", &data->PJL_job);
  if (rc < 0) last_error = rc;

  /* PJL language */
  rc = fetch_cstring(epref, plist, "PJLLanguage", &data->PJL_language);
  if (rc < 0) last_error = rc;

  /* Print Quality */
  if ((rc = param_read_string(plist, (pname = "PrintQuality"), &string_value))
      == 0) {
    /*  The only known values are -1, 0 and 1. Again, however, we assume the
        user knows what s/he is doing if another value is given. */
    rc = get_int_for_string(&string_value, print_quality_list, &temp);
    if (rc != 0) {
      if (rc != gs_error_VMerror) {
        eprintf1("%s" ERRPREF "Unknown print quality: `", epref);
        errwrite(dev->memory,
                 (const char *)string_value.data,
                 sizeof(char)*string_value.size);
        eprintf("'.\n");
      }
      last_error = rc;
      param_signal_error(plist, pname, last_error);
    }
    else {
      new_quality = true;
      if (temp < -1 || 1 < temp)
        eprintf2("%s" WARNPREF "Unknown print quality: %d.\n",
          wpref, temp);
      pcl3_set_printquality(data, temp);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Raster Graphics Quality */
  if ((rc = param_read_null(plist, (pname = "RasterGraphicsQuality"))) == 0)
    ;   /* ignore */
  else if (rc  < 0 &&
      (rc = param_read_int(plist, (pname = "RasterGraphicsQuality"), &temp))
        == 0) {
    if (0 <= temp && temp <= 2) requested.quality = temp;
    else {
      eprintf2(
        "%s" ERRPREF "Invalid value for raster graphics quality: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Colorant order */
  {
    bool temp;
    if ((rc = param_read_bool(plist, (pname = "SendBlackLast"), &temp)) == 0)
      data->order_CMYK = temp;
    else if (rc < 0 ) last_error = rc;
  }

  /* Sending of NULs */
  if ((rc = param_read_int(plist, (pname = "SendNULs"), &temp)) == 0) {
    if (data->NULs_to_send >= 0) data->NULs_to_send = temp;
    else {
      eprintf2(
        "%s" ERRPREF "Invalid value for SendNULs parameter: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0 ) last_error = rc;

  /* Shingling */
  if ((rc = param_read_null(plist, (pname = "Shingling"))) == 0)
    ;   /* ignore */
  else if (rc  < 0 &&
      (rc = param_read_int(plist, pname, &temp)) == 0) {
    if (0 <= temp && temp <= 2) requested.shingling = temp;
    else {
      eprintf2("%s" ERRPREF "Invalid value for shingling: %d.\n",
        epref, temp);
      last_error = gs_error_rangecheck;
      param_signal_error(plist, pname, last_error);
    }
  }
  else if (rc < 0) last_error = rc;

  /* Tumble */
  if (is_generic_device(dev))
    if ((rc = param_read_bool(plist, (pname = "Tumble"), &dev->tumble)) < 0)
      last_error = rc;

  /* UseCard */
  {
    bool temp;

    if ((rc = param_read_null(plist, (pname = "UseCard"))) == 0)
      dev->use_card = bn_null;
    else if (rc < 0 &&
        (rc = param_read_bool(plist, pname, &temp)) == 0)
      dev->use_card = (temp? bn_true: bn_false);
    else if (rc < 0 ) last_error = rc;
  }

  /* Process parameters defined by base classes (should occur after treating
     parameters defined for the derived class, see gsparam.h) */
  rc = eprn_put_params(device, plist);
  if (rc < 0 || (rc > 0 && last_error >= 0))
    last_error = rc;

  /* Act if the colour model was changed */
  if (previous_colour_model != dev->eprn.colour_model) set_palette(dev);

  if (last_error < 0) return_error(last_error);

  /* If we have seen new quality parameters, derive the old ones from them
     based on the current and possibly new value of the palette. */
  if (new_quality) pcl3_set_oldquality(data);

  /* If we have seen old quality parameters, store them */
  if (pcl_use_oldquality(data->level)) {
    if (requested.depletion >= 0) data->depletion = requested.depletion;
    if (requested.quality >= 0)
      data->raster_graphics_quality = requested.quality;
    if (requested.shingling >= 0) data->shingling = requested.shingling;
  }

  return 0;
}

/******************************************************************************

  Function: pcl3_open_device

******************************************************************************/

static int pcl3_open_device(gx_device *device)
{
  pcl3_Device *dev = (pcl3_Device *)device;
  const char
    *epref = dev->eprn.CUPS_messages? CUPS_ERRPREF: "",
    *wpref = dev->eprn.CUPS_messages? CUPS_WARNPREF: "";
  int rc;

  /* Constructor */
  if (!dev->initialized) init(dev);

#ifdef PCL3_MEDIA_FILE
  /* Change default media descriptions for 'unspec' */
  if (dev->eprn.media_file == NULL && dev->printer == pcl3_generic_new) {
    if ((rc = eprn_set_media_data(device, PCL3_MEDIA_FILE, 0)) != 0)
      return rc;
  }
#endif

  /* Check on rendering parameters */
  if ((dev->eprn.black_levels > 2 || dev->eprn.non_black_levels > 2) &&
      dev->file_data.print_quality == -1)
    eprintf2(
      "%s" WARNPREF "More than 2 intensity levels and draft quality\n"
      "%s    are unlikely to work in combination.\n", wpref, wpref);

  /* Ensure correct media request flags */
  eprn_set_media_flags((eprn_Device *)dev,
    (dev->file_data.media_source == -1? MS_BIG_FLAG: ms_none) |
      (dev->use_card == bn_true? PCL_CARD_FLAG: ms_none),
    (dev->use_card == bn_null? card_is_optional: NULL));

  dev->eprn.soft_tumble = false;

  /* Open the "eprn" device part */
  if ((rc = eprn_open_device(device)) != 0) return rc;

  /* if device has been subclassed (FirstPage/LastPage device) then make sure we use
   * the subclassed device.
   */
  while (device->child)
      device = device->child;

  dev = (pcl3_Device *)device;

  /* Fill the still unassigned parts of 'file_data' from the other data */
  {
    pcl_FileData *data = &dev->file_data;
    unsigned int j;

    /* Media handling */
    data->size = pcl3_page_size(&dev->table, dev->eprn.code);
    if (data->size == pcl_ps_default) {
      /*  This is due to a media description using a media size code for which
          there is no PCL Page Size code. This is either an error in a builtin
          description or the user specified it in a media configuration file.
          Note that there might be a "Card" flag, hence we should not talk
          about "size" only.
      */
      char buffer[50];

      eprintf2("%s" ERRPREF
        "The current configuration for this driver has identified the\n"
        "%s  page setup requested by the document as being for `",
        epref, epref);
      if (ms_find_name_from_code(buffer, sizeof(buffer),
          dev->eprn.code, flag_description) == 0) eprintf1("%s", buffer);
      else eprintf("UNKNOWN");  /* should never happen */
      eprintf3("' (%.0f x %.0f bp).\n"
        "%s  The driver does not know how to do this in PCL.\n",
        dev->MediaSize[0], dev->MediaSize[1], epref);
      if (dev->eprn.media_file != NULL)
        eprintf2(
          "%s  You should therefore not include such an entry in the\n"
          "%s  media configuration file.\n", epref, epref);
      return_error(gs_error_rangecheck);
    }
    data->duplex = -1;
    if (dev->Duplex_set > 0) {  /* Duplex is not null */
      if (dev->Duplex) {
        bool same_leading_edge;

        /* Find direction of default user space y axis in device space */
        int orient = dev->eprn.default_orientation;
        if (dev->MediaSize[1] < dev->MediaSize[0]) /* landscape */
          orient++;     /* rotate +90 degrees */

        same_leading_edge = (orient % 2 == 0  /* y axis is vertical */) !=
          (dev->tumble != false);
          /* If there were a native 'bool' type in C, the last parenthesis
             could be reliably replaced by "dev->tumble". This is safer and
             just as fast, provided the compiler is sufficiently intelligent. */

        dev->eprn.soft_tumble = dev->duplex_capability != Duplex_both &&
            ((same_leading_edge &&
                dev->duplex_capability != Duplex_sameLeadingEdge) ||
              (!same_leading_edge &&
                dev->duplex_capability != Duplex_oppositeLeadingEdge));
        if (dev->eprn.soft_tumble) same_leading_edge = !same_leading_edge;

        /*  I am assuming here that the values 1 and 2, specified by HP in
            BPL02705 as meaning "Long-Edge Binding" and "Short-Edge Binding",
            respectively, in fact mean what I've called the "same leading edge"
            and "opposite leading edge" settings for the second pass. */
        if (same_leading_edge) data->duplex = 1;
        else data->duplex = 2;
      }
      else data->duplex = 0;                    /* simplex */
    }

    /* It is almost not necessary to set the palette here because the default
       settings of eprn and pcl3 agree and all other calls are routed through
       the put_params routines. But there is a special case: I want to use
       'pcl_no_palette' if the printer cannot switch palettes. */
    set_palette(dev);

    /* Per-colorant information */
    for (j = 0; j < data->number_of_colorants; j++) {
      data->colorant_array[j].hres = (int)(dev->HWResolution[0] + 0.5);
      data->colorant_array[j].vres = (int)(dev->HWResolution[1] + 0.5);
    }
    if (data->palette == pcl_CMY || data->palette == pcl_RGB)
      for (j = 0; j < 3; j++)
        data->colorant_array[j].levels = dev->eprn.non_black_levels;
    else {
      data->colorant_array[0].levels = dev->eprn.black_levels;
      for (j = 1; j < data->number_of_colorants; j++)
        data->colorant_array[j].levels = dev->eprn.non_black_levels;
    }
  }

  return rc;
}

/******************************************************************************

  Function: pcl3_close_device

******************************************************************************/

static int pcl3_close_device(gx_device *device)
{
  pcl3_Device *dev = (pcl3_Device *)device;

  /*  HP recommends that a driver should send the Printer Reset command at the
      end of each print job in order to leave the printer in its default state.
      This is a matter of courtesy for the next print job which could otherwise
      inherit some of the properties set for the present job unless it starts
      with a Printer Reset command itself (every job generated with this driver
      does).

      Unfortunately, ghostscript does not have a corresponding device procedure.
      In particular, the 'close_device' procedure may be called multiple times
      during a job and for multi-file output it is even only called at the end
      of the sequence of files and then when 'dev->file' is already NULL.
      Hence this routine tries to get close by checking the 'configured' field:
      it is set if the pcl3_init_file() function has been called and therefore
      indicates that the driver has sent configuration commands to the printer.
      That part we can and should take back.

      Of course one might reset the printer at the end of every page, but this
      would entail having to repeat the initialization at the beginning of
      every page. I regard this as logically inappropriate.
  */

  if (dev->configured && dev->file != NULL) {
    pcl3_end_file(dev->file, &dev->file_data);
    dev->configured = false;
  }

  return eprn_close_device(device);
}

/******************************************************************************

  Function: pcl3_print_page

  This is the implementation of prn's print_page() method for this device.

  It initializes the printer if necessary and prints the page.

******************************************************************************/

/* Function to convert return codes from calls to pclgen routines. */
static int convert(int code)
{
    if (code > 0)   return gs_error_Fatal;
    if (code < 0)   return gs_error_ioerror;
    return 0;
}

static int pcl3_print_page(gx_device_printer *device, gp_file *out)
{
  int
    blank_lines,
    rc = 0;
  pcl3_Device *dev = (pcl3_Device *)device;
  const char *epref = dev->eprn.CUPS_messages? CUPS_ERRPREF: "";
  pcl_RasterData rd;
  unsigned int
    j,
    *lengths,
    planes;

  /* Make sure out cleanup code will cope with partially-initialised data. */
  memset(&rd, 0, sizeof(pcl_RasterData)); /* Belt and braces. */
  planes = 0;
  lengths = NULL;
  rd.next = NULL;
  rd.previous = NULL;
  rd.workspace[0] = NULL;
  rd.workspace[1] = NULL;

  /* If this is a new file or we've decided to re-configure, initialize the
     printer first */
  if (gdev_prn_file_is_new(device) || !dev->configured ||
      dev->configure_every_page) {
    rc = convert(pcl3_init_file(device->memory, out, &dev->file_data));
    if (rc) {
        (void) gs_note_error(rc);
        goto end;
    }
    dev->configured = true;
  }

  /* Initialize raster data structure */
  rd.global = &dev->file_data;
  planes = eprn_number_of_bitplanes((eprn_Device *)dev);
  lengths = (unsigned int *)malloc(planes*sizeof(unsigned int));
  if (!lengths) {
    rc = gs_note_error(gs_error_VMerror);
    goto end;
  }
  eprn_number_of_octets((eprn_Device *)dev, lengths);
  rd.next = (pcl_OctetString *)malloc(planes*sizeof(pcl_OctetString));
  if (!rd.next) {
    rc = gs_note_error(gs_error_VMerror);
    goto end;
  }
  for (j=0; j<planes; j++) { /* Make sure we can free at any point. */
    rd.next[j].str = NULL;
  }
  if (pcl_cm_is_differential(dev->file_data.compression)) {
    rd.previous = (pcl_OctetString *)malloc(planes*sizeof(pcl_OctetString));
    if (!rd.previous) {
      rc = gs_note_error(gs_error_VMerror);
      goto end;
    }
    for (j=0; j<planes; j++) { /* Make sure we can free at any point. */
      rd.previous[j].str = NULL;
    }
    for (j = 0; j < planes; j++) {
      rd.previous[j].str = (pcl_Octet *)malloc(lengths[j]*sizeof(eprn_Octet));
      if (!rd.previous[j].str) {
        rc = gs_note_error(gs_error_VMerror);
        goto end;
      }
    }
  }
  rd.width = 8*lengths[0];      /* all colorants have equal resolution */
  for (j = 0; j < planes; j++) {
    rd.next[j].str = (pcl_Octet *)malloc(lengths[j]*sizeof(eprn_Octet));
    if (!rd.next[j].str) {
      rc = gs_note_error(gs_error_VMerror);
      goto end;
    }
  }
    /* Note: 'pcl_Octet' must be identical with 'eprn_Octet'. */
  rd.workspace_allocated = lengths[0];
  for (j = 1; j < planes; j++)
    if (lengths[j] > rd.workspace_allocated)
      rd.workspace_allocated = lengths[j];
  for (j = 0;
      j < 2 && (j != 1 || dev->file_data.compression == pcl_cm_delta); j++) {
    rd.workspace[j] =
      (pcl_Octet *)malloc(rd.workspace_allocated*sizeof(pcl_Octet));
    if (!rd.workspace[j]) {
      rc = gs_note_error(gs_error_VMerror);
      goto end;
    }
  }

  /* Open the page and start raster mode */
  rc = convert(pcl3_begin_page(out, &dev->file_data));
  if (rc) {
    (void) gs_note_error(rc);
    goto end;
  }
  rc = convert(pcl3_begin_raster(out, &rd));
  if (rc) {
    (void) gs_note_error(rc);
    goto end;
  }

  /* Loop over scan lines */
  blank_lines = 0;
  while (eprn_get_planes((eprn_Device *)dev, (eprn_OctetString *)rd.next) == 0){
    /* Is this a blank (white) line? */
    if (dev->eprn.colour_model == eprn_DeviceRGB) {
      /*  White results if all three colorants use their highest intensity.
          Fortunately, PCL-3+ can only support two intensity levels for all
          colorants in an RGB palette, hence this intensity must be one for all
          colorants simultaneously.
          Because the planes returned by eprn_get_planes() are guaranteed to
          have no trailing zero octets, we can easily check that they are of
          equal length before proceeding further.
      */
      for (j = 1; j < planes && rd.next[j].length == rd.next[0].length; j++);
      if (j >= planes && rd.next[0].length == lengths[0]) {
        int k;
        /* All planes have the same length and cover the whole width of the
           page. Check that they all contain 0xFF. */
        j = 0;
        do {
          k = rd.next[j].length - 1;
          while (k > 0 && rd.next[j].str[k] == 0xFF) k--;
        } while (k == 0 && rd.next[j].str[0] == 0xFF && ++j < planes);
      }
    }
    else
      /* White is zero */
      for (j = 0; j < planes && rd.next[j].length == 0; j++);

    if (j == planes) blank_lines++;
    else {
      if (blank_lines > 0) {
        rc = convert(pcl3_skip_groups(out, &rd, blank_lines));
        if (rc) {
          (void) gs_note_error(rc);
          goto end;
        }
        blank_lines = 0;
      }
      rc = convert(pcl3_transfer_group(out, &rd));
      if (rc) {
        (void) gs_note_error(rc);
        goto end;
      }
    }
  }

  /* Terminate raster mode and close the page */
  rc = convert(pcl3_end_raster(out, &rd));
  if (rc) {
    (void) gs_note_error(rc);
    goto end;
  }
  rc = convert(pcl3_end_page(out, &dev->file_data));
  if (rc) {
    (void) gs_note_error(rc);
    goto end;
  }

end:
  /* Free dynamic storage */
  if (rd.next) {
    for (j = 0; j < planes; j++) {
      free(rd.next[j].str);
    }
  }
  if (rd.previous) {
    for (j = 0; j < planes; j++) {
      free(rd.previous[j].str);
    }
  }
  for (j = 0; j < 2; j++) {
    free(rd.workspace[j]);
  }
  free(lengths);
  free(rd.next);
  free(rd.previous);

  if (rc == gs_error_VMerror) {
    eprintf1("%s" ERRPREF "Memory allocation failure from malloc().\n",
      epref);
  }

  return rc;
}
