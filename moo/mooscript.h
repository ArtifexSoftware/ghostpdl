#include "memory_.h"
#include "math_.h"

#undef inline
#define byte fz_byte

#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"

#include "gp.h"

#include "gsgc.h"
#include "gstypes.h"
#include "gsstate.h"
#include "gsmatrix.h"
#include "gscoord.h"
#include "gsmemory.h"
#include "gsparam.h"
#include "gsdevice.h"
#include "scommon.h"
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsimage.h"
#include "gscspace.h"
#include "gsptype1.h"
#include "gscolor1.h"
#include "gscolor2.h"
#include "gscolor3.h"
#include "gsutil.h"
#include "gsicc.h"

#include "gstrans.h"

#include "gxpath.h"     /* gsshade.h depends on it */
#include "gxfixed.h"    /* gsshade.h depends on it */
#include "gxmatrix.h"   /* gxtype1.h depends on it */
#include "gsshade.h"
#include "gsfunc.h"
#include "gsfunc3.h"    /* we use stitching and exponential interp */

#include "gxfont.h"
#include "gxchar.h"
#include "gxcolor2.h" /* Required for definition of gs_pattern1_instance_t */
#include "gxtype1.h"
#include "gxfont1.h"
#include "gxfont42.h"
#include "gxfcache.h"
#include "gxistate.h"

#include "gxdevice.h" /* so we can include gxht.h below */
#include "gxht.h" /* gsht1.h is incomplete, we need storage size of gs_halftone */
#include "gsht1.h"

#include "gzstate.h"
#include "gzpath.h"
#include "gzcpath.h"

#include "gsicc_manage.h"
#include "gscms.h"
#include "gsicc_cache.h"

#include "pltop.h"
#include "plparse.h" /* for e_ExitLanguage */

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define ABS(a) ((a) < 0 ? -(a) : (a))
#endif

#undef printf
#undef putchar

fz_device *fz_new_ghost_device(gs_memory_t *memory, gs_state *pgs);
