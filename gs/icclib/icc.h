#ifndef ICC_H
#define ICC_H
/* 
 * International Color Consortium Format Library (icclib)
 *
 * Author:  Graeme W. Gill
 * Date:    99/11/29
 * Version: 1.23
 *
 * Copyright 1997, 1998, 1999 Graeme W. Gill
 * Please refer to COPYRIGHT file for details.
 */

/*
 *  Note XYZ scaling to 1.0, not 100.0
 */

/* Platform specific defines */
#define INT8   signed char		/* 8 bit signed */
#define INT16  signed short		/* 16 bit signed */
#define INT32  signed long		/* 32 bit signed */
#define ORD8   unsigned char	/* 8 bit unsigned */
#define ORD16  unsigned short	/* 16 bit unsigned */
#define ORD32  unsigned long	/* 32 bit unsigned */

#include "icc9809.h"	/* Standard ICC format definitions, version ICC.1:1998-09 with mods noted. */ 

/* Note that the prefix icm is used for the native Machine */
/* equivalents of the file structures defined in icc34.h */
/* It is assumed that the native machine size is 32 bits */

/* --------------------------------- */
/* Assumed constants                 */

#define MAX_CHAN 15		/* Maximum number of color channels */

/* --------------------------------- */
/* tag and other compound structures */

typedef int icmSig;	/* Otherwise un-enumerated 4 byte signature */

typedef struct {
	ORD32 l;			/* High and low components of signed 64 bit */
	INT32 h;
} int64bits;

typedef struct {
	ORD32 l,h;			/* High and low components of unsigned 64 bit */
} uint64bits;

/* XYZ Number */
typedef struct {
    double  X;
    double  Y;
    double  Z;
} icmXYZNumber;

/* Response 16 number */
typedef struct {
	double	       deviceValue;	/* The device value in range 0.0 - 1.0 */
	double	       measurement;	/* The reading value */
} icmResponse16Number;

/*
 *  read and write method error codes:
 *  0 = sucess
 *  1 = file format/logistical error
 *  2 = system error
 */

#define BASE_MEMBERS																	\
	/* Private: */																		\
	icTagTypeSignature  ttype;		/* The tag type signature */						\
	struct _icc    *icp;			/* Pointer to ICC we're a part of */				\
	int	           touched;			/* Flag for write bookeeping */						\
    int            refcount;		/* Reference count for sharing */					\
	unsigned int   (*get_size)(struct _icmBase *p);										\
	int            (*read)(struct _icmBase *p, unsigned long len, unsigned long of);	\
	int            (*write)(struct _icmBase *p, unsigned long of);						\
	void           (*free)(struct _icmBase *p);											\
	/* Public: */																		\
	void           (*dump)(struct _icmBase *p, FILE *op, int verb);						\
	int            (*allocate)(struct _icmBase *p);									

/* Base tag element data object */
struct _icmBase {
	BASE_MEMBERS
}; typedef struct _icmBase icmBase;

/* UInt8 Array */
struct _icmUInt8Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    unsigned int   *data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt8Array icmUInt8Array;
static icmBase *new_icmUInt8Array(struct _icc *icp);

/* uInt16 Array */
struct _icmUInt16Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt16Array icmUInt16Array;
static icmBase *new_icmUInt16Array(struct _icc *icp);

/* uInt32 Array */
struct _icmUInt32Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    unsigned int	*data;		/* Pointer to array of data */ 
}; typedef struct _icmUInt32Array icmUInt32Array;
static icmBase *new_icmUInt32Array(struct _icc *icp);

/* UInt64 Array */
struct _icmUInt64Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    uint64bits		*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmUInt64Array icmUInt64Array;
static icmBase *new_icmUInt64Array(struct _icc *icp);

/* u16Fixed16 Array */
struct _icmU16Fixed16Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmU16Fixed16Array icmU16Fixed16Array;
static icmBase *new_icmU16Fixed16Array(struct _icc *icp);

