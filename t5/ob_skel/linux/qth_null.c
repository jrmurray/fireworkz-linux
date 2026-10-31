/* qth_null.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Headless backend for qt_host.h: a fixed set of fonts with simple, predictable
 * metrics (every character is half an em wide), so that the skeleton can be built
 * and tested without Qt. The Qt backend replaces this. */

#include "ob_skel/linux/qt_host.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct QTH_FONT
{
    int32_t size_x;
    int32_t size_y;
};

static const struct
{
    const char * family;
    uint8_t fixed_pitch;
    uint8_t family_class;
}
null_families[] =
{
    { "Null Sans",  0, QTH_FAMILY_SWISS },
    { "Null Serif", 0, QTH_FAMILY_ROMAN },
    { "Null Mono",  1, QTH_FAMILY_MODERN }
};

void
qth_font_enumerate(QTH_FONT_ENUM_PROC proc, void * client)
{
    unsigned int family, style;

    for(family = 0; family < sizeof(null_families) / sizeof(null_families[0]); ++family)
    {
        for(style = 0; style < 4; ++style)
        {
            QTH_FONT_DESC desc;

            memset(&desc, 0, sizeof(desc));
            strcpy(desc.family, null_families[family].family);
            desc.weight = (style & 1) ? QTH_WEIGHT_BOLD : QTH_WEIGHT_NORMAL;
            desc.italic = (style & 2) ? 1 : 0;
            desc.fixed_pitch = null_families[family].fixed_pitch;
            desc.family_class = null_families[family].family_class;

            (*proc)(&desc, client);
        }
    }
}

QTH_FONT *
qth_font_create(const QTH_FONT_DESC * p_desc, int32_t size_x_pixits, int32_t size_y_pixits, uint8_t underline)
{
    QTH_FONT * p_font;

    (void) p_desc;
    (void) underline;

    if(NULL == (p_font = malloc(sizeof(*p_font))))
        return(NULL);

    p_font->size_x = size_x_pixits;
    p_font->size_y = size_y_pixits;

    return(p_font);
}

void
qth_font_destroy(QTH_FONT * p_font)
{
    free(p_font);
}

void
qth_font_metrics(const QTH_FONT * p_font, QTH_FONT_METRICS * p_metrics)
{
    p_metrics->ascent = (p_font->size_y * 4) / 5;
    p_metrics->descent = p_font->size_y - p_metrics->ascent;
    p_metrics->space_width = p_font->size_x / 2;
}

int32_t
qth_font_text_width(const QTH_FONT * p_font, const uint8_t * p_latin1, uint32_t n)
{
    (void) p_latin1;

    return((int32_t) ((p_font->size_x / 2) * (int32_t) n));
}


/* ---- Dialog boxes: nothing is shown, and a modal dialog is over as soon as it starts ---- */

struct QTH_DIALOG { int dummy; };

void qth_dlg_metrics(int32_t * p_four_h_du_px, int32_t * p_eight_v_du_px) { *p_four_h_du_px = 6; *p_eight_v_du_px = 13; }

int32_t qth_dlg_text_width(const uint8_t * p_text, uint32_t n) { (void) p_text; return((int32_t) (n * 6)); }

QTH_DIALOG * qth_dlg_create(QTH_WINDOW * p_parent, const char * p_caption, int width, int height, QTH_DLG_PROC proc, void * client)
{
    QTH_DIALOG * p_dlg = (QTH_DIALOG *) calloc(1, sizeof(*p_dlg));
    (void) p_parent; (void) p_caption; (void) width; (void) height; (void) proc; (void) client;
    return(p_dlg);
}

