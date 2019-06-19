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

/* dviprlib.h */

/* $Id: S_CFGBLD.H 1.1 1994/08/30 02:27:18 kaz Exp kaz $ */

#ifndef s_cfgbld_h_INCLUDED

/* $Id: S_CFG.H 1.1 1994/08/30 02:27:18 kaz Exp kaz $ */

#ifndef s_cfg_h_INCLUDED
#define CFG_MAGIC_NUMBER   ":K"
#define CFG_VERSION       2

#define CFG_NAME          0
#define CFG_ENCODE_INFO   1
#define CFG_STRINGS_TYPE_COUNT 2

#define CFG_STRINGS_NAME "name", "encode_info"

#define CFG_PINS           0
#define CFG_MINIMAL_UNIT   1
#define CFG_MAXIMAL_UNIT   2
#define CFG_DPI            3
#define CFG_UPPER_POS      4
#define CFG_ENCODE         5
#define CFG_CONSTANT       6
#define CFG_Y_DPI          7
#define CFG_INTEGER_TYPE_COUNT 8

#define CFG_INTEGER_NAME  "pins","minimal_unit","maximal_unit","dpi",\
"upper_position","encode","constant","y_dpi"

#define CFG_BIT_IMAGE_MODE  0
#define CFG_SEND_BIT_IMAGE  1
#define CFG_BIT_ROW_HEADER  2
#define CFG_AFTER_BIT_IMAGE 3
#define CFG_LINE_FEED       4
#define CFG_FORM_FEED       5
#define CFG_NORMAL_MODE     6
#define CFG_SKIP_SPACES     7
#define CFG_PRTCODE_TYPE_COUNT 8

#define CFG_PRTCODE_NAME "bit_image_mode", "send_bit_image", "bit_row_header",\
  "after_bit_image", "line_feed", "form_feed",\
  "normal_mode", "skip_spaces"

#define CFG_FMT_BIT         0x80
#define CFG_FMT_FORMAT_BIT  0x70
#define CFG_FMT_COLUMN_BIT  0x07
#define CFG_FMT_ISO_BIT     0x08

#define CFG_FMT_BIN_LTOH    0x00
#define CFG_FMT_BIN_HTOL    0x10
#define CFG_FMT_HEX_UPPER   0x20
#define CFG_FMT_HEX_LOWER   0x30
#define CFG_FMT_DECIMAL     0x40
#define CFG_FMT_OCTAL       0x50
#define CFG_FMT_ASSIGNMENT  0x60  /* assignment(user variables) */
#define CFG_FMT_STRINGS     0x70

#define CFG_EXPR_TYPE_BIT   0xe0
#define CFG_EXPR_VAL_BIT    0x1f
#define CFG_VAL           0x80
#define CFG_VAL_DEFAULT   (0x00 | CFG_VAL)
#define CFG_VAL_CONSTANT  (0x01 | CFG_VAL)
#define CFG_VAL_WIDTH     (0x02 | CFG_VAL)
#define CFG_VAL_HEIGHT    (0x03 | CFG_VAL)
#define CFG_VAL_PAGE      (0x04 | CFG_VAL)
#define CFG_VAL_PAGECOUNT (0x05 | CFG_VAL)
#define CFG_VAL_DATASIZE  (0x06 | CFG_VAL)
#define CFG_VAL_X_DPI     (0x07 | CFG_VAL)
#define CFG_VAL_Y_DPI     (0x08 | CFG_VAL)
#define CFG_VAL_PINS_BYTE (0x09 | CFG_VAL)
#define CFG_VAL_X_POS     (0x0a | CFG_VAL)
#define CFG_VAL_Y_POS     (0x0b | CFG_VAL)

/* user variable 'A' -- 'P' (0xa0 -- 0xaf) */
#define CFG_USERVAL       (0x20 | CFG_VAL)
#define CFG_USERVAL_COUNT 16

#define CFG_OP  0xc0
/* logic 2 operand */
#define CFG_OP_ADD  (0x00 | CFG_OP)
#define CFG_OP_SUB  (0x01 | CFG_OP)
#define CFG_OP_MUL  (0x02 | CFG_OP)
#define CFG_OP_DIV  (0x03 | CFG_OP)
#define CFG_OP_MOD  (0x04 | CFG_OP)

#define CFG_OP_SHL  (0x05 | CFG_OP)
#define CFG_OP_SHR  (0x06 | CFG_OP)
#define CFG_OP_AND  (0x07 | CFG_OP)
#define CFG_OP_OR   (0x08 | CFG_OP)
#define CFG_OP_XOR  (0x09 | CFG_OP)
/* extra operation (user cannot use) */
#define CFG_OP_POP  (0x2e | CFG_OP)
#define CFG_OP_NULL (0x2f | CFG_OP)

