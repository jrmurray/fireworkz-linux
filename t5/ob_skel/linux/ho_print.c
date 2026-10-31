/* linux/ho_print.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1994-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Printer interface stuff for Fireworkz for Linux: the page layout is done as in windows/ho_print.c,
 * but onto a QPrinter's painter (see qt_host.h), which can be a real printer or a PDF file. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "common/res_defs.h"

#include "ob_skel/ho_print.h"

#include "ob_skel/linux/qt_host.h"

extern void
host_printer_name_query(
    _OutRef_    P_UI_TEXT p_ui_text)
{
    char name[256];
    QTH_PRINTER_INFO info;

    p_ui_text->type = UI_TEXT_TYPE_NONE;

    qth_printer_info(name, sizeof(name), &info);

    if(info.valid && (0 != name[0]))
        status_assert(ui_text_alloc_from_tstr(p_ui_text, name));

    if(p_ui_text->type == UI_TEXT_TYPE_NONE)
    {
        p_ui_text->type = UI_TEXT_TYPE_RESID;
        p_ui_text->text.resource_id = MSG_DIALOG_NO_PRINTER;
    }
}

extern void
host_read_default_paper_details(
    P_PAPER p_paper)
{
    /* assume a sort of minimalist (big-margin) A4 */
    p_paper->x_size = 11901; /* 209.1 mm */
    p_paper->y_size = 16839; /* 297.0 mm */

    p_paper->lm = 567; /* 10 mm */
    p_paper->bm = 567; /* 10 mm */

    p_paper->rm = 844; /* 15 mm */
    p_paper->tm = 844; /* 15 mm */

    p_paper->print_area_x = p_paper->x_size - (p_paper->lm + p_paper->rm);
    p_paper->print_area_y = p_paper->y_size - (p_paper->tm + p_paper->bm);
}

_Check_return_
extern STATUS
host_read_printer_paper_details(
    P_PAPER p_paper)
{
    char name[256];
    QTH_PRINTER_INFO info;

    qth_printer_info(name, sizeof(name), &info);

    if(!info.valid || (info.paper_width <= 0))
    {
        host_read_default_paper_details(p_paper);

        return(create_error(ERR_PRINT_PAPER_DETAILS)); /* so that the read printer button can be disabled */
    }

    /* 1440 twips to the inch; pixits are twips */
    p_paper->x_size = info.paper_width;
    p_paper->y_size = info.paper_height;
    p_paper->lm = info.margin_left;
    p_paper->tm = info.margin_top;
    p_paper->rm = info.margin_right;
    p_paper->bm = info.margin_bottom;
    p_paper->print_area_x = info.print_width;
    p_paper->print_area_y = info.print_height;

    return(STATUS_OK);
}

/* c.f. view_rect_from_screen_rect_and_context */

static void
skel_rect_from_print_box(
    _InoutRef_  P_SKEL_RECT p_skel_rect /*page_num untouched*/,
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context)
{
    GDI_RECT gdi_rect;
    PIXIT_RECT pixit_rect;

    gdi_rect.tl.x = p_redraw_context->linux_ctx.paint_rect.tl.x + p_redraw_context->gdi_org.x;
    gdi_rect.tl.y = p_redraw_context->linux_ctx.paint_rect.tl.y + p_redraw_context->gdi_org.y;
    gdi_rect.br.x = p_redraw_context->linux_ctx.paint_rect.br.x + p_redraw_context->gdi_org.x;
    gdi_rect.br.y = p_redraw_context->linux_ctx.paint_rect.br.y + p_redraw_context->gdi_org.y;

    pixit_rect_from_window_rect(&pixit_rect, &gdi_rect, &p_redraw_context->host_xform);

    p_skel_rect->tl.pixit_point.x = pixit_rect.tl.x;
    p_skel_rect->tl.pixit_point.y = pixit_rect.tl.y;
    p_skel_rect->br.pixit_point.x = pixit_rect.br.x;
    p_skel_rect->br.pixit_point.y = pixit_rect.br.y;
}

/* draw the pages that go on one sheet */