void qth_dlg_destroy(QTH_DIALOG * p_dlg) { free(p_dlg); }
void qth_dlg_add(QTH_DIALOG * p_dlg, int kind, int id, int x, int y, int width, int height, int flags, int dropdown_height, const char * p_text)
{ (void) p_dlg; (void) kind; (void) id; (void) x; (void) y; (void) width; (void) height; (void) flags; (void) dropdown_height; (void) p_text; }
void qth_dlg_place(QTH_DIALOG * p_dlg, int x, int y) { (void) p_dlg; (void) x; (void) y; }
void qth_dlg_centre(QTH_DIALOG * p_dlg, int on_parent) { (void) p_dlg; (void) on_parent; }
void qth_dlg_at_pointer(QTH_DIALOG * p_dlg) { (void) p_dlg; }
void qth_dlg_exec(QTH_DIALOG * p_dlg) { (void) p_dlg; }
void qth_dlg_show(QTH_DIALOG * p_dlg) { (void) p_dlg; }
void qth_dlg_end(QTH_DIALOG * p_dlg) { (void) p_dlg; }
void qth_dlg_set_text(QTH_DIALOG * p_dlg, int id, const char * p_text) { (void) p_dlg; (void) id; (void) p_text; }
int  qth_dlg_get_text(QTH_DIALOG * p_dlg, int id, char * p_buf, int n) { (void) p_dlg; (void) id; if(n > 0) p_buf[0] = 0; return(0); }
void qth_dlg_set_check(QTH_DIALOG * p_dlg, int id, int state) { (void) p_dlg; (void) id; (void) state; }
void qth_dlg_set_enabled(QTH_DIALOG * p_dlg, int id, int enabled) { (void) p_dlg; (void) id; (void) enabled; }
void qth_dlg_set_focus(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; }
void qth_dlg_set_default(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; }
void qth_dlg_select_all(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; }
QTH_PAINTER * qth_dlg_painter(QTH_DIALOG * p_dlg) { (void) p_dlg; return(NULL); }
void qth_dlg_ctl_size(QTH_DIALOG * p_dlg, int id, int32_t * p_width, int32_t * p_height) { (void) p_dlg; (void) id; *p_width = 0; *p_height = 0; }
void qth_dlg_invalidate(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; }
void qth_dlg_list_clear(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; }
void qth_dlg_list_add(QTH_DIALOG * p_dlg, int id, const char * p_text) { (void) p_dlg; (void) id; (void) p_text; }
void qth_dlg_list_set_tab(QTH_DIALOG * p_dlg, int id, int pixels) { (void) p_dlg; (void) id; (void) pixels; }
void qth_dlg_list_set_sel(QTH_DIALOG * p_dlg, int id, int index) { (void) p_dlg; (void) id; (void) index; }
int  qth_dlg_list_get_sel(QTH_DIALOG * p_dlg, int id) { (void) p_dlg; (void) id; return(-1); }

/* end of qth_null.c */

/* ---- Painting, images, windows ---------------------------------------------- */

#include "ob_skel/linux/qth_null.h"

#include <stdio.h>

struct QTH_PAINTER
{
    int32_t dpi_x, dpi_y;
    char * log;
    size_t log_length, log_size;
};

static void
painter_logf(QTH_PAINTER * p_painter, const char * format, ...)
{
    char line[200];
    size_t n;
    va_list va;

    va_start(va, format);
    n = (size_t) vsnprintf(line, sizeof(line) - 1, format, va);
    va_end(va);

    if(n > sizeof(line) - 2)
        n = sizeof(line) - 2;
    line[n++] = '\n';

    if(p_painter->log_length + n + 1 > p_painter->log_size)
    {
        size_t new_size = (p_painter->log_size + n + 1) * 2;
        char * p_new = realloc(p_painter->log, new_size);
        if(NULL == p_new)
            return;
        p_painter->log = p_new;
        p_painter->log_size = new_size;
    }

    memcpy(p_painter->log + p_painter->log_length, line, n);
    p_painter->log_length += n;
    p_painter->log[p_painter->log_length] = '\0';
}

QTH_PAINTER *
qth_null_painter_new(int32_t dpi_x, int32_t dpi_y)
{
    QTH_PAINTER * p_painter = calloc(1, sizeof(*p_painter));

    if(NULL != p_painter)
    {
        p_painter->dpi_x = dpi_x;
        p_painter->dpi_y = dpi_y;
    }

    return(p_painter);
}

const char *
qth_null_painter_log(const QTH_PAINTER * p_painter)
{
    return((NULL != p_painter->log) ? p_painter->log : "");
}

void
qth_null_painter_clear_log(QTH_PAINTER * p_painter)
{
    p_painter->log_length = 0;
    if(NULL != p_painter->log)
        p_painter->log[0] = '\0';
}

