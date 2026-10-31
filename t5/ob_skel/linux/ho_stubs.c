/* ho_stubs.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux host functions for things that are not written yet. Each one says what it should do
 * and does the least that lets the rest of the program run: menus, printing, help and
 * Draw file output. (Save As is in ui_save.c, as it needs types private to that file) */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/ho_print.h"

#include "ob_skel/linux/qt_host.h"

/* ---- help: TODO open the user guide in the desktop's browser ---------------------------- */

T5_CMD_PROTO(extern, t5_cmd_help)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER_InRef_(p_t5_cmd);

    return(STATUS_OK);
}

/* ---- printing: TODO QPrinter, QPrintDialog and a QPainter on the printer ---------------- */

extern void
host_printer_name_query(
    _OutRef_    P_UI_TEXT p_ui_text)
{
    p_ui_text->type = UI_TEXT_TYPE_NONE;
}

_Check_return_
extern STATUS
host_read_printer_paper_details(
    P_PAPER p_paper)
{
    /* A4 with half-inch margins, and say that the printer could not be read */
    p_paper->x_size = (PIXIT) ((210 * PIXITS_PER_INCH) / 25.4);
    p_paper->y_size = (PIXIT) ((297 * PIXITS_PER_INCH) / 25.4);
    p_paper->lm = p_paper->rm = p_paper->tm = p_paper->bm = PIXITS_PER_INCH / 2;
    p_paper->print_area_x = p_paper->x_size - (p_paper->lm + p_paper->rm);
    p_paper->print_area_y = p_paper->y_size - (p_paper->tm + p_paper->bm);

    return(create_error(ERR_PRINT_PAPER_DETAILS));
}

_Check_return_
extern STATUS
host_print_document(
    _DocuRef_   P_DOCU p_docu,
    P_PRINT_CTRL p_print_ctrl)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER(p_print_ctrl);

    return(create_error(ERR_PRINT_UNKNOWN));
}

T5_MSG_PROTO(extern, t5_msg_draft_print, P_PRINT_CTRL p_print_ctrl)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER_InVal_(t5_message);
    UNREFERENCED_PARAMETER(p_print_ctrl);

    return(create_error(ERR_PRINT_UNKNOWN));
}

/* ---- Draw files: TODO draw the objects of a Draw file with a QPainterPath --------------- */

/* The size of the Draw file's bounding box in pixits */

extern void
host_read_drawfile_pixit_size(
    /*_In_reads_bytes_(DRAW_FILE_HEADER)*/ PC_ANY p_any,
    _OutRef_    P_PIXIT_SIZE p_pixit_size)
{
    PC_DRAW_FILE_HEADER p_draw_file_header = (PC_DRAW_FILE_HEADER) p_any;
    DRAW_POINT draw_size;

    draw_size.x = p_draw_file_header->bbox.x1 - p_draw_file_header->bbox.x0;
    draw_size.y = p_draw_file_header->bbox.y1 - p_draw_file_header->bbox.y0;

    /* flooring ok 'cos paths got rounded out on loading to worst-case pixels */
    p_pixit_size->cx = muldiv64_floor(draw_size.x, 1, 2 * RISCDRAW_PER_RISCOS) * 2 * PIXITS_PER_RISCOS;
    p_pixit_size->cy = muldiv64_floor(draw_size.y, 1, 2 * RISCDRAW_PER_RISCOS) * 2 * PIXITS_PER_RISCOS;
}

/* Draws nothing yet, so drawings and charts appear blank */

extern void
host_paint_drawfile(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _InRef_     PC_GR_SCALE_PAIR p_gr_scale_pair,
    _InRef_     PC_DRAW_DIAG p_draw_diag,
    _InVal_     BOOL eor_paths)
{
    UNREFERENCED_PARAMETER_InRef_(p_redraw_context);
    UNREFERENCED_PARAMETER_InRef_(p_pixit_point);
    UNREFERENCED_PARAMETER_InRef_(p_gr_scale_pair);
    UNREFERENCED_PARAMETER_InRef_(p_draw_diag);
    UNREFERENCED_PARAMETER_InVal_(eor_paths);
}

/* end of ho_stubs.c */
