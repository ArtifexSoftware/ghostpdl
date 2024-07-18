/* #included from psi/zfilter.c */

#include "surfx.h"

static int
urf_setup(os_ptr dop, int *width, int *bpp, byte *white)
{
    if (r_has_type(dop, t_dictionary)) {
        int code;
        bool cmyk;

        check_dict_read(*dop);
        if ((code = dict_int_param(dop, "Width", 1, max_int, URF_default_width, width)) < 0)
            return code;
        if ((code = dict_int_param(dop, "BPP", 8, 32, URF_default_bpp, bpp)) < 0)
            return code;
        if (*bpp != 8 && *bpp != 24 && *bpp != 32)
            return_error(gs_error_rangecheck);
        if ((code = dict_bool_param(dop, "CMYK", true, &cmyk)) < 0)
            return code;
        if ((cmyk && *bpp != 32) || (!cmyk && *bpp == 32))
            return_error(gs_error_rangecheck);
        *white = cmyk ? 0 : 0xFF;
        return 1;
    } else {
        *width = URF_default_width;
        *bpp = URF_default_bpp;
        *white = 0xFF;
        return 0;
    }
}

/* <source> <dict> URFDecode/filter <file> */
static int
zURFD(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream_URFD_state state;
    int code;

    if (s_URFD_template.set_defaults)
        s_URFD_template.set_defaults((stream_state *)&state);

    code = urf_setup(op, &state.width, &state.bpp, &state.white);
    if (code < 0)
        return code;

    return filter_read(i_ctx_p, 0, &s_URFD_template, (stream_state *) & state, 0);
}