void
qth_null_painter_free(QTH_PAINTER * p_painter)
{
    if(NULL != p_painter)
    {
        free(p_painter->log);
        free(p_painter);
    }
}

void
qth_painter_dpi(const QTH_PAINTER * p_painter, int32_t * p_dpi_x, int32_t * p_dpi_y)
{
    *p_dpi_x = p_painter->dpi_x;
    *p_dpi_y = p_painter->dpi_y;
}

void
qth_screen_dpi(int32_t * p_dpi_x, int32_t * p_dpi_y)
{
    *p_dpi_x = *p_dpi_y = 96;
}

void
qth_screen_work_area(int32_t * p_width, int32_t * p_height)
{
    *p_width = 1024;
    *p_height = 768;
}

#define RGB_ARGS(rgb) (rgb).r, (rgb).g, (rgb).b
#define RECT_ARGS(p) (int) (p)->left, (int) (p)->top, (int) (p)->right, (int) (p)->bottom

void
qth_paint_fill_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb)
{
    painter_logf(p_painter, "fill %d,%d,%d,%d #%02x%02x%02x", RECT_ARGS(p_rect), RGB_ARGS(rgb));
}

void
qth_paint_frame_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb)
{
    painter_logf(p_painter, "frame %d,%d,%d,%d #%02x%02x%02x", RECT_ARGS(p_rect), RGB_ARGS(rgb));
}

void
qth_paint_line(QTH_PAINTER * p_painter, int32_t x0, int32_t y0, int32_t x1, int32_t y1, QTH_RGB rgb, int dashed)
{
    painter_logf(p_painter, "%s %d,%d,%d,%d #%02x%02x%02x", dashed ? "dashed" : "line", (int) x0, (int) y0, (int) x1, (int) y1, RGB_ARGS(rgb));
}

void
qth_paint_invert_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect)
{
    painter_logf(p_painter, "invert %d,%d,%d,%d", RECT_ARGS(p_rect));
}

void
qth_paint_set_clip(QTH_PAINTER * p_painter, const QTH_RECT * p_rect)
{
    painter_logf(p_painter, "clip %d,%d,%d,%d", RECT_ARGS(p_rect));
}

void
qth_paint_text(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint8_t * p_latin1, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align)
{
    char text[64];
    uint32_t i;

    (void) p_font;

    for(i = 0; (i < n) && (i < sizeof(text) - 1); ++i)
        text[i] = (char) p_latin1[i];
    text[i] = '\0';

    if(NULL != p_background)
        painter_logf(p_painter, "text %d,%d align=%d \"%s\" #%02x%02x%02x on #%02x%02x%02x", (int) x, (int) y, align, text, RGB_ARGS(foreground), RGB_ARGS(*p_background));
    else
        painter_logf(p_painter, "text %d,%d align=%d \"%s\" #%02x%02x%02x", (int) x, (int) y, align, text, RGB_ARGS(foreground));
}

int32_t
qth_font_text_width16(const QTH_FONT * p_font, const uint16_t * p_utf16, uint32_t n)
{
    (void) p_utf16;

    return((int32_t) ((p_font->size_x / 2) * (int32_t) n));
}

void
qth_paint_text16(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint16_t * p_utf16, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align)
{
    char text[64];
    uint32_t i;

    for(i = 0; (i < n) && (i < sizeof(text) - 1); ++i)
        text[i] = (p_utf16[i] < 0x80) ? (char) p_utf16[i] : '?';
    text[i] = '\0';

    qth_paint_text(p_painter, p_font, x, y, (const uint8_t *) text, i, foreground, p_background, align);
}

struct QTH_IMAGE
{
    int32_t width, height;
};

QTH_IMAGE *
qth_image_load(const char * p_path)
{
    (void) p_path;
    return(NULL); /* the headless backend has no image codecs */
}

void
qth_image_size(const QTH_IMAGE * p_image, int32_t * p_width, int32_t * p_height)
{
    *p_width = p_image->width;
    *p_height = p_image->height;
}

void
qth_image_destroy(QTH_IMAGE * p_image)
{
    free(p_image);
}

void
qth_paint_image(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest)
{
    (void) p_image;
    painter_logf(p_painter, "image %d,%d,%d,%d", RECT_ARGS(p_dest));
}

