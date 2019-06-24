/* COPYRIGHT (C) 1990, 1992 Aladdin Enterprises.  All rights reserved.
   Distributed by Free Software Foundation, Inc.

   This file is part of Ghostscript.

   Ghostscript is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the Ghostscript General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   Ghostscript, but only under the conditions described in the Ghostscript
   General Public License.  A copy of this license is supposed to have been
   given to you along with Ghostscript so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.  */

/* dviprlib.c */

#include "stdio_.h"
#include "ctype_.h"
#include "malloc_.h"
#include "string_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gp.h"

/* include library header. */
#define dviprlib_implementation
#include "dviprlib.h"

/* The remainder of this file is a copy of the library for dviprt. */

/***** From rcfg.c *****/
/* $Id: RCFG.C 1.1 1994/08/30 02:27:02 kaz Exp kaz $ */

/*--- forward declarations ---*/
static int dviprt_read_S_cfg(dviprt_cfg_t *,dviprt_cfg_i *);
static int dviprt_read_QR_cfg(dviprt_cfg_t *,dviprt_cfg_i *);

/*--- library functions ---*/
int
dviprt_readcfg(const gs_memory_t *mem, char *ifname,dviprt_cfg_t *pcfg,uchar *pcodebuf,int codebuf_s,
    uchar *pworkbuf,int workbuf_s)
{
  int code;
  int ver;
  dviprt_cfg_i info;

  info.fname = ifname;
  info.line_no = -1;
  if (ifname) {
    info.file = gp_fopen(mem,ifname,gp_fmode_rb);
    if (info.file == NULL) {
      dviprt_printcfgerror(&info,"Cannot open.\n",-1);
      return CFG_ERROR_FILE_OPEN;
    }
  }
  else {
    /* Broken, but unused in gs. Just return an error for now. */
    /* info.file = stdin; */
    return CFG_ERROR_FILE_OPEN;
  }

  gp_fseek(info.file,16,0);
  ver = gp_fgetc(info.file);
  gp_fseek(info.file,0,0);
  info.codebuf = pcodebuf;
  info.readbuf = pworkbuf;
  info.codebuf_size = codebuf_s;
  info.readbuf_size = workbuf_s;
  code = (ver == 'S') ? dviprt_read_S_cfg(pcfg,&info)
    : dviprt_read_QR_cfg(pcfg,&info);

  if (ifname) gp_fclose(info.file);
  return code;
}

/*--- internal routines ---*/
static int
dviprt_read_S_cfg(dviprt_cfg_t *pcfg,dviprt_cfg_i *pinfo)
{
  gp_file *ifp;
  long intoff,stroff,codeoff;
  int i,count;
  uchar *pbuf,*rbuf;
  int code;
  char *ptype;
  int n;

  if ((code = dviprt_setcfgbuffer_(pinfo,100,0)) < 0) return code;
  dviprt_initcfg_(pcfg,pinfo);

  ifp = pinfo->file;
  rbuf = pinfo->readbuf;

  if (gp_fread(rbuf,20,1,ifp) < 1) {
    dviprt_printcfgerror(pinfo,"Read error.\n",-1);
  }
  if (rbuf[17] != 0xff || rbuf[18] != 0xff) {
  not_cfg:
    dviprt_printcfgerror(pinfo,"This file does not seem *.CFG.\n",-1);
    return CFG_ERROR_OTHER;
  }
  if (memcmp(rbuf,CFG_MAGIC_NUMBER,2))
    goto not_cfg;
  pcfg->version = rbuf[2] | ((uint)rbuf[3] << 8);
  if (pcfg->version > CFG_VERSION) {
    gs_sprintf(dviprt_message_buffer,
            "This *.CFG file is too new version(ver.%u).\n",pcfg->version);
    dviprt_printcfgerror(pinfo,dviprt_message_buffer,-1);
    return CFG_ERROR_OTHER;
  }

#define bytes2long(p) ((p)[0] | ((long)(p)[1]<<8) | \
                       ((long)(p)[2]<<16) | ((long)(p)[3]<<24))
  intoff = bytes2long(rbuf+4);
  stroff = bytes2long(rbuf+8);
  codeoff = bytes2long(rbuf+12);
#undef bytes2long

  gp_fseek(ifp,intoff,0);
  count = gp_fgetc(ifp);
  gp_fread(rbuf,count*3,1,ifp);

  pbuf = rbuf;
  for (i=0;i<count;i++) {
    n = pbuf[0];
    if (n >= CFG_INTEGER_TYPE_COUNT) {
      ptype = "integer";
    unknown_no:
      gs_sprintf(dviprt_message_buffer,
              "Unknown %s type value No.%d is found.\n",ptype,n);
      dviprt_printcfgerror(pinfo,dviprt_message_buffer,-1);
      return CFG_ERROR_OTHER;
    }
    pcfg->integer[n] = pbuf[1] | ((uint)pbuf[2]<<8);
    pbuf += 3;
  }

  gp_fseek(ifp,stroff,0);
  count = gp_fgetc(ifp);
  pbuf = NULL;
  for (i=0;i<count;i++) {
    int l;
    gp_fread(rbuf,3,1,ifp);
    n = rbuf[0];
    l = rbuf[1] | ((uint)rbuf[2]<<8);
    if (n >= CFG_STRINGS_TYPE_COUNT) {
      ptype = "strings";
      goto unknown_no;
    }
    if (pinfo->codebuf == NULL) {
      pcfg->strings[n] = (uchar *)malloc(l+1);
      if (pcfg->strings[n] == NULL) {
      no_memory:
        dviprt_printcfgerror(pinfo,"Memory exhausted.\n",-1);
        return CFG_ERROR_MEMORY;
      }
    }
    else {
      pcfg->strings[n] = pinfo->pcodebuf;
      pinfo->pcodebuf += (l+1);
    }
    gp_fread(pcfg->strings[n],l,1,ifp);
    *(pcfg->strings[n]+l) = 0;
  }

  gp_fseek(ifp,codeoff,0);
  count = gp_fgetc(ifp);
  for (i=0;i<count;i++) {
    int l;
    gp_fread(rbuf,3,1,ifp);
    n = rbuf[0];
    l = rbuf[1] | ((uint)rbuf[2]<<8);

    if (n >= CFG_PRTCODE_TYPE_COUNT) {
      ptype = "printer code";
      goto unknown_no;
    }
    if (pinfo->codebuf == NULL) {
      pcfg->prtcode[n] = (uchar *)malloc(l+1);
      if (pcfg->prtcode[n] == NULL)
        goto no_memory;
    }
    else {
      pcfg->prtcode[n] = pinfo->pcodebuf;
      pinfo->pcodebuf += (l+1);
    }
    gp_fread(pcfg->prtcode[n],l,1,ifp);
    *(pcfg->prtcode[n]+l) = 0;
    pcfg->prtcode_size[n] = l;
  }
  dviprt_resetcfgbuffer_(pinfo);
  return 0;
}

static int
dviprt_read_QR_cfg(dviprt_cfg_t *pcfg,dviprt_cfg_i *pinfo)
{
#define	TYPE_BIT		0xc0

#define	NO_NUM			0
#define	BINARY_LTOH		1
#define	BINARY_HTOL		2
#define	DECIMAL_3		3
#define	DECIMAL_4		4
#define	DECIMAL_5		5
#define	DECIMAL_V		6

#define	TOTAL_BYTE		0x80
#define	ISO_NUMBER		0x40
#define	DIVIDEBY_2		0x10
#define	DIVIDE_ALL		0x30
#define	MULT_CONST		0x08
  enum {
    BIT_IMAGE_MODE,
    NORML_MODE,
    SEND_BIT_IMAGE,
    SKIP_SPACES,
    LINE_FEED,
    FORM_FEED,
    AFTER_BIT_IMAGE,
    BIT_ROW_HEADER,
  };
  uchar *cfg_buf,*ptr;
  int ch, type, f_cont, f_type, pos, i, j, k, lens;
  int f_r_format;
  long offset;
  int old2new[] = {
    CFG_BIT_IMAGE_MODE,
    CFG_NORMAL_MODE,
    CFG_SEND_BIT_IMAGE,
    CFG_SKIP_SPACES,
    CFG_LINE_FEED,
    CFG_FORM_FEED,
    CFG_AFTER_BIT_IMAGE,
    CFG_BIT_ROW_HEADER,
  };

  ch =dviprt_setcfgbuffer_(pinfo,300,TEMP_CODEBUF_SIZE);
  if (ch < 0) return CFG_ERROR_MEMORY;
  dviprt_initcfg_(pcfg,pinfo);
  cfg_buf = pinfo->readbuf;
  if (gp_fread(cfg_buf,30,1,pinfo->file) < 1) {
    dviprt_printcfgerror(pinfo,"Read error.\n",-1);
  }
  if (cfg_buf[16] == 'P') {
    dviprt_printcfgerror(pinfo,"This is made by old version.\n",-1);
    return CFG_ERROR_OTHER;
  }
  else if (cfg_buf[16] == 'Q')
    f_r_format = 0;
  else if (cfg_buf[16] == 'R')
    f_r_format = 1;
  else
    f_r_format = -1;
  if (f_r_format == -1 || cfg_buf[18] != 0xff) {
    dviprt_printcfgerror(pinfo,"This is not the *.CFG file for dviprt.\n",-1);
    return CFG_ERROR_OTHER;
  }
  cfg_buf[16] = '\0';
  pcfg->version = 0;
  if (pinfo->temp_codebuf_f) {
    pcfg->strings[CFG_NAME] = malloc(strlen(cfg_buf)+1);
    if (pcfg->strings[CFG_NAME] == NULL) {
    no_memory:
      dviprt_printcfgerror(pinfo,"Memory exhausted.\n",-1);
      return CFG_ERROR_MEMORY;
    }
  }
  else {
    pcfg->strings[CFG_NAME] = pinfo->pcodebuf;
    pinfo->pcodebuf += strlen(cfg_buf);
    pinfo->pcodebuf++;
  }
  strcpy(pcfg->strings[CFG_NAME],cfg_buf);

  pcfg->integer[CFG_UPPER_POS] =
    (cfg_buf[17] & (CFG_LEFT_IS_LOW|CFG_NON_MOVING));
  pcfg->integer[CFG_ENCODE] =
    (cfg_buf[17] & 0x10) ? CFG_ENCODE_HEX : CFG_ENCODE_NULL;
  pcfg->integer[CFG_PINS] = ((uint) (cfg_buf[17]) & 0x0f);

  ptr = cfg_buf+23;
  pcfg->integer[CFG_MINIMAL_UNIT] = (uint)ptr[0] | ((uint)ptr[1]<<8);
  pcfg->integer[CFG_MAXIMAL_UNIT] = (uint)ptr[2] | ((uint)ptr[3]<<8);
  pcfg->integer[CFG_DPI] =
    f_r_format ? ((uint)ptr[4] | ((uint)ptr[5]<<8)) : 180;
  if (cfg_buf[20])
    pcfg->integer[CFG_CONSTANT] = cfg_buf[20];
  offset = cfg_buf[19];
  gp_fseek(pinfo->file,offset,0);

  for (i = 0; i <= BIT_ROW_HEADER; i++) {
    uchar *pstart,*plength;
    uchar prev = 1;
    if (pinfo->temp_codebuf_f) {
      pinfo->pcodebuf = pinfo->codebuf;
    }
    pstart = pinfo->pcodebuf;
    do {
      lens = gp_fgetc(pinfo->file);
      if (lens == EOF) break;
      gp_fread(cfg_buf,lens+3,1,pinfo->file);
      ptr = cfg_buf;
      f_cont = *ptr++;
      pos = *ptr++;
      f_type = *ptr++;
      type = f_type & 0x7;

      for (j = 0; j < lens; j++) {
        ch = *ptr++;
        if (pos == j && type != NO_NUM) {
          uchar *pfmt = pinfo->pcodebuf++;
          plength = pinfo->pcodebuf++;
          *pinfo->pcodebuf++ = CFG_VAL_DEFAULT;
          *plength = 1;
          j++;
          ptr++;
          switch (type) {
          case (BINARY_LTOH):
            *pfmt = CFG_FMT_BIT | CFG_FMT_BIN_LTOH | 2;
            break;
          case (BINARY_HTOL):
            *pfmt = CFG_FMT_BIT | CFG_FMT_BIN_HTOL | 2;
            break;
          case (DECIMAL_3):
            *pfmt = CFG_FMT_BIT | CFG_FMT_DECIMAL | 3;
            j++;
            ptr++;
            break;
          case (DECIMAL_4):
            *pfmt = CFG_FMT_BIT | CFG_FMT_DECIMAL | 4;
            j += 2;
            ptr += 2;
            break;
          case (DECIMAL_5):
            *pfmt = CFG_FMT_BIT | CFG_FMT_DECIMAL | 5;
            j += 3;
            ptr += 3;
            break;
          case (DECIMAL_V):
            *pfmt = CFG_FMT_BIT | CFG_FMT_DECIMAL;
            j++;
            ptr++;
            break;
          default:
            gs_sprintf(dviprt_message_buffer,"Unknown format %02X",type);
            dviprt_printcfgerror(pinfo,dviprt_message_buffer,-1);
            goto ex_func;
          }
          if (f_type & TOTAL_BYTE) {
            *pinfo->pcodebuf++ = CFG_VAL_PINS_BYTE;
            *pinfo->pcodebuf++ = CFG_OP_MUL;
            (*plength) += 2;
          }
          if ((k = (f_type & DIVIDE_ALL)) != 0) {
            *pinfo->pcodebuf = 0;
            for (; k > 0; k -= DIVIDEBY_2) {
              (*pinfo->pcodebuf)++;
            }
            pinfo->pcodebuf++;
            *pinfo->pcodebuf++ = CFG_OP_SHR;
            (*plength) += 2;
          }
          if (f_type & ISO_NUMBER) {
            *pfmt |= CFG_FMT_ISO_BIT;
          }
          if (f_type & MULT_CONST) {
            *pinfo->pcodebuf++ = CFG_VAL_CONSTANT;
            *pinfo->pcodebuf++ = CFG_OP_MUL;
            (*plength) += 2;
          }
          prev = 1;
        }
        else {
          if (prev == 1 || *plength >= 127) {
            plength = pinfo->pcodebuf++;
            *plength = 0;
          }
          (*plength)++;
          *pinfo->pcodebuf++ = ch;
          prev = 0;
        }
      }
    } while (f_cont & 0x80);
    *pinfo->pcodebuf++ = 0;
    { int n = old2new[i];
      uint l = pinfo->pcodebuf-pstart;
      pcfg->prtcode_size[n] = l - 1;
      if (pinfo->temp_codebuf_f) { /* allocate buffer */
        pcfg->prtcode[n] = (uchar *)malloc(l);
        if (pcfg->prtcode[n] == NULL)
          goto no_memory;
        memcpy(pcfg->prtcode[n],pstart,l);
      }
      else {
        pcfg->prtcode[n] = pstart;
      }
    }
  }
 ex_func:
  dviprt_resetcfgbuffer_(pinfo);
  return 0;
}
/***** End of rcfg.c *****/

