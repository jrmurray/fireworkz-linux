/* monotime.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1992-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Monotonic timer */

/* SKS October 1990 */

#include "common/gflags.h"

#if RISCOS && defined(__CC_NORCROFT)
#error You require assembler implementation s.monotasm
#endif

#include <time.h>

/******************************************************************************
*
* return the current monotonic time
*
* RISC OS: in centiseconds since the machine last started
* Windows: in milliseconds since the machine last started
* Linux:   in milliseconds from CLOCK_MONOTONIC
*
* clients should cater for wrapping (49 days on Windows)
*
******************************************************************************/

_Check_return_
extern MONOTIME
monotime(void)
{
#if WINDOWS
    __pragma(warning(suppress: 28159))
    DWORD  now = GetTickCount();
#elif LINUX
    struct timespec ts;
    U32 now = 0;

    if(0 == clock_gettime(CLOCK_MONOTONIC, &ts))
        now = (U32) ((U64) ts.tv_sec * 1000U + (U64) ts.tv_nsec / 1000000U);
#else
    time_t now = time(NULL);
#endif

    return((MONOTIME) now);
}

/* end of monotime.c */
