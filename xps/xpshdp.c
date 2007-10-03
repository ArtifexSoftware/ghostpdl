#include "ghostxps.h"

/* HD-Photo decoder should go here. */

int 
xps_decode_hdphoto(gs_memory_t *mem, byte *buf, int len, xps_image_t *image)
{
    return gs_throw(-1, "HD-Photo codec is not available");
}