/***** From rsrc.c *****/
/* $Id: RSRC.C 1.1 1994/08/30 02:27:02 kaz Exp kaz $ */

typedef struct {
  char *name;
  signed char type;
  uchar no;
  uchar spec_f;
  uchar req_f;
  char *info;
} dviprt_cfg_item_t;

typedef struct {
  long min;
  long max;
} dviprt_cfg_limit_t;

/*--- forward declarations ---*/
static int dviprt_set_select
  (dviprt_cfg_item_t *,uchar **,dviprt_cfg_t *,dviprt_cfg_i *);
static int dviprt_set_integer
  (dviprt_cfg_item_t *, uchar *, dviprt_cfg_t *,dviprt_cfg_i *);
static int dviprt_set_strings
  (dviprt_cfg_item_t *,uchar *,dviprt_cfg_t *,dviprt_cfg_i *);
static int dviprt_set_rpexpr
  (dviprt_cfg_item_t *,uchar *,int , dviprt_cfg_t *,dviprt_cfg_i *,int);
static int dviprt_set_code
  (dviprt_cfg_item_t *,uchar *,dviprt_cfg_t *,dviprt_cfg_i *);

static long dviprt_oct2long(uchar *,uchar *,uchar **);
static long dviprt_dec2long(uchar *,uchar *,uchar **);
static long dviprt_hex2long(uchar *,uchar *,uchar **);

static int dviprt_printtokenerror(dviprt_cfg_i *,char *,int ,int);

/*--- macros ---*/
#define strlcmp(tmplt,str,len) \
  (!(strncmp(tmplt,str,(int)(len)) == 0 && (int)(len) == strlen(tmplt)))
#define set_version(pcfg,v) ((pcfg)->version = MAX(v,(pcfg)->version))

enum {
  ERROR_UNKNOWN_VALUE,ERROR_UNKNOWN_FORMAT,ERROR_UNKNOWN_ESCSEQ,
  ERROR_OUTOFRANGE,
  ERROR_INVALID_VALUE,
  ERROR_COMPLICATED_EXPR,
  ERROR_INCOMPLETE,
};

/*--- library functions ---*/
int
dviprt_readsrc(const gs_memory_t *mem, char *fname,dviprt_cfg_t *pcfg,uchar *pcodebuf,int codebuf_s,
    uchar *pworkbuf,int workbuf_s)
{
  dviprt_cfg_i info;
  int code;
  gp_file *ifp;
  dviprt_cfg_item_t *pitem;
  int enc = CFG_ENCODE_NULL;
  enum { T_INTEGER,T_STRINGS,T_CODE,T_SELECT,T_UPPERPOS};
  static dviprt_cfg_limit_t pins_limit = { 8, 128 };
  static dviprt_cfg_limit_t positive_limit = { 1, 0x7fff };
  static dviprt_cfg_limit_t nonnegative_limit = { 0, 0x7fff};
  static dviprt_cfg_item_t dviprt_items[] = {
    {NULL,T_STRINGS,CFG_NAME,0,1,NULL},
    {NULL,T_INTEGER,CFG_PINS,0,1,(char*)&pins_limit},
    {NULL,T_INTEGER,CFG_MINIMAL_UNIT,0,0,(char*)&positive_limit},
    {NULL,T_INTEGER,CFG_MAXIMAL_UNIT,0,0,(char*)&positive_limit},
    {NULL,T_INTEGER,CFG_DPI,0,0,(char*)&positive_limit},
    {NULL,T_INTEGER,CFG_CONSTANT,0,0,(char*)&nonnegative_limit},
    {NULL,T_INTEGER,CFG_Y_DPI,0,0,(char*)&positive_limit},
    {NULL,T_CODE,CFG_BIT_IMAGE_MODE,0,1,NULL},
    {NULL,T_CODE,CFG_SEND_BIT_IMAGE,0,1,NULL},
    {NULL,T_CODE,CFG_BIT_ROW_HEADER,0,0,NULL},
    {NULL,T_CODE,CFG_AFTER_BIT_IMAGE,0,0,NULL},
    {NULL,T_CODE,CFG_LINE_FEED,0,0,NULL},
    {NULL,T_CODE,CFG_FORM_FEED,0,0,NULL},
    {NULL,T_CODE,CFG_NORMAL_MODE,0,1,NULL},
    {NULL,T_CODE,CFG_SKIP_SPACES,0,1,NULL},
    {NULL,T_UPPERPOS,CFG_UPPER_POS,0,1,NULL},
    {NULL,T_SELECT,CFG_ENCODE,0,0,(char*)dviprt_encodename},
    {NULL,-1},
  };
  static dviprt_cfg_item_t encode_info = {
    "encode",T_STRINGS,CFG_ENCODE_INFO,0,0,NULL
    };
  int prtcode_output_bytes[CFG_PRTCODE_TYPE_COUNT];

  info.line_no = -1;
  info.fname = fname;
  if (fname) {
    info.file = gp_fopen(mem,fname,"r");
    if (info.file == NULL) {
      dviprt_printcfgerror(&info,"Cannot open.\n",-1);
      return CFG_ERROR_FILE_OPEN;
    }
  }
  else {
    ifp = stdin;
  }
  ifp = info.file;

  info.codebuf = pcodebuf;
  info.readbuf = pworkbuf;
  info.codebuf_size = codebuf_s;
  info.readbuf_size = workbuf_s;
  /* allocate buffer */
  if (dviprt_setcfgbuffer_(&info,TEMP_READBUF_SIZE,TEMP_CODEBUF_SIZE) < 0) {
    gp_fclose(info.file);
    return CFG_ERROR_MEMORY;
  }

  /* initialize */
  dviprt_initcfg_(pcfg,&info);
  for (pitem = dviprt_items;pitem->type>=0;pitem++) {
    if (pitem->name == NULL) {
      switch (pitem->type) {
      case T_INTEGER:
      case T_SELECT:
      case T_UPPERPOS:
        pitem->name = dviprt_integername[pitem->no];
        break;
      case T_STRINGS:
        pitem->name = dviprt_stringsname[pitem->no];
        break;
      case T_CODE:
        pitem->name = dviprt_prtcodename[pitem->no];
        break;
      }
    }
    pitem->spec_f = 0;
  }
  encode_info.spec_f = 0;
  { int i;
    for (i=0;i<CFG_PRTCODE_TYPE_COUNT;i++)
      prtcode_output_bytes[i] = 0;
  }

  pcfg->version = 1;
  for ( ; ; ) {
    uchar *pbuf = info.readbuf;
    uchar *pchar;

    if (fgets(info.readbuf,info.readbuf_size,ifp) == NULL) break;
    info.line_no++;
    {
      int len = strlen(pbuf);
      if ((pbuf[0] < 'a' || pbuf[0] > 'z') && pbuf[0] != '_') {
        while (pbuf[len-1] != '\n') {
          if (fgets(info.readbuf,info.readbuf_size,ifp) == NULL)
            goto end_scan;
          len = strlen(pbuf);
        }
        continue;
      }
      if ( len > 0 && pbuf[len-1] == '\n')
        pbuf[len-1] = 0;
    }
    while (*pbuf && *pbuf != ':') pbuf++;
    if (*pbuf != ':') {
      dviprt_printcfgerror(&info,"Character ':' is expected.\n",-1);
      code = CFG_ERROR_SYNTAX;
      goto end_process;
    }
    pchar = pbuf-1;
    while (pchar >= info.readbuf && isspace(*pchar)) pchar--;
    *++pchar = 0;
    pbuf++;
    for (pitem = dviprt_items;pitem->name;pitem++) {
      if (strcmp(pitem->name,info.readbuf) == 0) break;
    }
    if (pitem->name == NULL) {
      dviprt_printcfgerror(&info,"Unknown item `",-1);
      dviprt_printmessage(info.readbuf,-1);
      dviprt_printmessage("'.\n",-1);
      code = CFG_ERROR_RANGE;
      goto end_process;
    }
  parse_more:
    while (*pbuf && isspace(*pbuf)) pbuf++;
    if (pitem->spec_f) {
      dviprt_printcfgerror(&info,NULL,0);
      gs_sprintf(dviprt_message_buffer,
              "Item `%s' is specified twice.\n",pitem->name);
      dviprt_printmessage(dviprt_message_buffer,-1);
      code = CFG_ERROR_SYNTAX;
      goto end_process;
    }
    switch (pitem->type) {
    case T_INTEGER:
      if ((code = dviprt_set_integer(pitem,pbuf,pcfg,&info)) < 0)
        goto end_process;
      if (pitem->no == CFG_PINS) {
        if (pcfg->integer[CFG_PINS] % 8) {
          dviprt_printcfgerror(&info,"Value must be a multiple of 8.\n",-1);
          code = CFG_ERROR_RANGE;
          goto end_process;
        }
        pcfg->integer[CFG_PINS] /= 8;
      }
      break;
    case T_STRINGS:
      if (info.temp_codebuf_f)
        info.pcodebuf = info.codebuf;
      if ((code = dviprt_set_strings(pitem,pbuf,pcfg,&info)) < 0)
        goto end_process;
      if (info.temp_codebuf_f) {
        pcfg->strings[pitem->no] =
          (uchar*)malloc(strlen(pcfg->strings[pitem->no])+1);
        if (pcfg->strings[pitem->no] == NULL) {
          goto no_more_memory;
        }
        strcpy(pcfg->strings[pitem->no],info.codebuf);
      }
      break;
    case T_CODE:
      if (info.temp_codebuf_f)
        info.pcodebuf = info.codebuf;
      if ((code = dviprt_set_code(pitem,pbuf,pcfg,&info)) < 0)
        goto end_process;
      prtcode_output_bytes[pitem->no] = code;
      if (info.temp_codebuf_f) {
        pcfg->prtcode[pitem->no] =
          (uchar*)malloc(pcfg->prtcode_size[pitem->no]+1);
        if (pcfg->prtcode[pitem->no] == NULL) {
        no_more_memory:
          dviprt_printcfgerror(&info,"Memory exhausted.\n",-1);
          code = CFG_ERROR_MEMORY;
          goto end_process;
        }
        memcpy(pcfg->prtcode[pitem->no],info.codebuf,
               pcfg->prtcode_size[pitem->no]+1);
      }
      break;
    case T_SELECT:
      if ((code = dviprt_set_select(pitem,&pbuf,pcfg,&info)) < 0)
        goto end_process;
      if (pitem->no == CFG_ENCODE) {
        pitem = &encode_info;
        goto parse_more;
      }
      break;
    case T_UPPERPOS:
      { uchar *ptmp;
        uchar upos=0;
        uchar opt = 0;
        if (*pbuf == 0) {
          dviprt_printcfgerror(&info,"No value.\n",-1);
          code = CFG_ERROR_SYNTAX;
          goto end_process;
        }
        while (*pbuf) {
          ptmp = pbuf;
          while (*ptmp && !isspace(*ptmp)) ptmp++;
          if (strlcmp("HIGH_BIT",pbuf,ptmp-pbuf) == 0)
            upos = CFG_TOP_IS_HIGH;
          else if (strlcmp("LOW_BIT",pbuf,ptmp-pbuf) == 0)
            upos = CFG_TOP_IS_LOW;
          else if (strlcmp("LEFT_IS_HIGH",pbuf,ptmp-pbuf) == 0)
            upos = CFG_LEFT_IS_HIGH;
          else if (strlcmp("LEFT_IS_LOW",pbuf,ptmp-pbuf) == 0)
            upos = CFG_LEFT_IS_LOW;
          else if (strlcmp("NON_MOVING",pbuf,ptmp-pbuf) == 0)
            opt = CFG_NON_MOVING;
          else if (strlcmp("HEX_MODE",pbuf,ptmp-pbuf) == 0)
            enc = CFG_ENCODE_HEX;
          else {
            dviprt_printtokenerror(&info,pbuf,(int)(ptmp-pbuf),ERROR_UNKNOWN_VALUE);
            code = CFG_ERROR_RANGE;
            goto end_process;
          }
          pbuf = ptmp;
          while (*pbuf && isspace(*pbuf)) pbuf++;
        }
        pcfg->integer[CFG_UPPER_POS] = upos | opt;
      }
      break;
    }
    pitem->spec_f = 1;
  }
 end_scan:

  info.line_no = -1;
  code = 0;
  for (pitem = dviprt_items;pitem->name;pitem++) {
    if (!pitem->spec_f && pitem->req_f) {
      gs_sprintf(dviprt_message_buffer,"%s not found.\n",pitem->name);
      dviprt_printcfgerror(&info,dviprt_message_buffer,-1);
      code++;
    }
  }
  if (code) { code = CFG_ERROR_RANGE; goto end_process; }

  if (pcfg->prtcode[CFG_LINE_FEED] == NULL) {
    if (info.temp_codebuf_f) {
      pcfg->prtcode[CFG_LINE_FEED] = info.pcodebuf;
      info.pcodebuf += 4;
    }
    else pcfg->prtcode[CFG_LINE_FEED] = (byte*)malloc(4);
    memcpy(pcfg->prtcode[CFG_LINE_FEED],"\002\x0d\x0a\000",4);
    pcfg->prtcode_size[CFG_LINE_FEED] = 3;
  }
  if (pcfg->prtcode[CFG_FORM_FEED] == NULL) {
    if (info.temp_codebuf_f) {
      pcfg->prtcode[CFG_FORM_FEED] = info.pcodebuf;
      info.pcodebuf += 4;
    }
    else pcfg->prtcode[CFG_FORM_FEED] = (byte*)malloc(4);
    memcpy(pcfg->prtcode[CFG_FORM_FEED],"\002\x0d\x0c\000",4);
    pcfg->prtcode_size[CFG_FORM_FEED] = 3;
  }
  if (pcfg->integer[CFG_DPI] < 0 && pcfg->integer[CFG_Y_DPI] < 0) {
    pcfg->integer[CFG_DPI] = 180;
  }
  else if (pcfg->integer[CFG_DPI] < 0 || pcfg->integer[CFG_Y_DPI] < 0) {
    if (pcfg->integer[CFG_DPI] < 0)
      pcfg->integer[CFG_DPI] = pcfg->integer[CFG_Y_DPI];
    pcfg->integer[CFG_Y_DPI] = -1;
  }
  else if (pcfg->integer[CFG_DPI] == pcfg->integer[CFG_Y_DPI]) {
    pcfg->integer[CFG_Y_DPI] = -1;
  }
  else if (pcfg->integer[CFG_Y_DPI] >= 0) { /* has y_dpi. */
    set_version(pcfg,2);
  }
  if (pcfg->integer[CFG_ENCODE] < 0) {
    pcfg->integer[CFG_ENCODE] = enc;
  }
  if (pcfg->integer[CFG_MAXIMAL_UNIT] < 0) {
    pcfg->integer[CFG_MAXIMAL_UNIT] = 0x7fff;
  }
  if (pcfg->integer[CFG_MINIMAL_UNIT] < 0) {
    uint v;
    v = (MAX(prtcode_output_bytes[CFG_SEND_BIT_IMAGE],0) +
         MAX(prtcode_output_bytes[CFG_AFTER_BIT_IMAGE],0) +
         MAX(prtcode_output_bytes[CFG_SKIP_SPACES],0))
      / (pcfg->integer[CFG_PINS]*8) +
        MAX(prtcode_output_bytes[CFG_BIT_ROW_HEADER],0);
    if (v == 0) v = 1;
    pcfg->integer[CFG_MINIMAL_UNIT] = v;
  }

  for (pitem = dviprt_items;pitem->type>=0;pitem++) {
    if (pitem->spec_f == 0) {
      gs_sprintf(dviprt_message_buffer,": %s:",pitem->name);
      switch (pitem->type) {
      case T_INTEGER:
        if (pcfg->integer[pitem->no] >= 0) {
          uint v = pcfg->integer[pitem->no];
          dviprt_printmessage(fname,-1);
          dviprt_printmessage(dviprt_message_buffer,-1);
          gs_sprintf(dviprt_message_buffer," %d\n",v);
          dviprt_printmessage(dviprt_message_buffer,-1);
        }
        break;
      default: break; /* do nothing */
      }
    }
  }

 end_process:
  if (fname) gp_fclose(ifp);
  dviprt_resetcfgbuffer_(&info);

  return code;
}

