/* ho_event.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Linux event loop step: the counterpart of windows/ho_event.c.
 *
 * Fireworkz does its background work (scheduled events, null events) when the window system has
 * no input for it. This is the one place that decides between waiting for input and doing that work. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

/******************************************************************************
*
* get and process events - returning only if foreground null events are wanted.
*
******************************************************************************/

_Check_return_
extern WM_EVENT
wm_event_get(
    _InVal_     BOOL fgNullEventsWanted)
{
    static BOOL last_event_was_null = FALSE;
    BOOL peek, bgScheduledEventsWanted, bgNullEventsWanted, bgNullTestWanted;

    if(qth_quit_requested())
        return(WM_EVENT_EXIT);

    /* anyone interested in setting up bgNulls better do their thinking in the null query handlers */
    bgScheduledEventsWanted = (scheduled_events_do_query() == SCHEDULED_EVENTS_REQUIRED);

    if(last_event_was_null)
    {
        /* only ask our null query handlers at the end of each real requested null event whether they
         * want to continue having nulls or not; this saves wasted time querying after every single event
         * null or otherwise; so we must request a single test null event to come through to query the
         * handlers whenever some idle time turns up */
        bgNullEventsWanted = (null_events_do_query() == NULL_EVENTS_REQUIRED);
        bgNullTestWanted   = FALSE;
    }
    else
    {
        bgNullEventsWanted = FALSE;
        bgNullTestWanted   = TRUE;
    }

    peek = (fgNullEventsWanted || bgScheduledEventsWanted || bgNullEventsWanted || bgNullTestWanted);

    /* if there is background work, only look at what is waiting; otherwise sleep until something happens */
    if(qth_process_events(peek ? 0 : -1))
    {
        last_event_was_null = FALSE;

        return(qth_quit_requested() ? WM_EVENT_EXIT : WM_EVENT_PROCESSED);
    }

    if(qth_quit_requested())
        return(WM_EVENT_EXIT);

    /* only allow our processors a look in now at nulls they wanted */
    last_event_was_null = TRUE;

    /* perform the initial idle time test if some idle time has come */
    if(bgNullTestWanted)
        bgNullEventsWanted = (null_events_do_query() == NULL_EVENTS_REQUIRED);

    /* processor is idle, dispatch scheduled & null events appropriately */
    if(bgScheduledEventsWanted)
    {
        (void) scheduled_events_do_events();

        bgScheduledEventsWanted = (scheduled_events_do_query() == SCHEDULED_EVENTS_REQUIRED);
    }

    if(bgNullEventsWanted)
        (void) null_events_do_events();

    if(fgNullEventsWanted)
        return(WM_EVENT_FG_NULL); /* we have our desired fg idle, so return */

    if(bgScheduledEventsWanted && !(fgNullEventsWanted || bgNullEventsWanted))
        (void) qth_process_events(10 /*ms*/); /* let a timer or input wake us, rather than spin */

    return(WM_EVENT_PROCESSED);
}

/* end of ho_event.c */
