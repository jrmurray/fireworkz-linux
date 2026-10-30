/* ho_font.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux host font functions: the counterpart of the font parts of windows/ho_paint.c,
 * with the native work done through qt_host.h */

#include "common/gflags.h"

#include "ob_skel/flags.h"

#include "ob_skel/linux/qt_host.h"

/******************************************************************************
*
* Return a machine specific font handle for the specified font.
*
* TODO: when painting, scale the size from pixits to device pixels using the
* host_xform in the REDRAW_CONTEXT, as the Windows host does. For now every font
* is made in pixits, which is right for formatting.
*
******************************************************************************/

_Check_return_
extern HOST_FONT
host_font_find(
    _InRef_     PC_HOST_FONT_SPEC p_host_font_spec,
    _InRef_maybenone_ PC_REDRAW_CONTEXT p_redraw_context)
{
    QTH_FONT_DESC desc;
    PIXIT size_y = p_host_font_spec->size_y;
    PIXIT size_x = (0 == p_host_font_spec->size_x) ? size_y : p_host_font_spec->size_x;

    UNREFERENCED_PARAMETER_InRef_(p_redraw_context);

    zero_struct(desc);

    xstrkpy(desc.family, elemof32(desc.family), p_host_font_spec->logfont.lfFaceName);
    desc.weight = p_host_font_spec->logfont.lfWeight;
    desc.italic = p_host_font_spec->logfont.lfItalic;
    desc.family_class = (U8) (p_host_font_spec->logfont.lfPitchAndFamily >> 4);
    desc.fixed_pitch = (FIXED_PITCH == (p_host_font_spec->logfont.lfPitchAndFamily & 0x0F));

    return(qth_font_create(&desc, size_x, size_y, p_host_font_spec->underline));
}

extern void
host_font_delete(
    _HfontRef_  HOST_FONT host_font,
    _InRef_maybenone_ PC_REDRAW_CONTEXT p_redraw_context)
{
    UNREFERENCED_PARAMETER_InRef_(p_redraw_context); /* TODO: a font cached in the redraw context */

    qth_font_destroy(host_font);
}

extern void
host_font_dispose(
    _InoutRef_  P_HOST_FONT p_host_font,
    _InRef_maybenone_ PC_REDRAW_CONTEXT p_redraw_context)
{
    HOST_FONT host_font = *p_host_font;

    if(HOST_FONT_NONE != host_font)
    {
        *p_host_font = HOST_FONT_NONE;

        host_font_delete(host_font, p_redraw_context);
    }
}

_Check_return_
extern PIXIT
host_fonty_uchars_width(
    _HfontRef_  HOST_FONT host_font,
    _In_reads_(uchars_n) PC_USTR uchars,
    _InVal_     U32 uchars_n)
{
    return((PIXIT) qth_font_text_width(host_font, (const uint8_t *) uchars, uchars_n));
}

/* end of ho_font.c */
