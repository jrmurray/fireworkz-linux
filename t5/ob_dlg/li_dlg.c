/* li_dlg.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The Linux counterpart of wi_dlg.c: the dialog engine's host side.
 *
 * So far this has only the measuring that the rest of the program uses (the width of text
 * in the dialog font, and how many items a list shows). The dialogs themselves, built from
 * the DIALOG_CONTROL tables as Qt widgets, are still to be written (see ReadMeL.txt). */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#if LINUX

#include "ob_skel/linux/qt_host.h"

#ifndef         UI_DLGIN_H
#include "ob_dlg/ui_dlgin.h"
#endif

/* Dialog units are defined from the average size of the dialog font's characters, as on Windows:
 * one horizontal dialog unit is a quarter of the average character width and one vertical dialog
 * unit an eighth of the character height. Measured at 96 dpi and scaled by the pixit transforms. */

static struct DIALOG_LINUX_METRICS
{
    int32_t pixels_per_four_h_du;
    int32_t pixels_per_eight_v_du;
}
dialog_metrics;

static void
dialog_metrics_init(void)
{
    if(0 == dialog_metrics.pixels_per_four_h_du)
    {
        static const uint8_t alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        QTH_FONT_DESC desc;
        QTH_FONT * p_font;

        memset(&desc, 0, sizeof(desc)); /* an empty family is the desktop's default */
        desc.weight = QTH_WEIGHT_NORMAL;

        dialog_metrics.pixels_per_four_h_du = 6;  /* the Windows XP Tahoma values, if there is no font */
        dialog_metrics.pixels_per_eight_v_du = 13;

        if(NULL != (p_font = qth_font_create(&desc, 12, 12, 0)))
        {
            QTH_FONT_METRICS metrics;

            qth_font_metrics(p_font, &metrics);

            dialog_metrics.pixels_per_four_h_du = (((qth_font_text_width(p_font, alphabet, 52) / 26) + 1) / 2);
            dialog_metrics.pixels_per_eight_v_du = metrics.ascent + metrics.descent;

            qth_font_destroy(p_font);
        }
    }
}

/* The width of the text in the dialog font, rounded out and a wee bit more, as multiples of dialog units */

_Check_return_
extern PIXIT
ui_width_from_tstr_host(
    _In_z_      PCTSTR tstr)
{
    const U32 tchars_n = tstrlen32(tstr);
    QTH_FONT_DESC desc;
    QTH_FONT * p_font;

    dialog_metrics_init();

    memset(&desc, 0, sizeof(desc));
    desc.weight = QTH_WEIGHT_NORMAL;

    if(NULL != (p_font = qth_font_create(&desc, 12, 12, 0)))
    {
        const int32_t width = qth_font_text_width(p_font, (const uint8_t *) tstr, tchars_n);

        qth_font_destroy(p_font);

        return(PIXITS_PER_WDU_H * (1 + muldiv64_ceil(width, 4, dialog_metrics.pixels_per_four_h_du)));
    }

    return((PIXIT) DIALOG_SYSCHARSL_H(tchars_n));
}

extern void
ui_list_size_estimate(
    _InVal_     S32 num_elements,
    _OutRef_    P_PIXIT_SIZE p_pixit_size)
{
    S32 show_elements = num_elements;
    S32 max_elements;
    GDI_SIZE screen_gdi_size;

    dialog_metrics_init();

    /* always give the idea that there should be *some* list elements */
    show_elements = MAX(3, show_elements);

    /* always dynamically limit to a portion of the host's work area */
#define UI_LIST_LIMIT_NUMERATOR     3
#define UI_LIST_LIMIT_DENOMINATOR   5
    host_work_area_gdi_size_query(&screen_gdi_size); /* NB pixels */

    /* remembering to go from pixels via (multiples of) dialog units */
    max_elements = muldiv64(screen_gdi_size.cy, UI_LIST_LIMIT_NUMERATOR * 8, UI_LIST_LIMIT_DENOMINATOR * dialog_metrics.pixels_per_eight_v_du) / (DIALOG_STDLISTITEM_V / PIXITS_PER_WDU_V);

    p_pixit_size->cx = 0;

    if(show_elements > max_elements)
    {
        show_elements = max_elements;
        p_pixit_size->cx += DIALOG_STDLISTEXTRA_H;
    }

    p_pixit_size->cy = show_elements * DIALOG_STDLISTITEM_V;
}

/* TODO: fill the list or combo box control with the source's items, as it is shown */

extern void
dialog_windows_build_list(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    UNREFERENCED_PARAMETER_InoutRef_(p_dialog);
    UNREFERENCED_PARAMETER(p_dialog_ictl);
}

#endif /* LINUX */

/* end of li_dlg.c */
