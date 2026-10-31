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
 *   PIXIT       1/1440 inch, as everywhere in Fireworkz.
 *   Font units  A font is made in whatever units the caller chooses, and its
 *               metrics and text widths come back in the same units: pixits
 *               when formatting, device pixels when painting.
 *   Device      Painting is in device pixels (the logical, device-independent
 *               pixels of the window or painter), origin top left, y down;
 *               rectangles are half open (right and bottom are outside).
 *               The C side has already turned pixits into pixels, using the
 *               host_xform and the DPI reported below.
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

/* size_x is the em width and size_y the em height, in the caller's font units;
 * equal for an unstretched font. Returns NULL if no font can be made. */
QTH_FONT * qth_font_create(const QTH_FONT_DESC * p_desc, int32_t size_x, int32_t size_y, uint8_t underline);

void qth_font_destroy(QTH_FONT * p_font);

typedef struct QTH_FONT_METRICS
{
    int32_t ascent;      /* font units above the baseline */
    int32_t descent;     /* font units below the baseline */
    int32_t space_width; /* advance of ' ' */
}
QTH_FONT_METRICS;

void qth_font_metrics(const QTH_FONT * p_font, QTH_FONT_METRICS * p_metrics);

/* Advance width in font units of n Latin-1 characters, kerning included. */
int32_t qth_font_text_width(const QTH_FONT * p_font, const uint8_t * p_latin1, uint32_t n);

/* The same for UTF-16, for text that Latin-1 cannot hold */
int32_t qth_font_text_width16(const QTH_FONT * p_font, const uint16_t * p_utf16, uint32_t n);

/* ---- Painting ------------------------------------------------------------- */

typedef struct QTH_RECT { int32_t left, top, right, bottom; } QTH_RECT;

typedef struct QTH_RGB { uint8_t r, g, b; } QTH_RGB;

/* What a window's paint event, or a printer or image, draws on. Owned by the
 * Qt side: valid only for the duration of the call that hands it over. */
typedef struct QTH_PAINTER QTH_PAINTER;

void qth_painter_dpi(const QTH_PAINTER * p_painter, int32_t * p_dpi_x, int32_t * p_dpi_y);

void qth_paint_fill_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb);

/* one device pixel wide, inside the rectangle */
void qth_paint_frame_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb);

/* one device pixel wide, from the centre of the first pixel to the centre of the last */
void qth_paint_line(QTH_PAINTER * p_painter, int32_t x0, int32_t y0, int32_t x1, int32_t y1, QTH_RGB rgb, int dashed);

/* each pixel becomes its complement, so that painting twice restores it */
void qth_paint_invert_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect);

/* Replaces any clip; nothing outside the rectangle is painted until the next call. */
void qth_paint_set_clip(QTH_PAINTER * p_painter, const QTH_RECT * p_rect);

#define QTH_ALIGN_LEFT   0
#define QTH_ALIGN_RIGHT  2
#define QTH_ALIGN_CENTRE 6

/* Latin-1 text with its baseline at y and, as the alignment says, x at its left,
 * right or middle. If p_background is not NULL the text's box is filled with it first. */
void qth_paint_text(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint8_t * p_latin1, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align);

void qth_paint_text16(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint16_t * p_utf16, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align);

/* ---- Images (toolbar bitmaps and the like) ---------------------------------- */

typedef struct QTH_IMAGE QTH_IMAGE;

QTH_IMAGE * qth_image_load(const char * p_path);  /* any format Qt reads; NULL on failure */
void qth_image_size(const QTH_IMAGE * p_image, int32_t * p_width, int32_t * p_height);
void qth_image_destroy(QTH_IMAGE * p_image);
void qth_paint_image(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest);

/* Part of an image (a button's icon out of a strip of them) into a rectangle; transparent, if not NULL, is
 * a colour whose pixels are left out, and opacity is 0 (invisible) to 255 (solid) */
void qth_paint_image_part(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_src, const QTH_RECT * p_dest, const QTH_RGB * p_transparent, int opacity);

/* The same, with the pixels of one colour left transparent (the way the Windows build paints markers) */
void qth_paint_image_keyed(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest, QTH_RGB transparent);

