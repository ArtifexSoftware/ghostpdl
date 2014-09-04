/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Simple high level forms */
#include "ghost.h"
#include "oper.h"
#include "gxdevice.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "igstate.h"
#include "gxdevsop.h"
#include "gscoord.h"
#include "gsform1.h"
#include "gspath.h"
#include "gxpath.h"
#include "gzstate.h"

/* support for high level formss */

/* [CTM before Form Matrix applied] <<Form dictionary>> .beginform -
 */
static int zbeginform(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gx_device *cdev = gs_currentdevice_inline(igs);
    int code;
    float BBox[4], Matrix[6];
    gs_form_template_t tmplate;
    gs_point pt;
    gs_fixed_rect box;

    check_type(*op, t_dictionary);
    check_dict_read(*op);

    code = read_matrix(imemory, op - 1, &tmplate.CTM);
    if (code < 0)
        return code;

    code = dict_floats_param(imemory, op, "BBox", 4, BBox, NULL);
    if (code < 0)
        return code;
    if (code == 0)
       return_error(e_undefined);
    tmplate.BBox.p.x = BBox[0];
    tmplate.BBox.p.y = BBox[1];
    pt.x = tmplate.BBox.q.x = BBox[2];
    pt.y = tmplate.BBox.q.y = BBox[3];
 
    code = dict_floats_param(imemory, op, "Matrix", 6, Matrix, NULL);
    if (code < 0)
        return code;
    if (code == 0)
       return_error(e_undefined);

    tmplate.form_matrix.xx = Matrix[0];
    tmplate.form_matrix.xy = Matrix[1];
    tmplate.form_matrix.yx = Matrix[2];
    tmplate.form_matrix.yy = Matrix[3];
    tmplate.form_matrix.tx = Matrix[4];
    tmplate.form_matrix.ty = Matrix[5];

    tmplate.pcpath = igs->clip_path;
    code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_form_begin,
                            &tmplate, 0);

    /* return value > 0 means the device sent us back a matrix
     * and wants the CTM set to that.
     */
    if (code > 0)
    {
        gs_setmatrix(igs, &tmplate.CTM);
        gs_distance_transform(tmplate.BBox.q.x, tmplate.BBox.q.y, &tmplate.CTM, &pt);

        /* A form can legitimately have negative co-ordinates in paths
         * because it can be translated. But we always clip paths to the
         * page which (clearly) can't have negative co-ordinates. NB this
         * wouldn't be a problem if we didn't reset the CTM, but that would
         * break the form capture.
         * So here we temporarily set the clip to permit negative values,
         * fortunately this works.....
         */
        /* We choose to permit negative values of the same magnitude as the
         * positive ones.
         */
        box.p.x = float2fixed(pt.x * -1);
        box.p.y = float2fixed(pt.y * -1);
        box.q.x = float2fixed(pt.x);
        box.q.y = float2fixed(pt.y);

        /* This gets undone when we grestore after the form is executed */
        code = gx_clip_to_rectangle(igs, &box);
    }

    pop(2);
    return code;
}

/* - .endform -
 */
static int zendform(i_ctx_t *i_ctx_p)
{
    gx_device *cdev = gs_currentdevice_inline(igs);
    int code;

    code = dev_proc(cdev, dev_spec_op)(cdev, gxdso_form_end,
                            0, 0);
    return code;
}

const op_def zform_op_defs[] =
{
    {"0.beginform", zbeginform},
    {"0.endform", zendform},
op_def_end(0)
};