/*--- internal routines ---*/
static int
dviprt_set_integer(dviprt_cfg_item_t *pitem,uchar *buf,dviprt_cfg_t *pcfg,
    dviprt_cfg_i *pinfo)
{
  uchar *pbuf = buf;
  long v = 0;
  long max = -1 ,min = -1;

  if (pitem->info != NULL) {
    dviprt_cfg_limit_t *plimit = (dviprt_cfg_limit_t *)pitem->info;
    min = plimit->min;
    max = plimit->max;
  }
  if (min < 0) min = 0;
  if (max < 0) max = 0xffff;
  if (*pbuf == 0) {
    dviprt_printcfgerror(pinfo,"No value.\n",-1);
    return CFG_ERROR_SYNTAX;
  }
  while (*pbuf) {
    if (!isdigit(*pbuf)) {
      if (isspace(*pbuf)) break;
      else goto invalid_val;
    }
    v = v*10 + *pbuf - '0';
    if (v > max) {
    out_of_range:
      dviprt_printtokenerror(pinfo,buf,strlen(buf),ERROR_OUTOFRANGE);
      dviprt_printcfgerror(pinfo,"",-1);
      gs_sprintf(dviprt_message_buffer,
              "(%u <= value <= %u).\n",(uint)min,(uint)max);
      dviprt_printmessage(dviprt_message_buffer,-1);
      return CFG_ERROR_RANGE;
    }
    pbuf++;
  }
  if (v < min) goto out_of_range;

  while (*pbuf) {
    if (!isspace(*pbuf)) {
    invalid_val:
      dviprt_printtokenerror(pinfo,buf,strlen(buf),ERROR_INVALID_VALUE);
      return CFG_ERROR_RANGE;
    }
    pbuf++;
  }
  pcfg->integer[pitem->no] = v;

  return 0;
}

static int
dviprt_set_strings(dviprt_cfg_item_t *pitem,uchar *buf,dviprt_cfg_t *pcfg,
    dviprt_cfg_i *pinfo)
{
  uchar *pend;
  long len;
  pend = buf+strlen(buf)-1;
  while (pend >= buf && isspace(*pend)) pend--;
  pend++;
  len = pend - buf;
  if (len > 0x7fffL) {
    dviprt_printcfgerror(pinfo,"Too long strings.\n",-1);
    return CFG_ERROR_RANGE;
  }

  pcfg->strings[pitem->no] = pinfo->pcodebuf;
  strncpy(pinfo->pcodebuf,buf,(int)len);
  pinfo->pcodebuf[len] = 0;
  pinfo->pcodebuf += len;
  pinfo->pcodebuf++;
  return 0;
}

static int
dviprt_set_select(dviprt_cfg_item_t *pitem,uchar **buf,dviprt_cfg_t *pcfg,
    dviprt_cfg_i *pinfo)
{
  int i;
  uchar *ptmp = *buf;
  uchar *pstart = *buf;
  uchar **opt;
  if (*pstart == 0) {
    dviprt_printcfgerror(pinfo,"No value.\n",-1);
    return CFG_ERROR_SYNTAX;
  }
  while (*ptmp && !isspace(*ptmp)) ptmp++;

  for (i=0,opt=(uchar**)pitem->info;*opt;i++,opt++) {
    if (strlcmp(*opt,pstart,ptmp-pstart) == 0) {
      pcfg->integer[pitem->no] = i;
      *buf = ptmp;
      return 0;
    }
  }
  dviprt_printtokenerror(pinfo,pstart,(int)(ptmp-pstart),ERROR_UNKNOWN_VALUE);
  return CFG_ERROR_RANGE;
}

#define CFG_TOKEN_ERROR     -1
#define CFG_TOKEN_LIMIT_BIT 0x100
#define CFG_TOKEN_FMT       0x200

static int
dviprt_get_codetype_token(dviprt_cfg_i *pinfo,uchar *pstart,uchar *pend,uchar *stopescseqchars,
    uchar *limitchars)
{
  while (pstart < pend && isspace(*pstart)) pstart++;
  if (pstart >= pend) {
    pinfo->token = pinfo->endtoken = pstart;
    return CFG_TOKEN_LIMIT_BIT;
  }
  else if (strchr(limitchars,*pstart)) {
    pinfo->token = pstart;
    pinfo->endtoken = pstart+1;
    return CFG_TOKEN_LIMIT_BIT | *pstart;
  }
  else if (*pstart == '\\') {
    int c;
    long v;
    uchar *pexpr,*pnext;

    pexpr = pinfo->token = pstart++;
    while (pstart < pend && !isspace(*pstart) &&
           *pstart != '\\' && !strchr(stopescseqchars,*pstart)) {
      pstart++;
    }
    pinfo->endtoken = pstart;
    if (pinfo->token + 1 == pinfo->endtoken) { /* '\\' only */
      return '\\';
    }
    pexpr++;
    if (pinfo->endtoken - pexpr == 1) {
      if (isdigit(*pexpr)) goto parse_decimal_numb;
      switch (*pexpr) {
      case 't': c = '\t'; break; /* tab */
      case 'v': c = '\v'; break; /* tab */
      case 'n': c = '\n'; break; /* line feed */
      case 'f': c = '\f'; break; /* form feed */
      case 'r': c = '\r'; break; /* carrige return */
      case 'e': c = 0x1b; break; /* escape code */
      case 's': c = 0x20; break; /* space */
      default:
        dviprt_printtokenerror(pinfo,pinfo->token,2,ERROR_UNKNOWN_ESCSEQ);
        return CFG_TOKEN_ERROR;
      }
      return c;
    }
    else if (strlcmp("SP",pexpr,pinfo->endtoken - pexpr) == 0)
      return 0x20;
    else if (strlcmp("ESC",pexpr,pinfo->endtoken - pexpr) == 0)
      return 0x1b;
    switch (*pexpr) {
    case 'x':
    case 'X':
      v = dviprt_hex2long(pexpr+1,pinfo->endtoken,&pnext);
    check_numb_range:
      if (pstart != pnext) {
        dviprt_printtokenerror(pinfo,pinfo->token,
                               (int)(pinfo->endtoken - pinfo->token), ERROR_INVALID_VALUE);
        return CFG_TOKEN_ERROR;
      }
      if (v >= 256) {
        dviprt_printtokenerror(pinfo,pinfo->token,
                               (int)(pinfo->endtoken - pinfo->token), ERROR_OUTOFRANGE);
        return CFG_TOKEN_ERROR;
      }
      pinfo->endtoken = pnext;
      return v;
    case '0':
      v = dviprt_oct2long(pexpr,pinfo->endtoken,&pnext);
      goto check_numb_range;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    parse_decimal_numb:
      v = dviprt_dec2long(pexpr,pinfo->endtoken,&pnext);
      goto check_numb_range;
    default:
      return CFG_TOKEN_FMT;
    }
  }
  else {
    pinfo->token = pstart;
    pinfo->endtoken = pstart+1;
    return *pstart;
  }
}

static long
dviprt_dec2long(uchar *start,uchar *end,uchar **next)
{
  long v = 0;
  while (start < end) {
    int c = *start;
    if (isdigit(c)) v = v*10 + c - '0';
    else break;
    start++;
  }
  *next = start;
  return v;
}

