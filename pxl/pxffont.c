/* Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxffont.c */
/* PCL XL font-finding procedures */

#include "string_.h"
#include "gx.h"
#include "gschar.h"
#include "gsmatrix.h"		/* for gsfont.h */
#include "gxfont.h"
#include "gxfont42.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxfont.h"

/*
 * Define the structure and parameters for doing font substitution.
 * Ideally, we should build in a few hundred well-known font names and
 * use PANOSE numbers; this is a refinement for the future.
 */
typedef enum {
  pxff_fixed = 1,
  pxff_variable = 2,
  pxff_serif = 4,
  pxff_sans = 8,
  pxff_upright = 0x10,
  pxff_oblique = 0x20,
  pxff_italic = 0x40,
  pxff_plain = 0x80,
  pxff_bold = 0x100,
  pxff_narrow = 0x200,
  pxff_symbolic = 0x400,
  pxff_typeface = 0x0010000,
    pxff_typeface_mask = 0xfff0000,
	/* This is a pretty random selection of typefaces. */
	/* It just happens to be the ones in the standard PostScript set */
	/* plus the ones in the Windows set. */
    pxffArial = pxff_typeface * 218,
    pxffAvantGarde = pxff_typeface * 31,
    pxffBookman = pxff_typeface * 402,
    pxffCourier = pxff_typeface * 3,
    pxffHelvetica = pxff_typeface * 4,
    pxffNewCenturySchlbk = pxff_typeface * 23,
    pxffPalatino = pxff_typeface * 15,
    pxffSymbol = pxff_typeface * 302,
    pxffTimes = pxff_typeface * 5,
    pxffWingdings = pxff_typeface * 2730,
	/* These are the remaining typefaces in the PCL XL base set. */
    pxffAlbertus = pxff_typeface * 266,
    pxffAntiqueOlive = pxff_typeface * 72,
    pxffClarendon = pxff_typeface * 44,
    pxffCoronet = pxff_typeface * 20,
    pxffGaramond = pxff_typeface * 18,
    pxffLetterGothic = pxff_typeface * 6,
    pxffLinePrinter = pxff_typeface * 0,
    pxffMarigold = pxff_typeface * 201,
    pxffUnivers = pxff_typeface * 52,
	/* These two are not in the H-P catalog. */
    pxffCGOmega = pxffGaramond,
    pxffCGTimes = pxffTimes
} px_font_flags_t;
#define pxff_spacing (pxff_fixed | pxff_variable)
#define pxff_serifs (pxff_serif | pxff_sans)
#define pxff_posture (pxff_upright | pxff_oblique)
#define pxff_weight (pxff_plain | pxff_bold)
#define pxff_width (pxff_narrow)
#define pxff_all_properties ((px_font_flags_t)0xffff)
#define pxff_all (pxff_all_properties | pxff_typeface_mask)
typedef struct px_font_desc_s {
  /* All font names are in Unicode.  See pxfont.h. */
  char16 fname[17];
  px_font_flags_t flags;
} px_font_desc_t;

/*
 * Here we list the 45 built-in LaserJet 5 and 6 fonts.  For host-based
 * testing, we also provide the file names of the corresponding Windows
 * TrueType fonts if available.
 */