#define CFG_OP_NAME	"+","-","*","/",\
"%","<",">","&",\
  "|","^",\
  /* 0x0a -- 0x0f */	NULL,NULL,\
  /* Undefined */	NULL,NULL,NULL,NULL,\
  \
  /* 0x10 -- 0x1f*/	NULL,NULL,NULL,NULL,\
  /* Undefined */	NULL,NULL,NULL,NULL,\
  NULL,NULL,NULL,NULL,\
  NULL,NULL,NULL,NULL,\
  \
  /* 0x20 -- 0x2d*/	NULL,NULL,NULL,NULL,\
  /* Undefined */	NULL,NULL,NULL,NULL,\
  NULL,NULL,NULL,NULL,\
  NULL,NULL,\
  \
  "pop","null"

#define CFG_TOP_IS_HIGH  0x00
#define CFG_TOP_IS_LOW   0x80
#define CFG_LEFT_IS_HIGH 0x40
#define CFG_LEFT_IS_LOW  0xc0
#define CFG_NON_MOVING   0x20
#define CFG_NON_TRANSPOSE_BIT 0x40
#define CFG_REVERSE_BIT       0x80
#define CFG_PIN_POSITION_BITS 0xc0

#define CFG_ENCODE_NAME "NULL","HEX","FAX","PCL1","PCL2"
#define CFG_ENCODE_NULL  0x00
#define CFG_ENCODE_HEX   0x01
#define CFG_ENCODE_FAX   0x02
#define CFG_ENCODE_PCL1  0x03
#define CFG_ENCODE_PCL2  0x04
#define CFG_ENCODE_LIPS3 0x05    /* Not implemented yet. */
#define CFG_ENCODE_ESCPAGE 0x06  /* Not implemented yet. */
#define CFG_ENCODE_COUNT 0x05

#define CFG_STACK_DEPTH  20

#define CFG_ERROR_OTHER        -1

#define CFG_ERROR_SYNTAX       -2
#define CFG_ERROR_RANGE        -3
#define CFG_ERROR_TYPE         -4

#define CFG_ERROR_FILE_OPEN    -5
#define CFG_ERROR_OUTPUT       -6
#define CFG_ERROR_MEMORY       -7
#define CFG_ERROR_DIV0         -8
#define CFG_ERROR_NOT_SUPPORTED -9

#if defined(MSDOS) || defined(_MSDOS)
#  ifndef __MSDOS__
#    define __MSDOS__
#  endif
#endif
#if defined(__MSDOS__) || defined(__STDC__)
#  ifndef __PROTOTYPES__
#    define __PROTOTYPES__
#  endif
#endif
#ifdef __NO_PROTOTYPES__
#  undef __PROTOTYPES__
#endif

#if defined(__MSDOS__) && !defined(__GNUC__) && !defined(__WATCOMC__)
# ifndef __MSDOS_REAL__
#   define __MSDOS_REAL__
# endif
#else
# ifndef far
#   define far /* */
# endif
#endif

typedef struct {
  unsigned int version;

  long integer[CFG_INTEGER_TYPE_COUNT];
  unsigned char *strings[CFG_STRINGS_TYPE_COUNT];
  unsigned char *prtcode[CFG_PRTCODE_TYPE_COUNT];
  unsigned int prtcode_size[CFG_PRTCODE_TYPE_COUNT];
} dviprt_cfg_t;

typedef struct dviprt_print_s dviprt_print;
struct dviprt_print_s {
  dviprt_cfg_t *printer;
  unsigned int bitmap_width;
  unsigned int bitmap_height;
  unsigned int buffer_width;

  unsigned int device_x;
  unsigned int device_y;
  unsigned int bitmap_x;
  unsigned int bitmap_y;
  unsigned int last_x;

  int page_count;
  unsigned char far *source_buffer;
  unsigned char far *encode_buffer;
  unsigned char far *psource;
  unsigned char far *poutput;
  int tempbuffer_f;

#ifdef __PROTOTYPES__
  int (*output_proc)(unsigned char far *,long ,void *);
  int (*output_maximal_unit)(dviprt_print *,unsigned char far *,unsigned int );
  long (*encode_getbuffersize_proc)(dviprt_print *,long);
  long (*encode_encode_proc)(dviprt_print *,long ,int);
#else
  int (*output_proc)();
  int (*output_maximal_unit)();
  long (*encode_getbuffersize_proc)();
  long (*encode_encode_proc)();
#endif
  void *pstream;

  unsigned long output_bytes;
  unsigned int uservar[CFG_USERVAL_COUNT];
};

#define dviprt_getscanlines(p) ((int)(p)->printer->integer[CFG_PINS]*8)
#define dviprt_getoutputbytes(p) ((unsigned long)(p)->output_bytes)