static long
dviprt_oct2long(uchar *start,uchar *end,uchar **next)
{
  long v = 0;
  while (start < end) {
    int c = *start;
    if (c >= '0' && c <= '7') v = v*8 + c - '0';
    else break;
    start++;
  }
  *next = start;
  return v;
}

static long
dviprt_hex2long(uchar *start,uchar *end,uchar **next)
{
  long v = 0;
  while (start < end) {
    int c = *start;
    if (isdigit(c)) v = v*16 + c - '0';
    else if (c >= 'A' && c <= 'F') v = v*16 + c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') v = v*16 + c - 'a' + 10;
    else break;
    start++;
  }
  *next = start;
  return v;
}

static int
dviprt_set_rpexpr(dviprt_cfg_item_t *pitem,uchar *pbuf,int len,dviprt_cfg_t *pcfg,
    dviprt_cfg_i *pinfo,int sp)
{
  uchar *pend = pbuf + len;
  uchar *plastop = NULL;
  int code;

  /* get left expr */
  while (pbuf < pend) {
    int par_count = 0;
    uchar *pcur = pbuf;
    while (pcur < pend) {
      if (*pcur == '(') par_count++;
      else if (*pcur == ')') par_count--;
      else if (!isdigit(*pcur) && !isalpha(*pcur) && par_count == 0) {
        /* operator */
        plastop = pcur;
      }
      pcur++;
    }
    if (par_count != 0) {
      dviprt_printtokenerror(pinfo,pbuf,(int)(pend-pbuf),ERROR_INCOMPLETE);
      return CFG_ERROR_SYNTAX;
    }
    if (plastop == NULL) {
      if (*pbuf != '(') break;
      pbuf++;
      pend--;
    }
    else break;
  }

  if (plastop == NULL) { /* no operator */
    ulong v;
    uchar *pdummy;
    if (*pbuf == '0') {
      uchar a,b,c;
      v = dviprt_oct2long(pbuf,pend,&pdummy);
    check_intval:
      if (pdummy != pend) goto unknown_value;
      if (v > 0xffff) {
        dviprt_printtokenerror(pinfo,pbuf,(int)(pend-pbuf),ERROR_OUTOFRANGE);
        return CFG_ERROR_RANGE;
      }
      a = v & 0x7f;
      b = (v>>7) & 0x7f;
      c = (v>>14) & 0x03;
      if (c) {
        *pinfo->pcodebuf++ = c;
        *pinfo->pcodebuf++ = 14;
        *pinfo->pcodebuf++ = CFG_OP_SHL;
      }
      if (b) {
        *pinfo->pcodebuf++ = b;
        *pinfo->pcodebuf++ = 7;
        *pinfo->pcodebuf++ = CFG_OP_SHL;
        if (c) *pinfo->pcodebuf++ = CFG_OP_OR;
      }
      if (a) {
        *pinfo->pcodebuf++ = a;
        if (b || c) *pinfo->pcodebuf++ = CFG_OP_OR;
      }
      code = 0;
    }
    else if (isdigit(*pbuf)) {
      v = dviprt_dec2long(pbuf,pend,&pdummy);
      goto check_intval;
    }
    else if (pend - pbuf > 1 && (*pbuf == 'x' || *pbuf == 'X')) {
      v = dviprt_hex2long(pbuf+1,pend,&pdummy);
      goto check_intval;
    }
    else if (pend - pbuf > 1) {
    unknown_value:
      dviprt_printtokenerror(pinfo,pbuf,(int)(pend-pbuf),ERROR_UNKNOWN_VALUE);
      return CFG_ERROR_RANGE;
    }
    else {
      switch (*pbuf) {
      case 'd': v = CFG_VAL_DEFAULT;
        if (pitem->no != CFG_SEND_BIT_IMAGE &&
            pitem->no != CFG_BIT_ROW_HEADER &&
            pitem->no != CFG_AFTER_BIT_IMAGE &&
            pitem->no != CFG_SKIP_SPACES)
          goto unavailable_value;
        break;
      case 'c': v = CFG_VAL_CONSTANT; break;
      case 'w': v = CFG_VAL_WIDTH; break;
      case 'h': v = CFG_VAL_HEIGHT; break;
      case 'r': v = CFG_VAL_X_DPI; break;
      case 'R': v = CFG_VAL_Y_DPI; break;
      case 'p': v = CFG_VAL_PAGE; break;
      case 'x': v = CFG_VAL_X_POS; break;
      case 'y': v = CFG_VAL_Y_POS; break;
      case 'v': v = CFG_VAL_PINS_BYTE; break;
      case 's':
        v = CFG_VAL_DATASIZE;
        if (pitem->no != CFG_SEND_BIT_IMAGE &&
            pitem->no != CFG_BIT_ROW_HEADER &&
            pitem->no != CFG_AFTER_BIT_IMAGE) {
        unavailable_value:
          dviprt_printcfgerror(pinfo,"",-1);
          gs_sprintf(dviprt_message_buffer,"Variable `%c' in ",(int)*pbuf);
          dviprt_printmessage(dviprt_message_buffer,-1);
          dviprt_printmessage(pbuf,(int)(pend-pbuf));
          gs_sprintf(dviprt_message_buffer," cannot be used in %s.\n",pitem->name);
          dviprt_printmessage(dviprt_message_buffer,-1);
          return CFG_ERROR_RANGE;
        }
        break;
      default:
        goto unknown_value;
      }
      *pinfo->pcodebuf++ = v;
      code = 0;
    }
  }
  else { /* has operator */
    uchar op;

    code = dviprt_set_rpexpr(pitem,pbuf,(int)(plastop-pbuf),pcfg,pinfo,sp+1);
    if (code < 0) return code;
    code = dviprt_set_rpexpr(pitem,plastop+1,(int)(pend-plastop-1),pcfg,pinfo,sp+2);
    if (code < 0) return code;

    switch (*plastop) {
    case '+': op = CFG_OP_ADD; break;
    case '-': op = CFG_OP_SUB; break;
    case '*': op = CFG_OP_MUL; break;
    case '/': op = CFG_OP_DIV; break;
    case '%': op = CFG_OP_MOD; break;
    case '<': op = CFG_OP_SHL; break;
    case '>': op = CFG_OP_SHR; break;
    case '&': op = CFG_OP_AND; break;
    case '|': op = CFG_OP_OR ; break;
    case '^': op = CFG_OP_XOR; break;
    default:
      dviprt_printcfgerror(pinfo,NULL,0);
      gs_sprintf(dviprt_message_buffer,"Unknown operator %c in ",(int)*pbuf);
      dviprt_printmessage(dviprt_message_buffer,-1);
      dviprt_printmessage(pbuf,(int)(pend-pbuf));
      dviprt_printmessage(".\n",-1);
      return CFG_ERROR_SYNTAX;
    }
    *pinfo->pcodebuf++ = op;
  }

  return code;
}

static int
dviprt_set_code(dviprt_cfg_item_t *pitem,uchar *buf,dviprt_cfg_t *pcfg,
    dviprt_cfg_i *pinfo)
{
  long prev_line;
  int prev_type = 1;
  uchar *pcount;
  uchar *pcode_begin;
  uchar *rbuf;
  int obytes = 0;

  prev_line = ftell(pinfo->file);
  pcode_begin = pinfo->pcodebuf;
  rbuf = pinfo->readbuf;

  for ( ; ; ) {
    while (*buf) {
      int c;
      c = dviprt_get_codetype_token(pinfo,buf,buf+strlen(buf),",","");
      if (c == CFG_TOKEN_LIMIT_BIT) break; /* no elements remain */
      else if (c == CFG_TOKEN_ERROR) return CFG_ERROR_SYNTAX;
      if ( c < 256) {        /* character code (raw data) */
        if (prev_type) {
        new_unit:
          pcount = pinfo->pcodebuf++;
          (*pcount) = 0;
          prev_type = 0;
        }
        if (*pcount == 127) goto new_unit;
        (*pcount)++;
        *pinfo->pcodebuf++ = c;
        buf = pinfo->endtoken;
        obytes++;
      }
      else if (c == CFG_TOKEN_FMT) { /* format */
        uchar *pexpr = pinfo->token;
        int div=0,iso=0,mul=0,tl=0;
        int cols=0;
        int fmt;
        uchar *plength;
        uchar *pstart;

        buf = pinfo->token+1;

        /* formats */
        switch (*buf) {
        case 'b': fmt = CFG_FMT_BIN_LTOH; break;
        case 'B': fmt = CFG_FMT_BIN_HTOL; break;
        case 'H': fmt = CFG_FMT_HEX_UPPER; break;
        case 'h': fmt = CFG_FMT_HEX_LOWER; break;
        case 'd': fmt = CFG_FMT_DECIMAL; break;
        case 'o': fmt = CFG_FMT_OCTAL; break;
        case 's':
          buf++;
          if (*buf != 't') goto unknown_format;
          fmt = CFG_FMT_STRINGS;
          break;
        default:
        unknown_format:
          dviprt_printtokenerror(pinfo,pexpr,(int)(pinfo->endtoken-pexpr),
                                 ERROR_UNKNOWN_FORMAT);
          return CFG_ERROR_SYNTAX;
        }
        buf++;

        /* columns */
        if (fmt == CFG_FMT_STRINGS) ;
        else {
          if (buf >= pinfo->endtoken) {
            dviprt_printtokenerror(pinfo,pexpr,(int)(pinfo->token-pexpr),ERROR_INCOMPLETE);
            return CFG_ERROR_SYNTAX;
          }

          if (!(*buf >= '1' && *buf <= '7') && *buf != '?') {
          invalid_cols:
            dviprt_printtokenerror(pinfo,pexpr,(int)(pinfo->endtoken-pexpr),
                                   ERROR_UNKNOWN_FORMAT);
            return CFG_ERROR_SYNTAX;
          }
          cols = (*buf == '?') ? 0 : *buf - '0';
          if (cols == 0 &&
              (fmt == CFG_FMT_BIN_LTOH || fmt == CFG_FMT_BIN_HTOL))
            goto invalid_cols;

          buf++;
          obytes += (cols == 0) ? 5 : cols;
        }

        /* additional format */
        while (buf < pinfo->endtoken) {
          switch (*buf) {
          case 'D': div++; break;
          case 'I': iso++; break;
          case 'M': mul++; break;
          case 'T': tl++; break;
          default:
            dviprt_printtokenerror(pinfo,pexpr,(int)(buf-pexpr),ERROR_UNKNOWN_FORMAT);
            return CFG_ERROR_SYNTAX;
          }
          if (div > 3 || iso > 1 || mul > 1 || tl > 1) {
            dviprt_printtokenerror(pinfo,pexpr,(int)(buf-pexpr),ERROR_UNKNOWN_FORMAT);
            return CFG_ERROR_SYNTAX;
          }
          buf++;
        }
        *pinfo->pcodebuf++ =
          CFG_FMT_BIT | fmt | (iso ? CFG_FMT_ISO_BIT : 0) | cols;
        plength = pinfo->pcodebuf;
        pinfo->pcodebuf++;
        pstart = pinfo->pcodebuf;

        if (*buf == ',' && *(buf+1) != '\"') {
          int code;
          buf++;
          pinfo->token = buf;
          while (*pinfo->token && *pinfo->token != ',' &&
                 *pinfo->token != '\\' && !isspace(*pinfo->token))
            pinfo->token++;
          if (pinfo->token == buf) {
            dviprt_printcfgerror(pinfo,"No expression is specified in ",-1);
            dviprt_printmessage(pexpr,(int)(buf-pexpr));
            dviprt_printmessage(".\n",-1);
            return CFG_ERROR_SYNTAX;
          }
          if ((code = dviprt_set_rpexpr(pitem,buf,(int)(pinfo->token-buf),pcfg,pinfo,0)) < 0)
            return code;
          buf = pinfo->token;
        }
        else {
          *pinfo->pcodebuf++ = CFG_VAL_DEFAULT;
        }
        if (mul) {
          *pinfo->pcodebuf++ = CFG_VAL_CONSTANT;
          *pinfo->pcodebuf++ = CFG_OP_MUL;
        }
        if (tl) {
          *pinfo->pcodebuf++ = CFG_VAL_PINS_BYTE;
          *pinfo->pcodebuf++ = CFG_OP_MUL;
        }
        if (div) {
          *pinfo->pcodebuf++ = div;
          *pinfo->pcodebuf++ = CFG_OP_SHR;
        }
        {
          int length = pinfo->pcodebuf-pstart;
          if (length > 255) {
            dviprt_printtokenerror(pinfo,pexpr,(int)(buf-pexpr),ERROR_COMPLICATED_EXPR);
            return CFG_ERROR_RANGE;
          }
          *plength++ = length & 0xff;
        }
        if (fmt == CFG_FMT_STRINGS) {
          uchar *pslen = pinfo->pcodebuf++;
          int len;
          if (strlen(buf) < 2 || *buf != ',' || *(buf+1) != '\"') {
            dviprt_printcfgerror(pinfo,"No strings specified in ",-1);
            dviprt_printmessage(pexpr,(int)(buf-pexpr));
            dviprt_printmessage(".\n",-1);
            return CFG_ERROR_SYNTAX;
          }
          buf += 2;
          for (len=0; ;len++) {
            c = dviprt_get_codetype_token(pinfo,buf,buf+strlen(buf),"\"","\"");
            if (c == CFG_TOKEN_ERROR) return CFG_ERROR_SYNTAX;
            else if (c == CFG_TOKEN_FMT) {
              dviprt_printcfgerror(pinfo,"The format ",-1);
              dviprt_printmessage(pinfo->token,
                                  (int)(pinfo->endtoken-pinfo->token));
              dviprt_printmessage(" cannot to be specified here.\n",-1);
              return CFG_ERROR_SYNTAX;
            }
            else if (c & CFG_TOKEN_LIMIT_BIT) {
              if ((c & 0xff) != '\"') {
                dviprt_printcfgerror(pinfo,
                                     "Strings must be enclosed with "
                                     "double quotations (\").\n",-1);
                return CFG_ERROR_SYNTAX;
              }
              buf = pinfo->endtoken;
              break;
            }
            *pinfo->pcodebuf++ = c;
            buf = pinfo->endtoken;
          }
          if (len > 255) {
            dviprt_printcfgerror(pinfo,"Too long strings.\n",-1);
            return CFG_ERROR_RANGE;
          }
          *pslen = len;
        }
        prev_type = 1;
      }
      else {
        dviprt_printcfgerror(pinfo,"Parse error. Unexpected token ",-1);
        dviprt_printmessage(pinfo->token,(int)(pinfo->endtoken-pinfo->token));
        dviprt_printmessage(".\n",-1);
        return CFG_ERROR_SYNTAX;
      }
    }
  next_line:
    if (fgets(rbuf,pinfo->readbuf_size,pinfo->file) == NULL) break;
    if (!isspace(rbuf[0]) && rbuf[0] != ';') {
      gp_fseek(pinfo->file,prev_line,0);
      break;
    }
    prev_line = ftell(pinfo->file);
    pinfo->line_no++;
    buf = rbuf;
    while (*buf && isspace(*buf)) buf++;
    if (*buf == ';')
      goto next_line; /* comment */
    {
      int len = strlen(rbuf);
      if (len > 0 && rbuf[len-1] == '\n')
        rbuf[len-1] = 0;
    }
  }
  pcfg->prtcode[pitem->no] = pcode_begin;
  pcfg->prtcode_size[pitem->no] = (pinfo->pcodebuf - pcode_begin) & 0xffff;
  *pinfo->pcodebuf++ = 0;
  return obytes;
}

