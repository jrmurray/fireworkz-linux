/* ho_paint.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux host painting: the counterpart of windows/ho_paint.c. Pixit geometry is
 * turned into device pixels here, with the context's host_xform; the Qt side
 * (qt_host.h) only ever sees pixels. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

static PC_REDRAW_CONTEXT cur_p_redraw_context = P_REDRAW_CONTEXT_NONE;

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
static STATUS
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

extern void
host_paint_start(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    cur_p_redraw_context = p_redraw_context; /* NB org.x,y must have been set up prior to calling this function */
}

extern void
host_paint_end(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    UNREFERENCED_PARAMETER_InRef_(p_redraw_context);

    cur_p_redraw_context = P_REDRAW_CONTEXT_NONE;
}

extern void
host_paint_rectangle_filled(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_RGB p_rgb)
{
    QTH_RECT rect;

    if(p_rgb->transparent)
        return;

    if(!status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect, p_redraw_context)))
        return;

    qth_paint_fill_rect(painter_of(p_redraw_context), &rect, qth_rgb(p_rgb));
}

extern void
host_paint_rectangle_outline(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_RGB p_rgb)
{
    QTH_RECT rect;

    if(p_rgb->transparent)
        return;

    if(!status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect, p_redraw_context)))
        return;

    qth_paint_frame_rect(painter_of(p_redraw_context), &rect, qth_rgb(p_rgb));
}

extern void
host_paint_rectangle_crossed(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_RGB p_rgb)
{
    QTH_PAINTER * const p_painter = painter_of(p_redraw_context);
    QTH_RECT rect;

    if(p_rgb->transparent)
        return;

    if(!status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect, p_redraw_context)))
        return;

    qth_paint_frame_rect(p_painter, &rect, qth_rgb(p_rgb));

    qth_paint_line(p_painter, rect.left, rect.top,        rect.right - 1, rect.bottom - 1, qth_rgb(p_rgb), 0);
    qth_paint_line(p_painter, rect.left, rect.bottom - 1, rect.right - 1, rect.top,        qth_rgb(p_rgb), 0);
}

/* Inverted plotting: the colours are not needed as the complement is always what is wanted */

extern void
host_invert_rectangle_filled(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_RGB p_fore,
    _InRef_     PC_RGB p_back)
{
    QTH_RECT rect;

    UNREFERENCED_PARAMETER_InRef_(p_fore);
    UNREFERENCED_PARAMETER_InRef_(p_back);

    if(!status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect, p_redraw_context)))
        return;

    qth_paint_invert_rect(painter_of(p_redraw_context), &rect);
}

extern void
host_invert_rectangle_outline(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_RGB p_fore,
    _InRef_     PC_RGB p_back)
{
    QTH_PAINTER * const p_painter = painter_of(p_redraw_context);
    QTH_RECT rect, r;

    UNREFERENCED_PARAMETER_InRef_(p_fore);
    UNREFERENCED_PARAMETER_InRef_(p_back);

    if(!status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect, p_redraw_context)))
        return;

    r = rect;

    if(r.left + 1 >= r.right)
    {   /* thin vertical line */
        r.right = r.left + 1;
        qth_paint_invert_rect(p_painter, &r);
    }
    else if(r.top + 1 >= r.bottom)
    {   /* thin horizontal line */
        r.bottom = r.top + 1;
        qth_paint_invert_rect(p_painter, &r);
    }
    else
    {   /* rectangle outline, each pixel inverted just once */
        r.right -= 1;
        r.bottom = r.top + 1;
        qth_paint_invert_rect(p_painter, &r);
        r = rect;
        r.left = r.right - 1;
        r.bottom -= 1;
        qth_paint_invert_rect(p_painter, &r);
        r = rect;
        r.left += 1;
        r.top = r.bottom - 1;
        qth_paint_invert_rect(p_painter, &r);
        r = rect;
        r.right = r.left + 1;
        r.top += 1;
        qth_paint_invert_rect(p_painter, &r);
    }
}

/* used to render the ruler: always very thin on screen, never scaled */

extern void
host_paint_line_solid(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_LINE p_pixit_line,
    _InRef_     PC_RGB p_rgb)
{
    PIXIT_RECT pixit_rect;
    QTH_RECT rect;

    if(p_rgb->transparent)
        return;

    pixit_rect.tl = p_pixit_line->tl;
    pixit_rect.br = p_pixit_line->br;

    if(p_pixit_line->horizontal)
        pixit_rect.br.y = pixit_rect.tl.y + p_redraw_context->one_program_pixel.y;
    else
        pixit_rect.br.x = pixit_rect.tl.x + p_redraw_context->one_program_pixel.x;

    if(!status_done(device_rect_from_pixit_rect(&rect, &pixit_rect, p_redraw_context)))
        return;

    if(p_pixit_line->horizontal)
        rect.bottom = rect.top + 1;
    else
        rect.right = rect.left + 1;

    qth_paint_fill_rect(painter_of(p_redraw_context), &rect, qth_rgb(p_rgb));
}

extern void
host_paint_line_dashed(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_LINE p_pixit_line,
    _InRef_     PC_RGB p_rgb)
{
    GDI_POINT tl, br;

    if(p_rgb->transparent)
        return;

    gdi_point_from_pixit_point_and_context(&tl, &p_pixit_line->tl, p_redraw_context);
    gdi_point_from_pixit_point_and_context(&br, &p_pixit_line->br, p_redraw_context);

    qth_paint_line(painter_of(p_redraw_context), tl.x, tl.y, br.x, br.y, qth_rgb(p_rgb), 1);
}