/* ---- Application and windows ------------------------------------------------ */

typedef struct QTH_WINDOW QTH_WINDOW;

void qth_screen_dpi(int32_t * p_dpi_x, int32_t * p_dpi_y);

/* The size of the part of the screen that windows can use, in pixels */
void qth_screen_work_area(int32_t * p_width, int32_t * p_height);

/* The event pump. Qt events are turned into these plain structs and handed to
 * the window's handler; the handler never sees Qt. */

enum
{
    QTH_EVENT_PAINT = 1, /* paint: draw the exposed rectangle on painter */
    QTH_EVENT_MOUSE,     /* mouse: press, release, move, double click */
    QTH_EVENT_KEY,       /* key: press with a code and any text it produces */
    QTH_EVENT_RESIZE,    /* resize: client area is now size.width x size.height */
    QTH_EVENT_SCROLL,    /* scroll: a scroll bar moved to value */
    QTH_EVENT_FOCUS,     /* focus: gained or lost */
    QTH_EVENT_TIMER,     /* timer: a timer set with qth_window_set_timer fired */
    QTH_EVENT_LEAVE,     /* leave: the pointer has left the window */
    QTH_EVENT_WHEEL,     /* wheel: delta is in 1/120ths of a notch, positive away from the user (up, or left) */
    QTH_EVENT_CLOSE      /* close: the user asked to close the window; call qth_window_destroy to agree */
};

enum { QTH_MOUSE_PRESS = 1, QTH_MOUSE_RELEASE, QTH_MOUSE_MOVE, QTH_MOUSE_DOUBLE };

#define QTH_BUTTON_LEFT   1
#define QTH_BUTTON_MIDDLE 2
#define QTH_BUTTON_RIGHT  4

#define QTH_MOD_SHIFT 1
#define QTH_MOD_CTRL  2
#define QTH_MOD_ALT   4

typedef struct QTH_EVENT
{
    int type;
    union
    {
        struct { QTH_PAINTER * p_painter; QTH_RECT exposed; } paint;
        struct { int action; int32_t x, y; int buttons; int modifiers; } mouse;
        struct { int32_t code; uint8_t latin1; int modifiers; } key; /* code is a Qt::Key; latin1 is 0 for none */
        struct { int32_t width, height; } resize;
        struct { int vertical; int32_t value; } scroll;
        struct { int gained; } focus;
        struct { int id; } timer;
        struct { int32_t x, y; int32_t delta_x, delta_y; int modifiers; } wheel;
    } u;
}
QTH_EVENT;

typedef void (* QTH_EVENT_PROC)(QTH_WINDOW * p_window, const QTH_EVENT * p_event, void * client);

enum
{
    QTH_WINDOW_DOCUMENT = 1, /* top level, with a title bar and frame */
    QTH_WINDOW_PANE,         /* a child that fills part of its parent: rulers, borders, the work area */
    QTH_WINDOW_HSCROLL,      /* a horizontal scroll bar child; sends QTH_EVENT_SCROLL */
    QTH_WINDOW_VSCROLL       /* a vertical scroll bar child */
};

typedef struct QTH_WINDOW_DESC
{
    int          kind;       /* QTH_WINDOW_xxx */
    QTH_WINDOW * p_parent;   /* NULL for QTH_WINDOW_DOCUMENT */
    QTH_RECT     bounds;     /* device pixels: on screen for a document window, in the parent for a pane */
    const char * p_title_latin1;
    QTH_EVENT_PROC proc;
    void *       client;     /* passed back to proc */
}
QTH_WINDOW_DESC;

QTH_WINDOW * qth_window_create(const QTH_WINDOW_DESC * p_desc);
void * qth_window_get_client(const QTH_WINDOW * p_window); /* what was given as client at creation */
void qth_window_destroy(QTH_WINDOW * p_window);

void qth_window_show(QTH_WINDOW * p_window, int visible);
void qth_window_set_title(QTH_WINDOW * p_window, const char * p_title_latin1);
void qth_window_set_bounds(QTH_WINDOW * p_window, const QTH_RECT * p_bounds);
void qth_window_get_bounds(const QTH_WINDOW * p_window, QTH_RECT * p_bounds);

