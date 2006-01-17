#include "std.h"
#include "gsmemory.h"
#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"

#include "png_.h"

#include "mt_error.h"
#include "metimage.h"

/*
 * PNG using libpng directly (no gs wrappers)
 */

struct mt_png_io_s
{
    byte *ptr;
    byte *lim;
};

private void mt_png_read(png_structp png, png_bytep data, png_size_t length)
{
    struct mt_png_io_s *io = png_get_io_ptr(png);
    if (io->ptr + length > io->lim)
	png_error(png, "Read Error");
    memcpy(data, io->ptr, length);
    io->ptr += length;
}

int
mt_decode_png(gs_memory_t *mem, byte *rbuf, int rlen, met_image_t *image)
{
    png_structp png;
    png_infop info;
    int color, stride, size, y;
    struct mt_png_io_s io;
    int npasses;
    int pass;

    io.ptr = rbuf;
    io.lim = rbuf + rlen;

    /* TODO use png_create_read_struct2 with gs memory functions */
    /* TODO fix mt_png_error to do the right thing */

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); // mt_png_error, mt_png_warn);
    if (!png)
	return mt_throw(-1, "png_create_read_struct");
    
    info = png_create_info_struct(png);
    if (!info)
	return mt_throw(-1, "png_create_info_struct");
    
    png_set_read_fn(png, &io, mt_png_read);

    if (setjmp(png_jmpbuf(png))) {
	png_destroy_read_struct(&png, &info, NULL);
	return mt_throw(-1, "png reading failed");
    }

    png_read_info(png, info);

    image->width = png_get_image_width(png, info);
    image->height = png_get_image_height(png, info);
    image->bits = png_get_bit_depth(png, info);
    color = png_get_color_type(png, info);

    if (png_get_interlace_type(png, info) == PNG_INTERLACE_ADAM7)
	npasses = png_set_interlace_handling(png);
    else
	npasses = 1;

    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color & PNG_COLOR_MASK_ALPHA)
	png_set_strip_alpha(png);

    if (image->bits == 16) {
	image->bits = 8;
	png_set_strip_16(png);
    }

    /* XXX does this return the transformed color type or not? */
    color = png_get_color_type(png, info);

    switch (color) {
    case PNG_COLOR_TYPE_GRAY:
	image->comps = 1;
	image->colorspace = MT_GRAY;
	break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
	image->comps = 1;
	image->colorspace = MT_GRAY;
	break;
    case PNG_COLOR_TYPE_PALETTE:
    case PNG_COLOR_TYPE_RGB:
	image->comps = 3;
	image->colorspace = MT_RGB;
	break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
	image->comps = 3;
	image->colorspace = MT_RGB;
	break;
    default:
	return mt_throw(-1, "cannot handle this png color type");
    }

    image->xres = 96;
    image->yres = 96;
    
    if (info->valid & PNG_INFO_pHYs) {
	png_uint_32 xres, yres;
	int unit;
	png_get_pHYs(png, info, &xres, &yres, &unit);
	if (unit == PNG_RESOLUTION_METER) {
	    image->xres = xres * 0.0254 + 0.5;
	    image->yres = yres * 0.0254 + 0.5;
	}
    }

    stride = (image->width * image->comps * image->bits + 7) / 8;
    size = stride * image->height;

    // NB: size * 2 hack covers up a miss calculated size of image buffer
    // preventing memory usage outside of the allocation.
    image->samples = gs_alloc_bytes(mem, size * 2, "decodepng");

    for (pass = 0; pass < npasses; pass++) {
	for (y = 0; y < image->height; y++) {
	    png_read_row(png, image->samples + (y * stride), NULL);
	}
    }

    png_read_end(png, NULL);           
    png_destroy_read_struct(&png, &info, NULL);

    return mt_okay;
}