static char *
dviprt_src_errorno2message(int type)
{
  switch (type) {
  case ERROR_OUTOFRANGE:
    return "Out of range.\n";
  case ERROR_UNKNOWN_VALUE:
    return "Unknown value.\n";
  case ERROR_UNKNOWN_ESCSEQ:
    return "Unknown escape sequence.\n";
  case ERROR_COMPLICATED_EXPR:
    return "Too complicated expression.\n";
  case ERROR_INCOMPLETE:
    return "Incomplete specification.\n";
  case ERROR_UNKNOWN_FORMAT:
    return "Unknown format.\n";
  case ERROR_INVALID_VALUE:
    return "Invalid value.\n";
  default:
    return NULL;
  }
}

static int
dviprt_printtokenerror(dviprt_cfg_i *pinfo,char *token,int len,int type)
{
  char *msg;

  dviprt_printcfgerror(pinfo,token,len);
  dviprt_printmessage("\n",-1);

  if ((msg = dviprt_src_errorno2message(type)) != NULL)
    dviprt_printcfgerror(pinfo,msg,-1);
  return 0;
}

#undef strlcmp
#undef set_version
/***** End of rsrc.c *****/

/***** From util.c *****/
/* $Id: UTIL.C 1.1 1994/08/30 02:27:02 kaz Exp kaz $ */

char *dviprt_integername[] = { CFG_INTEGER_NAME, NULL };
char *dviprt_stringsname[] = { CFG_STRINGS_NAME, NULL };
char *dviprt_prtcodename[] = { CFG_PRTCODE_NAME, NULL };
char *dviprt_encodename[] = { CFG_ENCODE_NAME, NULL };

#if 0
static FILE *dviprt_messagestream = stderr;
#else  /* patch for glibc 2.1.x by Shin Fukui <shita@april.co.jp> */
static FILE *dviprt_messagestream;
#endif

/*--- library functions ---*/
int
dviprt_setmessagestream(FILE *fp)
{
  dviprt_messagestream = fp;
  return 0;
}

/*--- internal routines ---*/
static int
dviprt_initcfg_(dviprt_cfg_t *pcfg,dviprt_cfg_i *pinfo)
{
  int i;

  for (i=0;i<CFG_INTEGER_TYPE_COUNT;i++)
    pcfg->integer[i] = -1;
  for (i=0;i<CFG_STRINGS_TYPE_COUNT;i++)
    pcfg->strings[i] = NULL;
  for (i=0;i<CFG_PRTCODE_TYPE_COUNT;i++) {
    pcfg->prtcode[i] = NULL;
    pcfg->prtcode_size[i] = 0;
  }
  pinfo->pcodebuf = pinfo->codebuf;
  pinfo->line_no = 0;
  return 0;
}

static int
dviprt_setcfgbuffer_(dviprt_cfg_i *pinfo,int rsize,int csize)
{
  pinfo->temp_readbuf_f = pinfo->temp_codebuf_f = 0;

  if (pinfo->readbuf == NULL) {
    pinfo->readbuf_size = rsize;
    if (rsize>0) {
      pinfo->temp_readbuf_f = 1;
      pinfo->readbuf = (uchar *)malloc(rsize);
      if (pinfo->readbuf == NULL) {
      no_mem:
        dviprt_printmessage(pinfo->fname,-1);
        dviprt_printmessage("Memory exhausted.\n",-1);
        return CFG_ERROR_MEMORY;
      }
    }
  }
  if (pinfo->codebuf == NULL) {
    pinfo->codebuf_size = csize;
    if (csize>0) {
      pinfo->temp_codebuf_f = 1;
      pinfo->codebuf = (uchar *)malloc(csize);
      if (pinfo->codebuf == NULL) goto no_mem;
    }
  }
  return 0;
}

static int
dviprt_resetcfgbuffer_(dviprt_cfg_i *pinfo)
{
  if (pinfo->temp_readbuf_f) free(pinfo->readbuf);
  if (pinfo->temp_codebuf_f) free(pinfo->codebuf);
  pinfo->temp_codebuf_f = pinfo->temp_readbuf_f = 0;
  return 0;
}

char dviprt_message_buffer[128];

static int
dviprt_printmessage(char *str,int len)
{
  if (dviprt_messagestream && str) {
    if (len >= 0) fwrite(str,len,1,dviprt_messagestream);
    else fputs(str,dviprt_messagestream);
    fflush(dviprt_messagestream);
  }
  return 0;
}

static int
dviprt_printcfgerrorheader(dviprt_cfg_i *pinfo)
{
  if (pinfo) {
    char *fn = pinfo->fname;
    if (fn == NULL) fn = "-";
    dviprt_printmessage(fn,-1);
    dviprt_printmessage(": ",-1);
    if (pinfo->line_no>0) {
      gs_sprintf(dviprt_message_buffer,"%d: ",pinfo->line_no);
      dviprt_printmessage(dviprt_message_buffer,-1);
    }
  }
  return 0;
}

static int
dviprt_printerror(char *msg,int len)
{
  dviprt_printmessage("*ERROR* ",-1);
  dviprt_printmessage(msg,len);
  return 0;
}

static int
dviprt_printcfgerror(dviprt_cfg_i *pinfo,char *msg,int len)
{
  dviprt_printcfgerrorheader(pinfo);
  dviprt_printerror(msg,len);
  return 0;
}

static int
dviprt_printwarning(char *msg,int len)
{
  dviprt_printmessage("*WARNING* ",-1);
  dviprt_printmessage(msg,len);
  return 0;
}

static int
dviprt_printcfgwarning(dviprt_cfg_i *pinfo,char *msg,int len)
{
  dviprt_printcfgerrorheader(pinfo);
  dviprt_printwarning(msg,len);
  return 0;
}
/***** End of util.c *****/

/***** From print.c *****/
/* $Id: PRINT.C 1.1 1994/08/30 02:27:02 kaz Exp kaz $ */

/*--- forward declarations ---*/
static int dviprt_getmaximalwidth(dviprt_print *);
static int dviprt_flush_buffer(dviprt_print *,uchar far *);
static int dviprt_output_transpose(dviprt_print *,uchar far *,uint);
static int dviprt_output_nontranspose(dviprt_print *,uchar far *,uint);
static int dviprt_output_nontranspose_reverse(dviprt_print *,uchar far *,uint);
static int dviprt_reverse_bits(uchar far *,uint);
static int dviprt_transpose8x8(uchar far *,uint, uchar far *,uint);
static int dviprt_output_expr(dviprt_print *,int,uint,uint);
static int dviprt_default_outputproc(uchar far *,long ,void *);
static long dviprt_getbuffersize(dviprt_print *);

/*--- library functions ---*/
long
dviprt_initlibrary(dviprt_print *pprint,dviprt_cfg_t *pprt,uint width,uint height)
{
  dviprt_encoder *pencode;
  uint pins = pprt->integer[CFG_PINS]*8;

  pprint->printer = pprt;
  height += (pins-1);
  height /= pins;
  height *= pins;
  pprint->bitmap_width = width; /* width by bytes */
  pprint->bitmap_height = height;
  pprint->buffer_width = MIN(width,pprt->integer[CFG_MAXIMAL_UNIT]);
  pprint->page_count = 0;
  pprint->output_bytes = 0;
  pprint->tempbuffer_f = 0;
  pencode = dviprt_getencoder_((int)pprt->integer[CFG_ENCODE]);
  if (pencode == NULL) return CFG_ERROR_NOT_SUPPORTED;
  pprint->encode_getbuffersize_proc = pencode->getworksize;
  pprint->encode_encode_proc = pencode->encode;

  pprint->output_bytes = 0;
  pprint->pstream = NULL;
  pprint->output_proc = NULL;

  if (pprt->integer[CFG_UPPER_POS] & CFG_NON_TRANSPOSE_BIT) {
    pprint->output_maximal_unit =
      (pprt->integer[CFG_UPPER_POS] & CFG_REVERSE_BIT) ?
        dviprt_output_nontranspose_reverse : dviprt_output_nontranspose;
  }
  else
    pprint->output_maximal_unit = dviprt_output_transpose;
  return dviprt_getbuffersize(pprint);
}

int
  dviprt_setstream
#ifdef __PROTOTYPES__
  (dviprt_print *pprint,int (*func)(uchar far *,long ,void*),void *pstream)
#else
(pprint,func,pstream)
dviprt_print *pprint;
int (*func)();
void *pstream;
#endif
{
  if (pprint->page_count) {
    int code = dviprt_output_expr(pprint,CFG_FORM_FEED,0,0);
    if (code < 0) return code;
    pprint->page_count = 0;
  }
  pprint->output_bytes = 0;
  pprint->pstream = pstream;
  pprint->output_proc = (func == NULL) ? dviprt_default_outputproc : func;
  return 0;
}

int
dviprt_setbuffer(dviprt_print *pprint,uchar far *buf)
{
  if (pprint->tempbuffer_f) {
    BufferFree(pprint->encode_buffer);
  }
  pprint->tempbuffer_f = 0;
  if (buf == NULL) {
    long s = dviprt_getbuffersize(pprint);
    if (s) {
      buf = (uchar far *)BufferAlloc(s);
      if (buf == NULL) return CFG_ERROR_MEMORY;
      pprint->tempbuffer_f = 1;
    }
  }
  pprint->encode_buffer = buf;
  pprint->source_buffer =
    buf + pprint->encode_getbuffersize_proc(pprint,dviprt_getmaximalwidth(pprint));
  return 0;
}

int
dviprt_beginpage(dviprt_print *pprint)
{
  int code;
  pprint->device_x = pprint->device_y = 0;
  pprint->bitmap_x = pprint->bitmap_y = 0;
  if (pprint->page_count == 0) { /* bit_image_mode */
    code = dviprt_output_expr(pprint,CFG_BIT_IMAGE_MODE,0,0);
  }
  else { /* form_feed */
    code = dviprt_output_expr(pprint,CFG_FORM_FEED,0,0);
  }
  pprint->page_count++;
  if (code < 0) return code;
  return 0;
}

