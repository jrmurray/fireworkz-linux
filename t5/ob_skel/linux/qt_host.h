/* qt_host.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The narrow C interface between the Fireworkz skeleton and the Qt 6 host layer.
 *
 * The skeleton's host_*() functions (the counterpart of ob_skel/windows/ho_*.c)
 * are written in C in ob_skel/linux/ho_*.c. Everything that needs Qt goes through
 * the qth_*() functions declared here, which are implemented in C++ in
 * ob_skel/linux/qt/ and, for tests, by the headless backend in qth_null.c.
 *
 * This header is plain C with no dependency on the Fireworkz headers, so that
 * the Qt side can include it as it is.
 *
 * Conventions
 *
 *   PIXIT       1/1440 inch, as everywhere in Fireworkz. Font sizes and font
 *               metrics are exchanged in pixits, so that formatting does not
 *               depend on the screen's pixel grid.
 *   Text        8-bit Latin-1 (what Fireworkz calls TSTR/USTR on this target).
 *               Family names that Latin-1 cannot express are not enumerated.
 *   Threads     All calls are made from the GUI thread.
 *   Handles     Opaque; owned by the caller until the matching *_destroy().
 */

#ifndef QT_HOST_H
#define QT_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Fonts ---------------------------------------------------------------- */

#define QTH_FACENAME_LEN 64 /* including the terminating NUL */

#define QTH_WEIGHT_NORMAL 400
#define QTH_WEIGHT_SEMIBOLD 600
#define QTH_WEIGHT_BOLD 700

/* generic family, as FF_xxx on Windows, used to invent Fireworkz font classes */
enum
{
    QTH_FAMILY_DONTCARE = 0,
    QTH_FAMILY_ROMAN,       /* serif */
    QTH_FAMILY_SWISS,       /* sans serif */
    QTH_FAMILY_MODERN,      /* fixed pitch */
    QTH_FAMILY_SCRIPT,
    QTH_FAMILY_DECORATIVE
};

typedef struct QTH_FONT_DESC
{
    char     family[QTH_FACENAME_LEN];
    int32_t  weight;        /* QTH_WEIGHT_xxx */
    uint8_t  italic;
    uint8_t  fixed_pitch;
    uint8_t  family_class;  /* QTH_FAMILY_xxx */
    uint8_t  symbol;        /* not a text font (dingbats etc) */
}
QTH_FONT_DESC;

typedef struct QTH_FONT QTH_FONT; /* a sized font ready for measuring and painting */

typedef void (* QTH_FONT_ENUM_PROC)(const QTH_FONT_DESC * p_desc, void * client);

/* Report each installed style (regular, bold, italic, ...) of each family once. */
void qth_font_enumerate(QTH_FONT_ENUM_PROC proc, void * client);

/* size_x_pixits is the em width and size_y_pixits the em height; equal for an
 * unstretched font. Returns NULL if no font can be made. */
QTH_FONT * qth_font_create(const QTH_FONT_DESC * p_desc, int32_t size_x_pixits, int32_t size_y_pixits, uint8_t underline);

void qth_font_destroy(QTH_FONT * p_font);

typedef struct QTH_FONT_METRICS
{
    int32_t ascent;      /* pixits above the baseline */
    int32_t descent;     /* pixits below the baseline */
    int32_t space_width; /* advance of ' ' */
}
QTH_FONT_METRICS;

void qth_font_metrics(const QTH_FONT * p_font, QTH_FONT_METRICS * p_metrics);

/* Advance width in pixits of n Latin-1 characters, kerning included. */
int32_t qth_font_text_width(const QTH_FONT * p_font, const uint8_t * p_latin1, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* QT_HOST_H */

/* end of qt_host.h */
