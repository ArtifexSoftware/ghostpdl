/*
	File 'sfnt.h'
	
	Contains 'sfnt' resource structure description
	
	Copyright 1991 Apple Computer, Inc.
	
*/

#ifndef sfntIncludes
#define sfntIncludes

typedef unsigned       char   uint8; /* 8-bit unsigned integer */
typedef   signed       char    int8; /* 8-bit signed integer */
typedef unsigned short  int  uint16; /* 16-bit unsigned integer */
typedef   signed short  int   int16; /* 16-bit signed integer */
typedef unsigned  long  int  uint32; /* 32-bit unsigned integer */
typedef   signed  long  int   int32; /* 32-bit signed integer */
#if 0
typedef   signed  long  int   Fixed; /* 16.16 32-bit signed fixed-point number */
#endif
typedef   signed short  int   FUnit; /* Smallest measurable distance in em space (16-bit signed integer) */
typedef   signed short  int   FWord; /* 16-bit signed integer that describes a quantity in FUnits */
typedef unsigned short  int  uFWord; /* 16-bit unsigned integer that describes a quantity in FUnits */
typedef   signed short  int F2Dot14; /* 2.14 16-bit signed fixed-point number */
#if 0
typedef   signed  long  int F26Dot6; /* 26.6 32-bit signed fixed-point number */
#endif

typedef struct {
	unsigned long bc;
	unsigned long ad;
} BigDate;

typedef struct {
	uint32 tag;
	uint32 checkSum;
	uint32 offset;
	uint32 length;
} sfnt_DirectoryEntry;

#define SFNT_VERSION	0x10000

/*
 *	The search fields limits numOffsets to 4096.
 */
typedef struct {
	Fixed version;					/* 1.0 */
	uint16 numOffsets;		/* number of tables */
	uint16 searchRange;		/* (max2 <= numOffsets)*16 */
	uint16 entrySelector;		/* log2(max2 <= numOffsets) */
	uint16 rangeShift;			/* numOffsets*16-searchRange*/
	sfnt_DirectoryEntry table[1];		/* table[numOffsets] */
} sfnt_OffsetTable;

#define OFFSETTABLESIZE		12		/* not including any entries */

typedef enum sfntHeaderFlagBits {
	Y_POS_SPECS_BASELINE = 1,
	X_POS_SPECS_LSB = 2,
	HINTS_USE_POINTSIZE = 4,
	USE_INTEGER_SCALING = 8,
	INSTRUCTIONS_CHANGE_ADVANCEWIDTHS = 16,
	X_POS_SPECS_BASELINE = 32,
	Y_POS_SPECS_TSB = 64
} sfntHeaderFlagBits;

#define SFNT_MAGIC					0x5F0F3CF5
#define SHORT_INDEX_TO_LOC_FORMAT		0
#define LONG_INDEX_TO_LOC_FORMAT		1
#define GLYPH_DATA_FORMAT				0
#define FONT_HEADER_VERSION			0x10000

typedef struct {
	Fixed		version;			/* for this table, set to 1.0 */
	Fixed		fontRevision;		/* For Font Manufacturer */
	unsigned long	checkSumAdjustment;
	unsigned long	magicNumber; 		/* signature, should always be 0x5F0F3CF5  == MAGIC */
	unsigned short	flags;
	unsigned short	unitsPerEm;		/* Specifies how many in Font Units we have per EM */

	BigDate		created;
	BigDate		modified;

	/** This is the font wide bounding box in ideal space
	(baselines and metrics are NOT worked into these numbers) **/
	short		xMin;
	short		yMin;
	short		xMax;
	short		yMax;

	unsigned short		macStyle;			/* macintosh style word */
	unsigned short		lowestRecPPEM; 	/* lowest recommended pixels per Em */

	/* 0: fully mixed directional glyphs, 1: only strongly L->R or T->B glyphs, 
	   -1: only strongly R->L or B->T glyphs, 2: like 1 but also contains neutrals,
	   -2: like -1 but also contains neutrals */
	short		fontDirectionHint;

	short		indexToLocFormat;
	short		glyphDataFormat;
} sfnt_FontHeader;

#define METRIC_HEADER_FORMAT		0x10000

