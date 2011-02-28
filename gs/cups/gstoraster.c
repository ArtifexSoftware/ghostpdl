/* -*- Mode: C; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 8 -*-

Copyright (c) 2008, Till Kamppeter
Copyright (c) 2011, Tim Waugh
Copyright (c) 2011, Richard Hughes

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

MIT Open Source License  -  http://www.opensource.org/

*/

/* $Id$ */

/* PS/PDF to CUPS Raster filter based on Ghostscript */

#include <stdio.h>
#include <stdlib.h>
#include <cups/cups.h>
#include <stdarg.h>
#include <fcntl.h>
#include <cups/raster.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "colord.h"

#define PDF_MAX_CHECK_COMMENT_LINES	20

#ifndef GS
#define GS "gs"
#endif
#ifndef BINDIR
#define BINDIR "/usr/bin"
#endif
#ifndef CUPS_FONTPATH
#define CUPS_FONTPATH "/usr/share/cups/fonts"
#endif
#ifndef CUPSDATA
#define CUPSDATA "/usr/share/cups"
#endif

typedef enum {
  GS_DOC_TYPE_PDF,
  GS_DOC_TYPE_PS,
  GS_DOC_TYPE_UNKNOWN
} GsDocType;

#ifdef CUPS_RASTER_SYNCv1
typedef cups_page_header2_t gs_page_header;
#else
typedef cups_page_header_t gs_page_header;
#endif /* CUPS_RASTER_SYNCv1 */

static GsDocType
parse_doc_type(FILE *fp)
{
  char buf[5];
  GsDocType doc_type;
  char *rc;

  /* get the first few bytes of the file */
  doc_type = GS_DOC_TYPE_UNKNOWN;
  rewind(fp);
  rc = fgets(buf,sizeof(buf),fp);
  if (rc == NULL)
    goto out;

  /* is PDF */
  if (strncmp(buf,"%PDF",4) == 0) {
    doc_type = GS_DOC_TYPE_PDF;
    goto out;
  }

  /* is PS */
  if (strncmp(buf,"%!",2) == 0) {
    doc_type = GS_DOC_TYPE_PS;
    goto out;
  }
out:
  return doc_type;
}

static void
parse_pdf_header_options(FILE *fp, gs_page_header *h)
{
  char buf[4096];
  int i;

  rewind(fp);
  /* skip until PDF start header */
  while (fgets(buf,sizeof(buf),fp) != 0) {
    if (strncmp(buf,"%PDF",4) == 0) {
      break;
    }
  }
  for (i = 0;i < PDF_MAX_CHECK_COMMENT_LINES;i++) {
    if (fgets(buf,sizeof(buf),fp) == 0) break;
    if (strncmp(buf,"%%PDFTOPDFNumCopies",19) == 0) {
      char *p;

      p = strchr(buf+19,':');
      h->NumCopies = atoi(p+1);
    } else if (strncmp(buf,"%%PDFTOPDFCollate",17) == 0) {
      char *p;

      p = strchr(buf+17,':');
      while (*p == ' ' || *p == '\t') p++;
      if (strncasecmp(p,"true",4) == 0) {
        h->Collate = CUPS_TRUE;
      } else {
        h->Collate = CUPS_FALSE;
      }
    }
  }
}

