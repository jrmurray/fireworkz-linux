/* qt_smoke.cpp */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Smoke test for the Qt backend of qt_host.h. Run with QT_QPA_PLATFORM=offscreen. */

#include "ob_skel/linux/qt_host.h"
#include "ob_skel/linux/qt/qth_qt.h"

#include <cstdio>
#include <cstring>

static int n_failures = 0;

#define check(cond) \
    do { if(!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++n_failures; } } while(0)

static unsigned int n_fonts, n_bold, n_italic;

static void
count_font(const QTH_FONT_DESC * p_desc, void *)
{
    ++n_fonts;
    if(p_desc->weight >= QTH_WEIGHT_BOLD)
        ++n_bold;
    if(p_desc->italic)
        ++n_italic;

    check(p_desc->family[0] != '\0');
}

static void
test_fonts(void)
{
    QTH_FONT_DESC desc;
    QTH_FONT_METRICS metrics;
    QTH_FONT * p_font;

    qth_font_enumerate(count_font, nullptr);
    check(n_fonts > 0);
    std::printf("qt_smoke: %u font styles (%u bold, %u italic)\n", n_fonts, n_bold, n_italic);

    std::memset(&desc, 0, sizeof(desc));
    std::strcpy(desc.family, "Sans Serif");
    desc.weight = QTH_WEIGHT_NORMAL;

    p_font = qth_font_create(&desc, 240, 240, 0); /* 12pt in pixits */
    check(nullptr != p_font);

    qth_font_metrics(p_font, &metrics);
    check((metrics.ascent > 0) && (metrics.descent > 0));
    check((metrics.space_width > 0) && (metrics.space_width < 240));

    {
    const uint8_t text[] = "abcdefgh";
    const int32_t w2 = qth_font_text_width(p_font, text, 2);
    const int32_t w4 = qth_font_text_width(p_font, text, 4);
    const int32_t w8 = qth_font_text_width(p_font, text, 8);
    check((0 < w2) && (w2 < w4) && (w4 < w8));
    check(0 == qth_font_text_width(p_font, text, 0));
    } /*block*/

    qth_font_destroy(p_font);
    check(nullptr == qth_font_create(&desc, 0, 0, 0));
}

static unsigned int n_paints, n_resizes;
static const QTH_RGB red = { 255, 0, 0 }, white = { 255, 255, 255 };

static void
handler(QTH_WINDOW *, const QTH_EVENT * p_event, void *)
{
    switch(p_event->type)
    {
    case QTH_EVENT_PAINT:
        {
        QTH_PAINTER * const p = p_event->u.paint.p_painter;
        const QTH_RECT all = { 0, 0, 200, 100 };
        const QTH_RECT box = { 10, 10, 60, 40 };
        const QTH_RECT inv = { 20, 20, 30, 30 };
        const QTH_RECT frame = { 100, 10, 150, 40 };
        int32_t dpi_x, dpi_y;

        ++n_paints;
        qth_painter_dpi(p, &dpi_x, &dpi_y);
        check((dpi_x > 0) && (dpi_y > 0));

        qth_paint_fill_rect(p, &all, white);
        qth_paint_fill_rect(p, &box, red);
        qth_paint_invert_rect(p, &inv);          /* red becomes cyan */
        qth_paint_frame_rect(p, &frame, red);

        { /* nothing outside the clip is painted */
        const QTH_RECT clip = { 160, 60, 170, 70 };
        const QTH_RECT big = { 150, 50, 200, 100 };
        qth_paint_set_clip(p, &clip);
        qth_paint_fill_rect(p, &big, red);
        } /*block*/
        break;
        }

    case QTH_EVENT_RESIZE:
        ++n_resizes;
        break;

    default:
        break;
    }
}

static void
test_window(void)
{
    QTH_WINDOW_DESC desc;
    QTH_WINDOW * p_window;
    QTH_RECT bounds;
    uint32_t argb;

    std::memset(&desc, 0, sizeof(desc));
    desc.kind = QTH_WINDOW_DOCUMENT;
    desc.bounds.right = 200;
    desc.bounds.bottom = 100;
    desc.p_title_latin1 = "qt_smoke";
    desc.proc = handler;

    p_window = qth_window_create(&desc);
    check(nullptr != p_window);

    qth_window_show(p_window, 1);
    qth_window_invalidate(p_window, nullptr);
    qth_qt_process_events();
    qth_window_update_now(p_window);
    check(n_paints > 0);
    check(n_resizes > 0);

    qth_window_get_bounds(p_window, &bounds);
    check((200 == bounds.right - bounds.left) && (100 == bounds.bottom - bounds.top));

    check(qth_qt_window_pixel(p_window, 15, 15, &argb) && (0xffff0000u == argb));    /* filled red */
    check(qth_qt_window_pixel(p_window, 25, 25, &argb) && (0xff00ffffu == argb));    /* inverted: cyan */
    check(qth_qt_window_pixel(p_window, 100, 20, &argb) && (0xffff0000u == argb));   /* frame, left edge */
    check(qth_qt_window_pixel(p_window, 120, 20, &argb) && (0xffffffffu == argb));   /* inside the frame: white */
    check(qth_qt_window_pixel(p_window, 165, 65, &argb) && (0xffff0000u == argb));   /* inside the clip */
    check(qth_qt_window_pixel(p_window, 190, 90, &argb) && (0xffffffffu == argb));   /* outside the clip */

    qth_window_destroy(p_window);
    qth_qt_process_events();
}

int
main(void)
{
    test_fonts();
    test_window();

    if(0 != n_failures)
    {
        std::fprintf(stderr, "qt_smoke: %d failure(s)\n", n_failures);
        return(1);
    }

    std::printf("qt_smoke: all checks passed\n");
    return(0);
}

/* end of qt_smoke.cpp */
