/* Copyright (C) EPSON SOFTWARE DEVELOPMENT LABORATORY, INC. 1999,2000.
   Copyright (C) SEIKO EPSON CORPORATION 2000-2006,2009.

   Ghostscript printer driver for EPSON ESC/Page and ESC/Page-Color.

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

#ifndef gdevescv_INCLUDED
#define gdevescv_INCLUDED

#ifndef  TRUE
#  define TRUE            1
#endif

#ifndef  FALSE
#  define FALSE           0
#endif

#define GS              (0x1d)
#define ESC_GS          "\035"
#define ESC_FF          "\014"
#define ESC_CR          "\015"
#define ESC_LF          "\012"
#define ESC_BS          "\010"

#define POINT                                   72
#define MMETER_PER_INCH                         25.4

#define ESCPAGE_DEFAULT_WIDTH                   (4840)
#define ESCPAGE_DEFAULT_HEIGHT                  (6896)

#define ESCPAGE_LEFT_MARGIN_DEFAULT             5. / (MMETER_PER_INCH / POINT)
#define ESCPAGE_BOTTOM_MARGIN_DEFAULT           5. / (MMETER_PER_INCH / POINT)
#define ESCPAGE_RIGHT_MARGIN_DEFAULT            5. / (MMETER_PER_INCH / POINT)
#define ESCPAGE_TOP_MARGIN_DEFAULT              5. / (MMETER_PER_INCH / POINT)

#define NUM_OF_PAPER_TABLES                     23
#define MAX_PAPER_SIZE_DELTA                    5

#define ESCPAGE_OPTION_MANUALFEED               "ManualFeed"
#define ESCPAGE_OPTION_CASSETFEED               "Casset"
#define ESCPAGE_OPTION_FACEUP                   "FaceUp"
#define ESCPAGE_OPTION_DUPLEX                   "Duplex"
#define ESCPAGE_OPTION_DUPLEX_TUMBLE            "Tumble"
#define ESCPAGE_OPTION_MEDIATYPE                "MediaType"
#define ESCPAGE_OPTION_RIT                      "RITOff"
#define ESCPAGE_OPTION_LANDSCAPE                "Landscape"
#define ESCPAGE_OPTION_TONERDENSITY             "TonerDensity"
#define ESCPAGE_OPTION_TONERSAVING              "TonerSaving"
#define ESCPAGE_OPTION_COLLATE                  "Collate"
#define ESCPAGE_OPTION_JOBID                    "JobID"
#define ESCPAGE_OPTION_USERNAME                 "UserName"
#define ESCPAGE_OPTION_DOCUMENT                 "Document"
#define ESCPAGE_OPTION_HOSTNAME                 "HostName"
#define ESCPAGE_OPTION_COMMENT                  "eplComment"
#define ESCPAGE_OPTION_EPLModelJP               "EPLModelJP"
#define ESCPAGE_OPTION_EPLCapFaceUp             "EPLCapFaceUp"
#define ESCPAGE_OPTION_EPLCapDuplexUnit         "EPLCapDuplexUnit"
#define ESCPAGE_OPTION_EPLCapMaxResolution      "EPLCapMaxResolution"

#define ESCPAGE_JOBID_MAX                       255
#define ESCPAGE_USERNAME_MAX                    255
#define ESCPAGE_HOSTNAME_MAX                    255
#define ESCPAGE_DOCUMENT_MAX                    255
#define ESCPAGE_MODELNAME_MAX                   255
#define ESCPAGE_COMMENT_MAX                     255

#define ESCPAGE_TUMBLE_DEFAULT                  FALSE                   /* Long age */
#define ESCPAGE_RIT_DEFAULT                     FALSE
#define ESCPAGE_FACEUP_DEFAULT                  FALSE
#define ESCPAGE_FACEUP_DEFAULT                  FALSE

#define ESCPAGE_MEDIATYPE_DEFAULT               0                       /* NORMAL */
#define ESCPAGE_MEDIACHAR_MAX                   32