/* s15Fixed16 Array */
struct _icmS15Fixed16Array {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    double			*data;		/* Pointer to array of hight data */ 
}; typedef struct _icmS15Fixed16Array icmS15Fixed16Array;
static icmBase *new_icmS15Fixed16Array(struct _icc *icp);

/* XYZ Array */
struct _icmXYZArray {
	BASE_MEMBERS
	unsigned long	size;		/* Allocated and used size of the array */
    icmXYZNumber	*data;		/* Pointer to array of data */ 
}; typedef struct _icmXYZArray icmXYZArray;
static icmBase *new_icmXYZArray(struct _icc *icp);

/* Curve */
typedef enum {
    icmCurveUndef           = -1, /* Undefined curve */
    icmCurveLin             = 0,  /* Linear transfer curve */
    icmCurveGamma           = 1,  /* Gamma power transfer curve */
    icmCurveSpec            = 2   /* Specified curve */
} icmCurveStyle;

struct _icmCurve {
	BASE_MEMBERS
    icmCurveStyle   flag;		/* Style of curve */
	unsigned long	size;		/* Allocated and used size of the array */
    double         *data;  		/* Curve data scaled to range 0.0 - 1.0 */
								/* or data[0] = gamma value */
	/* Translate a value through the curve, return warning flags */
	int (*lookup_fwd) (struct _icmCurve *p, double *out, double *in);	/* Forwards */
	int (*lookup_bwd) (struct _icmCurve *p, double *out, double *in);	/* Backwards */

	/* Private: Reverse lookup accelerator */
	double rmin, rmax, rw;		/* Range and step size of reverse grid */
	long rsize;		/* Number of reverse lists */
	int **rlists;	/* Array of list of fwd values that may contain output value */

}; typedef struct _icmCurve icmCurve;
static icmBase *new_icmCurve(struct _icc *icp);

/* Data */
typedef enum {
    icmDataUndef           = -1, /* Undefined data curve */
    icmDataASCII           = 0,  /* ASCII data curve */
    icmDataBin             = 1   /* Binary data */
} icmDataStyle;

struct _icmData {
	BASE_MEMBERS
    icmDataStyle	flag;		/* Style of data */
	unsigned long	size;		/* Allocated and used size of the array (inc ascii null) */
    unsigned char	*data;  	/* data or string, NULL if size == 0 */
}; typedef struct _icmData icmData;
static icmBase *new_icmData(struct _icc *icp);

/* text */
struct _icmText {
	BASE_MEMBERS
	unsigned long	  size;			/* Allocated and used size of desc, inc null */
	char              *data;		/* ascii string (null terminated), NULL if size==0 */
}; typedef struct _icmText icmText;
static icmBase *new_icmText(struct _icc *icp);

/* The base date time number */
struct _icmDateTimeNumber {
	BASE_MEMBERS
    unsigned int      year;
    unsigned int      month;
    unsigned int      day;
    unsigned int      hours;
    unsigned int      minutes;
    unsigned int      seconds;
}; typedef struct _icmDateTimeNumber icmDateTimeNumber;
static icmBase *new_icmDateTimeNumber(struct _icc *icp);

#ifdef NEW
/ * DeviceSettings */

/*
   I think this all works like this:

Valid setting = (   (platform == platform1 and platform1.valid)
                 or (platform == platform2 and platform2.valid)
                 or ...
                )

where
	platformN.valid = (   platformN.combination1.valid
	                   or platformN.combination2.valid
	                   or ...
	                  )

where
	platformN.combinationM.valid = (    platformN.combinationM.settingstruct1.valid
	                                and platformN.combinationM.settingstruct2.valid
	                                and ...
	                               )

where
	platformN.combinationM.settingstructP.valid = (   platformN.combinationM.settingstructP.setting1.valid
	                                               or platformN.combinationM.settingstructP.setting2.valid
	                                               or ...
	                                              )

 */