/* The size of what the window scrolls over, and where in it the client area is. */
void qth_window_set_extent(QTH_WINDOW * p_window, int32_t width, int32_t height);
void qth_window_scroll_to(QTH_WINDOW * p_window, int32_t x, int32_t y);

/* Scroll bar windows (QTH_WINDOW_HSCROLL / VSCROLL): the range is min..max inclusive, page is
 * the size of the thumb in the same units, and the value is where the thumb is. Moving the
 * thumb sends QTH_EVENT_SCROLL; setting it from here does not. */
void qth_window_set_scroll_range(QTH_WINDOW * p_window, int32_t min, int32_t max, int32_t page);
void qth_window_set_scroll_value(QTH_WINDOW * p_window, int32_t value);

int32_t qth_window_get_scroll_value(const QTH_WINDOW * p_window);

/* Where the pointer is, in the window's client coordinates (which may be outside it), and screen conversions */
void qth_window_mouse_position(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y);
void qth_window_to_screen(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y);
void qth_window_screen_rect(const QTH_WINDOW * p_window, QTH_RECT * p_rect);

/* Move what is shown by (dx, dy) pixels and invalidate what is uncovered */
void qth_window_scroll_contents(QTH_WINDOW * p_window, int32_t dx, int32_t dy);

/* A repeating timer: sends QTH_EVENT_TIMER with this id every ms until killed */
void qth_window_set_timer(QTH_WINDOW * p_window, int id, int32_t ms);
void qth_window_kill_timer(QTH_WINDOW * p_window, int id);

/* Things the Windows host asks the system about */
enum
{
    QTH_METRIC_SCROLLBAR_WIDTH = 1, /* of a vertical scroll bar */
    QTH_METRIC_SCROLLBAR_HEIGHT,    /* of a horizontal scroll bar */
    QTH_METRIC_DRAG_X,              /* movement before a press becomes a drag */
    QTH_METRIC_DRAG_Y,
    QTH_METRIC_DOUBLECLICK_X,       /* movement allowed between the clicks of a double click */
    QTH_METRIC_DOUBLECLICK_Y,
    QTH_METRIC_DOUBLECLICK_MS
};

int32_t qth_metric(int which);

/* Ask for a paint event over the rectangle (NULL for all); update_now delivers it before returning. */
void qth_window_invalidate(QTH_WINDOW * p_window, const QTH_RECT * p_rect_or_null);
void qth_window_update_now(QTH_WINDOW * p_window);

/* Up to QTH_N_MARKERS inverted rectangles drawn over the window after its own painting, for
 * things like the line shown while dragging a split. Not in the window's own coordinates until painted. */
#define QTH_N_MARKERS 4
void qth_window_set_marker(QTH_WINDOW * p_window, int slot, const QTH_RECT * p_rect, int visible);

enum { QTH_STATE_NORMAL = 0, QTH_STATE_MINIMIZED, QTH_STATE_MAXIMIZED };
void qth_window_set_state(QTH_WINDOW * p_window, int state);

void qth_window_set_caret(QTH_WINDOW * p_window, const QTH_RECT * p_rect, int visible);
void qth_window_set_focus(QTH_WINDOW * p_window);

/* Mouse events go to this window even outside it, until released: for drags. */
void qth_window_capture_mouse(QTH_WINDOW * p_window, int capture);

enum { QTH_CURSOR_ARROW = 0, QTH_CURSOR_IBEAM, QTH_CURSOR_CROSS, QTH_CURSOR_HAND, QTH_CURSOR_WAIT, QTH_CURSOR_SIZE_H, QTH_CURSOR_SIZE_V, QTH_CURSOR_SIZE_ALL };

void qth_window_set_cursor(QTH_WINDOW * p_window, int cursor);

void qth_beep(void);

/* QTH_MOD_xxx for the modifier keys held down now */
int qth_keyboard_modifiers(void);

/* ---- Menus ------------------------------------------------------------------------ */

/* A menu bar, popup or submenu. Items are addressed by their position, separators included, as
 * in the Windows API that the shared code is written for. Texts are Latin-1; & marks the mnemonic. */

typedef struct QTH_MENU QTH_MENU;

