/* ho_paint_util.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Helpers shared by the Linux painting files: the painter in a redraw context, and
 * pixit rectangles turned into device ones */

#ifndef HO_PAINT_UTIL_H
#define HO_PAINT_UTIL_H

#include "ob_skel/linux/qt_host.h"

static inline QTH_PAINTER *
painter_of(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    return((QTH_PAINTER *) p_redraw_context->linux_ctx.painter);
}

static inline QTH_RGB
qth_rgb(
    _InRef_     PC_RGB p_rgb)
{
    QTH_RGB qth_rgb;

    qth_rgb.r = p_rgb->r;
    qth_rgb.g = p_rgb->g;
    qth_rgb.b = p_rgb->b;

    return(qth_rgb);
}

static inline QTH_RECT
qth_rect(
    _InRef_     PC_GDI_RECT p_gdi_rect)
{
    QTH_RECT qth_rect;

    qth_rect.left   = p_gdi_rect->tl.x;
    qth_rect.top    = p_gdi_rect->tl.y;
    qth_rect.right  = p_gdi_rect->br.x;
    qth_rect.bottom = p_gdi_rect->br.y;

    return(qth_rect);
}

/* STATUS_DONE iff there is something to plot */

_Check_return_
static inline STATUS
device_rect_from_pixit_rect(
    _Out_      QTH_RECT * p_qth_rect,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    GDI_RECT gdi_rect;

    if(!status_done(gdi_rect_limited_from_pixit_rect_and_context(&gdi_rect, p_pixit_rect, p_redraw_context)))
    {
        zero_struct_ptr(p_qth_rect);
        return(STATUS_OK);
    }

    *p_qth_rect = qth_rect(&gdi_rect);

    return(STATUS_DONE);
}

#endif /* HO_PAINT_UTIL_H */

/* end of ho_paint_util.h */