/* The Settings Structure holds an array of settings of a particular type */
struct _icmSettingStruct {
	BASE_MEMBERS
	icSettingsSig       settingSig;		/* Setting identification */
	unsigned long       numSettings; 	/* number of setting values */
	union {								/* Setting values - type depends on Sig */
		icUInt64Number      *resolution;
		icDeviceMedia       *media;
		icDeviceDither      *halftone;
	}
}; typedef struct _icmSettingStruct icmSettingStruct;
static icmBase *new_icmSettingStruct;

/* A Setting Combination holds all arrays of different setting types */
struct _icmSettingComb {
	unsigned long       numStructs;   /* num of setting structures */
	icmSettingStruct    *data;
}; typedef struct _icmSettingComb icmSettingComb;
static icmBase *new_icmSettingComb;

/* A Platform Entry holds all setting combinations */
struct _icmPlatformEntry {
	icPlatformSignature platform;
	unsigned long       numCombinations;    /* num of settings and allocated array size */
	icmSettingComb      *data; 
}; typedef struct _icmPlatformEntry icmPlatformEntry;
static icmBase *new_icmPlatformEntry;

/* The Device Settings holds all platform settings */
struct _icmDeviceSettings {
	unsigned long       numPlatforms;	/* num of platforms and allocated array size */
	icmPlatformEntry    *data;			/* Array of pointers to platform entry data */
}; typedef struct _icmDeviceSettings icmDeviceSettings;
static icmBase *new_icmDeviceSettings;

#endif /* NEW */

/* lut */
struct _icmLut {
/* Private: */
	BASE_MEMBERS
	/* Cache appropriate normalization routines */
	int dinc[MAX_CHAN];				/* Dimensional increment through clut */
	int dcube[1 << MAX_CHAN];		/* Hyper cube offsets */

	/* return non zero if matrix is non-unity */
	int (*nu_matrix) (struct _icmLut *pp);

	/* return the minimum and maximum values of the given channel in the clut */
	void (*min_max) (struct _icmLut *pp, double *minv, double *maxv, int chan);

	/* Translate color values through 3x3 matrix, input tables only, multi-dimensional lut, */
	/* or output tables, */
	int (*lookup_matrix)  (struct _icmLut *pp, double *out, double *in);
	int (*lookup_input)   (struct _icmLut *pp, double *out, double *in);
	int (*lookup_clut_nl) (struct _icmLut *pp, double *out, double *in);
	int (*lookup_clut_sx) (struct _icmLut *pp, double *out, double *in);
	int (*lookup_output)  (struct _icmLut *pp, double *out, double *in);

/* Public: */
    unsigned int	inputChan;      /* Num of input channels */
    unsigned int	outputChan;     /* Num of output channels */
    unsigned int	clutPoints;     /* Num of grid points */
    unsigned int	inputEnt;       /* Num of in-table entries (must be 256 for Lut8) */
    unsigned int	outputEnt;      /* Num of out-table entries (must be 256 for Lut8) */
    double			e[3][3];		/* 3 * 3 array */
	double	        *inputTable;	/* The in-table: [inputChan * inputEnt] */
	double	        *clutTable;		/* The clut: [(clutPoints ^ inputChan) * outputChan] */
	double	        *outputTable;	/* The out-table: [outputChan * outputEnt] */
	/* inputTable  is organized [inputChan 0..ic-1][inputEnt 0..ie-1] */
	/* clutTable   is organized [inputChan 0, 0..cp-1]..[inputChan ic-1, 0..cp-1]
	                                                                [outputChan 0..oc-1] */
	/* outputTable is organized [outputChan 0..oc-1][outputEnt 0..oe-1] */