static void
add_pdf_header_options(gs_page_header *h, cups_array_t *gs_args)
{
  int i;
  char tmpstr[1024];

  /* Simple boolean, enumerated choice, numerical, and string parameters */
  if (h->MediaClass[0] |= '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-sMediaClass=%s", h->MediaClass);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->MediaColor[0] |= '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-sMediaColor=%s", h->MediaColor);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->MediaType[0] |= '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-sMediaType=%s", h->MediaType);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->OutputType[0] |= '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-sOutputType=%s", h->OutputType);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->AdvanceDistance) {
    snprintf(tmpstr, sizeof(tmpstr), "-dAdvanceDistance=%d",
	     (unsigned)(h->AdvanceDistance));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->AdvanceMedia) {
    snprintf(tmpstr, sizeof(tmpstr), "-dAdvanceMedia=%d",
	     (unsigned)(h->AdvanceMedia));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->Collate) {
    cupsArrayAdd(gs_args, strdup("-dCollate"));
  }
  if (h->CutMedia) {
    snprintf(tmpstr, sizeof(tmpstr), "-dCutMedia=%d",
	     (unsigned)(h->CutMedia));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->Duplex) {
    cupsArrayAdd(gs_args, strdup("-dDuplex"));
  }
  if ((h->HWResolution[0] != 100) || (h->HWResolution[1] != 100))
    snprintf(tmpstr, sizeof(tmpstr), "-r%dx%d",
	     (unsigned)(h->HWResolution[0]), (unsigned)(h->HWResolution[1]));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-r100x100");
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->InsertSheet) {
    cupsArrayAdd(gs_args, strdup("-dInsertSheet"));
  }
  if (h->Jog) {
    snprintf(tmpstr, sizeof(tmpstr), "-dJog=%d",
	     (unsigned)(h->Jog));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->LeadingEdge) {
    snprintf(tmpstr, sizeof(tmpstr), "-dLeadingEdge=%d",
	     (unsigned)(h->LeadingEdge));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->ManualFeed) {
    cupsArrayAdd(gs_args, strdup("-dManualFeed"));
  }
  if (h->MediaPosition) {
    snprintf(tmpstr, sizeof(tmpstr), "-dMediaPosition=%d",
	     (unsigned)(h->MediaPosition));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->MediaWeight) {
    snprintf(tmpstr, sizeof(tmpstr), "-dMediaWeight=%d",
	     (unsigned)(h->MediaWeight));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->MirrorPrint) {
    cupsArrayAdd(gs_args, strdup("-dMirrorPrint"));
  }
  if (h->NegativePrint) {
    cupsArrayAdd(gs_args, strdup("-dNegativePrint"));
  }
  if (h->NumCopies != 1) {
    snprintf(tmpstr, sizeof(tmpstr), "-dNumCopies=%d",
	     (unsigned)(h->NumCopies));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->Orientation) {
    snprintf(tmpstr, sizeof(tmpstr), "-dOrientation=%d",
	     (unsigned)(h->Orientation));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->OutputFaceUp) {
    cupsArrayAdd(gs_args, strdup("-dOutputFaceUp"));
  }
  if (h->PageSize[0] != 612)
    snprintf(tmpstr, sizeof(tmpstr), "-dDEVICEWIDTHPOINTS=%d",
	     (unsigned)(h->PageSize[0]));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-dDEVICEWIDTHPOINTS=612");
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->PageSize[1] != 792)
    snprintf(tmpstr, sizeof(tmpstr), "-dDEVICEHEIGHTPOINTS=%d",
	     (unsigned)(h->PageSize[1]));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-dDEVICEHEIGHTPOINTS=792");
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->Separations) {
    cupsArrayAdd(gs_args, strdup("-dSeparations"));
  }
  if (h->TraySwitch) {
    cupsArrayAdd(gs_args, strdup("-dTraySwitch"));
  }
  if (h->Tumble) {
    cupsArrayAdd(gs_args, strdup("-dTumble"));
  }
  if (h->cupsMediaType) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsMediaType=%d",
	     (unsigned)(h->cupsMediaType));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsBitsPerColor != 1)
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsBitsPerColor=%d",
	     (unsigned)(h->cupsBitsPerColor));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsBitsPerColor=1");
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->cupsColorOrder != CUPS_ORDER_CHUNKED)
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsColorOrder=%d",
	     (unsigned)(h->cupsColorOrder));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsColorOrder=%d",
	     CUPS_ORDER_CHUNKED);
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->cupsColorSpace != CUPS_CSPACE_K)
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsColorSpace=%d",
	     (unsigned)(h->cupsColorSpace));
  else
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsColorSpace=%d",
	     CUPS_CSPACE_K);
  cupsArrayAdd(gs_args, strdup(tmpstr));
  if (h->cupsCompression) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsCompression=%d",
	     (unsigned)(h->cupsCompression));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsRowCount) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsRowCount=%d",
	     (unsigned)(h->cupsRowCount));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsRowFeed) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsRowFeed=%d",
	     (unsigned)(h->cupsRowFeed));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsRowStep) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsRowStep=%d",
	     (unsigned)(h->cupsRowStep));
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
#ifdef CUPS_RASTER_SYNCv1
  if (h->cupsBorderlessScalingFactor != 1.0f) {
    snprintf(tmpstr, sizeof(tmpstr), "-dcupsBorderlessScalingFactor=%.4f",
	     h->cupsBorderlessScalingFactor);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  for (i=0; i <= 15; i ++)
    if (h->cupsInteger[i]) {
      snprintf(tmpstr, sizeof(tmpstr), "-dcupsInteger%d=%d",
	       i, (unsigned)(h->cupsInteger[i]));
      cupsArrayAdd(gs_args, strdup(tmpstr));
    }
  for (i=0; i <= 15; i ++)
    if (h->cupsReal[i]) {
      snprintf(tmpstr, sizeof(tmpstr), "-dcupsReal%d=%.4f",
	       i, h->cupsReal[i]);
      cupsArrayAdd(gs_args, strdup(tmpstr));
    }
  for (i=0; i <= 15; i ++)
    if (h->cupsString[i][0] != '\0') {
      snprintf(tmpstr, sizeof(tmpstr), "-scupsString%d=%s",
	       i, h->cupsString[i]);
      cupsArrayAdd(gs_args, strdup(tmpstr));
    }
  if (h->cupsMarkerType[0] != '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-scupsMarkerType=%s",
	     h->cupsMarkerType);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsRenderingIntent[0] != '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-scupsRenderingIntent=%s",
	     h->cupsRenderingIntent);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
  if (h->cupsPageSizeName[0] != '\0') {
    snprintf(tmpstr, sizeof(tmpstr), "-scupsPageSizeName=%s",
	     h->cupsPageSizeName);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }
#endif /* CUPS_RASTER_SYNCv1 */
}

static int
gs_spawn (const char *filename,
          cups_array_t *gs_args,
          char **envp,
          FILE *fp)
{
  char *argument;
  char buf[BUFSIZ];
  char **gsargv;
  const char* apos;
  int fds[2];
  int i;
  int n;
  int numargs;
  int pid;
  int status = 1;

  /* Put Ghostscript command line argument into an array for the "exec()"
     call */
  numargs = cupsArrayCount(gs_args);
  gsargv = calloc(numargs + 1, sizeof(char *));
  for (argument = (char *)cupsArrayFirst(gs_args), i = 0; argument;
       argument = (char *)cupsArrayNext(gs_args), i++) {
    gsargv[i] = argument;
  }
  gsargv[i] = NULL;

  /* Debug output: Full Ghostscript command line and environment variables */
  fprintf(stderr, "DEBUG: Ghostscript command line:");
  for (i = 0; gsargv[i]; i ++) {
    if ((strchr(gsargv[i],' ')) || (strchr(gsargv[i],'\t')))
      apos = "'";
    else
      apos = "";
    fprintf(stderr, " %s%s%s", apos, gsargv[i], apos);
  }
  fprintf(stderr, "\n");

  for (i = 0; envp[i]; i ++)
    fprintf(stderr, "DEBUG: envp[%d]=\"%s\"\n", i, envp[i]);

  /* Create a pipe for feeding the job into Ghostscript */
  if (pipe(fds))
  {
    fds[0] = -1;
    fds[1] = -1;
    fprintf(stderr, "ERROR: Unable to establish pipe for Ghostscript call");
    goto out;
  }

  /* Set the "close on exec" flag on each end of the pipe... */
  if (fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);
    fds[0] = -1;
    fds[1] = -1;
    fprintf(stderr, "ERROR: Unable to set \"close on exec\" flag on read end of the pipe for Ghostscript call");
    goto out;
  }
  if (fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC))
  {
    close(fds[0]);
    close(fds[1]);
    fprintf(stderr, "ERROR: Unable to set \"close on exec\" flag on write end of the pipe for Ghostscript call");
    goto out;
  }

  if ((pid = fork()) == 0)
  {
    /* Couple pipe with STDIN of Ghostscript process */
    if (fds[0] != 0) {
      close(0);
      if (fds[0] > 0)
        dup(fds[0]);
      else {
        fprintf(stderr, "ERROR: Unable to couple pipe with STDIN of Ghostscript process");
        goto out;
      }
    }

    /* Execute Ghostscript command line ... */
    execve(filename, gsargv, envp);
    perror(filename);
    goto out;
  }

  /* Feed job data into Ghostscript */
  while ((n = fread(buf, 1, BUFSIZ, fp)) > 0) {
    if (write(fds[1], buf, n) != n) {
      fprintf(stderr, "ERROR: Can't feed job data into Ghostscript");
      goto out;
    }
  }
  close (fds[1]);

  if (waitpid (pid, &status, 0) == -1) {
    perror ("gs");
    goto out;
  }
out:
  free(gsargv);
  return status;
}