enum
{
    QTH_MENU_ACTIVATE = 1,   /* an item was chosen: client is the item's */
    QTH_MENU_ABOUT_TO_SHOW   /* a menu is about to be shown: client is the menu's, so enabling can be brought up to date */
};

/* menu is the one that is about to be shown, or that the chosen item is in */
typedef void (* QTH_MENU_PROC)(QTH_WINDOW * p_window, int kind, void * client, QTH_MENU * p_menu);

/* Give the window a menu bar (replacing any), or make a popup menu for it */
QTH_MENU * qth_window_create_menu_bar(QTH_WINDOW * p_window, QTH_MENU_PROC proc);
QTH_MENU * qth_menu_create_popup(QTH_WINDOW * p_window, QTH_MENU_PROC proc, void * client);

/* shortcut_text is shown beside the item and may be NULL; it is not itself a keyboard shortcut */
void qth_menu_add_item(QTH_MENU * p_menu, const char * p_text, const char * p_shortcut_text, void * client);
void qth_menu_add_separator(QTH_MENU * p_menu);
QTH_MENU * qth_menu_add_submenu(QTH_MENU * p_menu, const char * p_text, void * client);

void qth_menu_set_item_enabled(QTH_MENU * p_menu, int index, int enabled);
QTH_MENU * qth_menu_get_submenu(QTH_MENU * p_menu, int index); /* NULL if the item has none */

/* Show a popup menu at the pointer and return when it has been dealt with */
void qth_menu_popup(QTH_MENU * p_menu);

/* ---- Dialog boxes ----------------------------------------------------------------- */

/* A dialog is a window of ordinary desktop widgets, built control by control by the dialog engine
 * (ob_dlg) from its tables. Controls are addressed by an id the caller chooses; positions and sizes
 * are in pixels, within the dialog's client area. Texts are Latin-1; & marks a mnemonic. */

typedef struct QTH_DIALOG QTH_DIALOG;

enum QTH_DLG_KIND
{
    QTH_DLG_LABEL = 1,   /* static text; flags say how it is aligned */
    QTH_DLG_FRAMED_TEXT, /* static text in a sunken frame */
    QTH_DLG_GROUP,       /* captioned group box */
    QTH_DLG_PUSH,        /* push button; QTH_DLGF_DEFAULT for the default one */
    QTH_DLG_RADIO,
    QTH_DLG_CHECK,
    QTH_DLG_TRISTATE,
    QTH_DLG_EDIT,        /* QTH_DLGF_MULTILINE, _READONLY, _RIGHT */
    QTH_DLG_SPIN,        /* an edit with up and down arrows beside it */
    QTH_DLG_LIST,
    QTH_DLG_COMBO,       /* QTH_DLGF_READONLY makes it a drop-down list only; dropdown_height is the drop-down's depth */
    QTH_DLG_BLANK        /* drawn by the caller on QTH_DLG_EV_PAINT; a click sends QTH_DLG_EV_CLICK, arg 0 for the
                          * left button and 1 for the right */
};

enum
{
    QTH_DLGF_DEFAULT   = 0x01,
    QTH_DLGF_MULTILINE = 0x02,
    QTH_DLGF_READONLY  = 0x04,
    QTH_DLGF_RIGHT     = 0x08,
    QTH_DLGF_CENTRE    = 0x10,
    QTH_DLGF_LEFT      = 0x20
};

enum
{
    QTH_DLG_EV_CLICK = 1,  /* push button, radio or check box pressed */
    QTH_DLG_EV_TEXT,       /* the user changed an edit's (or editable combo's) text */
    QTH_DLG_EV_SELECT,     /* the user chose a list or combo item: arg is its index */
    QTH_DLG_EV_DBLCLK,     /* a list item was double clicked */
    QTH_DLG_EV_FOCUS,      /* the control got the keyboard focus */
    QTH_DLG_EV_SPIN,       /* arrow pressed: arg is +1 for up, -1 for down */
    QTH_DLG_EV_HELP,       /* F1 */
    QTH_DLG_EV_CLOSE,      /* closed from the window frame (Escape, the close button): act as Cancel */
    QTH_DLG_EV_PAINT,      /* a QTH_DLG_BLANK control wants drawing: use qth_dlg_painter() */
    QTH_DLG_EV_HOVER,      /* the pointer entered (arg 1) or left (arg 0) a blank control */
    QTH_DLG_EV_PRESS       /* a mouse button went down (arg 1) or up (arg 0) on a blank control */
};

