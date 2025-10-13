/* ss_linest.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1992-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Spreadsheet object module header */

/* SKS February 2014 split out of xp_ss.h for charts */

#ifndef SS_LINEST_H
#define SS_LINEST_H

#ifndef            EV_EVAL_H
#include "cmodules/ev_eval.h"
#endif

#ifndef            MATHXTR2_H
#include "cmodules/mathxtr2.h"
#endif

typedef struct SS_LINEST
{
    P_PROC_LINEST_DATA_GET p_proc_get;
    P_PROC_LINEST_DATA_PUT p_proc_put;
    CLIENT_HANDLE client_handle;
    U32 m; /* number of independent x variables */
    U32 n; /* number of data points */
}
SS_LINEST, * P_SS_LINEST;

#endif /* SS_LINEST_H */

/* end of ss_linest.h */