void
qth_paint_image_keyed(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest, QTH_RGB transparent)
{
    (void) p_image;
    painter_logf(p_painter, "image-keyed %d,%d,%d,%d #%02x%02x%02x", RECT_ARGS(p_dest), RGB_ARGS(transparent));
}

void
qth_paint_image_part(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_src, const QTH_RECT * p_dest, const QTH_RGB * p_transparent, int opacity)
{
    (void) p_image;
    (void) p_transparent;
    painter_logf(p_painter, "image-part %d,%d,%d,%d to %d,%d,%d,%d opacity %d", RECT_ARGS(p_src), RECT_ARGS(p_dest), opacity);
}

struct QTH_WINDOW
{
    QTH_WINDOW_DESC desc;
    QTH_NULL_WINDOW_STATE state;
};

static int n_windows;

QTH_WINDOW *
qth_window_create(const QTH_WINDOW_DESC * p_desc)
{
    QTH_WINDOW * p_window = calloc(1, sizeof(*p_window));

    if(NULL == p_window)
        return(NULL);

    p_window->desc = *p_desc;
    p_window->state.bounds = p_desc->bounds;
    qth_window_set_title(p_window, p_desc->p_title_latin1);
    ++n_windows;

    return(p_window);
}

void *
qth_window_get_client(const QTH_WINDOW * p_window)
{
    return(p_window->desc.client);
}

void
qth_window_destroy(QTH_WINDOW * p_window)
{
    if(NULL != p_window)
    {
        --n_windows;
        free(p_window);
    }
}

void
qth_window_show(QTH_WINDOW * p_window, int visible)
{
    p_window->state.visible = visible;
}

void
qth_window_set_title(QTH_WINDOW * p_window, const char * p_title_latin1)
{
    snprintf(p_window->state.title, sizeof(p_window->state.title), "%s", (NULL != p_title_latin1) ? p_title_latin1 : "");
}

void
qth_window_set_bounds(QTH_WINDOW * p_window, const QTH_RECT * p_bounds)
{
    p_window->state.bounds = *p_bounds;
}

void
qth_window_get_bounds(const QTH_WINDOW * p_window, QTH_RECT * p_bounds)
{
    *p_bounds = p_window->state.bounds;
}

void
qth_window_set_extent(QTH_WINDOW * p_window, int32_t width, int32_t height)
{
    p_window->state.extent_width = width;
    p_window->state.extent_height = height;
}

void
qth_window_scroll_to(QTH_WINDOW * p_window, int32_t x, int32_t y)
{
    p_window->state.scroll_x = x;
    p_window->state.scroll_y = y;
}

void
qth_window_set_scroll_range(QTH_WINDOW * p_window, int32_t min, int32_t max, int32_t page)
{
    p_window->state.range_min = min;
    p_window->state.range_max = max;
    p_window->state.range_page = page;
}

void
qth_window_set_scroll_value(QTH_WINDOW * p_window, int32_t value)
{
    p_window->state.scroll_value = value;
}

int32_t
qth_window_get_scroll_value(const QTH_WINDOW * p_window)
{
    return(p_window->state.scroll_value);
}

void
qth_window_mouse_position(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y)
{
    *p_x = p_window->state.mouse_x;
    *p_y = p_window->state.mouse_y;
}

void
qth_window_to_screen(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y)
{
    *p_x += p_window->state.bounds.left;
    *p_y += p_window->state.bounds.top;
}

void
qth_window_screen_rect(const QTH_WINDOW * p_window, QTH_RECT * p_rect)
{
    *p_rect = p_window->state.bounds;
}

void
qth_window_scroll_contents(QTH_WINDOW * p_window, int32_t dx, int32_t dy)
{
    p_window->state.contents_dx += dx;
    p_window->state.contents_dy += dy;
}

void
qth_window_set_timer(QTH_WINDOW * p_window, int id, int32_t ms)
{
    p_window->state.timer_id = id;
    p_window->state.timer_ms = ms;
}

void
qth_window_kill_timer(QTH_WINDOW * p_window, int id)
{
    if(p_window->state.timer_id == id)
        p_window->state.timer_id = 0;
}