_Check_return_
static STATUS
print_one_printer_page(
    _DocuRef_   P_DOCU p_docu,
    /*const*/ P_PRINT_CTRL p_print_ctrl,
    _InVal_     ARRAY_INDEX page_index,
    _InVal_     UINT pages_per_printer_page)
{
    REDRAW_CONTEXT_CACHE redraw_context_cache;
    REDRAW_CONTEXT redraw_context;
    const P_REDRAW_CONTEXT p_redraw_context = &redraw_context;
    QTH_PRINT_GEOMETRY geometry;
    GDI_POINT pixels_per_inch, page_size, printing_offset, phys_page_size;
    int lrm, tbm;
    GDI_POINT offset[2];
    UINT page_inner_index;
    S32 scale;
    QTH_PAINTER * const p_painter = qth_print_page_painter();

    if(NULL == p_painter)
        return(create_error(ERR_PRINT_UNKNOWN));

    qth_print_geometry(&geometry);

    pixels_per_inch.x = geometry.dpi_x;
    pixels_per_inch.y = geometry.dpi_y;

    page_size.x = geometry.page_width;
    page_size.y = geometry.page_height;

    printing_offset.x = geometry.offset_x;
    printing_offset.y = geometry.offset_y;

    phys_page_size.x = geometry.phys_width;
    phys_page_size.y = geometry.phys_height;

    {
    const int margin_left = printing_offset.x;
    const int margin_top = printing_offset.y;
    const int margin_right = phys_page_size.x - page_size.x - margin_left;
    const int margin_bottom = phys_page_size.y - page_size.y - margin_top;

    lrm = MAX(margin_left, margin_right);
    tbm = MAX(margin_top, margin_bottom);
    } /*block*/

    zero_struct_ptr(p_redraw_context);

    zero_struct_fn(redraw_context_cache);
    p_redraw_context->p_redraw_context_cache = &redraw_context_cache;

    p_redraw_context->flags.printer = 1;

    p_redraw_context->linux_ctx.painter = p_painter;

    /* the painter starts at the printable part of the sheet, so if we want the paper's printable area to
     * start further down (across) the sheet, a +ve offset is added when going to device space, i.e. our
     * top margin less the printer's top offset: gdi_org is subtracted from the point so that is -ve in that case */
    if(p_print_ctrl->flags.two_up)
    {
        GDI_POINT pa, spa;
        PIXIT_POINT print_area;
        S32 scale1, scale2;

        pa.x = phys_page_size.x / 2 - 2 * lrm;
        pa.y = phys_page_size.y - 2 * tbm;

        print_area.x = p_docu->page_def.size_x - margin_left_from(&p_docu->page_def, 0) - margin_right_from(&p_docu->page_def, 0);
        print_area.y = p_docu->page_def.size_y - p_docu->page_def.margin_top - p_docu->page_def.margin_bottom;

        scale1 = muldiv64(100 * PIXITS_PER_INCH, pa.x, print_area.x * pixels_per_inch.x);
        scale2 = muldiv64(100 * PIXITS_PER_INCH, pa.y, print_area.y * pixels_per_inch.y);
        scale = MIN(scale1, scale2);

        offset[0].x = lrm; /* ignore printing_offset for the mo */
        offset[0].y = tbm;
        offset[1].x = phys_page_size.x / 2 + lrm;
        offset[1].y = offset[0].y;

        /* attempt to centre page images in these holes */
        spa.x = (int) muldiv64(print_area.x * pixels_per_inch.x, scale, 100 * PIXITS_PER_INCH);
        spa.y = (int) muldiv64(print_area.y * pixels_per_inch.y, scale, 100 * PIXITS_PER_INCH);

        offset[0].x += (pa.x - spa.x) / 2;
        offset[1].x += (pa.x - spa.x) / 2;
        offset[0].y += (pa.y - spa.y) / 2;
        offset[1].y += (pa.y - spa.y) / 2;
    }
    else
    {
        PIXIT_POINT tl;

        scale = p_docu->paper_scale;

        tl.x = margin_left_from(&p_docu->page_def, 0);
        tl.y = p_docu->page_def.margin_top;

        offset[0].x = (int) muldiv64(tl.x, scale * pixels_per_inch.x, 100 * PIXITS_PER_INCH); /* ignore printing_offset for the mo */
        offset[0].y = (int) muldiv64(tl.y, scale * pixels_per_inch.y, 100 * PIXITS_PER_INCH);
    }

    p_redraw_context->host_xform.scale.t.x = scale;
    p_redraw_context->host_xform.scale.t.y = scale;
    p_redraw_context->host_xform.scale.b.x = 100;
    p_redraw_context->host_xform.scale.b.y = 100;

    p_redraw_context->display_mode = DISPLAY_PRINT_AREA;

    p_redraw_context->border_width.x = p_redraw_context->border_width.y = p_docu->page_def.grid_size;

    host_redraw_context_set_host_xform(p_redraw_context, &p_redraw_context->host_xform);

    host_redraw_context_fillin(p_redraw_context);

    for(page_inner_index = 0; page_inner_index < pages_per_printer_page; ++page_inner_index)
    {
        const S32 page_ident = page_index + page_inner_index;

        if(array_index_is_valid(&p_print_ctrl->h_page_list, page_ident))
        {
            const PC_PAGE_ENTRY p_page_entry = array_ptrc_no_checks(&p_print_ctrl->h_page_list, PAGE_ENTRY, page_ident);

            p_redraw_context->gdi_org.x = printing_offset.x - offset[page_inner_index].x;
            p_redraw_context->gdi_org.y = printing_offset.y - offset[page_inner_index].y;

            p_redraw_context->linux_ctx.paint_rect.tl.x = 0;
            p_redraw_context->linux_ctx.paint_rect.tl.y = 0;
            p_redraw_context->linux_ctx.paint_rect.br.x = page_size.x;
            p_redraw_context->linux_ctx.paint_rect.br.y = page_size.y;

            if(p_print_ctrl->flags.two_up)
            {
                const int hx = phys_page_size.x / 2;
                if(page_inner_index == 0)
                    p_redraw_context->linux_ctx.paint_rect.br.x = hx - printing_offset.x;
                else
                    p_redraw_context->linux_ctx.paint_rect.tl.x = hx - printing_offset.x;
            }

            p_redraw_context->linux_ctx.host_machine_clip_rect = p_redraw_context->linux_ctx.paint_rect;

            { /* really do clip to this now */
            QTH_RECT clip;
            clip.left = p_redraw_context->linux_ctx.host_machine_clip_rect.tl.x;
            clip.top = p_redraw_context->linux_ctx.host_machine_clip_rect.tl.y;
            clip.right = p_redraw_context->linux_ctx.host_machine_clip_rect.br.x;
            clip.bottom = p_redraw_context->linux_ctx.host_machine_clip_rect.br.y;
            qth_paint_set_clip(p_painter, &clip);
            } /*block*/

            if((p_page_entry->page.x >= 0) && (p_page_entry->page.y >= 0))
            {
                PIXIT_RECT print_area;
                SKELEVENT_REDRAW skelevent_redraw;

                print_area.tl.x = 0;
                print_area.tl.y = 0;
                print_area.br.x = p_docu->page_def.size_x - margin_left_from(&p_docu->page_def, p_page_entry->page.y) - margin_right_from(&p_docu->page_def, p_page_entry->page.y);
                print_area.br.y = p_docu->page_def.size_y - p_docu->page_def.margin_top - p_docu->page_def.margin_bottom;

                zero_struct_fn(skelevent_redraw);

                skelevent_redraw.flags.show_content = TRUE;

                skelevent_redraw.redraw_context = redraw_context;

                skel_rect_from_print_box(&skelevent_redraw.clip_skel_rect, p_redraw_context);
                skelevent_redraw.clip_skel_rect.tl.page_num = p_page_entry->page;
                skelevent_redraw.clip_skel_rect.br.page_num = p_page_entry->page;

                host_paint_start(&skelevent_redraw.redraw_context);
                view_redraw_page(p_docu, &skelevent_redraw, &print_area);
                host_paint_end(&skelevent_redraw.redraw_context);
            }
        }
    }

    return(STATUS_OK);
}