	/* Helper function to setup the three tables contents */
	int (*set_tables) (
		struct _icmLut *p,						/* Pointer to Lut object */
		void   *cbctx,							/* Opaque callback context pointer value */
		icColorSpaceSignature insig, 			/* Input color space */
		icColorSpaceSignature outsig, 			/* Output color space */
		void (*infunc)(void *cbctx, double *out, double *in),
								/* Input transfer function, inspace->inspace' (NULL = default) */
		double *inmin, double *inmax,			/* Maximum range of inspace' values */
												/* (NULL = default) */
		void (*clutfunc)(void *cbntx, double *out, double *in),
								/* inspace' -> outspace' transfer function */
		double *clutmin, double *clutmax,		/* Maximum range of outspace' values */
												/* (NULL = default) */
		void (*outfunc)(void *cbntx, double *out, double *in));
								/* Output transfer function, outspace'->outspace (NULL = deflt) */
		
}; typedef struct _icmLut icmLut;
static icmBase *new_icmLut(struct _icc *icp);

/* Measurement Data */
struct _icmMeasurement {
	BASE_MEMBERS
    icStandardObserver           observer;       /* Standard observer */
    icmXYZNumber                 backing;        /* XYZ for backing */
    icMeasurementGeometry        geometry;       /* Meas. geometry */
    double                       flare;          /* Measurement flare */
    icIlluminant                 illuminant;     /* Illuminant */
}; typedef struct _icmMeasurement icmMeasurement;
static icmBase *new_icmMeasurement(struct _icc *icp);

/* Named color */

/* Structure that holds each named color data */
typedef struct {
	struct _icc      *icp;				/* Pointer to ICC we're a part of */
	char              root[32];			/* Root name for color */
	double            pcsCoords[3];		/* icmNC2: PCS coords of color */
	double            deviceCoords[MAX_CHAN];	/* Dev coords of color */
} icmNamedColorVal;

struct _icmNamedColor {
	BASE_MEMBERS
    unsigned int      vendorFlag;		/* Bottom 16 bits for IC use */
    unsigned int      count;			/* Count of named colors */
    unsigned int      nDeviceCoords;	/* Num of device coordinates */
    char              prefix[32];		/* Prefix for each color name (null terminated) */
    char              suffix[32];		/* Suffix for each color name (null terminated) */
    icmNamedColorVal  *data;			/* Array of [count] color values */
}; typedef struct _icmNamedColor icmNamedColor;
static icmBase *new_icmNamedColor(struct _icc *icp);

/* textDescription */
struct _icmTextDescription {
	/* Private: */
	BASE_MEMBERS
	int            (*core_read)(struct _icmTextDescription *p, char **bpp, char *end);
	int            (*core_write)(struct _icmTextDescription *p, char **bpp);

	/* Public: */
	unsigned long	  size;			/* Allocated and used size of desc, inc null */
	char              *desc;		/* ascii string (null terminated) */

	unsigned int      ucLangCode;	/* UniCode language code */
	unsigned long	  ucSize;		/* Allocated and used size of ucDesc in wchars, inc null */
	ORD16             *ucDesc;		/* The UniCode description (null terminated) */

	ORD16             scCode;		/* ScriptCode code */
	unsigned long	  scSize;		/* Used size of scDesc in bytes, inc null */
	ORD8              scDesc[67];	/* ScriptCode Description (null terminated, max 67) */
}; typedef struct _icmTextDescription icmTextDescription;
static icmBase *new_icmTextDescription(struct _icc *icp);

/* Profile sequence structure */
struct _icmDescStruct {
	struct _icc      *icp;				/* Pointer to ICC we're a part of */
	int             (*allocate)(struct _icmDescStruct *p);	/* Allocate method */
    icmSig            deviceMfg;		/* Dev Manufacturer */
    unsigned int      deviceModel;		/* Dev Model */
    uint64bits        attributes;		/* Dev attributes */
    icTechnologySignature technology;	/* Technology sig */
	icmTextDescription device;			/* Manufacturer text (sub structure) */
	icmTextDescription model;			/* Model text (sub structure) */
}; typedef struct _icmDescStruct icmDescStruct;
static icmBase *new_icmDescStruct();

/* Profile sequence description */
struct _icmProfileSequenceDesc {
	BASE_MEMBERS
    unsigned int      count;			/* Number of descriptions */
	icmDescStruct     *data;			/* array of [count] descriptions */
}; typedef struct _icmProfileSequenceDesc icmProfileSequenceDesc;
static icmBase *new_icmProfileSequenceDesc(struct _icc *icp);