static char *
get_ppd_icc_fallback (ppd_file_t *ppd, char **qualifier)
{
  char full_path[1024];
  char *icc_profile = NULL;
  char qualifer_tmp[1024];
  const char *profile_key;
  ppd_attr_t *attr;

  /* get profile attr, falling back to CUPS */
  profile_key = "APTiogaProfile";
  attr = ppdFindAttr(ppd, profile_key, NULL);
  if (attr == NULL) {
    profile_key = "cupsICCProfile";
    attr = ppdFindAttr(ppd, profile_key, NULL);
  }

  /* create a string for a quick comparion */
  snprintf(qualifer_tmp, sizeof(qualifer_tmp),
           "%s.%s.%s",
           qualifier[0],
           qualifier[1],
           qualifier[2]);

  /* neither */
  if (attr == NULL) {
    fprintf(stderr, "INFO: no profiles specified in PPD\n");
    goto out;
  }

  /* try to find a profile that matches the qualifier exactly */
  for (;attr != NULL; attr = ppdFindNextAttr(ppd, profile_key, NULL)) {
    fprintf(stderr, "INFO: found profile %s in PPD with qualifier '%s'\n",
            attr->value, attr->spec);

    /* invalid entry */
    if (attr->spec == NULL || attr->value == NULL)
      continue;

    /* expand to a full path if not already specified */
    if (attr->value[0] != '/')
      snprintf(full_path, sizeof(full_path),
               "%s/profiles/%s", CUPSDATA, attr->value);
    else
      strncpy(full_path, attr->value, sizeof(full_path));

    /* check the file exists */
    if (access(full_path, 0)) {
      fprintf(stderr, "INFO: found profile %s in PPD that does not exist\n",
              full_path);
      continue;
    }

    /* matches the qualifier */
    if (strcmp(qualifer_tmp, attr->spec) == 0) {
      icc_profile = strdup(full_path);
      goto out;
    }
  }

  /* no match */
  if (attr == NULL) {
    fprintf(stderr, "INFO: no profiles in PPD for qualifier '%s'\n",
            qualifer_tmp);
    goto out;
  }

out:
  return icc_profile;
}