typedef struct {
	Fixed		version;				/* for this table, set to 1.0 */
	short		ascender;
	short		descender;
	short		lineGap;				/* linespacing = ascender - descender + linegap */
	unsigned short	advanceMax;	
	short		sideBearingMin;		/* left or top */
	short		otherSideBearingMin;	/* right or bottom */
	short		extentMax; 			/* Max of ( SB[i] + bounds[i] ), i loops through all glyphs */
	short		caretSlopeNumerator;
	short		caretSlopeDenominator;
	short		caretOffset;

	unsigned long	reserved1, reserved2;	/* set to 0 */

	short		metricDataFormat;		/* set to 0 for current format */
	unsigned short	numberLongMetrics;		/* if format == 0 */
} sfnt_MetricsHeader;

typedef sfnt_MetricsHeader sfnt_HorizontalHeader;
typedef sfnt_MetricsHeader sfnt_VerticalHeader;

#define MAX_PROFILE_VERSION		0x10000

typedef struct {
	Fixed			version;				/* for this table, set to 1.0 */
	unsigned short		numGlyphs;
	unsigned short		maxPoints;			/* in an individual glyph */
	unsigned short		maxContours;			/* in an individual glyph */
	unsigned short		maxCompositePoints;	/* in an composite glyph */
	unsigned short		maxCompositeContours;	/* in an composite glyph */
	unsigned short		maxElements;			/* set to 2, or 1 if no twilightzone points */
	unsigned short		maxTwilightPoints;		/* max points in element zero */
	unsigned short		maxStorage;			/* max number of storage locations */
	unsigned short		maxFunctionDefs;		/* max number of FDEFs in any preprogram */
	unsigned short		maxInstructionDefs;		/* max number of IDEFs in any preprogram */
	unsigned short		maxStackElements;		/* max number of stack elements for any individual glyph */
	unsigned short		maxSizeOfInstructions;	/* max size in bytes for any individual glyph */
	unsigned short		maxComponentElements;	/* number of glyphs referenced at top level */
	unsigned short		maxComponentDepth;	/* levels of recursion, 1 for simple components */
} sfnt_maxProfileTable;


typedef struct {
	unsigned short	advance;
	short 	sideBearing;
} sfnt_GlyphMetrics;

typedef sfnt_GlyphMetrics sfnt_HorizontalMetrics;
typedef sfnt_GlyphMetrics sfnt_VerticalMetrics;

typedef short sfnt_ControlValue;

/*
 *	Char2Index structures, including platform IDs
 */
typedef struct {
	unsigned short	format;
	unsigned short	length;
	unsigned short	version;
} sfnt_mappingTable;

typedef struct {
	unsigned short	platformID;
	unsigned short	specificID;
	unsigned long	offset;
} sfnt_platformEntry;

typedef struct {
	unsigned short	version;
	unsigned short	numTables;
	sfnt_platformEntry platform[1];	/* platform[numTables] */
} sfnt_char2IndexDirectory;
#define SIZEOFCHAR2INDEXDIR		4

typedef struct {
	unsigned short platformID;
	unsigned short specificID;
	unsigned short languageID;
	unsigned short nameID;
	unsigned short length;
	unsigned short offset;
} sfnt_NameRecord;

typedef struct {
	unsigned short format;
	unsigned short count;
	unsigned short stringOffset;
/*	sfnt_NameRecord[count]	*/
} sfnt_NamingTable;

#define DEVWIDTHEXTRA	2	/* size + max */
/*
 *	Each record is n+2 bytes, padded to long word alignment.
 *	First byte is ppem, second is maxWidth, rest are widths for each glyph
 */
typedef struct {
	short				version;
	short				numRecords;
	long				recordSize;
	/* Byte widths[numGlyphs+DEVWIDTHEXTRA] * numRecords */
} sfnt_DeviceMetrics;

#define	stdPostTableFormat		0x10000
#define	wordPostTableFormat	0x20000
#define	bytePostTableFormat	0x28000
#define	richardsPostTableFormat	0x30000

typedef struct {
	Fixed	version;
	Fixed	italicAngle;
	short	underlinePosition;
	short	underlineThickness;
	short	isFixedPitch;
	short	pad;
	unsigned long	minMemType42;
	unsigned long	maxMemType42;
	unsigned long	minMemType1;
	unsigned long	maxMemType1;
/* if version == 2.0
	{
		numberGlyphs;
		unsigned short[numberGlyphs];
		pascalString[numberNewNames];
	}
	else if version == 2.5
	{
		numberGlyphs;
		int8[numberGlyphs];
	}
*/		
} sfnt_PostScriptInfo;

