/* qth_qt.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Test hooks for the Qt backend (qth_qt.cpp) */

#ifndef QTH_QT_H
#define QTH_QT_H

#include "ob_skel/linux/qt_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deliver pending Qt events (paint, resize, ...) without entering the event loop */
void qth_qt_process_events(void);

/* Read a pixel of what the window shows, as 0xAARRGGBB, by rendering it offscreen.
 * Returns 0 if the window or pixel is not available. */
int qth_qt_window_pixel(QTH_WINDOW * p_window, int32_t x, int32_t y, uint32_t * p_argb);

#ifdef __cplusplus
}
#endif

#endif /* QTH_QT_H */

/* end of qth_qt.h */