/* signature (only ever used for technology ??) */
struct _icmSignature {
	BASE_MEMBERS
    icTechnologySignature sig;	/* Signature */
}; typedef struct _icmSignature icmSignature;
static icmBase *new_icmSignature(struct _icc *icp);

/* Per channel Screening Data */
typedef struct {
    double            frequency;		/* Frequency */
    double            angle;			/* Screen angle */
    icSpotShape       spotShape;		/* Spot Shape encodings below */
} icmScreeningData;

struct _icmScreening {
	BASE_MEMBERS
    unsigned int      screeningFlag;	/* Screening flag */
    unsigned int      channels;			/* Number of channels */
    icmScreeningData  *data;			/* Array of screening data */
}; typedef struct _icmScreening icmScreening;
static icmBase *new_icmScreening(struct _icc *icp);

/* Under color removal, black generation */
struct _icmUcrBg {
	BASE_MEMBERS
    unsigned int      UCRcount;			/* Undercolor Removal Curve length */
    double           *UCRcurve;		    /* The array of UCR curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
    unsigned int      BGcount;			/* Black generation Curve length */
    double           *BGcurve;			/* The array of BG curve values, 0.0 - 1.0 */
										/* or 0.0 - 100 % if count = 1 */
	unsigned long	  size;				/* Allocated and used size of desc, inc null */
	char              *string;			/* UcrBg description (null terminated) */
}; typedef struct _icmUcrBg icmUcrBg;
static icmBase *new_icmUcrBg(struct _icc *icp);

/* viewingConditionsType */
struct _icmViewingConditions {
	BASE_MEMBERS
    icmXYZNumber    illuminant;		/* In candelas per sq. meter */
    icmXYZNumber    surround;		/* In candelas per sq. meter */
    icIlluminant    stdIlluminant;	/* See icIlluminant defines */
}; typedef struct _icmViewingConditions icmViewingConditions;
static icmBase *new_icmViewingConditions(struct _icc *icp);

/* Postscript Color Rendering Dictionary names type */
struct _icmCrdInfo {
	BASE_MEMBERS
    unsigned long    ppsize;		/* Postscript product name size (including null) */
    char            *ppname;		/* Postscript product name (null terminated) */
	unsigned long    crdsize[4];	/* Rendering intent 0-3 CRD names sizes (icluding null) */
	char            *crdname[4];	/* Rendering intent 0-3 CRD names (null terminated) */
}; typedef struct _icmCrdInfo icmCrdInfo;
static icmBase *new_icmCrdInfo(struct _icc *icp);

/* ------------------------------------------------- */
/* The Profile header */
struct _icmHeader {
	/* Private: */
	unsigned int           (*get_size)(struct _icmHeader *p);
	int                    (*read)(struct _icmHeader *p, unsigned long len, unsigned long of);
	int                    (*write)(struct _icmHeader *p, unsigned long of);
	void                   (*free)(struct _icmHeader *p);
	struct _icc            *icp;			/* Pointer to ICC we're a part of */
    unsigned int            size;			/* Profile size in bytes */

	/* public: */
	void                   (*dump)(struct _icmHeader *p, FILE *op, int verb);

	/* Values that must be set before writing */
    icProfileClassSignature deviceClass;	/* Type of profile */
    icColorSpaceSignature   colorSpace;		/* Clr space of data */
    icColorSpaceSignature   pcs;			/* PCS: XYZ or Lab */
    icRenderingIntent       renderingIntent;/* Rendering intent */

	/* Values that should be set before writing */
    icmSig                  manufacturer;	/* Dev manufacturer */
    icmSig		            model;			/* Dev model */
    uint64bits              attributes;		/* Device attributes.l */
    unsigned int            flags;			/* Various bits */

	/* Values that may optionally be set before writing */
    /* uint64bits           attributes;		   Device attributes.h (see above) */
    icmSig                  creator;		/* Profile creator */