#ifdef __PROTOTYPES__
extern int dviprt_readsrc(const gs_memory_t *mem, char *,dviprt_cfg_t *,
                          unsigned char *,int ,unsigned char *, int);
extern int dviprt_readcfg(const gs_memory_t *mem, char *,dviprt_cfg_t *,
                          unsigned char *,int , unsigned char *,int);

extern int dviprt_beginpage(dviprt_print *);
extern long dviprt_initlibrary(dviprt_print *,dviprt_cfg_t *,
                               unsigned int ,unsigned int );
extern int dviprt_endbitmap(dviprt_print *);
extern int dviprt_setstream
  (dviprt_print *p,int(*f)(unsigned char far*,long,void*),void *s);
extern int dviprt_setbuffer(dviprt_print *,unsigned char far *);
extern int dviprt_outputscanlines(dviprt_print *, unsigned char far *);
extern int dviprt_output(dviprt_print *,unsigned char far *,long );
extern int dviprt_unsetbuffer(dviprt_print *);

extern int dviprt_setmessagestream(FILE *);
extern int dviprt_writesrc(char *,char *,dviprt_cfg_t *,unsigned char *,int );
extern int dviprt_writecfg(char *,char *,dviprt_cfg_t *,unsigned char *,int );
extern int dviprt_writec(char *,char *,dviprt_cfg_t *,unsigned char *,int );
#else /* !__PROTOTYPES__ */
extern int dviprt_readsrc();
extern int dviprt_readcfg();
extern long dviprt_initlibrary();
extern int dviprt_setstream();
extern int dviprt_setbuffer();
extern int dviprt_beginpage();
extern int dviprt_outputscanlines();
extern int dviprt_endbitmap();
extern int dviprt_output ();
extern int dviprt_unsetbuffer();
extern int dviprt_setmessagestream();
extern int dviprt_writesrc();
extern int dviprt_writecfg();
extern int dviprt_writec();
#endif /* __PROTOTYPES__ */

extern char *dviprt_integername[];
extern char *dviprt_stringsname[];
extern char *dviprt_prtcodename[];
extern char *dviprt_encodename[];

#define s_cfg_h_INCLUDED
#endif /* s_cfg_h_INCLUDED */

#define TEMP_CODEBUF_SIZE 2048
#define TEMP_READBUF_SIZE 1024

/* MS-DOS (real-mode) */
#ifdef __MSDOS_REAL__
# define BufferCpy(a,b,c) _fmemcpy(a,b,c)
# define BufferCmp(a,b,c) _fmemcmp(a,b,c)
# define BufferAlloc(a)   _fmalloc(a)
# define BufferFree(a)    _ffree(a)
#else
# define BufferCpy(a,b,c) memcpy(a,b,c)
# define BufferCmp(a,b,c) memcmp(a,b,c)
# define BufferAlloc(a)   malloc(a)
# define BufferFree(a)    free(a)
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#endif

#define CFG_LIBERROR_UNKNOWN_VALUE  -256
#define CFG_LIBERROR_UNKNOWN_FORMAT -257
#define CFG_LIBERROR_UNKNOWN_ESCSEQ -258
#define CFG_LIBERROR_OUTOFRANGE     -259
#define CFG_LIBERROR_INVALID_VALUE  -260
#define CFG_LIBERROR_COMPLICATED_EXPR  -261
#define CFG_LIBERROR_INCOMPLETE      -262

typedef struct {
  uchar *fname;
  gp_file *file;
  int line_no;

  char temp_readbuf_f;
  char temp_codebuf_f;
  uchar *readbuf;
  uchar *codebuf;
  int readbuf_size;
  int codebuf_size;

  uchar *pcodebuf;

  uchar *token;
  uchar *endtoken;
} dviprt_cfg_i;

#ifdef dviprlib_implementation
typedef struct {
  int no;
  long (*getworksize)(dviprt_print *,long);
  long (*encode)(dviprt_print *,long,int);
} dviprt_encoder;

static dviprt_encoder *dviprt_getencoder_(int);
static int dviprt_setcfgbuffer_(dviprt_cfg_i *,int ,int);
static int dviprt_resetcfgbuffer_(dviprt_cfg_i *);
static int dviprt_initcfg_(dviprt_cfg_t *,dviprt_cfg_i *);
static int dviprt_printmessage(char *,int);
static int dviprt_printerror(char *,int);
static int dviprt_printwarning(char *,int);
static int dviprt_printcfgerror(dviprt_cfg_i *,char *,int);
static int dviprt_printcfgwarning(dviprt_cfg_i *,char *,int);

extern int dviprt_print_headercomment_(char *,char *,char *,FILE *);
extern char dviprt_message_buffer[];
#endif dviprlib_implementation

#define s_cfgbld_h_INCLUDED
#endif /* s_cfgbld_h_INCLUDED */
