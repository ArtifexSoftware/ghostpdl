/* Copyright (C) 1998, 1999 Norihito Ohmori.

   Ghostscript printer driver
   for Canon LBP, BJC-680J and BJC-880J printers (LIPS II+/III/IVc/IV)

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

/*$Id: gdevlips.h,v 1.4 2002/10/12 23:24:34 tillkamppeter Exp $ */
/* Common header file for LIPS driver */

#ifndef gdevlips_h_INCLUDED
#define gdevlips_h_INCLUDED

#define	LIPS_ESC	0x1b
#define	LIPS_FF		0x0c
#define	LIPS_CSI	0x9b	/* LIPS_ESC "[" */
#define	LIPS_DCS	0x90	/* LIPS_ESC "P" */
#define	LIPS_ST		0x9c	/* LIPS_ESC "\\" */
#define LIPS_SP         0x20
#define LIPS_IS1        0x1f
#define LIPS_IS2        0x1e

#define TRUE 1
#define FALSE 0

/* Media Size */
#define LIPS_A3_HEIGHT 1190
#define LIPS_A3_WIDTH  842
#define LIPS_A4_HEIGHT LIPS_A3_WIDTH
#define LIPS_A4_WIDTH  595
#define LIPS_A5_HEIGHT LIPS_A4_WIDTH
#define LIPS_A5_WIDTH  421
#define LIPS_POSTCARD_HEIGHT 419
#define LIPS_POSTCARD_WIDTH 284
#define LIPS_B4_HEIGHT 1032
#define LIPS_B4_WIDTH  729
#define LIPS_B5_HEIGHT LIPS_B4_WIDTH
#define LIPS_B5_WIDTH  516
#define LIPS_B6_HEIGHT LIPS_B5_WIDTH
#define LIPS_B6_WIDTH  363
#define LIPS_LETTER_HEIGHT 792
#define LIPS_LETTER_WIDTH  612
#define LIPS_LEGAL_HEIGHT  1008
#define LIPS_LEGAL_WIDTH  LIPS_LETTER_WIDTH
#define LIPS_LEDGER_HEIGHT 1224
#define LIPS_LEDGER_WIDTH LIPS_LETTER_HEIGHT
#define LIPS_EXECTIVE_HEIGHT 756
#define LIPS_EXECTIVE_WIDTH 522
#define LIPS_ENVYOU4_HEIGHT 666
#define LIPS_ENVYOU4_WIDTH  298

#define LIPS_HEIGHT_MAX LIPS_A3_HEIGHT
#define LIPS_WIDTH_MAX  LIPS_A3_WIDTH
#define LIPS_HEIGHT_MIN LIPS_POSTCARD_HEIGHT
#define LIPS_WIDTH_MIN LIPS_POSTCARD_WIDTH

#define	LIPS_HEIGHT_MAX_720	11905	/* =420mm */
#define	LIPS_WIDTH_MAX_720	8418	/* =294mm */

#define LIPS_MAX_SIZE 51840	/* 72 inch */

#define GS_A4_WIDTH 597
#define GS_A4_HEIGHT 841

#define	MMETER_PER_INCH	25.4

#define A3_SIZE 12
#define A4_SIZE 14
#define A5_SIZE 16
#define POSTCARD_SIZE 18
#define B4_SIZE 24
#define B5_SIZE 26
#define B6_SIZE 28
#define LETTER_SIZE 30
#define LEGAL_SIZE 32
#define LEDGER_SIZE 34
#define EXECUTIVE_SIZE 40
#define ENVYOU4_SIZE 50
#define USER_SIZE 80

#define LANDSCAPE 1

#define LIPS_MANUALFEED_DEFAULT FALSE
#define LIPS_CASSETFEED_DEFAULT 0
/* 0, 1 to 3, 10 to 17 (1 to 3 is not reccomended. It's obsoleted value.) */
/* 0: Auto Feed */
/* 1: MP tray, 2: Lower casset, 3: Upper casset */
/* 10: MP tray, 11: casset 1, 12: casset 2, 13: casset 3, .... */

#define LIPS_TUMBLE_DEFAULT FALSE
#define LIPS_NUP_DEFAULT 1
#define LIPS_PJL_DEFAULT FALSE
#define LIPS_FACEUP_DEFAULT FALSE

/* display text to printer panel */
#define LIPS_USERNAME_MAX 12	/* If your printer have two line display,
                                   this value can change to 14 */
#define LIPS_MEDIACHAR_MAX 32
#define LIPS_USERNAME_DEFAULT "Ghostscript"
#define LIPS_MEDIATYPE_DEFAULT "Default"	/* Dummy */
#define LIPS2P_COMPRESS_DEFAULT FALSE
#define LIPS3_COMPRESS_DEFAULT FALSE
#define LIPS4C_COMPRESS_DEFAULT TRUE
#define LIPS4_COMPRESS_DEFAULT FALSE