int
main (int argc, char **argv, char *envp[])
{
  char buf[BUFSIZ];
  char *icc_profile = NULL;
  char **qualifier = NULL;
  char *tmp;
  char tmpstr[1024];
  const char *t = NULL;
  cups_array_t *gs_args = NULL;
  cups_option_t *options = NULL;
  FILE *fp = NULL;
  GsDocType doc_type;
  gs_page_header h;
  int fd;
  int i;
  int n;
  int num_options;
  int status = 1;
  ppd_file_t *ppd = NULL;

  if (argc < 6 || argc > 7) {
    fprintf(stderr, "ERROR: %s job-id user title copies options [file]\n",
      argv[0]);
    goto out;
  }

  num_options = cupsParseOptions(argv[5], 0, &options);

  t = getenv("PPD");
  if ((ppd = ppdOpenFile(t)) == NULL) {
    fprintf(stderr, "ERROR: Failed to open PPD: %s", t);
    goto out;
  }

  ppdMarkDefaults (ppd);
  cupsMarkOptions (ppd, num_options, options);

  if (argc == 6) {
    /* stdin */

    fd = cupsTempFd(buf,BUFSIZ);
    if (fd < 0) {
      fprintf(stderr, "ERROR: Can't create temporary file");
      goto out;
    }
    /* remove name */
    unlink(buf);

    /* copy stdin to the tmp file */
    while ((n = read(0,buf,BUFSIZ)) > 0) {
      if (write(fd,buf,n) != n) {
        fprintf(stderr, "ERROR: Can't copy stdin to temporary file");
        close(fd);
        goto out;
      }
    }
    if (lseek(fd,0,SEEK_SET) < 0) {
        fprintf(stderr, "ERROR: Can't rewind temporary file");
        close(fd);
        goto out;
    }

    if ((fp = fdopen(fd,"rb")) == 0) {
        fprintf(stderr, "ERROR: Can't fdopen temporary file");
        close(fd);
        goto out;
    }
  } else {
    /* argc == 7 filename is specified */

    if ((fp = fopen(argv[6],"rb")) == 0) {
        fprintf(stderr, "ERROR: Can't open input file %s",argv[6]);
        goto out;
    }
  }

  /* find out file type */
  doc_type = parse_doc_type(fp);
  if (doc_type == GS_DOC_TYPE_UNKNOWN) {
    fprintf(stderr, "ERROR: Can't detect file type");
    goto out;
  }

  qualifier = colord_get_qualifier_for_ppd (ppd);
  if (qualifier != NULL) {

    fprintf(stderr, "DEBUG: PPD uses qualifier '%s.%s.%s'\n",
            qualifier[0], qualifier[1], qualifier[2]);
    icc_profile = colord_get_profile_for_device_id (getenv("PRINTER"),
                                                    (const char**) qualifier);

    /* fall back to the PPD */
    if (icc_profile == NULL)
      icc_profile = get_ppd_icc_fallback (ppd, qualifier);

    if(icc_profile != NULL)
      fprintf(stderr, "DEBUG: Using ICC Profile '%s'\n", icc_profile);
  }

  /* Ghostscript parameters */
  gs_args = cupsArrayNew(NULL, NULL);
  if (!gs_args)
  {
    fprintf(stderr, "ERROR: Unable to allocate memory for Ghostscript arguments array\n");
    exit(1);
  }

  /* Part of Ghostscript command line which is not dependent on the job and/or
     the driver */
  snprintf(tmpstr, sizeof(tmpstr), "%s/%s", BINDIR, GS);
  cupsArrayAdd(gs_args, strdup(tmpstr));
  cupsArrayAdd(gs_args, strdup("-dQUIET"));
  /*cupsArrayAdd(gs_args, strdup("-dDEBUG"));*/
  cupsArrayAdd(gs_args, strdup("-dPARANOIDSAFER"));
  cupsArrayAdd(gs_args, strdup("-dNOPAUSE"));
  cupsArrayAdd(gs_args, strdup("-dBATCH"));
  if (doc_type == GS_DOC_TYPE_PS)
    cupsArrayAdd(gs_args, strdup("-dNOMEDIAATTRS"));
  cupsArrayAdd(gs_args, strdup("-sDEVICE=cups"));
  cupsArrayAdd(gs_args, strdup("-sstdout=%stderr"));
  cupsArrayAdd(gs_args, strdup("-sOutputFile=%stdout"));

  /* setPDF specific options */
  if (doc_type == GS_DOC_TYPE_PDF) {
    cupsRasterInterpretPPD(&h,ppd,num_options,options,0);
    parse_pdf_header_options(fp, &h);

    /* fixed other values that pdftopdf handles */
    h.MirrorPrint = CUPS_FALSE;
    h.Orientation = CUPS_ORIENT_0;

    /* get all the data from the header and pass it to ghostscript */
    add_pdf_header_options (&h, gs_args);
  }

  /* CUPS font path */
  if ((t = getenv("CUPS_FONTPATH")) == NULL)
    t = CUPS_FONTPATH;
  snprintf(tmpstr, sizeof(tmpstr), "-I%s", t);
  cupsArrayAdd(gs_args, strdup(tmpstr));

  /* set the device output ICC profile */
  if(icc_profile != NULL) {
    snprintf(tmpstr, sizeof(tmpstr), "-sOutputICCProfile=%s", icc_profile);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }

  /* Switch to taking PostScript commands on the Ghostscript command line */
  cupsArrayAdd(gs_args, strdup("-c"));

  if ((t = cupsGetOption("profile", num_options, options)) != NULL) {
    snprintf(tmpstr, sizeof(tmpstr), "<</cupsProfile(%s)>>setpagedevice", t);
    cupsArrayAdd(gs_args, strdup(tmpstr));
  }

  /* Mark the end of PostScript commands supplied on the Ghostscript command
     line (with the "-c" option), so that we can supply the input file name */
  cupsArrayAdd(gs_args, strdup("-f"));

  /* Let Ghostscript read from STDIN */
  cupsArrayAdd(gs_args, strdup("-_"));

  /* Execute Ghostscript command line ... */
  snprintf(tmpstr, sizeof(tmpstr), "%s/%s", BINDIR, GS);

  /* call Ghostscript */
  rewind(fp);
  status = gs_spawn (tmpstr, gs_args, envp, fp);
out:
  if (fp)
    fclose(fp);
  if (qualifier != NULL) {
    for (i=0; qualifier[i] != NULL; i++)
      free(qualifier[i]);
    free(qualifier);
  }
  if (gs_args) {
    while ((tmp = cupsArrayFirst(gs_args)) != NULL) {
      cupsArrayRemove(gs_args,tmp);
      free(tmp);
    }
    cupsArrayDelete(gs_args);
  }
  free(icc_profile);
  if (ppd)
    ppdClose(ppd);
  return status;
}