int
dviprt_outputscanlines(dviprt_print *pprint,uchar far *fb)
{
  dviprt_cfg_t *pprt;
  int code;
  uint bw;

  pprt = pprint->printer;
  bw = pprint->bitmap_width;

  if (pprt->prtcode_size[CFG_SKIP_SPACES] <= 0) {
    pprint->bitmap_x = bw;
    pprint->last_x = 0;
  }
  else {
    uint uw,rw;
    uint mu;
    uint bx,lx;
    uint pins = dviprt_getscanlines(pprint);
    mu = pprt->integer[CFG_MINIMAL_UNIT];
    bx = lx = 0;

    for (rw= bw; rw > 0 ;rw -= uw) {
      uint x,y;
      uchar far *in;

      uw = MIN(mu,rw);
      in = fb + bx;
      for (x = uw; x>0 ; x--) {
        uchar far *test;
        test = in;
        for (y = pins ; y > 0 ;y--) {
          if (*test) goto next_unit;
          test += bw;
        }
        in++;
      }

      if (bx > lx) {
        pprint->bitmap_x = bx;
        pprint->last_x = lx;
        code = dviprt_flush_buffer(pprint,fb); /* output buffered unit */
        if (code < 0) return code;
        lx = pprint->last_x + uw;
      }
      else lx += uw;

    next_unit:
      bx += uw;
    }
    pprint->bitmap_x = bx;
    pprint->last_x = lx;
  }
  code = (pprint->last_x < pprint->bitmap_x) ? dviprt_flush_buffer(pprint,fb) : 0;
  pprint->bitmap_y++;
  return code;
}

int
dviprt_endbitmap(dviprt_print *pprint)
{
  if (pprint->page_count) {
    int code = dviprt_output_expr(pprint,CFG_NORMAL_MODE,0,0);
    if (code < 0) return code;
  }
  return 0;
}

int
dviprt_unsetbuffer(dviprt_print *pprint)
{
  if (pprint->tempbuffer_f) {
    BufferFree(pprint->encode_buffer);
    pprint->tempbuffer_f = 0;
  }
  return 0;
}

int
dviprt_output(dviprt_print *pprint,uchar far *buf,long s)
{
  int c = pprint->output_proc(buf,s,pprint->pstream);
  pprint->output_bytes += s;
  return c;
}

/*--- internal routines ---*/
static int
dviprt_getmaximalwidth(dviprt_print *pprint)
{
  return MIN(pprint->printer->integer[CFG_MAXIMAL_UNIT],pprint->bitmap_width);
}

static long
dviprt_getbuffersize(dviprt_print *pprint)
{
  long w = dviprt_getmaximalwidth(pprint);
  long e = pprint->encode_getbuffersize_proc(pprint,w);
  switch (pprint->printer->integer[CFG_UPPER_POS]
          & (CFG_NON_TRANSPOSE_BIT | CFG_REVERSE_BIT)) {
  case CFG_LEFT_IS_HIGH:
    return e;
  default:
    return w * dviprt_getscanlines(pprint) + e;
  }
}

static int
dviprt_flush_buffer(dviprt_print *pprint,uchar far *fb)
{
  dviprt_cfg_t *pprt;
  int code;

  pprt = pprint->printer;
  while (pprint->device_y < pprint->bitmap_y) { /* skip vertical spaces */
    pprint->device_y++;
    code = dviprt_output_expr(pprint,CFG_LINE_FEED,0,0);
    pprint->device_x = 0;
  }
  while (pprint->last_x < pprint->bitmap_x) {
    int w;
    while (pprint->device_x < pprint->last_x) { /* skip horizontal spaces */
      w = MIN(pprt->integer[CFG_MAXIMAL_UNIT],
              pprint->last_x - pprint->device_x);
      code = dviprt_output_expr(pprint,CFG_SKIP_SPACES,w,0);
      pprint->device_x += w;
    }
    w = MIN(pprt->integer[CFG_MAXIMAL_UNIT],pprint->bitmap_x-pprint->last_x);
    code = pprint->output_maximal_unit(pprint,fb+pprint->last_x,w);
    if (code < 0) return code;
    pprint->last_x += w;
    if (!(pprt->integer[CFG_UPPER_POS] & CFG_NON_MOVING))
      pprint->device_x += w;
  }
  return 0;
}

static int
dviprt_output_nontranspose(dviprt_print *pprint,uchar far *fb,uint width)
{
  int code;
  uint dsize;
  uint y;
  uint pins;

  pins = dviprt_getscanlines(pprint);

  dsize = 0;
  pprint->psource = fb;
  for (y = pins ; y>0 ; y--) {
    int dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,0);
    if (dsize_line < 0) return dsize_line;
    dsize += dsize_line;
    pprint->psource += pprint->bitmap_width;
  }

  code = dviprt_output_expr(pprint,CFG_SEND_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  pprint->psource = fb;
  for (y = pins ; y>0 ; y--) {
    int dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,1);
    code = dviprt_output_expr(pprint,CFG_BIT_ROW_HEADER,width,dsize_line);
    if (code < 0) return code;
    code = dviprt_output(pprint,pprint->poutput,dsize_line);
    if (code < 0) return code;
    pprint->psource += pprint->bitmap_width;
  }

  code = dviprt_output_expr(pprint,CFG_AFTER_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  return 0;
}

static int
dviprt_output_nontranspose_reverse(dviprt_print *pprint,uchar far *fb,uint width)
{
  uchar far *psrc;
  uchar far *pbuf;
  int code;
  uint src_size;
  uint dsize;
  uint y;
  uint pins;

  pins = dviprt_getscanlines(pprint);
  src_size = width * pins;

  psrc = pprint->source_buffer;
  for (y = pins ; y > 0; y--) {
    uint i;
    pbuf = fb;
    for (i=width;i>0;i--) *psrc++ = *pbuf++;
    fb += pprint->bitmap_width;
  }

  /* here, reverse bits */
  psrc = pprint->source_buffer;
  dviprt_reverse_bits(psrc,src_size);

  dsize = 0;
  pprint->psource = pprint->source_buffer;
  for (y = pins ; y>0 ; y--) {
    int dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,0);
    if (dsize_line < 0) return dsize_line;
    dsize += dsize_line;
    pprint->psource += width;
  }

  code = dviprt_output_expr(pprint,CFG_SEND_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  pprint->psource = pprint->source_buffer;
  for (y = pins ; y>0 ; y--) {
    int dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,1);
    code = dviprt_output_expr(pprint,CFG_BIT_ROW_HEADER,width,dsize_line);
    if (code < 0) return code;
    code = dviprt_output(pprint,pprint->poutput,dsize_line);
    if (code < 0) return code;
    pprint->psource += width;
  }

  code = dviprt_output_expr(pprint,CFG_AFTER_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  return 0;
}

static int
dviprt_output_transpose(dviprt_print *pprint,uchar far *fb,uint width)
{
  uchar far *psrc;
  uchar far *pbuf;
  int code;
  uint src_size;
  uint dsize;
  uint pins,pins8;
  int y;

  pins8 = pprint->printer->integer[CFG_PINS];
  pins = pins8 * 8;
  src_size = width * pins;
  psrc = pprint->source_buffer;
  {
    uchar far *pdst;
    uint sskip;
    sskip = pprint->bitmap_width * 8;
    for (y = pins8-1; y >= 0 ; y--) {
      uint i;
      pbuf = fb;
      pdst = psrc;
      for (i=width;i>0;i--) {
        dviprt_transpose8x8(pbuf,pprint->bitmap_width,pdst,pins8);
        pbuf ++;
        pdst += pins;
      }
      fb += sskip;
      psrc++;
    }
  }

  psrc = pprint->source_buffer;

  /* here, reverse bits */
  if (pprint->printer->integer[CFG_UPPER_POS] & CFG_REVERSE_BIT)
    dviprt_reverse_bits(psrc,src_size);

  dsize = 0;
  pprint->psource = pprint->source_buffer;
  for (y = pins ; y>0 ; y--) {
    int dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,0);
    if (dsize_line < 0) return dsize_line;
    dsize += dsize_line;
    pprint->psource += width;
  }

  code = dviprt_output_expr(pprint,CFG_SEND_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  pprint->psource = pprint->source_buffer;
  for (y = pins ; y>0 ; y--) {
    uint dsize_line;
    dsize_line = pprint->encode_encode_proc(pprint,(long)width,1);
    code = dviprt_output(pprint,pprint->poutput,dsize_line);
    if (code < 0) return code;
    pprint->psource += width;
  }

  code = dviprt_output_expr(pprint,CFG_AFTER_BIT_IMAGE,width,dsize);
  if (code < 0) return code;

  return 0;
}

static int
dviprt_transpose8x8(uchar far *inp,uint line_size,uchar far *outp,uint dist)
{
  register uint ae, bf, cg, dh;
  {
    uchar far *ptr4 = inp + (line_size << 2);
    ae = ((uint)*inp << 8) + *ptr4;
    inp += line_size, ptr4 += line_size;
    bf = ((uint)*inp << 8) + *ptr4;
    inp += line_size, ptr4 += line_size;
    cg = ((uint)*inp << 8) + *ptr4;
    inp += line_size, ptr4 += line_size;
    dh = ((uint)*inp << 8) + *ptr4;
  }
  /* Check for all 8 bytes being the same. */
  /* This is especially worth doing for the case where all are zero. */
  if ( ae == bf && ae == cg && ae == dh && (ae >> 8) == (ae & 0xff) ) {
    if ( ae == 0 ) goto store;
    *outp = -((ae >> 7) & 1);
    outp += dist;
    *outp = -((ae >> 6) & 1);
    outp += dist;
    *outp = -((ae >> 5) & 1);
    outp += dist;
    *outp = -((ae >> 4) & 1);
    outp += dist;
    *outp = -((ae >> 3) & 1);
    outp += dist;
    *outp = -((ae >> 2) & 1);
    outp += dist;
    *outp = -((ae >> 1) & 1);
    outp += dist;
    *outp = -(ae & 1);
  }
  else {
    register uint temp;

    /* Transpose a block of bits between registers. */
#define transpose(r,s,mask,shift)\
    r ^= (temp = ((s >> shift) ^ r) & mask);\
      s ^= temp << shift

        /* Transpose blocks of 4 x 4 */
#define transpose4(r) transpose(r,r,0x00f0,4)
        transpose4(ae);
    transpose4(bf);
    transpose4(cg);
    transpose4(dh);

    /* Transpose blocks of 2 x 2 */
    transpose(ae, cg, 0x3333, 2);
    transpose(bf, dh, 0x3333, 2);

    /* Transpose blocks of 1 x 1 */
    transpose(ae, bf, 0x5555, 1);
    transpose(cg, dh, 0x5555, 1);

  store:	*outp = ae >> 8;
    outp += dist;
    *outp = bf >> 8;
    outp += dist;
    *outp = cg >> 8;
    outp += dist;
    *outp = dh >> 8;
    outp += dist;
    *outp = (uchar)ae;
    outp += dist;
    *outp = (uchar)bf;
    outp += dist;
    *outp = (uchar)cg;
    outp += dist;
    *outp = (uchar)dh;
  }
  return 0;
}

static int
dviprt_reverse_bits(uchar far *buf,uint s)
{
  static uchar rev[256] = {
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
    0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
    0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
    0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
    0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
    0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
    0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
    0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
    0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
    0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
    0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
    0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
    0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
    0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
    0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
    0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
    0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
  };
  while (s-- > 0) {
    *buf = rev[*buf];
    buf++;
  }
  return 0;
}