typedef struct px_stored_font_s {
  px_font_desc_t descriptor;
  const char *file_name;
} px_stored_font_t;
private const char *known_font_file_prefixes[] = {
  "fonts/", "/windows/system/", "/windows/fonts/", "/win95/fonts/", "/winnt/fonts/", 0
};
private const px_stored_font_t known_fonts[] = {
  {{{'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','X','b'},
    pxffAlbertus | pxff_variable | pxff_sans | pxff_upright | pxff_bold},
   "albrxb.ttf"},
  {{{'A','l','b','e','r','t','u','s',' ',' ',' ',' ',' ',' ','M','d'},
    pxffAlbertus | pxff_variable | pxff_sans | pxff_upright | pxff_plain},
   "albrmd.ttf"},
  {{{'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ',' ',' '},
    pxffAntiqueOlive | pxff_variable | pxff_sans | pxff_upright | pxff_plain},
   "anto.ttf"},
  {{{'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','B','d'},
    pxffAntiqueOlive | pxff_variable | pxff_sans | pxff_upright | pxff_bold},
   "antob.ttf"},
  {{{'A','n','t','i','q','O','l','i','v','e',' ',' ',' ',' ','I','t'},
    pxffAntiqueOlive | pxff_variable | pxff_sans | pxff_oblique | pxff_plain},
   "antoi.ttf"},
  {{{'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffArial | pxff_variable | pxff_sans | pxff_upright | pxff_plain},
   "arial.ttf"},
  {{{'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','B','d'},
    pxffArial | pxff_variable | pxff_sans | pxff_upright | pxff_bold},
   "arialbd.ttf"},
  {{{'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ','B','d','I','t'},
    pxffArial | pxff_variable | pxff_sans | pxff_oblique | pxff_bold},
   "arialbi.ttf"},
  {{{'A','r','i','a','l',' ',' ',' ',' ',' ',' ',' ',' ',' ','I','t'},
    pxffArial | pxff_variable | pxff_sans | pxff_oblique | pxff_plain},
   "ariali.ttf"},
  {{{'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffCGOmega | pxff_variable | pxff_sans | pxff_upright | pxff_plain},
   "cgom.ttf"},
  {{{'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','B','d'},
    pxffCGOmega | pxff_variable | pxff_sans | pxff_upright | pxff_bold},
   "cgomi.ttf"},
  {{{'C','G',' ','O','m','e','g','a',' ',' ',' ',' ','B','d','I','t'},
    pxffCGOmega | pxff_variable | pxff_sans | pxff_oblique | pxff_bold},
   "cgombi.ttf"},
  {{{'C','G',' ','O','m','e','g','a',' ',' ',' ',' ',' ',' ','I','t'},
    pxffCGOmega | pxff_variable | pxff_sans | pxff_oblique | pxff_plain},
   "cgomi.ttf"},
  {{{'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffCGTimes | pxff_variable | pxff_serif | pxff_upright | pxff_plain},
   "cgti.ttf"},
  {{{'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','B','d'},
    pxffCGTimes | pxff_variable | pxff_serif | pxff_upright | pxff_bold},
   "cgtib.ttf"},
  {{{'C','G',' ','T','i','m','e','s',' ',' ',' ',' ','B','d','I','t'},
    pxffCGTimes | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_bold},
   "cgtibi.ttf"},
  {{{'C','G',' ','T','i','m','e','s',' ',' ',' ',' ',' ',' ','I','t'},
    pxffCGTimes | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_plain},
   "cgtii.ttf"},
  {{{'C','l','a','r','e','n','d','o','n',' ',' ',' ','C','d','B','d'},
    pxffClarendon | pxff_variable | pxff_serif | pxff_upright | pxff_bold},
   "clarbc.ttf"},
  {{{'C','o','r','o','n','e','t',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffCoronet | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_plain},
   "coro.ttf"},
  {{{'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffCourier | pxff_fixed | pxff_serif | pxff_upright | pxff_plain},
   "cour.ttf"},
  {{{'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','B','d'},
    pxffCourier | pxff_fixed | pxff_serif | pxff_upright | pxff_bold},
   "courbd.ttf"},
  {{{'C','o','u','r','i','e','r',' ',' ',' ',' ',' ',' ',' ','I','t'},
    pxffCourier | pxff_fixed | pxff_serif | pxff_oblique | pxff_plain},
   "couri.ttf"},
  {{{'C','o','u','r','i','e','r',' ',' ',' ',' ',' ','B','d','I','t'},
    pxffCourier | pxff_fixed | pxff_serif | pxff_oblique | pxff_bold},
   "courbi.ttf"},
  {{{'G','a','r','a','m','o','n','d',' ','A','n','t','i','q','u','a'},
    pxffGaramond | pxff_variable | pxff_serif | pxff_upright | pxff_plain},
   "garaa.ttf"},
  {{{'G','a','r','a','m','o','n','d',' ',' ',' ',' ',' ','H','l','b'},
    pxffGaramond | pxff_variable | pxff_serif | pxff_upright | pxff_bold},
   "garrah.ttf"},
  {{{'G','a','r','a','m','o','n','d',' ',' ',' ',' ','K','r','s','v'},
    pxffGaramond | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_plain},
   "garak.ttf"},
  {{{'G','a','r','a','m','o','n','d',' ','K','r','s','v','H','l','b'},
    pxffGaramond | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_bold},
   "garrkh.ttf"},
  {{{'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ',' ',' '},
    pxffLetterGothic | pxff_fixed | pxff_sans | pxff_upright | pxff_plain},
   "letr.ttf"},
  {{{'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','B','d'},
    pxffLetterGothic | pxff_fixed | pxff_sans | pxff_upright | pxff_bold},
   "letrb.ttf"},
  {{{'L','e','t','t','e','r','G','o','t','h','i','c',' ',' ','I','t'},
    pxffLetterGothic | pxff_fixed | pxff_sans | pxff_oblique | pxff_plain},
   "letri.ttf"},
  /* The line printer fonts are identical except for symbol set. */
#define LinePrinter(ss1,ss2,ss3)\
  {{{'L','i','n','e',' ','P','r','i','n','t','e','r',' ',ss1,ss2,ss3},\
    pxffLinePrinter | pxff_fixed | pxff_serif | pxff_upright | pxff_plain},\
   0}
  LinePrinter(' ', '0', 'N'),
  LinePrinter('1', '0', 'U'),
  LinePrinter('1', '1', 'U'),
  LinePrinter('1', '2', 'U'),
  LinePrinter(' ', '1', 'U'),
  LinePrinter(' ', '2', 'N'),
  LinePrinter(' ', '5', 'N'),
  LinePrinter(' ', '8', 'U'),
#undef LinePrinter
  {{{'M','a','r','i','g','o','l','d',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffMarigold | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_plain},
   "mari.ttf"},
  {{{'S','y','m','b','o','l',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
    pxffSymbol | pxff_variable | pxff_upright | pxff_symbolic},
   "symbol.ttf"},
  {{{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ',' ',' '},
    pxffTimes | pxff_variable | pxff_serif | pxff_upright | pxff_plain},
   "times.ttf"},
  {{{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','B','d'},
    pxffTimes | pxff_variable | pxff_serif | pxff_upright | pxff_bold},
   "timesbd.ttf"},
  {{{'T','i','m','e','s','N','e','w','R','m','n',' ','B','d','I','t'},
    pxffTimes | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_bold},
   "timesbi.ttf"},
  {{{'T','i','m','e','s','N','e','w','R','m','n',' ',' ',' ','I','t'},
    pxffTimes | pxff_variable | pxff_serif | pxff_oblique | pxff_italic | pxff_plain},
   "timesi.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','B','d'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_upright | pxff_bold},
   "univb.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','B','d'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_upright | pxff_bold | pxff_narrow},
   "univcb.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ','C','d','B','d','I','t'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_oblique | pxff_bold | pxff_narrow},
   "univcbi.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','B','d','I','t'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_oblique | pxff_bold},
   "univbi.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ',' ',' ','M','d'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_upright | pxff_plain},
   "univm.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','C','d','M','d'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_upright | pxff_plain | pxff_narrow},
   "univmc.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ','C','d','M','d','I','t'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_oblique | pxff_plain | pxff_narrow},
   "univmci.ttf"},
  {{{'U','n','i','v','e','r','s',' ',' ',' ',' ',' ','M','d','I','t'},
    pxffUnivers | pxff_variable | pxff_sans | pxff_oblique | pxff_plain},
   "univmi.ttf"},
  {{{'W','i','n','g','d','i','n','g','s',' ',' ',' ',' ',' ',' ',' '},
    pxffWingdings | pxff_variable | pxff_upright | pxff_symbolic},
   "wingding.ttf"}
};
#define num_known_fonts countof(known_fonts)
const uint px_num_known_fonts = num_known_fonts;

/*
 * Define some font name substrings that give clues about appropriate
 * substitutions.
 */
#define ffAvantGarde (pxffAvantGarde | pxff_serif)
#define ffBookman (pxffBookman | pxff_serif)
#define ffCourier (pxffCourier | pxff_fixed)
#define ffHelvetica (pxffHelvetica | pxff_sans)
#define ffNewCenturySchlbk (pxffNewCenturySchlbk | pxff_serif)
#define ffPalatino (pxffPalatino | pxff_serif)
#define ffTimes (pxffTimes | pxff_serif)
private const px_font_desc_t subst_font_families[] = {
	/* Guess at suitable substitutions for random unknown fonts. */
  {{'G','o','t','h'}, ffTimes},
  {{'G','r','o','t'}, ffTimes},
  {{'R','o','m','a','n'}, ffTimes},
  {{'B','o','o','k'}, ffNewCenturySchlbk},
	/* If the family name appears in the font name, */
	/* use a font from that family. */
  {{'A','l','b','e','r','t','u','s'}, ffHelvetica},
  {{'A','r','i','a','l'}, ffHelvetica},
  {{'A','v','a','n','t'}, ffAvantGarde},
  {{'B','o','d','o','n','i'}, ffBookman},
  {{'B','o','o','k','m','a','n'}, ffBookman},
  {{'C','e','n','t','u','r','y'}, ffNewCenturySchlbk},
  {{'C','l','a','r','e','n','d','o','n'}, ffTimes},
  {{'C','o','u','r'}, ffCourier},
  {{'G','e','n','e','v','a'}, ffHelvetica},
  {{'G','o','u','d','y'}, ffNewCenturySchlbk},
  {{'G','r','a','p','h','o','s'}, ffCourier},
  {{'H','e','l','v'}, ffHelvetica},
  {{'M','e','t','r','o','s','t','y','l','e'}, ffHelvetica},
  {{'N','e','w','Y','o','r','k'}, ffTimes},
  {{'P','a','l'}, ffPalatino},
  {{'R','o','m'}, ffTimes},
  {{'S','a','n'}, ffHelvetica},
  {{'S','c','h','l','b','k'}, ffNewCenturySchlbk},
  {{'S','e','r','i','f'}, ffTimes},
  {{'S','o','r','t','s'}, pxff_symbolic},
  {{'S','w','i','s','s'}, ffHelvetica},
  {{'T','i','m','e','s'}, ffTimes},
  {{'U','n','i','v','e','r','s'}, ffHelvetica},
	/* Substitute for Adobe Multiple Master fonts. */
  {{'M','y','r','i','a','d'}, ffTimes},
  {{'M','i','n','i','o','n'}, ffHelvetica},
};
private const px_font_desc_t subst_font_attributes[] = {
  {{'M','o','n'}, pxff_fixed},
  {{'T','y','p','e','w','r','i','t','e','r'}, pxff_fixed},
  {{'I','t'}, pxff_italic},
  {{'O','b','l','i','q','u','e'}, pxff_oblique},
  {{'B','d'}, pxff_bold},
  {{'B','o','l','d'}, pxff_bold},
  {{'b','o','l','d'}, pxff_bold},
  {{'D','e','m','i'}, pxff_bold},
  {{'H','e','a','v','y'}, pxff_bold},
  {{'S','b'}, pxff_bold},
  {{'X','b'}, pxff_bold},
  {{'C','d'}, pxff_narrow},
  {{'C','o','n','d'}, pxff_narrow},
  {{'N','a','r','r','o','w'}, pxff_narrow},
};

/* All the font-related structures are big-endian, so: */
#define u16(ptr) uint16at(ptr, true)
#define s16(ptr) sint16at(ptr, true)
#define u32(ptr) uint32at(ptr, true)

#if ARCH_IS_BIG_ENDIAN
#  define pxd_native_byte_order pxd_big_endian
#else
#  define pxd_native_byte_order 0
#endif

/* ---------------- Operator utilities ---------------- */

/* Compute the length of a Unicode string. */
private uint
ustrlen(const char16 *ustr)
{	uint i;
	for ( i = 0; ustr[i]; ++i ) ;
	return i;
}

/* Search for a Unicode string in another Unicode string. */
private const char16 *
ustrstr(const char16 *str, uint strlen, const char16 *pattern, uint patlen)
{	uint i;

	for ( i = 0; i + patlen <= strlen; ++i )
	  if ( !memcmp(str + i, pattern, patlen * sizeof(char16)) )
	    return (const char16 *)(str + i);
	return 0;
}

/* Widen and/or byte-swap a font name to Unicode if needed. */
/* The argument is a ubyte or uint16 array; the result is a uint16 array */
/* with the elements in native byte order. */
/* We don't deal with mappings: we just widen 8-bit to 16-bit characters */
/* and hope for the best.... */
private int
px_widen_font_name(px_value_t *pfnv, px_state_t *pxs)
{	uint type = pfnv->type;

	if ( (type & (pxd_uint16 | pxd_big_endian)) ==
	     (pxd_uint16 | pxd_native_byte_order)
	   )
	  return 0;		/* already in correct format */
	{ byte *old_data = (byte *)pfnv->value.array.data; /* break const */
	  uint size = pfnv->value.array.size;
	  char16 *new_data;
	  uint i;

	  if ( type & pxd_on_heap )
	    old_data = (byte *)
	      (new_data =
	       (char16 *)gs_resize_object(pxs->memory, old_data,
					  size * 2, "px_widen_font_name"));
	  else
	    new_data =
	      (char16 *)gs_alloc_byte_array(pxs->memory, size, sizeof(char16),
					    "px_widen_font_name");
	  if ( new_data == 0 )
	    return_error(errorInsufficientMemory);
	  for ( i = size; i; )
	    { --i;
	      new_data[i] =
		(type & pxd_ubyte ? old_data[i] :
		 uint16at(old_data + i * 2, type & pxd_big_endian));
	    }
	  pfnv->value.array.data = (byte *)new_data;
	}	  
	pfnv->type = (type & ~(pxd_ubyte | pxd_big_endian)) |
	  (pxd_uint16 | pxd_native_byte_order | pxd_on_heap);
	return 0;
}

/* ---------------- Operator implementations ---------------- */

/* Open the file for a known font. */
private FILE *
px_open_font_file(const char *fname)
{	const char **pprefix = known_font_file_prefixes;

	for ( ; *pprefix; ++pprefix )
	  { char file_name[80];
	    FILE *in;

	    strcpy(file_name, *pprefix);
	    strcat(file_name, fname);
	    in = fopen(file_name, "rb");
	    if ( in )
	      return in;
	    in = fopen(file_name, "r");
	    if ( in )
	      return in;
	  }
	return 0;
}

/* Look up a font name and return its description, if known. */
/* The font name must already have been widened to Unicode. */
private const px_stored_font_t *
px_find_known_font(px_value_t *pfnv, px_state_t *pxs)
{	uint i;

	if ( pfnv->value.array.size != 16 )
	  return 0;
	for ( i = 0; i < num_known_fonts; ++i )
	  if ( !memcmp(known_fonts[i].descriptor.fname,
		       pfnv->value.array.data, 16 * sizeof(char16))
	     )
	    return &known_fonts[i];
	return 0;
}

/* Look up a font name and return an existing font. */
/* This procedure may widen and/or byte-swap the font name. */
/* If this font is supposed to be built in but no .TTF file is available, */
/* or if loading the font fails, return >= 0 and store 0 in *ppxfont. */
int
px_find_existing_font(px_value_t *pfnv, px_font_t **ppxfont,
  px_state_t *pxs)
{	int code;

	/* Normalize the font name to Unicode. */
	code = px_widen_font_name(pfnv, pxs);
	if ( code < 0 )
	  return code;

	/* Check if we know the font already. */
	{ void *pxfont;

	  if ( px_dict_find(&pxs->font_dict, pfnv, &pxfont) )
	    { /* Make sure this isn't a partially defined font */
	      /* in the process of being downloaded. */
	      if ( ((px_font_t *)pxfont)->pfont )
		{ *ppxfont = pxfont;
		  return 0;
		}
	    }
	}

	/* Check for a built-in font. */
	{ const px_stored_font_t *psf = px_find_known_font(pfnv, pxs);

	  if ( psf == 0 )
	    return_error(errorFontUndefined);
	  *ppxfont = 0;		/* in case of later failure */
	  if ( psf->file_name == 0 )
	    return 0;

	  { /* Load the TrueType font data. */
	    FILE *in = px_open_font_file(psf->file_name);
	    px_font_t *pxfont;

	    if ( in == 0 )
	      return 0;		/* TrueType font not available */
	    code =
	      pl_load_tt_font(in, pxs->font_dir, pxs->memory,
			      (psf - known_fonts + pxs->known_fonts_base_id),
			      &pxfont);
	    if ( code >= 0 )
	      { pxfont->storage = pxfsInternal;
                pxfont->data_are_permanent = false;
	        pxfont->params.symbol_set = 0;	/* not known */
		/* The character complements are bogus.... */
	        memcpy(pxfont->character_complement,
		       (psf->descriptor.flags & pxff_symbolic ?
			"\377\377\377\377\377\377\377\376" :
			"\377\377\377\377\000\000\000\006"),
		       8);
	        code = px_dict_put(&pxs->font_dict, pfnv, pxfont);
		if ( code >= 0 )
		  *ppxfont = pxfont;
	      }
	  }
	}
	return code;
}

/* Look for a partial match with a known font. */
private int
px_find_subst_font(px_font_flags_t flags, px_font_flags_t mask,
  px_font_t **ppxfont, const char16 **psfname, px_state_t *pxs)
{	uint i;

	for ( i = 0; i < num_known_fonts; ++i )
	  { px_font_flags_t known_flags = known_fonts[i].descriptor.flags;
	    /* Require an exact match on typeface, but only inclusion */
	    /* on properties. */
	    if ( !((known_flags ^ flags) & pxff_typeface_mask & mask) &&
		 !(flags & ~known_flags & pxff_all_properties & mask)
	       )
	      { const char16 *sfname = known_fonts[i].descriptor.fname;
	        px_value_t fnv;
		int code;

		fnv.type = pxd_array | pxd_uint16 | pxd_native_byte_order;
		fnv.value.array.data = (const byte *)sfname;
		fnv.value.array.size = ustrlen(sfname);
		code = px_find_existing_font(&fnv, ppxfont, pxs);
		if ( code < 0 || *ppxfont == 0 )
		  continue;
		*psfname = sfname;
		return 0;
	      }
	  }
	return_error(errorFontUndefinedNoSubstituteFound);
}

/* Look up a font name and return a font, possibly with substitution. */
/* This procedure implements most of the SetFont operator. */
/* This procedure may widen and/or byte-swap the font name. */
int
px_find_font(px_value_t *pfnv, uint symbol_set, px_font_t **ppxfont,
  px_state_t *pxs)
{	int code;
	bool built_in;

	/* Check if we know the font already. */
	/* Note that px_find_existing_font normalizes the font name. */

	code = px_find_existing_font(pfnv, ppxfont, pxs);
	if ( code >= 0 )
	  { if ( *ppxfont != 0 )
	      return code;
	    built_in = true;
	  }
	else if ( code == errorFontUndefined )
	  built_in = false;
	else
	  return code;

	/* If we're asking for an unknown symbol set, give up. */
	{ const pl_symbol_map_t **pmap = pl_built_in_symbol_maps;
	  const pl_symbol_map_t *map;

	  for ( ; (map = *pmap) != 0; ++pmap )
	    { uint map_set = (map->id[0] << 8) + map->id[1];
	      if ( map_set == symbol_set )
		break;
	    }
	  if ( map == 0 )
	    return_error(built_in ? errorSymbolSetRemapUndefined :
			 errorFontUndefinedNoSubstituteFound);
	}

	/* Try to find a substitute font. */

	{ const char16 *fname = (const char16 *)pfnv->value.array.data;
	  uint fnsize = pfnv->value.array.size;
	  px_font_flags_t flags = 0;
	  const char16 *sfname;
	  int i;

	  if ( built_in )
	    flags = px_find_known_font(pfnv, pxs)->descriptor.flags;
	  else
	    { for ( i = 0; i < countof(subst_font_families); ++i )
	        { const char16 *pattern = subst_font_families[i].fname;
		  if ( ustrstr(fname, fnsize, pattern, ustrlen(pattern)) )
		    flags = subst_font_families[i].flags;
		}
	      for ( i = 0; i < countof(subst_font_attributes); ++i )
		{ const char16 *pattern = subst_font_attributes[i].fname;
		  if ( ustrstr(fname, fnsize, pattern, ustrlen(pattern)) )
		    flags |= subst_font_attributes[i].flags;
		}
	    }
	  if ( flags == 0 )
	    { /* This is a completely unknown font.  Substitute Courier. */
	      flags = pxffCourier | pxff_fixed | pxff_serif | pxff_upright | pxff_plain;
	    }
	  if ( (code = px_find_subst_font(flags, pxff_all, ppxfont, &sfname, pxs)) >= 0 ||
	       (code = px_find_subst_font(flags, pxff_all_properties, ppxfont, &sfname, pxs)) >= 0 ||
#define mask (pxff_serifs | pxff_posture | pxff_weight | pxff_symbolic)
	       (code = px_find_subst_font(flags, mask, ppxfont, &sfname, pxs)) >= 0 ||
#undef mask
#define mask (pxff_symbolic)
	       (code = px_find_subst_font(flags, mask, ppxfont, &sfname, pxs)) >= 0 ||
#undef mask
	       (code = px_find_subst_font(0, 0, ppxfont, &sfname, pxs)) >= 0
	     )
	    { if ( code >= 0 && !built_in )
	        { /* Issue a warning. */
		  px_value_t sfnv;
		  static const char *mid_message = " substituted for ";
		  char message[px_max_error_line + 1];

		  message[0] = 0;
		  sfnv.type = pxd_array | pxd_uint16 | pxd_native_byte_order;
		  sfnv.value.array.data = (byte *)sfname; /* break const, but not really */
		  sfnv.value.array.size = ustrlen(sfname);
		  px_concat_font_name(message, px_max_error_line, &sfnv);
		  strncat(message, mid_message,
			  px_max_error_line - strlen(message));
		  message[px_max_error_line] = 0;
		  px_concat_font_name(message, px_max_error_line, pfnv);
		  code = px_record_warning(message, true, pxs);
		}
	    }
	  return code;
	}
}
