/* ho_text.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux painting of the text of documents, through fonty (the skeleton's font handling) */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/ho_paint_util.h"

/* ---- fonty text: the text of documents -------------------------------------------------- */

_Check_return_
static inline PIXIT /* NB y +ve down */
fonty_paint_calc_shift_y(
    _InRef_     PC_FONT_CONTEXT p_font_context)
{
    if(p_font_context->font_spec.superscript)
        return((p_font_context->ascent * 0) / 64);

    if(p_font_context->font_spec.subscript)
        return((p_font_context->ascent * 38) / 64);

    return(0);
}

/* Paint the text with its baseline at base_line below the point; Qt draws from the baseline,
 * so the Windows adjustment by the font's ascent is not wanted */

static void
fonty_text_paint_uchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(uchars_n) PC_UCHARS uchars,
    _InVal_     S32 uchars_n,
    _InVal_     PIXIT base_line,
    _InVal_     FONTY_HANDLE fonty_handle)
{
    PIXIT_POINT pixit_point = *p_pixit_point;
    const PC_FONT_CONTEXT p_font_context = p_font_context_from_fonty_handle(fonty_handle);
    HOST_FONT host_font;
    GDI_POINT gdi_point;

    pixit_point.y += base_line;
    pixit_point.y += fonty_paint_calc_shift_y(p_font_context);

    gdi_point_from_pixit_point_and_context(&gdi_point, &pixit_point, p_redraw_context);

    host_font = fonty_host_font_from_fonty_handle_redraw(p_redraw_context, fonty_handle, p_docu->flags.draft_mode);
    assert(HOST_FONT_NONE != host_font);
    if(HOST_FONT_NONE == host_font)
        return;

    qth_paint_text(painter_of(p_redraw_context), host_font,
                   gdi_point.x, gdi_point.y,
                   (const uint8_t *) uchars, (uint32_t) uchars_n,
                   qth_rgb(&p_font_context->font_spec.colour), NULL, QTH_ALIGN_LEFT);
}

extern void
fonty_text_paint_rubout_uchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(uchars_n) PC_UCHARS uchars,
    _InVal_     S32 uchars_n,
    _InVal_     PIXIT base_line,
    _InRef_     PC_RGB p_rgb_back,
    _InVal_     FONTY_HANDLE fonty_handle,
    _InRef_     PC_PIXIT_RECT p_pixit_rect_rubout)
{
    QTH_RECT rect;

    PTR_ASSERT(p_pixit_rect_rubout);

    if(status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect_rubout, p_redraw_context)))
    {
        PC_RGB p_rgb_back_fill = p_rgb_back->transparent ? &rgb_stash[COLOUR_OF_PAPER] : p_rgb_back;

        qth_paint_fill_rect(painter_of(p_redraw_context), &rect, qth_rgb(p_rgb_back_fill));
    }

    if(0 == uchars_n)
        return;

    fonty_text_paint_uchars(p_docu, p_redraw_context, p_pixit_point, uchars, uchars_n, base_line, fonty_handle);
}

extern void
fonty_text_paint_simple_uchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(uchars_n) PC_UCHARS uchars,
    _InVal_     S32 uchars_n,
    _InVal_     PIXIT base_line,
    _InRef_     PC_RGB p_rgb_back,
    _InVal_     FONTY_HANDLE fonty_handle)
{
    UNREFERENCED_PARAMETER_InRef_(p_rgb_back);

    if(0 == uchars_n)
        return;

    /* TODO: drawfile output, which draws the text as Draw objects */

    fonty_text_paint_uchars(p_docu, p_redraw_context, p_pixit_point, uchars, uchars_n, base_line, fonty_handle);
}

#if TSTR_IS_SBSTR

/* The same for UTF-16 text, such as the UTF-8 chunks of a document converted for display */

static void
fonty_text_paint_wchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(wchars_n) PCWCH wchars,
    _InVal_     S32 wchars_n,
    _InVal_     PIXIT base_line,
    _InVal_     FONTY_HANDLE fonty_handle)
{
    PIXIT_POINT pixit_point = *p_pixit_point;
    const PC_FONT_CONTEXT p_font_context = p_font_context_from_fonty_handle(fonty_handle);
    HOST_FONT host_font;
    GDI_POINT gdi_point;

    pixit_point.y += base_line;
    pixit_point.y += fonty_paint_calc_shift_y(p_font_context);

    gdi_point_from_pixit_point_and_context(&gdi_point, &pixit_point, p_redraw_context);

    host_font = fonty_host_font_from_fonty_handle_redraw(p_redraw_context, fonty_handle, p_docu->flags.draft_mode);
    assert(HOST_FONT_NONE != host_font);
    if(HOST_FONT_NONE == host_font)
        return;

    qth_paint_text16(painter_of(p_redraw_context), host_font,
                     gdi_point.x, gdi_point.y,
                     (const uint16_t *) wchars, (uint32_t) wchars_n,
                     qth_rgb(&p_font_context->font_spec.colour), NULL, QTH_ALIGN_LEFT);
}

extern void
fonty_text_paint_rubout_wchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(wchars_n) PCWCH wchars,
    _InVal_     S32 wchars_n,
    _InVal_     PIXIT base_line,
    _InRef_     PC_RGB p_rgb_back,
    _InVal_     FONTY_HANDLE fonty_handle,
    _InRef_     PC_PIXIT_RECT p_pixit_rect_rubout)
{
    QTH_RECT rect;

    if(status_done(device_rect_from_pixit_rect(&rect, p_pixit_rect_rubout, p_redraw_context)))
    {
        PC_RGB p_rgb_back_fill = p_rgb_back->transparent ? &rgb_stash[COLOUR_OF_PAPER] : p_rgb_back;

        qth_paint_fill_rect(painter_of(p_redraw_context), &rect, qth_rgb(p_rgb_back_fill));
    }

    if(0 == wchars_n)
        return;

    fonty_text_paint_wchars(p_docu, p_redraw_context, p_pixit_point, wchars, wchars_n, base_line, fonty_handle);
}

extern void
fonty_text_paint_simple_wchars(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _In_reads_(wchars_n) PCWCH wchars,
    _InVal_     S32 wchars_n,
    _InVal_     PIXIT base_line,
    _InRef_     PC_RGB p_rgb_back,
    _InVal_     FONTY_HANDLE fonty_handle)
{
    UNREFERENCED_PARAMETER_InRef_(p_rgb_back);

    if(0 == wchars_n)
        return;

    fonty_text_paint_wchars(p_docu, p_redraw_context, p_pixit_point, wchars, wchars_n, base_line, fonty_handle);
}

#endif /* TSTR_IS_SBSTR */

/* end of ho_text.c */
