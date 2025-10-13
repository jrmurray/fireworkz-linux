/* profile.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1992-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Profiler interface */

/* SKS February 1992 */

#ifndef PROFILE_H
#define PROFILE_H

#if RISCOS

#ifndef  _kernel_h
#include "kernel.h"
#endif

#define Profiler_Init       0x088000
/* these from profilei.h */

#ifndef Profiler_Init
#define Profiler_Init                   0x088000
#define Profiler_On                     0x088001
#define Profiler_Off                    0x088002
#define Profiler_Output                 0x088003
#define Profiler_Reset                  0x088004
#define Profiler_TaskChange             0x088005
#define Profiler_Fini                   0x088006
#endif

enum PROFILE_SORT_BY
{
    PROFILE_SORT_BY_NONE = 0,
    PROFILE_SORT_BY_ADDR,
    PROFILE_SORT_BY_COUNT,
    PROFILE_SORT_BY_NAME
};

/*
external functions
*/

static inline _kernel_oserror *
profile_init(void)
{
    return(_swix(Profiler_Init, 0));
}

static inline _kernel_oserror *
profile_on(void)
{
    return(_swix(Profiler_On, 0));
}

static inline _kernel_oserror *
profile_off(void)
{
    return(_swix(Profiler_Off, 0));
}

static inline _kernel_oserror *
profile_output(
    enum PROFILE_SORT_BY sort_by,
    const char * filename,
    const char * sep)
{
    return(_swix(Profiler_Output, _INR(0,2), sort_by, filename, sep));
}

static inline _kernel_oserror *
profile_reset(void)
{
    return(_swix(Profiler_Reset, 0));
}

static inline _kernel_oserror *
profile_taskchange(void)
{
    return(_swix(Profiler_TaskChange, 0));
}

static inline _kernel_oserror *
profile_fini(void)
{
    return(_swix(Profiler_Fini, 0));
}

#endif /* RISCOS */

#endif /* PROFILE_H */

/* end of profile.h */