typedef enum outlinePacking {
	ONCURVE = 1,
	XSHORT = 2,
	YSHORT = 4,
	REPEAT_FLAGS = 8,
/* IF XSHORT */
	SHORT_X_IS_POS = 16,		/* the short vector is positive */
/* ELSE */
	NEXT_X_IS_ZERO = 16,		/* the relative x coordinate is zero */
/* ENDIF */
/* IF YSHORT */
	SHORT_Y_IS_POS = 32,		/* the short vector is positive */
/* ELSE */
	NEXT_Y_IS_ZERO = 32		/* the relative y coordinate is zero */
/* ENDIF */
} outlinePacking;

typedef enum componentPacking {
	COMPONENTCTRCOUNT = -1,
	ARG_1_AND_2_ARE_WORDS = 1,		/* if not, they are bytes */
	ARGS_ARE_XY_VALUES = 2,			/* if not, they are points */
	ROUND_XY_TO_GRID = 4,
	WE_HAVE_A_SCALE = 8,				/* if not, Sx = Sy = 1.0 */
	NON_OVERLAPPING = 16,
	MORE_COMPONENTS = 32,			/* if not, this is the last one */
	WE_HAVE_AN_X_AND_Y_SCALE = 64,	/* Sx != Sy */
	WE_HAVE_A_TWO_BY_TWO = 128,		/* t00, t01, t10, t11 */
	WE_HAVE_INSTRUCTIONS = 256,		/* short count followed by instructions follows */
	USE_MY_METRICS = 512				/* use my metrics for parent glyph */
} componentPacking;

typedef struct {
	unsigned short firstCode;
	unsigned short entryCount;
	short idDelta;
	unsigned short idRangeOffset;
} sfnt_subheader;

typedef struct {
	unsigned short segCountX2;
	unsigned short searchRange;
	unsigned short entrySelector;
	unsigned short rangeShift;
} sfnt_4_subheader;

/* sfnt_enum.h */

typedef enum {
	plat_Unicode,
	plat_Macintosh,
	plat_ISO,
	plat_MS
} platformEnums;

#define tag_FontHeader			  'daeh'  /* (*(LPDWORD)"head") */
#define tag_HoriHeader			  'aehh'  /* (*(LPDWORD)"hhea") */
#define tag_VertHeader			  'aehv'  /* (*(LPDWORD)"vhea") */
#define tag_IndexToLoc			  'acol'  /* (*(LPDWORD)"loca") */
#define tag_MaxProfile			  'pxam'  /* (*(LPDWORD)"maxp") */
#define tag_ControlValue		  ' tvc'  /* (*(LPDWORD)"cvt ") */
#define tag_PreProgram			  'perp'  /* (*(LPDWORD)"prep") */
#define tag_GlyphData			    'fylg'  /* (*(LPDWORD)"glyf") */
#define tag_HorizontalMetrics	'xtmh'  /* (*(LPDWORD)"hmtx") */
#define tag_VerticalMetrics	  'xtmv'  /* (*(LPDWORD)"vmtx") */
#define tag_CharToIndexMap	  'pamc'  /* (*(LPDWORD)"cmap") */
#define tag_FontProgram			  'mgpf'  /* (*(LPDWORD)"fpgm") */

#define tag_Kerning				    'nrek'  /* (*(LPDWORD)"kern") */
#define tag_HoriDeviceMetrics	'xmdh'  /* (*(LPDWORD)"hdmx") */
#define tag_NamingTable			  'eman'  /* (*(LPDWORD)"name") */
#define tag_PostScript			  'tsop'  /* (*(LPDWORD)"post") */

#if 0
/* access.h */
#define fNoError			 0
#define fTableNotFound		-1
#define fNameNotFound		-2
#define fMemoryError		-3
#define fUnimplemented		-4
#define fCMapNotFound		-5
#define fGlyphNotFound		-6

typedef long FontError;
#endif

typedef struct FontTableInfo {
	long		offset;			/* from beginning of sfnt to beginning of the table */
	long		length;			/* length of the table */
	long		checkSum;		/* checkSum of the table */
} FontTableInfo;

#define RAW_TRUE_TYPE_SIZE 512

#endif
