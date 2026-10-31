/* ho_stubs.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux host functions for things that are not written yet. Each one says what it should do
 * and does the least that lets the rest of the program run: help and draft printing. (Save As is in ui_save.c, as it needs types private to that file) */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/ho_print.h"

#include "ob_skel/linux/qt_host.h"

/* ---- draft printing: TODO as the printing in ho_print.c but by sending text lines ------ */

T5_MSG_PROTO(extern, t5_msg_draft_print, P_PRINT_CTRL p_print_ctrl)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER_InVal_(t5_message);
    UNREFERENCED_PARAMETER(p_print_ctrl);

    return(create_error(ERR_PRINT_UNKNOWN));
}

/* end of ho_stubs.c */