static int
dviprt_output_expr(dviprt_print *pprint,int numb,uint width,uint dsize)
{
  uchar *pcode;
  dviprt_cfg_t *pprt;
  uint v;
  uint len;

  pprt = pprint->printer;
  if (pprt->prtcode[numb] == NULL) return 0;
  pcode = pprt->prtcode[numb];
  len = pprt->prtcode_size[numb];

  while (*pcode && len>0) {
    if (*pcode & CFG_FMT_BIT) {
      uint stack[CFG_STACK_DEPTH];
      int stack_p = -1;
      uchar fmt = *pcode++;
      uint l = *pcode++;
      len -=2;
      while (l>0) {
        if ((*pcode & CFG_EXPR_TYPE_BIT) == CFG_OP) {
          uint v2 = stack[stack_p--];
          v = stack[stack_p--];
          switch (*pcode) {
          case CFG_OP_ADD: v += v2; break;
          case CFG_OP_SUB: v -= v2; break;
          case CFG_OP_MUL: v *= v2; break;
          case CFG_OP_DIV:
            if (v2 == 0) { /* divided by zero. */
            div_by_zero:
              return CFG_ERROR_DIV0;
            }
            v /= v2;
            break;
          case CFG_OP_MOD:
            if (v2 == 0) goto div_by_zero;
            v %= v2;
            break;
          case CFG_OP_SHL: v <<= v2; break;
          case CFG_OP_SHR: v >>= v2; break;
          case CFG_OP_AND: v &= v2; break;
          case CFG_OP_OR : v |= v2; break;
          case CFG_OP_XOR: v ^= v2; break;
          default:
            ;
          }
          stack[++stack_p] = v;
        }
        else if ((*pcode & CFG_EXPR_TYPE_BIT) == CFG_VAL) {
          {
            switch (*pcode) {
            case CFG_VAL_DEFAULT: v = width*8; break;
            case CFG_VAL_CONSTANT: v = pprt->integer[CFG_CONSTANT]; break;
            case CFG_VAL_WIDTH: v = pprint->bitmap_width*8; break;
            case CFG_VAL_HEIGHT: v = pprint->bitmap_height; break;
            case CFG_VAL_PAGE:
            case CFG_VAL_PAGECOUNT: v = pprint->page_count; break;
            case CFG_VAL_DATASIZE: v = dsize; break;
            case CFG_VAL_X_DPI: v = (int)pprt->integer[CFG_DPI]; break;
            case CFG_VAL_Y_DPI:
              v = pprt->integer[CFG_Y_DPI] > 0 ?
                pprt->integer[CFG_Y_DPI] : pprt->integer[CFG_DPI];
              break;
            case CFG_VAL_PINS_BYTE: v = pprt->integer[CFG_PINS]; break;
            case CFG_VAL_X_POS: v = pprint->device_x*8; break;
            case CFG_VAL_Y_POS:
              v = pprint->device_y * dviprt_getscanlines(pprint);
              break;
            }
          }
          stack[++stack_p] = v;
        }
        else {
          stack[++stack_p] = *pcode;
        }
        l--; pcode++; len--;
      }
      v = stack[stack_p];
      if ((fmt & CFG_FMT_FORMAT_BIT) == CFG_FMT_STRINGS) {
        uint l = *pcode++;
        len--;
        while (v-- > 0)
          dviprt_output(pprint,(uchar far *)pcode,l);
        pcode += l;
        len -= l;
      }
      else if ((fmt & CFG_FMT_FORMAT_BIT) == CFG_FMT_ASSIGNMENT) {
        pprint->uservar[fmt&0x0f] = v;
      }
      else { uchar valbuf[10];
             int cols = fmt & CFG_FMT_COLUMN_BIT;
             int i;

             switch (fmt & CFG_FMT_FORMAT_BIT) {
             case CFG_FMT_BIN_LTOH:
               for (i=0;i<cols;i++) {
                 valbuf[i] = v&0xff;
                 v >>= 8;
               }
               break;
             case CFG_FMT_BIN_HTOL:
               for (i=cols-1;i>=0;i--) {
                 valbuf[i] = v&0xff;
                 v >>= 8;
               }
               break;
             default:
               { char *f;
                 char fmtbuf[10];
                 switch(fmt & CFG_FMT_FORMAT_BIT) {
                 case CFG_FMT_HEX_UPPER: f = "X"; break;
                 case CFG_FMT_HEX_LOWER: f = "x"; break;
                 case CFG_FMT_DECIMAL: f = "u"; break;
                 case CFG_FMT_OCTAL: f = "o"; break;
                 }
                 if (cols == 0)
                   strcpy(fmtbuf,"%");
                 else
                   gs_sprintf(fmtbuf,"%%0%d",cols);
                 strcat(fmtbuf,f);
                 gs_sprintf(valbuf,fmtbuf,stack[stack_p]);
                 cols = strlen(valbuf);
                 if (fmt & CFG_FMT_ISO_BIT)
                   valbuf[cols-1] |= 0x10;
               }
               break;
             }
             dviprt_output(pprint,(uchar far *)valbuf, cols);
           }
    }
    else {
      uint l = *pcode++;
      len--;
      dviprt_output(pprint,(uchar far *)pcode,l);
      pcode += l;
      len -= l;
    }
  }
  return 0;
}

static int
dviprt_default_outputproc(uchar far *buf,long s,void *fp)
{
#ifdef __MSDOS_REAL__
  while (s-- > 0) {
    if (fputc(*buf++,fp) == EOF)
      return CFG_ERROR_OUTPUT;
  }
  return 0;
#else
  return fwrite(buf,s,1,fp) == 1 ? 0 : CFG_ERROR_OUTPUT;
#endif
}
/***** End of print.c *****/

/***** From encode.c *****/
/* $Id: ENCODE.C 1.1 1994/08/30 02:27:02 kaz Exp kaz $ */

#define DVIPRT_SUPPORT_FAX 1
#define DVIPRT_SUPPORT_PCL 1

/*--- forward declarations ---*/
static long dviprt_null_getworksize(dviprt_print *,long );
static long dviprt_null_encode(dviprt_print *,long ,int );
static long dviprt_hex_getworksize(dviprt_print *,long );
static long dviprt_hex_encode(dviprt_print *,long ,int );
#if DVIPRT_SUPPORT_FAX
static long dviprt_fax_getworksize(dviprt_print *,long );
static long dviprt_fax_encode(dviprt_print *,long ,int );
#endif
#if DVIPRT_SUPPORT_PCL
static long dviprt_pcl1_getworksize(dviprt_print *,long );
static long dviprt_pcl1_encode(dviprt_print *,long ,int );
static long dviprt_pcl2_getworksize(dviprt_print *,long );
static long dviprt_pcl2_encode(dviprt_print *,long ,int );
#endif

static dviprt_encoder dviprt_encoder_list[] = {
  { CFG_ENCODE_NULL, dviprt_null_getworksize,dviprt_null_encode},
  { CFG_ENCODE_HEX, dviprt_hex_getworksize,dviprt_hex_encode},
#if DVIPRT_SUPPORT_FAX
  { CFG_ENCODE_FAX, dviprt_fax_getworksize,dviprt_fax_encode},
#endif
#if DVIPRT_SUPPORT_PCL
  { CFG_ENCODE_PCL1, dviprt_pcl1_getworksize,dviprt_pcl1_encode},
  { CFG_ENCODE_PCL2, dviprt_pcl2_getworksize,dviprt_pcl2_encode},
#endif
  { -1 },
};

/*--- internal routines ---*/
/* The users MUST NOT USE these functions */
static dviprt_encoder *
dviprt_getencoder_(int no)
{
  int i;
  for (i=0;dviprt_encoder_list[i].no >= 0;i++)
    if (no == dviprt_encoder_list[i].no)
      return dviprt_encoder_list+i;
  return NULL;
}

static long
dviprt_null_getworksize(dviprt_print *pprint,long s)
{
  return 0;
}
static long
dviprt_null_encode(dviprt_print *pprint,long s,int f)
{
  pprint->poutput = pprint->psource;
  return s;
}

static long
dviprt_hex_getworksize(dviprt_print *pprint,long s)
{
  return s*2;
}
static long
dviprt_hex_encode(dviprt_print *pprint,long s,int f)
{
  if (f) {
    uchar far *w;
    uchar far *buf;
    static uchar hex[] = "0123456789ABCDEF";
    int i=s;
    w = pprint->poutput = pprint->encode_buffer;
    buf = pprint->psource;
    while (i-->0) {
      *w++ = hex[(*buf>>4)&0x0f];
      *w++ = hex[*buf&0x0f];
      buf++;
    }
  }
  return s*2;
}

#if DVIPRT_SUPPORT_PCL
static long
dviprt_pcl1_getworksize(dviprt_print *pprint,long s)
{
  return s*2;
}
static long
dviprt_pcl1_encode(dviprt_print *pprint,long s,int f)
{
  uchar far *src;
  uchar far *end;
  uchar far *out;
  long total = 0;

  src = pprint->psource;
  end = src + s;
  out = pprint->poutput = pprint->encode_buffer;

  while ( src < end  ) {
    uchar test = *src++;
    uchar far *run = src;
    while ( src < end && *src == test ) src++;
    if (f) {
      while ( src - run > 255 ) {
        *out++ = 255;
        *out++ = test;
        total += 2;
        run += 256;
      }
      *out++ = src - run;
      *out++ = test;
      total += 2;
    }
    else {
      total += (((src-run)/255 + ((src-run)%255) ? 1 : 0)) * 2;
    }
  }
  return total;
}

static long
dviprt_pcl2_getworksize(dviprt_print *pprint,long s)
{
  return s + s/127 + 1;
}
static long
dviprt_pcl2_encode(dviprt_print *pprint,long s,int f)
{
  uchar far *exam;
  uchar far *cptr;
  uchar far *end;
  uchar far *src;
  long total = 0;

  src = pprint->psource;
  exam = src;
  cptr = pprint->poutput = pprint->encode_buffer;
  end = exam + s;

  for ( ; ; ) {
    uchar test = *exam++;
    int len;
    while ((test != *exam) && (exam < end))
      test = *exam++;
    if (exam < end) exam--;
    len = exam - src;
    if (f) {
      while (len > 0){
        int i;
        int count = len;
        if (count>127) count=127;
        *cptr++=count-1;
        for (i = 0 ; i < count ; i++) *cptr++ = *src++;
        len -= count;
        total += (count+1);
      }
    }
    else {
      total += len + ((len/127)+(len%127 ? 1 : 0));
    }
    if (exam >= end) break;
    exam++;
    while ((test == *exam) && (exam < end))
      exam++;
    len = exam - src;
    if (f) {
      while (len > 0) {
        int count = len;
        if (count > 127) count = 127;
        *cptr++ = (257-count);
        *cptr++ = test;
        len -= count;
        total += 2;
      }
    }
    else {
      total += ((len/127 + (len%127) ? 1 : 0)) * 2;
    }
    if (exam >= end) break;
    src = exam;
  }
  return total;
}
#endif /* DVIPRT_SUPPORT_PCL */

#if DVIPRT_SUPPORT_FAX
static long
dviprt_fax_getworksize(dviprt_print *pprint,long s)
{
  long size = s * 8 + 7;
  return MAX(size*2/9+4,size/8) * dviprt_getscanlines(pprint) + 1;
}

#define MAX_FAX_WIDTH	2623
typedef struct {
  int	data;
  int	length;
} FaxEncode_t;
typedef struct {
  uchar i_bitbuf;
  uchar far *i_buf;
  int i_count;

  uchar o_bitbuf;
  uchar far *o_buf;
  int o_count;
  int o_bufcount;
} FaxEncodeInfo;
static int dviprt_fax_set_white(int,FaxEncodeInfo *);
static int dviprt_fax_set_black(int,FaxEncodeInfo *);
static int dviprt_fax_set_bitcount(FaxEncode_t *,FaxEncodeInfo *);