int32_t
qth_metric(int which)
{
    switch(which)
    {
    case QTH_METRIC_SCROLLBAR_WIDTH:
    case QTH_METRIC_SCROLLBAR_HEIGHT:   return(16);
    case QTH_METRIC_DRAG_X:
    case QTH_METRIC_DRAG_Y:             return(4);
    case QTH_METRIC_DOUBLECLICK_X:
    case QTH_METRIC_DOUBLECLICK_Y:      return(4);
    case QTH_METRIC_DOUBLECLICK_MS:     return(500);
    default:                            return(0);
    }
}

void
qth_window_invalidate(QTH_WINDOW * p_window, const QTH_RECT * p_rect_or_null)
{
    (void) p_rect_or_null;
    ++p_window->state.n_invalidates;
}

void
qth_window_update_now(QTH_WINDOW * p_window)
{
    QTH_EVENT event;
    QTH_PAINTER * p_painter = qth_null_painter_new(96, 96);

    memset(&event, 0, sizeof(event));
    event.type = QTH_EVENT_PAINT;
    event.u.paint.p_painter = p_painter;
    event.u.paint.exposed.right = p_window->state.bounds.right - p_window->state.bounds.left;
    event.u.paint.exposed.bottom = p_window->state.bounds.bottom - p_window->state.bounds.top;

    qth_null_post_event(p_window, &event);

    qth_null_painter_free(p_painter);
}

void
qth_window_set_caret(QTH_WINDOW * p_window, const QTH_RECT * p_rect, int visible)
{
    p_window->state.caret = *p_rect;
    p_window->state.caret_visible = visible;
}

void
qth_window_set_marker(QTH_WINDOW * p_window, int slot, const QTH_RECT * p_rect, int visible)
{
    if((slot >= 0) && (slot < QTH_N_MARKERS))
    {
        p_window->state.marker[slot] = *p_rect;
        p_window->state.marker_visible[slot] = visible;
    }
}

void
qth_window_set_state(QTH_WINDOW * p_window, int state)
{
    p_window->state.state = state;
}

void
qth_window_set_focus(QTH_WINDOW * p_window)
{
    QTH_EVENT event;

    memset(&event, 0, sizeof(event));
    event.type = QTH_EVENT_FOCUS;
    event.u.focus.gained = 1;

    qth_null_post_event(p_window, &event);
}

void
qth_window_capture_mouse(QTH_WINDOW * p_window, int capture)
{
    p_window->state.mouse_captured = capture;
}

void
qth_window_set_cursor(QTH_WINDOW * p_window, int cursor)
{
    p_window->state.cursor = cursor;
}

static int g_modifiers;

int
qth_keyboard_modifiers(void)
{
    return(g_modifiers);
}

void
qth_null_set_modifiers(int modifiers)
{
    g_modifiers = modifiers;
}

static unsigned int n_beeps;

void
qth_beep(void)
{
    ++n_beeps;
}

unsigned int
qth_null_n_beeps(void)
{
    return(n_beeps);
}

static int quit_code;
static int quit_requested;

int
qth_process_events(int wait_ms)
{
    (void) wait_ms;
    return(0);
}

int
qth_quit_requested(void)
{
    return(quit_requested);
}

int
qth_app_run(void)
{
    return(quit_code); /* no events to wait for */
}

void
qth_app_quit(int exit_code)
{
    quit_requested = 1;
    quit_code = exit_code;
}

void
qth_null_post_event(QTH_WINDOW * p_window, const QTH_EVENT * p_event)
{
    if(NULL != p_window->desc.proc)
        (*p_window->desc.proc)(p_window, p_event, p_window->desc.client);
}

void
qth_null_window_state(const QTH_WINDOW * p_window, QTH_NULL_WINDOW_STATE * p_state)
{
    *p_state = p_window->state;
}

int
qth_null_n_windows(void)
{
    return(n_windows);
}

/* ---- Menus: kept as a tree so that a test can look at it and choose items ------------------ */

#define NULL_MENU_MAX_ITEMS 64