_Check_return_
extern STATUS
host_print_document(
    _DocuRef_   P_DOCU p_docu,
    /*const*/ P_PRINT_CTRL p_print_ctrl)
{
    STATUS status = STATUS_OK;
    ARRAY_INDEX outer_copies = p_print_ctrl->flags.collate ? p_print_ctrl->copies : 1;
    ARRAY_INDEX inner_copies = p_print_ctrl->flags.collate ? 1 : p_print_ctrl->copies;
    UINT pages_per_printer_page;
    BOOL first_sheet = TRUE;
    const BOOL landscape = p_print_ctrl->flags.landscape || p_print_ctrl->flags.two_up;

    if(outer_copies < 1)
        outer_copies = 1;
    if(inner_copies < 1)
        inner_copies = 1;

    if(!qth_print_copies_by_printer())
    {   /* a printer that cannot make several copies is sent the pages several times */
        if(inner_copies > 1)
        {
            assert(1 == outer_copies);
            outer_copies = inner_copies;
            inner_copies = 1;
        }
    }

    if(!qth_print_begin(p_docu->docu_name.leaf_name, landscape, (int) inner_copies, 0 /* done by outer_copies */))
        return(create_error(ERR_PRINT_WONT_OPEN));

    pages_per_printer_page = p_print_ctrl->flags.two_up ? 2 : 1;

    { /* print n collated sets */
    ARRAY_INDEX outer_copy_idx;
    for(outer_copy_idx = 0; outer_copy_idx < outer_copies; ++outer_copy_idx)
    {
        const ARRAY_INDEX page_count = array_elements(&p_print_ctrl->h_page_list);
        ARRAY_INDEX page_index;
        PRINTER_PERCENTAGE printer_percentage;

        print_percentage_initialise(p_docu, &printer_percentage, page_count);

        for(page_index = 0; page_index < page_count; page_index += pages_per_printer_page)
        {
            if(!first_sheet)
                if(!qth_print_next_page())
                    status_break(status = create_error(ERR_PRINT_UNKNOWN));

            first_sheet = FALSE;

            status_break(status = print_one_printer_page(p_docu, p_print_ctrl, page_index, pages_per_printer_page));

            print_percentage_page_inc(&printer_percentage);
        }

        print_percentage_finalise(&printer_percentage);

        status_break(status);
    }
    } /*block*/

    if(!qth_print_end(status_fail(status)) && status_ok(status))
        status = create_error(ERR_PRINT_UNKNOWN);

    return(status);
}

/* Print Setup: the desktop's printer chooser, which has the printer's own options */

T5_CMD_PROTO(extern, t5_cmd_print_setup)
{
    const P_VIEW p_view = p_view_from_viewno_caret(p_docu);

    UNREFERENCED_PARAMETER_InVal_(t5_message);
    UNREFERENCED_PARAMETER_InRef_(p_t5_cmd);

    (void) qth_printer_setup_dialog(VIEW_NOT_NONE(p_view) ? p_view->main[WIN_BACK].hwnd : NULL);

    return(STATUS_OK);
}

/* end of linux/ho_print.c */