static long
dviprt_fax_encode(dviprt_print *pprint,long s,int f)
{
  static FaxEncode_t eol_code = {0x800,12};
  int allruns = 0;
  int width = s * 8;
  int top_bit_count = 8;
  FaxEncodeInfo info;
  uchar far *src_end;
  uchar recover;

  src_end = pprint->psource + s;
  recover = *src_end;
  *src_end = 0xaa;

  /* initializing */
  info.i_buf = pprint->psource;
  info.i_bitbuf = *info.i_buf++;
  info.i_count = 8;
  info.o_buf = pprint->poutput = pprint->encode_buffer;
  info.o_bitbuf = 0;
  info.o_count = 8;
  info.o_bufcount = 0;

  dviprt_fax_set_bitcount(&eol_code,&info);

  for(;;){
    static uchar ROW[9] =
      {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    static uchar MASK[9] =
      {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
    int white_run_length;
    int black_run_length;

    /* count run-length */
    /* remaining bits in the current byte are white? */
    if (!(info.i_bitbuf &= MASK[info.i_count])){
      do{
        top_bit_count += 8; /* next byte is also white	*/
      } while(!(info.i_bitbuf = *info.i_buf++));
      info.i_count = 8;
    }
    /* current byte contains white and black bits 	*/
    while(!(info.i_bitbuf & ROW[info.i_count]))
      info.i_count--;		/* skip white bits 	*/
    white_run_length = top_bit_count - (black_run_length = info.i_count);

    /* remaining bits in the current byte are black?	*/
    if (info.i_bitbuf == MASK[info.i_count]){
      do{
        black_run_length += 8;
        /* next byte is also black	*/
      } while((info.i_bitbuf = *info.i_buf++) == 0xff);
      info.i_count = 8;
    }
    else info.i_count--;	/* skip the top black bit	*/

    /* current byte contains white and black bits 	*/
    while(info.i_bitbuf & ROW[info.i_count])
      info.i_count--;		/* skip black bits	 		*/
    black_run_length -= (top_bit_count = info.i_count);

    /* output */
    if((allruns += white_run_length) < width)
      dviprt_fax_set_white(white_run_length,&info);
    else{
      dviprt_fax_set_white(white_run_length + width - allruns,&info);
      break;
    }
    if((allruns += black_run_length) < width)
      dviprt_fax_set_black(black_run_length,&info);
    else{
      dviprt_fax_set_black(black_run_length + width - allruns,&info);
      break;
    }
  }

  info.o_bufcount++;
  if (info.o_count < 8)
    *info.o_buf++ = info.o_bitbuf;
  else
    *info.o_buf++ = 0;
  *src_end = recover;

  return info.o_bufcount;
}

static int
dviprt_fax_set_bitcount(FaxEncode_t *pt,FaxEncodeInfo *info)
{
  int	data, length;

  data = pt->data;
  length = pt->length;
  while( (length -= info->o_count) >= 0 ){
    *info->o_buf++ = ((data << (8 - info->o_count))|info->o_bitbuf);
    info->o_bitbuf = 0;
    info->o_bufcount++;
    data >>= info->o_count;
    info->o_count = 8;
  }
  info->o_bitbuf |= (data << (8 - info->o_count));
  info->o_count = -length;
  return 0;
}

static int
dviprt_fax_set_white(int count,FaxEncodeInfo *info)
{
  static FaxEncode_t white_count_list[]={
    { 0x00AC,      8,	}, /*    0 */
    { 0x0038,      6,	}, /*    1 */
    { 0x000E,      4,	}, /*    2 */
    { 0x0001,      4,	}, /*    3 */
    { 0x000D,      4,	}, /*    4 */
    { 0x0003,      4,	}, /*    5 */
    { 0x0007,      4,	}, /*    6 */
    { 0x000F,      4,	}, /*    7 */
    { 0x0019,      5,	}, /*    8 */
    { 0x0005,      5,	}, /*    9 */
    { 0x001C,      5,	}, /*   10 */
    { 0x0002,      5,	}, /*   11 */
    { 0x0004,      6,	}, /*   12 */
    { 0x0030,      6,	}, /*   13 */
    { 0x000B,      6,	}, /*   14 */
    { 0x002B,      6,	}, /*   15 */
    { 0x0015,      6,	}, /*   16 */
    { 0x0035,      6,	}, /*   17 */
    { 0x0072,      7,	}, /*   18 */
    { 0x0018,      7,	}, /*   19 */
    { 0x0008,      7,	}, /*   20 */
    { 0x0074,      7,	}, /*   21 */
    { 0x0060,      7,	}, /*   22 */
    { 0x0010,      7,	}, /*   23 */
    { 0x000A,      7,	}, /*   24 */
    { 0x006A,      7,	}, /*   25 */
    { 0x0064,      7,	}, /*   26 */
    { 0x0012,      7,	}, /*   27 */
    { 0x000C,      7,	}, /*   28 */
    { 0x0040,      8,	}, /*   29 */
    { 0x00C0,      8,	}, /*   30 */
    { 0x0058,      8,	}, /*   31 */
    { 0x00D8,      8,	}, /*   32 */
    { 0x0048,      8,	}, /*   33 */
    { 0x00C8,      8,	}, /*   34 */
    { 0x0028,      8,	}, /*   35 */
    { 0x00A8,      8,	}, /*   36 */
    { 0x0068,      8,	}, /*   37 */
    { 0x00E8,      8,	}, /*   38 */
    { 0x0014,      8,	}, /*   39 */
    { 0x0094,      8,	}, /*   40 */
    { 0x0054,      8,	}, /*   41 */
    { 0x00D4,      8,	}, /*   42 */
    { 0x0034,      8,	}, /*   43 */
    { 0x00B4,      8,	}, /*   44 */
    { 0x0020,      8,	}, /*   45 */
    { 0x00A0,      8,	}, /*   46 */
    { 0x0050,      8,	}, /*   47 */
    { 0x00D0,      8,	}, /*   48 */
    { 0x004A,      8,	}, /*   49 */
    { 0x00CA,      8,	}, /*   50 */
    { 0x002A,      8,	}, /*   51 */
    { 0x00AA,      8,	}, /*   52 */
    { 0x0024,      8,	}, /*   53 */
    { 0x00A4,      8,	}, /*   54 */
    { 0x001A,      8,	}, /*   55 */
    { 0x009A,      8,	}, /*   56 */
    { 0x005A,      8,	}, /*   57 */
    { 0x00DA,      8,	}, /*   58 */
    { 0x0052,      8,	}, /*   59 */
    { 0x00D2,      8,	}, /*   60 */
    { 0x004C,      8,	}, /*   61 */
    { 0x00CC,      8,	}, /*   62 */
    { 0x002C,      8,	}, /*   63 */

    { 0x001B,      5,	}, /*   64 */
    { 0x0009,      5,	}, /*  128 */
    { 0x003A,      6,	}, /*  192 */
    { 0x0076,      7,	}, /*  256 */
    { 0x006C,      8,	}, /*  320 */
    { 0x00EC,      8,	}, /*  384 */
    { 0x0026,      8,	}, /*  448 */
    { 0x00A6,      8,	}, /*  512 */
    { 0x0016,      8,	}, /*  576 */
    { 0x00E6,      8,	}, /*  640 */
    { 0x0066,      9,	}, /*  704 */
    { 0x0166,      9,	}, /*  768 */
    { 0x0096,      9,	}, /*  832 */
    { 0x0196,      9,	}, /*  896 */
    { 0x0056,      9,	}, /*  960 */
    { 0x0156,      9,	}, /* 1024 */
    { 0x00D6,      9,	}, /* 1088 */
    { 0x01D6,      9,	}, /* 1152 */
    { 0x0036,      9,	}, /* 1216 */
    { 0x0136,      9,	}, /* 1280 */
    { 0x00B6,      9,	}, /* 1344 */
    { 0x01B6,      9,	}, /* 1408 */
    { 0x0032,      9,	}, /* 1472 */
    { 0x0132,      9,	}, /* 1536 */
    { 0x00B2,      9,	}, /* 1600 */
    { 0x0006,      6,	}, /* 1664 */
    { 0x01B2,      9,	}, /* 1728 */

    { 0x0080,     11,	}, /* 1792 */
    { 0x0180,     11,	}, /* 1856 */
    { 0x0580,     11,	}, /* 1920 */
    { 0x0480,     12,	}, /* 1984 */
    { 0x0C80,     12,	}, /* 2048 */
    { 0x0280,     12,	}, /* 2112 */
    { 0x0A80,     12,	}, /* 2176 */
    { 0x0680,     12,	}, /* 2240 */
    { 0x0E80,     12,	}, /* 2304 */
    { 0x0380,     12,	}, /* 2368 */
    { 0x0B80,     12,	}, /* 2432 */
    { 0x0780,     12,	}, /* 2496 */
    { 0x0F80,     12,	}, /* 2560 */
  };
  while (count >= 64){
    if(count <= MAX_FAX_WIDTH){
      dviprt_fax_set_bitcount((white_count_list + 63) + (count/64),info);
      break;
    }
    dviprt_fax_set_white(MAX_FAX_WIDTH,info);
    dviprt_fax_set_black(0,info);
    count -= MAX_FAX_WIDTH;
  }
  dviprt_fax_set_bitcount(white_count_list + (count & 63) ,info);
  return 0;
}

static int
dviprt_fax_set_black(int count,FaxEncodeInfo *info)
{
  static FaxEncode_t black_count_list[]={
    { 0x03B0,     10,	}, /*    0 */
    { 0x0002,      3,	}, /*    1 */
    { 0x0003,      2,	}, /*    2 */
    { 0x0001,      2,	}, /*    3 */
    { 0x0006,      3,	}, /*    4 */
    { 0x000C,      4,	}, /*    5 */
    { 0x0004,      4,	}, /*    6 */
    { 0x0018,      5,	}, /*    7 */
    { 0x0028,      6,	}, /*    8 */
    { 0x0008,      6,	}, /*    9 */
    { 0x0010,      7,	}, /*   10 */
    { 0x0050,      7,	}, /*   11 */
    { 0x0070,      7,	}, /*   12 */
    { 0x0020,      8,	}, /*   13 */
    { 0x00E0,      8,	}, /*   14 */
    { 0x0030,      9,	}, /*   15 */
    { 0x03A0,     10,	}, /*   16 */
    { 0x0060,     10,	}, /*   17 */
    { 0x0040,     10,	}, /*   18 */
    { 0x0730,     11,	}, /*   19 */
    { 0x00B0,     11,	}, /*   20 */
    { 0x01B0,     11,	}, /*   21 */
    { 0x0760,     11,	}, /*   22 */
    { 0x00A0,     11,	}, /*   23 */
    { 0x0740,     11,	}, /*   24 */
    { 0x00C0,     11,	}, /*   25 */
    { 0x0530,     12,	}, /*   26 */
    { 0x0D30,     12,	}, /*   27 */
    { 0x0330,     12,	}, /*   28 */
    { 0x0B30,     12,	}, /*   29 */
    { 0x0160,     12,	}, /*   30 */
    { 0x0960,     12,	}, /*   31 */
    { 0x0560,     12,	}, /*   32 */
    { 0x0D60,     12,	}, /*   33 */
    { 0x04B0,     12,	}, /*   34 */
    { 0x0CB0,     12,	}, /*   35 */
    { 0x02B0,     12,	}, /*   36 */
    { 0x0AB0,     12,	}, /*   37 */
    { 0x06B0,     12,	}, /*   38 */
    { 0x0EB0,     12,	}, /*   39 */
    { 0x0360,     12,	}, /*   40 */
    { 0x0B60,     12,	}, /*   41 */
    { 0x05B0,     12,	}, /*   42 */
    { 0x0DB0,     12,	}, /*   43 */
    { 0x02A0,     12,	}, /*   44 */
    { 0x0AA0,     12,	}, /*   45 */
    { 0x06A0,     12,	}, /*   46 */
    { 0x0EA0,     12,	}, /*   47 */
    { 0x0260,     12,	}, /*   48 */
    { 0x0A60,     12,	}, /*   49 */
    { 0x04A0,     12,	}, /*   50 */
    { 0x0CA0,     12,	}, /*   51 */
    { 0x0240,     12,	}, /*   52 */
    { 0x0EC0,     12,	}, /*   53 */
    { 0x01C0,     12,	}, /*   54 */
    { 0x0E40,     12,	}, /*   55 */
    { 0x0140,     12,	}, /*   56 */
    { 0x01A0,     12,	}, /*   57 */
    { 0x09A0,     12,	}, /*   58 */
    { 0x0D40,     12,	}, /*   59 */
    { 0x0340,     12,	}, /*   60 */
    { 0x05A0,     12,	}, /*   61 */
    { 0x0660,     12,	}, /*   62 */
    { 0x0E60,     12,	}, /*   63 */

    { 0x03C0,     10,	}, /*   64 */
    { 0x0130,     12,	}, /*  128 */
    { 0x0930,     12,	}, /*  192 */
    { 0x0DA0,     12,	}, /*  256 */
    { 0x0CC0,     12,	}, /*  320 */
    { 0x02C0,     12,	}, /*  384 */
    { 0x0AC0,     12,	}, /*  448 */
    { 0x06C0,     13,	}, /*  512 */
    { 0x16C0,     13,	}, /*  576 */
    { 0x0A40,     13,	}, /*  640 */
    { 0x1A40,     13,	}, /*  704 */
    { 0x0640,     13,	}, /*  768 */
    { 0x1640,     13,	}, /*  832 */
    { 0x09C0,     13,	}, /*  896 */
    { 0x19C0,     13,	}, /*  960 */
    { 0x05C0,     13,	}, /* 1024 */
    { 0x15C0,     13,	}, /* 1088 */
    { 0x0DC0,     13,	}, /* 1152 */
    { 0x1DC0,     13,	}, /* 1216 */
    { 0x0940,     13,	}, /* 1280 */
    { 0x1940,     13,	}, /* 1344 */
    { 0x0540,     13,	}, /* 1408 */
    { 0x1540,     13,	}, /* 1472 */
    { 0x0B40,     13,	}, /* 1536 */
    { 0x1B40,     13,	}, /* 1600 */
    { 0x04C0,     13,	}, /* 1664 */
    { 0x14C0,     13, }, /* 1728 */

    { 0x0080,     11,	}, /* 1792 */
    { 0x0180,     11,	}, /* 1856 */
    { 0x0580,     11,	}, /* 1920 */
    { 0x0480,     12,	}, /* 1984 */
    { 0x0C80,     12,	}, /* 2048 */
    { 0x0280,     12,	}, /* 2112 */
    { 0x0A80,     12,	}, /* 2176 */
    { 0x0680,     12,	}, /* 2240 */
    { 0x0E80,     12,	}, /* 2304 */
    { 0x0380,     12,	}, /* 2368 */
    { 0x0B80,     12,	}, /* 2432 */
    { 0x0780,     12,	}, /* 2496 */
    { 0x0F80,     12,	}, /* 2560 */
  };

  while (count >= 64){
    if(count <= MAX_FAX_WIDTH){
      dviprt_fax_set_bitcount((black_count_list + 63) + (count/64),info);
      break;
    }
    dviprt_fax_set_black(MAX_FAX_WIDTH,info);
    dviprt_fax_set_white(0,info);
    count -= MAX_FAX_WIDTH;
  }
  dviprt_fax_set_bitcount(black_count_list + (count & 63),info);
  return 0;
}
#endif /* DVIPRT_SUPPORT_FAX */
/***** End of encode.c *****/
