/* qth_null.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Test hooks for the headless backend (qth_null.c): record what is painted and
 * inject events, so that the host layer can be tested without Qt. */

#ifndef QTH_NULL_H
#define QTH_NULL_H

#include "ob_skel/linux/qt_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A painter that records each call as a line of text, e.g. "fill 0,0,10,10 #ff0000" */
QTH_PAINTER * qth_null_painter_new(int32_t dpi_x, int32_t dpi_y);
const char * qth_null_painter_log(const QTH_PAINTER * p_painter);
void qth_null_painter_clear_log(QTH_PAINTER * p_painter);
void qth_null_painter_free(QTH_PAINTER * p_painter);

/* Deliver an event to a window's handler, as the event loop would */
void qth_null_post_event(QTH_WINDOW * p_window, const QTH_EVENT * p_event);

/* What the host layer has asked of a window */
typedef struct QTH_NULL_WINDOW_STATE
{
    int visible;
    char title[128];
    QTH_RECT bounds;
    int32_t extent_width, extent_height;
    int32_t scroll_x, scroll_y;
    int caret_visible;
    QTH_RECT caret;
    int cursor;
    int mouse_captured;
    unsigned int n_invalidates;
}
QTH_NULL_WINDOW_STATE;

void qth_null_window_state(const QTH_WINDOW * p_window, QTH_NULL_WINDOW_STATE * p_state);

int qth_null_n_windows(void); /* created and not yet destroyed */

#ifdef __cplusplus
}
#endif

#endif /* QTH_NULL_H */

/* end of qth_null.h */
