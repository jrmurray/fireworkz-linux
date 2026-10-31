/* linux/ho_help.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1994-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Help for Fireworkz for Linux: the help pages (HTML, from the RISC OS help) and web addresses are
 * opened by the desktop in its browser. The Windows version asks HTML Help to open a .chm file, whose
 * name the menus still give; there is no such file here, so the help pages stand in for it. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

#define HELP_INDEX_PATH TEXT("Help") FILE_DIR_SEP_TSTR TEXT("index.html")

/* Open the help pages' contents */

_Check_return_
extern STATUS
linux_help_contents(void)
{
    TCHARZ filename[BUF_MAX_PATHSTRING];

    if(file_find_on_path(filename, elemof32(filename), file_get_resources_path(), HELP_INDEX_PATH) <= 0)
        return(create_error(ERR_HELP_FAILURE));

    if(!qth_open_url(filename))
        return(create_error(ERR_HELP_FAILURE));

    return(STATUS_OK);
}

T5_CMD_PROTO(extern, t5_cmd_help)
{
    const PC_ARGLIST_ARG p_args = pc_arglist_args(&p_t5_cmd->arglist_handle, 1); /* NB just the common span */

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    switch(t5_message)
    {
    default: default_unhandled();
    case T5_CMD_HELP_CONTENTS:
    case T5_CMD_HELP_SEARCH_KEYWORD: /* there is no index to search: show the contents */
        return(linux_help_contents());

    case T5_CMD_HELP_URL:
        if(!qth_open_url(p_args[0].val.tstr))
            return(create_error(ERR_HELP_URL_FAILURE));
        return(STATUS_OK);
    }
}

/* end of linux/ho_help.c */