	/* Values that are not normally set, since they have defaults */
    icmSig                  cmmId;			/* CMM for profile */
    int            			majv, minv, bfv;/* Format version - major, minor, bug fix */
    icmDateTimeNumber       date;			/* Creation Date */
    icPlatformSignature     platform;		/* Primary Platform */
    icmXYZNumber            illuminant;		/* Profile illuminant */

}; typedef struct _icmHeader icmHeader;
static icmHeader *new_icmHeader(struct _icc *icp);

/* ---------------------------------------------------------- */
/* Objects for accessing lookup functions */

/* Public: Parameter to get_luobj function */
typedef enum {
    icmFwd           = 0,  /* Device to PCS, or Device 1 to Last Device */
    icmBwd           = 1,  /* PCS to Device, or Last Device to Device */
    icmGamut         = 2,  /* PCS Gamut check */
    icmPreview       = 3   /* PCS to PCS preview */
} icmLookupFunc;

/* Public: Parameter to get_luobj function */
typedef enum {
    icmLuOrdNorm     = 0,  /* Normal profile preference: Lut, matrix, monochrome */
    icmLuOrdRev      = 1   /* Reverse profile preference: monochrome, matrix, monochrome */
} icmLookupOrder;

/* Publix: Lookup algorithm object type */
typedef enum {
    icmMonoFwdType       = 0,	/* Monochrome, Forward */
    icmMonoBwdType       = 1,	/* Monochrome, Backward */
    icmMatrixFwdType     = 2,	/* Matrix, Forward */
    icmMatrixBwdType     = 3,	/* Matrix, Backward */
    icmLutType           = 4	/* Multi-dimensional Lookup Table */
} icmLuAlgType;

#define LU_BASE_MEMBERS																	\
	/* Private: */																		\
	icmLuAlgType   ttype;		    		/* The object tag */						\
	struct _icc    *icp;					/* Pointer to ICC we're a part of */		\
	icRenderingIntent intent;				/* Rendering intent */						\
	icmLookupFunc function;					/* Functionality requested */				\
	icmXYZNumber pcswht, whitePoint, blackPoint;	/* Stuff needed for absolute intent */	\
    icColorSpaceSignature   inSpace;		/* Clr space of input */					\
    icColorSpaceSignature   outSpace;		/* Clr space of output */					\
	/* Public: */																		\
	void           (*free)(struct _icmLuBase *p);										\
	void           (*lutspaces) (struct _icmLuBase *p, icColorSpaceSignature *ins, int *inn,	\
	                                                    icColorSpaceSignature *outs, int *outn);\
											/* Internal native colorspaces */           \
	void           (*spaces) (struct _icmLuBase *p, icColorSpaceSignature *ins, int *inn,	\
	                                                icColorSpaceSignature *outs, int *outn,	\
	                                                icmLuAlgType *alg);                   	\
											/* External overall colorspaces */           \
	int            (*lookup) (struct _icmLuBase *p, double *out, double *in);

	/* Translate color values through profile */
	/* 0 = success */
	/* 1 = warning: clipping occured */
	/* 2 = fatal: other error */

/* Base lookup object */
struct _icmLuBase {
	LU_BASE_MEMBERS
}; typedef struct _icmLuBase icmLuBase;

/* Monochrome  Fwd & Bwd type object */
struct _icmLuMono {
	LU_BASE_MEMBERS
	icmCurve    *grayCurve;
}; typedef struct _icmLuMono icmLuMono;
static icmLuBase *new_icmLuMonoFwd(struct _icc *icp,
    icColorSpaceSignature inSpace, icColorSpaceSignature outSpace,
	icmXYZNumber whitePoint, icmXYZNumber blackPoint,
	icRenderingIntent intent, icmLookupFunc func);
static icmLuBase *new_icmLuMonoBwd(struct _icc *icp,
    icColorSpaceSignature inSpace, icColorSpaceSignature outSpace,
	icmXYZNumber whitePoint, icmXYZNumber blackPoint,
	icRenderingIntent intent, icmLookupFunc func);