#define ESCPAGE_MANUALFEED_DEFAULT              FALSE
#define ESCPAGE_CASSETFEED_DEFAULT              0

#define ESCPAGE_DPI_MIN                         60
#define ESCPAGE_DPI_MAX                         1200

#define ESCPAGE_HEIGHT_MAX                      ( 1369 + MAX_PAPER_SIZE_DELTA ) /* A3PLUS */
#define ESCPAGE_WIDTH_MAX                       (  933 + MAX_PAPER_SIZE_DELTA ) /* A3PLUS */
#define ESCPAGE_HEIGHT_MIN                      (  420 - MAX_PAPER_SIZE_DELTA ) /* POSTCARD */
#define ESCPAGE_WIDTH_MIN                       (  279 - MAX_PAPER_SIZE_DELTA ) /* MON */

#define RES1200                                 1200
#define RES600                                  600
#define JPN                                     TRUE
#define ENG                                     FALSE

#define X_DPI           600
#define Y_DPI           600
#define VCACHE          0x3FF

#define ESCPAGE_DEVICENAME_COLOR                "eplcolor"
#define ESCPAGE_DEVICENAME_MONO                 "eplmono"

/* ---------------- Device definition ---------------- */

typedef struct gx_device_escv_s {
  gx_device_vector_common;

  int           colormode;      /* 0=ESC/Page(Monochrome), 1=ESC/Page-Color */
  bool          manualFeed;     /* Use manual feed */
  int           cassetFeed;     /* Input Casset */
  bool          RITOff;         /* RIT Control */
  bool          Collate;
  int           toner_density;
  bool          toner_saving;
  int           prev_paper_size;
  int           prev_paper_width;
  int           prev_paper_height;
  int           prev_num_copies;
  int           prev_feed_mode;
  int           orientation;
  bool          faceup;
  int           MediaType;
  bool          first_page;
  bool          Duplex;
  bool          Tumble;
  int           ncomp;
  int           MaskReverse;
  int           MaskState;
  bool          c4map;          /* 4bit ColorMap */
  bool          c8map;          /* 8bit ColorMap */
  int           prev_x;
  int           prev_y;
  gx_color_index        prev_color;
  gx_color_index        current_color;
  double        lwidth;
  long          cap;
  long          join;
  long          reverse_x;
  long          reverse_y;
  gs_matrix     xmat;		/* matrix */
  int           bx;
  int           by;
  int           w;		/* width */
  int           h;		/* height */
  int           roll;
  float         sx;		/* scale x */
  float         sy;		/* scale y */
  long          dd;
  int           ispath;
  gx_bitmap_id  id_cache[VCACHE + 1];    /* for Font Downloading */
  char          JobID[ESCPAGE_JOBID_MAX + 1];
  char          UserName[ESCPAGE_USERNAME_MAX + 1];
  char          HostName[ESCPAGE_HOSTNAME_MAX + 1];
  char          Document[ESCPAGE_DOCUMENT_MAX + 1];
  char          Comment[ESCPAGE_COMMENT_MAX + 1];
  gs_param_string gpsJobID;
  gs_param_string gpsUserName;
  gs_param_string gpsHostName;
  gs_param_string gpsDocument;
  gs_param_string gpsComment;
  bool            modelJP;
  bool            capFaceUp;
  bool            capDuplexUnit;
  int             capMaxResolution;
} gx_device_escv;

typedef struct EPaperTable_s{
  int           width;          /* paper width (unit: point) */
  int           height;         /* paper height (unit: point) */
  int           escpage;        /* number of papersize in ESC/PAGE */
  const char    *name;          /* Paper Name */
} EPaperTable;

typedef struct paper_candidate_s {
  const EPaperTable *paper;
  int absw;                     /* absolute delta width */
  int absh;                     /* absolute delta height */
  int score;                    /* select priority score */
  bool isfillw;                 /* inside printable area width? */
  bool isfillh;                 /* inside printable area height? */
  bool isminw;                  /* best fit width? */
  bool isminh;                  /* best fit height? */
} paper_candidate;

#endif /* gdevescv_INCLUDED */

/* end of file */
