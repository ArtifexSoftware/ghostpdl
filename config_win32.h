/* configuration header file for compiling under Microsoft Windows */

/* update package version here */
#define PACKAGE "jbig2dec"
#define VERSION "0.3"

/* define this iff you are linking to/compiling in libpng */
#define HAVE_LIBPNG

#ifdef _MSC_VER /* Microsoft Visual C+*/

  typedef signed char             int8_t;
  typedef short int               int16_t;
  typedef int                     int32_t;
  typedef __int64                 int64_t;
 
  typedef unsigned char             uint8_t;
  typedef unsigned short int        uint16_t;
  typedef unsigned int              uint32_t;
  /* no uint64_t */

#  define vsnprintf _vsnprintf
#  define snprintf _snprintf

#endif /* _MSC_VER */
