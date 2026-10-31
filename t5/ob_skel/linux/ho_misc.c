/* ho_misc.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Misc host stuff - no fixed abode */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

/* Make a beep sound, to inform the user that something has happened. */

extern void
host_bleep(void)
{
    qth_beep();
}

_Check_return_
extern U32
host_rand_between(
    _InVal_ U32 lo /*incl*/,
    _InVal_ U32 hi /*excl*/) /* NB NOT like RANDBETWEEN() */
{
    U32 res = lo;

    if(hi > lo)
    {
        const U32 n = hi - lo;
        const U32 mask = n - 1;
        const U32 r = (U32) rand();
        assert(n < (U32) RAND_MAX);
        if(0 == (n & mask))
        {   /* n is a power of two - saves a divide */
            res = lo + (r & mask);
        }
        else
        {
            res = lo + (r % n);
        }
    }

    return(res);
}

_Check_return_
extern BOOL
host_shift_pressed(void)
{
    return(0 != (qth_keyboard_modifiers() & QTH_MOD_SHIFT));
}

_Check_return_
extern BOOL
host_ctrl_pressed(void)
{
    return(0 != (qth_keyboard_modifiers() & QTH_MOD_CTRL));
}

/* Nothing is held by the host layer that the Qt side does not release itself. */

extern void
host_shutdown(void)
{
}

/* end of ho_misc.c */
