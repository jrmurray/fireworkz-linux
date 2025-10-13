/* fs_rtf.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1993-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Local header file for RTF save object module for Fireworkz */

/* JAD Jul 1993 */

#ifndef            SK_SLOT_H
#include "ob_cells/sk_slot.h"
#endif

/*
internal structure
*/

typedef struct RTF_STYLESHEET
{
    U32 style_number;
    TCHARZ name[36];
}
RTF_STYLESHEET, * P_RTF_STYLESHEET;

/* end of fs_rtf.h */