#define	LIPS2P_DPI_DEFAULT 240
#define	LIPS2P_LEFT_MARGIN_DEFAULT    (float)(5. / MMETER_PER_INCH)
#define	LIPS2P_BOTTOM_MARGIN_DEFAULT  (float)(5. / MMETER_PER_INCH)
#define	LIPS2P_RIGHT_MARGIN_DEFAULT   (float)(5. / MMETER_PER_INCH)
#define	LIPS2P_TOP_MARGIN_DEFAULT     (float)(5. / MMETER_PER_INCH)

#define	LIPS3_DPI_DEFAULT 300
#define	LIPS3_LEFT_MARGIN_DEFAULT     (float)(5. / MMETER_PER_INCH)
#define	LIPS3_BOTTOM_MARGIN_DEFAULT   (float)(5. / MMETER_PER_INCH)
#define	LIPS3_RIGHT_MARGIN_DEFAULT    (float)(5. / MMETER_PER_INCH)
#define	LIPS3_TOP_MARGIN_DEFAULT      (float)(5. / MMETER_PER_INCH)

#define	BJC880J_DPI_DEFAULT 360
#define	BJC880J_LEFT_MARGIN_DEFAULT   (float)(5. / MMETER_PER_INCH)
#define	BJC880J_BOTTOM_MARGIN_DEFAULT (float)(8. / MMETER_PER_INCH)
#define	BJC880J_RIGHT_MARGIN_DEFAULT  (float)(5. / MMETER_PER_INCH)
#define	BJC880J_TOP_MARGIN_DEFAULT    (float)(2. / MMETER_PER_INCH)

#define	LIPS4_DPI_DEFAULT 600

#define	LIPS4_LEFT_MARGIN_DEFAULT     (float)(5. / MMETER_PER_INCH)
#define	LIPS4_BOTTOM_MARGIN_DEFAULT   (float)(5. / MMETER_PER_INCH)
#define	LIPS4_RIGHT_MARGIN_DEFAULT    (float)(5. / MMETER_PER_INCH)
#define	LIPS4_TOP_MARGIN_DEFAULT      (float)(5. / MMETER_PER_INCH)

/* Define the default resolutions. */

#define LIPS2P_DPI1 80
#define LIPS2P_DPI2 120
#define LIPS2P_DPI_MAX 240
#define LIPS3_DPI_MAX 300
#define BJC880J_DPI_MAX 360
#define LIPS4_DPI_MAX 600
#define	LIPS4_DPI_SUPERFINE 1200
#define LIPS_DPI_MIN 60

#define LIPS2P_STRING "lips2p:"
#define LIPS3_STRING "lips3:"
#define LIPS4_STRING "lips4:"
#define BJC880J_STRING "bjc880j:"
#define L4VMONO_STRING "lips4v:"
#define L4VCOLOR_STRING "lips4vc:"
#define LIPS_VERSION "2.3.6"
/* ``LIPS*_STRING'' + ``VERSION string'' < 17 characters */

#define LIPS_OPTION_MANUALFEED              "ManualFeed"
#define LIPS_OPTION_CASSETFEED              "Casset"
#define LIPS_OPTION_USER_NAME               "UserName"
#define LIPS_OPTION_PJL			    "PJL"

/* LIPS IV Option */
#define LIPS_OPTION_DUPLEX_TUMBLE           "Tumble"
#define LIPS_OPTION_NUP			    "Nup"
#define LIPS_OPTION_TONERDENSITY	    "TonerDensity"
#define LIPS_OPTION_TONERSAVING		    "TonerSaving"
#define LIPS_OPTION_FONTDOWNLOAD	    "FontDL"
#define LIPS_OPTION_FACEUP		    "OutputFaceUp"
#define LIPS_OPTION_MEDIATYPE		    "MediaType"

#define lips_params_common \
    int cassetFeed;                    /* Input Casset */ \
    char Username[LIPS_USERNAME_MAX + 1];  /* Display text on printer panel */\
    bool pjl; \
    int toner_density;\
    bool toner_saving;\
    bool toner_saving_set;\
    int prev_paper_size; \
    int prev_paper_width; \
    int prev_paper_height; \
    int prev_num_copies; \
    int prev_feed_mode

#define lips4_params_common \
    int prev_duplex_mode;\
    int nup;\
    bool faceup;\
    char mediaType[LIPS_MEDIACHAR_MAX + 1]

int lips_media_selection(int width, int height);
int lips_packbits_encode(byte * inBuff, byte * outBuff, int Length);
int lips_mode3format_encode(byte * inBuff, byte * outBuff, int Length);
int lips_rle_encode(byte * inBuff, byte * outBuff, int Length);

#endif