/* Set the clipping rectangle to the pixit one specified, also clipping it to the parent rectangle currently defined */

static void
clip_rect_adjust_by_flags(
    _InoutRef_  P_PIXIT_RECT p_pixit_rect,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InVal_     RECT_FLAGS rect_flags)
{
    if(0 == * (PC_U32) &rect_flags)
        return;

    if(rect_flags.reduce_left_by_2)
        p_pixit_rect->tl.x += p_redraw_context->border_width.x << 1;
    if(rect_flags.reduce_left_by_1)
        p_pixit_rect->tl.x += p_redraw_context->border_width.x;

    if(rect_flags.reduce_up_by_2)
        p_pixit_rect->tl.y += p_redraw_context->border_width.y << 1;
    if(rect_flags.reduce_up_by_1)
        p_pixit_rect->tl.y += p_redraw_context->border_width.y;

    if(rect_flags.extend_right_by_1)
        p_pixit_rect->br.x += p_redraw_context->border_width.x;
    if(rect_flags.reduce_right_by_1)
        p_pixit_rect->br.x -= p_redraw_context->border_width.x;

    if(rect_flags.extend_down_by_1)
        p_pixit_rect->br.y += p_redraw_context->border_width.y;
    if(rect_flags.reduce_down_by_1)
        p_pixit_rect->br.y -= p_redraw_context->border_width.y;
}

_Check_return_
static BOOL
clip_to_pixit_rect(
    _InoutRef_  P_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect)
{
    QTH_RECT qth_clip;
    GDI_RECT clip_rect;

    assert_EQ(sizeof32(RECT_FLAGS), sizeof32(U32));

    if(!status_done(device_rect_from_pixit_rect(&qth_clip, p_pixit_rect, p_redraw_context)))
        return(FALSE);

    clip_rect.tl.x = MAX(p_redraw_context->linux_ctx.host_machine_clip_rect.tl.x, qth_clip.left);
    clip_rect.tl.y = MAX(p_redraw_context->linux_ctx.host_machine_clip_rect.tl.y, qth_clip.top);
    clip_rect.br.x = MIN(p_redraw_context->linux_ctx.host_machine_clip_rect.br.x, qth_clip.right);
    clip_rect.br.y = MIN(p_redraw_context->linux_ctx.host_machine_clip_rect.br.y, qth_clip.bottom);

    if((clip_rect.tl.x >= clip_rect.br.x) || (clip_rect.tl.y >= clip_rect.br.y))
        return(FALSE);

    p_redraw_context->linux_ctx.host_machine_clip_rect = clip_rect;

    qth_clip = qth_rect(&clip_rect);
    qth_paint_set_clip(painter_of(p_redraw_context), &qth_clip);

    return(TRUE);
}

extern BOOL
host_set_clip_rectangle(
    _InoutRef_  P_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect,
    _InVal_     RECT_FLAGS rect_flags)
{
    PIXIT_RECT pixit_rect = *p_pixit_rect;

    clip_rect_adjust_by_flags(&pixit_rect, p_redraw_context, rect_flags);

    return(clip_to_pixit_rect(p_redraw_context, &pixit_rect));
}

/* Given two pixit rectangles clip them together and then set the clip region to that of the resulting box */

extern BOOL
host_set_clip_rectangle2(
    _InoutRef_  P_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_RECT p_pixit_rect_paper,
    _InRef_     PC_PIXIT_RECT p_rect_object,
    _InVal_     RECT_FLAGS rect_flags)
{
    PIXIT_RECT pixit_rect = *p_rect_object;

    clip_rect_adjust_by_flags(&pixit_rect, p_redraw_context, rect_flags);

    pixit_rect.tl.x = MAX(p_pixit_rect_paper->tl.x, pixit_rect.tl.x);
    pixit_rect.tl.y = MAX(p_pixit_rect_paper->tl.y, pixit_rect.tl.y);
    pixit_rect.br.x = MIN(p_pixit_rect_paper->br.x, pixit_rect.br.x);
    pixit_rect.br.y = MIN(p_pixit_rect_paper->br.y, pixit_rect.br.y);

    return(clip_to_pixit_rect(p_redraw_context, &pixit_rect));
}

/* Reinstate the clip rectangle held in the redraw context */

extern void
host_restore_clip_rectangle(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    const QTH_RECT qth_clip = qth_rect(&p_redraw_context->linux_ctx.host_machine_clip_rect);

    qth_paint_set_clip(painter_of(p_redraw_context), &qth_clip);
}

extern void
host_fonty_text_paint_uchars_simple(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(uchars_n) PC_USTR uchars,
    _InVal_     U32 uchars_n,
    _InRef_     PC_RGB p_rgb_foreground,
    _InRef_     PC_RGB p_rgb_background,
    _HfontRef_  HOST_FONT host_font,
    _InVal_     int text_align_lcr)
{
    GDI_POINT gdi_point;
    QTH_RGB background;

    if(0 == uchars_n)
        return;

    gdi_point_from_pixit_point_and_context(&gdi_point, p_pixit_point, p_redraw_context);

    background = qth_rgb(p_rgb_background);

    qth_paint_text(painter_of(p_redraw_context), host_font,
                   gdi_point.x, gdi_point.y,
                   (const uint8_t *) uchars, uchars_n,
                   qth_rgb(p_rgb_foreground),
                   p_rgb_background->transparent ? NULL : &background,
                   text_align_lcr);
}

/* end of ho_paint.c */