/* 3D Matrix Fwd & Bwd type object */
struct _icmLuMatrix {
	LU_BASE_MEMBERS
	icmCurve    *redCurve,  *greenCurve,  *blueCurve;
    double		mx[3][3];	/* 3 * 3 conversion matrix */
}; typedef struct _icmLuMatrix icmLuMatrix;
static icmLuBase *new_icmLuMatrixFwd(struct _icc *icp,
    icColorSpaceSignature inSpace, icColorSpaceSignature outSpace,
	icmXYZNumber whitePoint, icmXYZNumber blackPoint,
	icRenderingIntent intent, icmLookupFunc func);
static icmLuBase *new_icmLuMatrixBwd(struct _icc *icp,
    icColorSpaceSignature inSpace, icColorSpaceSignature outSpace,
	icmXYZNumber whitePoint, icmXYZNumber blackPoint,
	icRenderingIntent intent, icmLookupFunc func);


/* Multi-D. Lut type object */
struct _icmLuLut {
	LU_BASE_MEMBERS
/* private: */
	icmLut *lut;								/* Lut to use */
	int    usematrix;							/* non-zero if matrix should be used */
    double imx[3][3];							/* 3 * 3 inverse conversion matrix */
	int    imx_valid;							/* Inverse matrix is valid */
	void (*in_normf)(double *out, double *in);	/* Lut input data normalizing function */
	void (*in_denormf)(double *out, double *in);/* Lut input data de-normalizing function */
	void (*out_normf)(double *out, double *in);	/* Lut output data normalizing function */
	void (*out_denormf)(double *out, double *in);/* Lut output de-normalizing function */

/* public: */
	/* Overall lookup function */
	int (*lookup_clut) (struct _icmLut *pp, double *out, double *in);	/* clut function */

	/* Components of lookup */
	int (*in_abs)  (struct _icmLuLut *p, double *out, double *in);	/* Should be in base class ? */
	int (*matrix)  (struct _icmLuLut *p, double *out, double *in);
	int (*input)   (struct _icmLuLut *p, double *out, double *in);
	int (*clut)    (struct _icmLuLut *p, double *out, double *in);
	int (*output)  (struct _icmLuLut *p, double *out, double *in);
	int (*out_abs) (struct _icmLuLut *p, double *out, double *in);	/* Should be in base class ? */

	/* Some inverse components */
	int (*inv_in_abs)  (struct _icmLuLut *p, double *out, double *in);	/* Should be in base class ? */
	int (*inv_matrix)  (struct _icmLuLut *p, double *out, double *in);
	int (*inv_out_abs) (struct _icmLuLut *p, double *out, double *in);	/* Should be in base class ? */

	/* Get various types of information about the LuLut */
	void (*get_info) (struct _icmLuLut *p, icmLut **lutp,
	                 icmXYZNumber *pcswhtp, icmXYZNumber *whitep,
	                 icmXYZNumber *blackp);

	/* Get the input space and output space ranges */
	void (*get_ranges) (struct _icmLuLut *p,
		double *inmin, double *inmax,		/* Maximum range of inspace values */
		double *outmin, double *outmax);	/* Maximum range of outspace values */

}; typedef struct _icmLuLut icmLuLut;
static icmLuBase *new_icmLuLut(struct _icc *icp, icTagSignature ttag,
    icColorSpaceSignature inSpace, icColorSpaceSignature outSpace,
	icmXYZNumber whitePoint, icmXYZNumber blackPoint,
	icRenderingIntent intent, icmLookupFunc func);

/* ---------------------------------------------------------- */
/* A tag */
typedef struct {
    icTagSignature      sig;			/* The tag signature */
	icTagTypeSignature  ttype;			/* The tag type signature */
    unsigned int        offset;			/* File offset to start header */
    unsigned int        size;			/* Size in bytes */
	icmBase            *objp;			/* In memory data structure */
} icmTag;

/* Pseudo intents valid as parameter to get_luobj(): */

	/* To be specified where an intent is not appropriate */
