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
    int32_t range_min, range_max, range_page, scroll_value;
    int32_t contents_dx, contents_dy;
    int32_t mouse_x, mouse_y; /* what qth_window_mouse_position reports; set by the test */
    int timer_id;
    int state;
    int marker_visible[QTH_N_MARKERS];
    QTH_RECT marker[QTH_N_MARKERS];
    int32_t timer_ms;
}
QTH_NULL_WINDOW_STATE;

void qth_null_window_state(const QTH_WINDOW * p_window, QTH_NULL_WINDOW_STATE * p_state);

unsigned int qth_null_n_beeps(void);

/* what qth_keyboard_modifiers() reports */
void qth_null_set_modifiers(int modifiers);

int qth_null_n_windows(void); /* created and not yet destroyed */

/* The menu bar the host layer made, as a test sees it */
QTH_MENU * qth_null_menu_bar(void);
int qth_null_menu_n_items(const QTH_MENU * p_menu);
const char * qth_null_menu_item_text(const QTH_MENU * p_menu, int index);
int qth_null_menu_item_enabled(const QTH_MENU * p_menu, int index);

/* Choose an item, or open a submenu (which brings its enabling up to date), as the user would */
void qth_null_menu_choose(QTH_MENU * p_menu, int index);
void qth_null_menu_open(QTH_MENU * p_parent, int index);

#ifdef __cplusplus
}
#endif

#endif /* QTH_NULL_H */

/* end of qth_null.h */