typedef void (* QTH_DLG_PROC)(QTH_DIALOG * p_dlg, int event, int id, int arg, void * client);

/* The size in pixels of a quarter of the dialog font's average character width and an eighth of its height
 * (a horizontal and vertical dialog unit), and the height of the title bar. */
void qth_dlg_metrics(int32_t * p_four_h_du_px, int32_t * p_eight_v_du_px);

/* The width in pixels of Latin-1 text in the dialog font */
int32_t qth_dlg_text_width(const uint8_t * p_text, uint32_t n);

/* parent may be NULL. The dialog is not shown until qth_dlg_exec or qth_dlg_show. */
QTH_DIALOG * qth_dlg_create(QTH_WINDOW * p_parent, const char * p_caption, int width, int height, QTH_DLG_PROC proc, void * client);
void qth_dlg_destroy(QTH_DIALOG * p_dlg);

void qth_dlg_add(QTH_DIALOG * p_dlg, int kind, int id, int x, int y, int width, int height, int flags, int dropdown_height, const char * p_text);

/* Where the dialog goes, in screen pixels; or at the centre of the screen or of its parent, or by the pointer */
void qth_dlg_place(QTH_DIALOG * p_dlg, int x, int y);
void qth_dlg_centre(QTH_DIALOG * p_dlg, int on_parent);
void qth_dlg_at_pointer(QTH_DIALOG * p_dlg);

/* Modal: shows the dialog and runs the event loop until qth_dlg_end. Modeless: shows it and returns. */
void qth_dlg_exec(QTH_DIALOG * p_dlg);
void qth_dlg_show(QTH_DIALOG * p_dlg);
void qth_dlg_end(QTH_DIALOG * p_dlg);

void qth_dlg_set_text(QTH_DIALOG * p_dlg, int id, const char * p_text);
/* Returns the length of the text, which is truncated to fit if n is too small to hold it with its terminator */
int  qth_dlg_get_text(QTH_DIALOG * p_dlg, int id, char * p_buf, int n);
void qth_dlg_set_check(QTH_DIALOG * p_dlg, int id, int state); /* 0 off, 1 on, 2 don't care */
void qth_dlg_set_enabled(QTH_DIALOG * p_dlg, int id, int enabled);
void qth_dlg_set_focus(QTH_DIALOG * p_dlg, int id);
void qth_dlg_set_default(QTH_DIALOG * p_dlg, int id);
void qth_dlg_select_all(QTH_DIALOG * p_dlg, int id);

/* The painter for the control being drawn, with the origin at its top left; only valid during QTH_DLG_EV_PAINT */
QTH_PAINTER * qth_dlg_painter(QTH_DIALOG * p_dlg);
void qth_dlg_ctl_size(QTH_DIALOG * p_dlg, int id, int32_t * p_width, int32_t * p_height);
void qth_dlg_invalidate(QTH_DIALOG * p_dlg, int id);

void qth_dlg_list_clear(QTH_DIALOG * p_dlg, int id);
void qth_dlg_list_add(QTH_DIALOG * p_dlg, int id, const char * p_text);
void qth_dlg_list_set_tab(QTH_DIALOG * p_dlg, int id, int pixels);
void qth_dlg_list_set_sel(QTH_DIALOG * p_dlg, int id, int index); /* -1 for none */
int  qth_dlg_list_get_sel(QTH_DIALOG * p_dlg, int id);            /* -1 for none */

/* Deliver the events that are waiting and return non-zero if there were any. If there are none, wait
 * for one for up to wait_ms milliseconds (0 is not at all, -1 is as long as it takes). */
int qth_process_events(int wait_ms);

/* Non-zero once qth_app_quit() has been called */
int qth_quit_requested(void);

/* Runs the event loop until qth_app_quit(). Returns the exit code. */
int  qth_app_run(void);
void qth_app_quit(int exit_code);

#ifdef __cplusplus
}
#endif

#endif /* QT_HOST_H */

/* end of qt_host.h */