#define icmDefaultIntent ((icRenderingIntent)98)
	/* Represents icAbsoluteColorimetric, with overide XYZ PCS in/out */
#define icmAbsoluteColorimetricXYZ ((icRenderingIntent)99)

/* The ICC object */
struct _icc {
	unsigned int (*get_size)(struct _icc *p);				/* Return total size needed, 0 = err. */
	int          (*read)(struct _icc *p, FILE *fp, unsigned long of);	/* Returns error code */
	int          (*write)(struct _icc *p, FILE *fp, unsigned long of);/* Returns error code */
	void         (*dump)(struct _icc *p, FILE *op, int verb);	/* Dump whole icc */
	void         (*free)(struct _icc *p);						/* Free whole icc */
	int          (*find_tag)(struct _icc *p, icTagSignature sig);
															/* Returns 1 if found, 2 readable */
	icmBase *    (*read_tag)(struct _icc *p, icTagSignature sig);
															/* Returns pointer to object */
	icmBase *    (*add_tag)(struct _icc *p, icTagSignature sig, icTagTypeSignature ttype);
															/* Returns pointer to object */
	icmBase *    (*link_tag)(struct _icc *p, icTagSignature sig, icTagSignature ex_sig);
															/* Returns pointer to object */
	int          (*unread_tag)(struct _icc *p, icTagSignature sig);
														/* Unread a tag (free on refcount == 0 */
	int          (*read_all_tags)(struct _icc *p); /* Read all the tags, non-zero on error. */

	int          (*delete_tag)(struct _icc *p, icTagSignature sig);
															/* Returns 0 if deleted OK */
	icmLuBase *  (*get_luobj) (struct _icc *p, icmLookupFunc func,			/* Functionality */
	                                           icRenderingIntent intent,	/* Intent */
	                                           icmLookupOrder order);		/* Search Order */
													/* Return appropriate lookup object */
													/* NULL on error, check errc+err for reason */

	FILE            *fp;				/* File associated with object */
    icmHeader       *header;			/* The header */
	unsigned long    of;				/* Offset of the profile within the file */
    unsigned int     count;				/* Num tags in the profile */
    icmTag          *data;    			/* The tagTable and tagData */
	char             err[512];			/* Error message */
	int              errc;				/* Error code */

	}; typedef struct _icc icc;
extern icc *new_icc(void);

/* macro to allow updating the file pointer on systems with memory relocation */
#define icc_update_fp(icc, nfp)  ((icc)->fp = (nfp))

/* ---------------------------------------------------------- */
/* Public function declarations */
/* Create an empty object. Return null on error */
icc *new_icc(void);

/* Auxiliary support: */
/* (Maybe all the "2str" functions should be made public ?) */
/* Return a string that represents a tag */
char *tag2str(int tag);

/* Return a tag created from a string */
int str2tag(char *str);

/* Return a text abreviation of a colorspace */
char *ColorSpaceSignature2str(icColorSpaceSignature sig);

/* Return a text abreviation of a color lookup algorithm */
char *LuAlg2str(icmLuAlgType alg);

/* Should the following be left public, or force use of icmLut->set_tables() ? */

/* Parameter to getNormFunc function */
typedef enum {
    icmFromLuti   = 0,  /* return "fromo Lut normalized index" conversion function */
    icmToLuti     = 1,  /* return "to Lut normalized index" conversion function */
    icmFromLutv   = 2,  /* return "from Lut normalized value" conversion function */
    icmToLutv     = 3   /* return "to Lut normalized value" conversion function */
} icmNormFlag;

/* Return an appropriate color space normalization function, */
/* given the color space and Lut type */
/* Return 0 on success, 1 on match failure */
int getNormFunc(
	icColorSpaceSignature csig, 
	icTagTypeSignature    tagSig,
	icmNormFlag           flag,
	void               (**nfunc)(double *out, double *in)
);

/* ---------------------------------------------------------- */

#endif /* ICC_H */