struct QTH_MENU
{
    QTH_WINDOW * p_window;
    QTH_MENU_PROC proc;
    char text[NULL_MENU_MAX_ITEMS][64];
    char shortcut[NULL_MENU_MAX_ITEMS][32];
    int enabled[NULL_MENU_MAX_ITEMS];
    void * client[NULL_MENU_MAX_ITEMS];
    QTH_MENU * p_sub[NULL_MENU_MAX_ITEMS];
    int n_items;
};

static QTH_MENU *
null_menu_new(QTH_WINDOW * p_window, QTH_MENU_PROC proc)
{
    QTH_MENU * p_menu = calloc(1, sizeof(*p_menu));

    if(NULL != p_menu)
    {
        p_menu->p_window = p_window;
        p_menu->proc = proc;
    }

    return(p_menu);
}

static QTH_MENU * null_menu_bar;

QTH_MENU *
qth_window_create_menu_bar(QTH_WINDOW * p_window, QTH_MENU_PROC proc)
{
    return(null_menu_bar = null_menu_new(p_window, proc));
}

QTH_MENU *
qth_menu_create_popup(QTH_WINDOW * p_window, QTH_MENU_PROC proc, void * client)
{
    (void) client;
    return(null_menu_new(p_window, proc));
}

static int
null_menu_add(QTH_MENU * p_menu, const char * p_text, const char * p_shortcut, void * client)
{
    int i = p_menu->n_items;

    if(i >= NULL_MENU_MAX_ITEMS)
        return(-1);

    snprintf(p_menu->text[i], sizeof(p_menu->text[i]), "%s", (NULL != p_text) ? p_text : "");
    snprintf(p_menu->shortcut[i], sizeof(p_menu->shortcut[i]), "%s", (NULL != p_shortcut) ? p_shortcut : "");
    p_menu->enabled[i] = 1;
    p_menu->client[i] = client;
    p_menu->p_sub[i] = NULL;

    return(p_menu->n_items++);
}

void
qth_menu_add_item(QTH_MENU * p_menu, const char * p_text, const char * p_shortcut_text, void * client)
{
    (void) null_menu_add(p_menu, p_text, p_shortcut_text, client);
}

void
qth_menu_add_separator(QTH_MENU * p_menu)
{
    (void) null_menu_add(p_menu, NULL, NULL, NULL);
}

QTH_MENU *
qth_menu_add_submenu(QTH_MENU * p_menu, const char * p_text, void * client)
{
    int i = null_menu_add(p_menu, p_text, NULL, client);

    if(i < 0)
        return(NULL);

    return(p_menu->p_sub[i] = null_menu_new(p_menu->p_window, p_menu->proc));
}

void
qth_menu_set_item_enabled(QTH_MENU * p_menu, int index, int enabled)
{
    if((index >= 0) && (index < p_menu->n_items))
        p_menu->enabled[index] = enabled;
}

QTH_MENU *
qth_menu_get_submenu(QTH_MENU * p_menu, int index)
{
    return(((index >= 0) && (index < p_menu->n_items)) ? p_menu->p_sub[index] : NULL);
}

void
qth_menu_popup(QTH_MENU * p_menu)
{
    (void) p_menu;
}

/* test hooks */

QTH_MENU *
qth_null_menu_bar(void)
{
    return(null_menu_bar);
}

int
qth_null_menu_n_items(const QTH_MENU * p_menu)
{
    return(p_menu->n_items);
}

const char *
qth_null_menu_item_text(const QTH_MENU * p_menu, int index)
{
    return(p_menu->text[index]);
}

int
qth_null_menu_item_enabled(const QTH_MENU * p_menu, int index)
{
    return(p_menu->enabled[index]);
}

void
qth_null_menu_choose(QTH_MENU * p_menu, int index)
{
    if((index >= 0) && (index < p_menu->n_items) && (NULL != p_menu->proc))
        (*p_menu->proc)(p_menu->p_window, QTH_MENU_ACTIVATE, p_menu->client[index], p_menu);
}

void
qth_null_menu_open(QTH_MENU * p_parent, int index)
{
    QTH_MENU * p_sub = qth_menu_get_submenu(p_parent, index);

    if((NULL != p_sub) && (NULL != p_parent->proc))
        (*p_parent->proc)(p_parent->p_window, QTH_MENU_ABOUT_TO_SHOW, p_parent->client[index], p_sub);
}
