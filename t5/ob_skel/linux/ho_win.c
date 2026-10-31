/* ho_win.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1993-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* Linux host views and windows. Derived from windows/ho_win.c: the layout, scrolling,
 * updating, click and key code is shared in spirit; the window procedures are replaced
 * by one event handler (see the end of the file) that receives QTH_EVENTs from the Qt layer. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

// Define some static border information

#define DRAG_TIMER_ID           128
#define TRIPLE_CLICK_TIMER_ID   129

#define MIN_SPLIT_X             5
#define MIN_SPLIT_Y             5

#define PIXEL_HORZ              1
#define PIXEL_VERT              1
#define PIXEL_HORZ2             2
#define PIXEL_VERT2             2

#define SCROLL_RANGE 0x4000 // a number with a little bit of granularity, especially for wacky monitors

typedef struct SPLIT_INFO
{
    struct SPLIT_INFO_DRAGGING
    {
        PIXIT_RECT clip_pixit_rect;
        PIXIT_RECT valid_pixit_rect;
        PIXIT_POINT previous_pixit_point;

        RECT old_horz_rect;
        RECT old_vert_rect;

        struct SPLIT_INFO_DRAGGING_FLAGS
        {
            BOOL old_horz_rect;
            BOOL old_vert_rect;
        } flags;
    } dragging;

    struct SPLIT_INFO_FLAGS
    {
        BOOL horz;
        BOOL vert;
        BOOL horz_box;
        BOOL vert_box;
    } flags;

    POINTER_SHAPE pointer_shape;
}
SPLIT_INFO;

static SPLIT_INFO split_info;

typedef struct HO_WIN_STATE
{
    /* Cached information about the world we are in */
    struct
    {
        POINT scroll;
        POINT split;
    } metrics;

    /* Drag state, defines all important stuff about drags, active != 0 then valid */
    struct
    {
        BOOL threaded;      /* possible to call host_drag_start */
        BOOL pending;       /* drag is pending, i.e. timer active */
        BOOL enabled;       /* drag is in progress */
        BOOL right_button;  /* will start on right button */

        /* Extra drag state, if threaded / pending or enabled set then this info is valid */
        DOCNO docno;
        VIEWNO viewno;
        HWND hwnd;
        POINT start;
        POINT dist;
        PIXIT_POINT start_pixit_point;
        P_ANY p_reason_data;

        EVENT_HANDLER event_handler;
        int capture_size;
    } drag;

    /* Triple click stuff, if hwnd != 0 then valid for sending event */
    struct
    {
        HWND hwnd;
        POINT start;
        POINT dist;
    } triple;

    /* State for the caret, this identifies which window without our world has the caret */
    struct
    {
        GDI_RECT gdi_rect;                      // bounding box defining the caret

        struct
        {
            HWND hwnd;                          // window that the caret lives in
            POINT dimensions;                   // visible size of caret (pixels)
            POINT point;                        // visible position (scrolled etc)
        } visible;

    } caret;

    /* State for pending redraw stuff */
    struct
    {
        BOOL in_update_fast;
        REDRAW_FLAGS redraw_flags;              // new redraw_flags
        BOOL redraw_flags_valid;
    }
    update;
}
HO_WIN_STATE;

static HO_WIN_STATE ho_win_state;

/* Extra information encoded into the window information for panes */

typedef struct HOST_EVENT_DESC
{
    BOOL scroll_x;
    BOOL scroll_y;
    BOOL set_scroll_x;
    BOOL set_scroll_y;
    BOOL is_slave;
    PANE_ID pane_id;
    int slave_id;
    int ruler_id;
    int border_id;
    int xform_index;
    P_PROC_EVENT p_proc_event;
    int offset_of_hwnd;
}
HOST_EVENT_DESC; typedef const HOST_EVENT_DESC * PC_HOST_EVENT_DESC;


/* Function list */


/* Win32-flavoured helpers that the shared window code is written in terms of,
 * done with the Qt layer so that the code itself can stay as it is */

#define SB_CTL 2

#define SM_CXDOUBLECLK  QTH_METRIC_DOUBLECLICK_X
#define SM_CYDOUBLECLK  QTH_METRIC_DOUBLECLICK_Y
#define SM_CXDRAG       QTH_METRIC_DRAG_X
#define SM_CYDRAG       QTH_METRIC_DRAG_Y

#define SM_CXBORDER 1000 /* a border is one pixel */
#define SM_CYBORDER 1001

static inline int
GetSystemMetrics(
    _InVal_     int which)
{
    return((which >= SM_CXBORDER) ? 1 : qth_metric(which));
}

/* nothing is ever left pending in a Qt window: the check that wants this is for a Windows hazard */
#define GetUpdateRect(hwnd, p_rect, erase) FALSE

#define GetDoubleClickTime() ((UINT) qth_metric(QTH_METRIC_DOUBLECLICK_MS))

#define DD_DEFDRAGDELAY 400

#define GetProfileInt(section, key, default_value) (default_value)

/* the messages that send_click_to_view() is told about; only their identity matters */
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONDBLCLK    0x0206

static inline void
SetRect(
    RECT * p_rect,
    int left,
    int top,
    int right,
    int bottom)
{
    p_rect->left = left;
    p_rect->top = top;
    p_rect->right = right;
    p_rect->bottom = bottom;
}

static inline void
InvalidateRect(
    HWND hwnd,
    const RECT * p_rect,
    BOOL erase)
{
    (void) erase;

    if(NULL == p_rect)
        qth_window_invalidate(hwnd, NULL);
    else
    {
        const QTH_RECT rect = { p_rect->left, p_rect->top, p_rect->right, p_rect->bottom };
        qth_window_invalidate(hwnd, &rect);
    }
}

static inline void
UpdateWindow(
    HWND hwnd)
{
    qth_window_update_now(hwnd);
}

static inline void
ScrollWindow(
    HWND hwnd,
    int dx,
    int dy,
    const RECT * p_rect,
    const RECT * p_clip)
{
    (void) p_rect;
    (void) p_clip;

    qth_window_scroll_contents(hwnd, dx, dy);
}

static inline void
SetScrollPos(
    HWND hwnd,
    int bar,
    int pos,
    BOOL redraw)
{
    (void) bar;
    (void) redraw;

    qth_window_set_scroll_value(hwnd, pos);
}

static inline int
GetScrollPos(
    HWND hwnd,
    int bar)
{
    (void) bar;

    return(qth_window_get_scroll_value(hwnd));
}

static inline void
ClientToScreen(
    HWND hwnd,
    POINT * p_point)
{
    int32_t x = p_point->x, y = p_point->y;

    qth_window_to_screen(hwnd, &x, &y);

    p_point->x = x;
    p_point->y = y;
}

static inline void
GetWindowRect(
    HWND hwnd,
    RECT * p_rect)
{
    QTH_RECT rect;

    qth_window_screen_rect(hwnd, &rect);

    p_rect->left = rect.left;
    p_rect->top = rect.top;
    p_rect->right = rect.right;
    p_rect->bottom = rect.bottom;
}

static inline BOOL
SetTimer(
    HWND hwnd,
    int id,
    int ms,
    void * p_proc)
{
    (void) p_proc;

    qth_window_set_timer(hwnd, id, ms);

    return(TRUE);
}

static HWND g_hwnd_capture = HOST_WND_NONE;

static inline void
SetCapture(
    HWND hwnd)
{
    g_hwnd_capture = hwnd;
    qth_window_capture_mouse(hwnd, 1);
}

static inline void
ReleaseCapture(void)
{
    if(HOST_WND_NONE != g_hwnd_capture)
    {
        qth_window_capture_mouse(g_hwnd_capture, 0);
        g_hwnd_capture = HOST_WND_NONE;
    }
}

/* the pointer's position in the client coordinates of the window */

static void
GetMessagePosAsClient(
    HWND hwnd,
    int * p_x,
    int * p_y)
{
    int32_t x, y;

    qth_window_mouse_position(hwnd, &x, &y);

    *p_x = x;
    *p_y = y;
}

#define HO_WIN_DRAG_NULL_CLIENT_HANDLE ((CLIENT_HANDLE) 0x00000004)

PROC_EVENT_PROTO(static, null_event_ho_win);

static void
show_view_pane_set(
    _ViewRef_   P_VIEW p_view);

static void
calc_pane_posn(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view);

static void
set_pane_posn(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view);

static void
move_pane(
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id,
    _InVal_     BOOL show);

static void
scroll_and_recache(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id,
    _InRef_     PC_GDI_POINT p_offset);

static void
scroll_pane(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     S32 new_scroll);

static void
scroll_pane_absolute(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InRef_     PC_GDI_POINT p_gdi_point,
    _InVal_     BOOL redraw_scroll);

_Check_return_
static STATUS
slave_window_create(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id);

static void
slave_window_destroy(
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id);

_Check_return_
static int
point_to_blip_pos(
    _InVal_     S32 pos,
    _InVal_     S32 range);

_Check_return_
static S32
point_from_blip_pos(
    _InVal_     int pos,
    _InVal_     S32 range);

static void
move_slave(
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id,
    _In_        int x,
    _In_        int y,
    _In_        int h,
    _In_        int w);

_Check_return_
static STATUS
ensure_pane_set(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view);

static void
refresh_back_window(
    _ViewRef_   P_VIEW p_view);

PROC_EVENT_PROTO(static, proc_event_back_window);

static void
get_split_info(
    _InRef_     PC_VIEW_POINT p_view_point);

static void
get_split_drag_info(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click,
    _OutRef_    P_PIXIT_POINT p_pixit_point);

static void
render_split_drag_info(
    _ViewRef_   P_VIEW p_view,
    P_PIXIT_POINT p_pixit_point);

static void
rect_from_view_rect(
    _OutRef_    PRECT p_rect,
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEW_RECT p_view_rect,
    _InRef_opt_ PC_RECT_FLAGS p_rect_flags,
    _InVal_     EVENT_HANDLER event_handler);

static void
update_pane_window(
    _HwndRef_   HWND hwnd,
    _InVal_     REDRAW_FLAGS redraw_flags);

_Check_return_
static STATUS
send_key_to_docu(
    _DocuRef_   P_DOCU p_docu,
    _In_        KMAP_CODE key_code);

static void
enable_triple_click(
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y);

static void
disable_triple_click(void);

static void
send_click_to_view(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _HwndRef_   HWND hwnd,
    _InVal_     UINT uiMsg,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed);

static void
start_drag_monitor(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     BOOL right_button);

static void
stop_drag_monitor(void);

static void
send_mouse_event(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     T5_MESSAGE t5_message,
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed,
    _InVal_     EVENT_HANDLER event_handler);

static void
maybe_scroll(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler);

_Check_return_
static STATUS
view_pane_create(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id);

static void
view_pane_destroy(
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id);

static void
view_point_from_window_point_and_context(
    _OutRef_    P_VIEW_POINT p_view_point,
    _InRef_     PC_GDI_POINT p_gdi_point,
    _InRef_     PC_CLICK_CONTEXT p_click_context);





/* This table is used in the window handling code to determine the characteristics of the event handler being used.
 * It is mainly used by the scrolling and event processing functions.
 */

static const HOST_EVENT_DESC
host_event_desc_table[/*event_handler*/] =
{
    // Scroll using X offset
    //    Scroll using Y offset
    //       Set X scroll pos
    //          Set Y scroll pos
    //             Is slave
    //               Pane ID                Slave ID                    Ruler                       Border                      XFORM index    p_event handler
    { 1, 1, 0, 0, 0, WIN_PANE_SPLIT_DIAG,   0,                          0,                          0,                          XFORM_PANE, view_event_pane_window, offsetof32(VIEW, pane[WIN_PANE_SPLIT_DIAG].hwnd)    },  /* pane window */
    { 1, 1, 0, 0, 0, WIN_PANE_SPLIT_VERT,   0,                          0,                          0,                          XFORM_PANE, view_event_pane_window, offsetof32(VIEW, pane[WIN_PANE_SPLIT_VERT].hwnd)    },  /* pane window */
    { 1, 1, 0, 0, 0, WIN_PANE_SPLIT_HORZ,   0,                          0,                          0,                          XFORM_PANE, view_event_pane_window, offsetof32(VIEW, pane[WIN_PANE_SPLIT_HORZ].hwnd)    },  /* pane window */
    { 1, 1, 0, 0, 0, WIN_PANE,              0,                          0,                          0,                          XFORM_PANE, view_event_pane_window, offsetof32(VIEW, pane[WIN_PANE].hwnd)               },  /* pane window */

    { 1, 0, 0, 0, 1, WIN_PANE_SPLIT_HORZ,   WIN_BORDER_HORZ_SPLIT,      0,                          0,                          XFORM_HORZ, view_event_border_horz, offsetof32(VIEW, edge[WIN_BORDER_HORZ_SPLIT].hwnd)  },  /* horizontal border for split*/
    { 1, 0, 0, 0, 1, WIN_PANE,              WIN_BORDER_HORZ,            0,                          0,                          XFORM_HORZ, view_event_border_horz, offsetof32(VIEW, edge[WIN_BORDER_HORZ].hwnd)        },  /* horizontal border */
    { 0, 1, 0, 0, 1, WIN_PANE_SPLIT_VERT,   WIN_BORDER_VERT_SPLIT,      0,                          0,                          XFORM_VERT, view_event_border_vert, offsetof32(VIEW, edge[WIN_BORDER_VERT_SPLIT].hwnd)  },  /* vertical border for split*/
    { 0, 1, 0, 0, 1, WIN_PANE,              WIN_BORDER_VERT,            0,                          0,                          XFORM_VERT, view_event_border_vert, offsetof32(VIEW, edge[WIN_BORDER_VERT].hwnd)        },  /* vertical border */

    { 1, 0, 0, 0, 1, WIN_PANE_SPLIT_HORZ,   WIN_RULER_HORZ_SPLIT,       0,                          0,                          XFORM_HORZ, view_event_ruler_horz,  offsetof32(VIEW, edge[WIN_RULER_HORZ_SPLIT].hwnd)   },  /* horizontal ruler for split*/
    { 1, 0, 0, 0, 1, WIN_PANE,              WIN_RULER_HORZ,             0,                          0,                          XFORM_HORZ, view_event_ruler_horz,  offsetof32(VIEW, edge[WIN_RULER_HORZ].hwnd)         },  /* horizontal ruler */
    { 0, 1, 0, 0, 1, WIN_PANE_SPLIT_VERT,   WIN_RULER_VERT_SPLIT,       0,                          0,                          XFORM_VERT, view_event_ruler_vert,  offsetof32(VIEW, edge[WIN_RULER_VERT_SPLIT].hwnd)   },  /* vertical ruler for split*/
    { 0, 1, 0, 0, 1, WIN_PANE,              WIN_RULER_VERT,             0,                          0,                          XFORM_VERT, view_event_ruler_vert,  offsetof32(VIEW, edge[WIN_RULER_VERT].hwnd)         },  /* vertical ruler */

    { 0, 0, 1, 0, 1, WIN_PANE_SPLIT_HORZ,   WIN_HSCROLL_SPLIT_HORZ,     WIN_RULER_HORZ_SPLIT,       WIN_BORDER_HORZ_SPLIT,      XFORM_BACK, NULL,                   offsetof32(VIEW, edge[WIN_HSCROLL_SPLIT_HORZ].hwnd) },  /* horizontal scroll bar for split */
    { 0, 0, 1, 0, 1, WIN_PANE,              WIN_HSCROLL,                WIN_RULER_HORZ,             WIN_BORDER_HORZ,            XFORM_BACK, NULL,                   offsetof32(VIEW, edge[WIN_HSCROLL].hwnd)            },  /* horizontal scroll bar */
    { 0, 0, 0, 1, 1, WIN_PANE_SPLIT_VERT,   WIN_VSCROLL_SPLIT_VERT,     WIN_RULER_VERT_SPLIT,       WIN_BORDER_VERT_SPLIT,      XFORM_BACK, NULL,                   offsetof32(VIEW, edge[WIN_VSCROLL_SPLIT_VERT].hwnd) },  /* vertical scroll bar for split */
    { 0, 0, 0, 1, 1, WIN_PANE,              WIN_VSCROLL,                WIN_RULER_VERT,             WIN_BORDER_VERT,            XFORM_BACK, NULL,                   offsetof32(VIEW, edge[WIN_VSCROLL].hwnd)            },  /* vertical scroll bar */

    { 0, 0, 0, 0, 0, WIN_PANE_NONE,         0,                          0,                          0,                          XFORM_BACK, proc_event_back_window, offsetof32(VIEW, main[WIN_BACK].hwnd)               },  /* back window */
    { 0, 0, 0, 0, 0, WIN_PANE_NONE,         0,                          0,                          0,                          XFORM_BACK, NULL,                   offsetof32(VIEW, main[WIN_TOOLBAR].hwnd)            },  /* toolbar */
    { 0, 0, 0, 0, 0, WIN_PANE_NONE,         0,                          0,                          0,                          XFORM_BACK, NULL,                   offsetof32(VIEW, main[WIN_SLE].hwnd)                },  /* sle */
    { 0, 0, 0, 0, 0, WIN_PANE_NONE,         0,                          0,                          0,                          XFORM_BACK, NULL,                   offsetof32(VIEW, main[WIN_UPPER_STATUS_LINE].hwnd)  },  /* upper status line */
    { 0, 0, 0, 0, 0, WIN_PANE_NONE,         0,                          0,                          0,                          XFORM_BACK, NULL,                   offsetof32(VIEW, main[WIN_LOWER_STATUS_LINE].hwnd)  }   /* lower status line */
};

/* Table defines the windows and borders that are associated with the given
 * pane.  It is indexed by the pane ID and gives the windows etc. that can
 * be updated, scroll etc..
 */

typedef struct HOST_PANE_DESC
{
    int hscroll_id; EVENT_HANDLER hscroll_event_handler;
    int vscroll_id; EVENT_HANDLER vscroll_event_handler;
    int hborder_id;
    int vborder_id;
    int hruler_id;
    int vruler_id;
}
HOST_PANE_DESC; typedef const HOST_PANE_DESC * PC_HOST_PANE_DESC;

static const HOST_PANE_DESC
host_pane_desc_table[/*pane_id*/ NUMPANE] =
{
//    Horizontal scroll         Horizontal scroll event hdlr Vertical scroll         Vertical scroll event hdlr   Horz border            Vert border            Horz ruler            Vert ruler
    { WIN_HSCROLL_SPLIT_HORZ,   EVENT_HANDLER_HSCROLL_SPLIT, WIN_VSCROLL_SPLIT_VERT, EVENT_HANDLER_VSCROLL_SPLIT, WIN_BORDER_HORZ_SPLIT, WIN_BORDER_VERT_SPLIT, WIN_RULER_HORZ_SPLIT, WIN_RULER_VERT_SPLIT },
    { WIN_HSCROLL,              EVENT_HANDLER_HSCROLL,       WIN_VSCROLL_SPLIT_VERT, EVENT_HANDLER_VSCROLL_SPLIT, WIN_BORDER_HORZ,       WIN_BORDER_VERT_SPLIT, WIN_RULER_HORZ,       WIN_RULER_VERT_SPLIT },
    { WIN_HSCROLL_SPLIT_HORZ,   EVENT_HANDLER_HSCROLL_SPLIT, WIN_VSCROLL,            EVENT_HANDLER_VSCROLL,       WIN_BORDER_HORZ_SPLIT, WIN_BORDER_VERT,       WIN_RULER_HORZ_SPLIT, WIN_RULER_VERT       },
    { WIN_HSCROLL,              EVENT_HANDLER_HSCROLL,       WIN_VSCROLL,            EVENT_HANDLER_VSCROLL,       WIN_BORDER_HORZ,       WIN_BORDER_VERT,       WIN_RULER_HORZ,       WIN_RULER_VERT       }
};

/* Mapping table that can be used to map a pane_id to an event_handler */

typedef struct INDEX_TO_EVENT_HANDLER_DESC
{
    EVENT_HANDLER event_handler;
}
INDEX_TO_EVENT_HANDLER_DESC, * P_INDEX_TO_EVENT_HANDLER_DESC;

static INDEX_TO_EVENT_HANDLER_DESC
pane_to_event_handler_table[/*pane_id*/ NUMPANE] =
{
    { EVENT_HANDLER_PANE_SPLIT_DIAG },
    { EVENT_HANDLER_PANE_SPLIT_VERT },
    { EVENT_HANDLER_PANE_SPLIT_HORZ },
    { EVENT_HANDLER_PANE }
};

/* Mapping table that can be used to map a slave_id to an event_handler */

static INDEX_TO_EVENT_HANDLER_DESC
slave_to_event_handler_table[/*slave_id*/] =
{
    { EVENT_HANDLER_BORDER_HORZ_SPLIT },
    { EVENT_HANDLER_BORDER_HORZ },
    { EVENT_HANDLER_BORDER_VERT_SPLIT },
    { EVENT_HANDLER_BORDER_VERT },

    { EVENT_HANDLER_RULER_HORZ_SPLIT },
    { EVENT_HANDLER_RULER_HORZ },
    { EVENT_HANDLER_RULER_VERT_SPLIT },
    { EVENT_HANDLER_RULER_VERT }
};


#define set_rect(p_rect, x, y, dx, dy) \
    do { (p_rect)->left = (x); (p_rect)->top = (y); (p_rect)->right = (x) + (dx); (p_rect)->bottom = (y) + (dy); } while(0)

typedef struct INTERNAL_SLAVE_ENTRY
{
    EVENT_HANDLER event_handler;
    PANE_ID pane_id;
    BOOL track_events;
    BOOL is_horz_scroll_widget;
    BOOL is_vert_scroll_widget;
}
INTERNAL_SLAVE_ENTRY;

static const INTERNAL_SLAVE_ENTRY
internal_slave_table[/*slave_id*/ /*NUMEDGE*/] =
{   /*     event handler                pane associated         track_events hsw vsw */
    { EVENT_HANDLER_BORDER_HORZ_SPLIT,  WIN_PANE_SPLIT_VERT,    1 }, /* horizontal border for with vertical split line */
    { EVENT_HANDLER_BORDER_HORZ,        WIN_PANE,               1 }, /* horizontal border */
    { EVENT_HANDLER_BORDER_VERT_SPLIT,  WIN_PANE_SPLIT_VERT,    1 }, /* vertical border for with horizontal split line */
    { EVENT_HANDLER_BORDER_VERT,        WIN_PANE,               1 }, /* vertical border */

    { EVENT_HANDLER_RULER_HORZ_SPLIT,   WIN_PANE_SPLIT_HORZ,    1 }, /* horizontal ruler for with vertical split line */
    { EVENT_HANDLER_RULER_HORZ,         WIN_PANE,               1 }, /* horizontal ruler */
    { EVENT_HANDLER_RULER_VERT_SPLIT,   WIN_PANE_SPLIT_HORZ,    1 }, /* vertical ruler for with horizontal split line */
    { EVENT_HANDLER_RULER_VERT,         WIN_PANE,               1 }, /* vertical ruler */

    { EVENT_HANDLER_HSCROLL_SPLIT,      WIN_PANE_SPLIT_HORZ,    0, 1, 0 }, /* horizontal scroll bar */
    { EVENT_HANDLER_HSCROLL,            WIN_PANE,               0, 1, 0 }, /* horizontal scroll bar for split */
    { EVENT_HANDLER_VSCROLL_SPLIT,      WIN_PANE_SPLIT_VERT,    0, 0, 1 }, /* vertical scroll bar */
    { EVENT_HANDLER_VSCROLL,            WIN_PANE,               0, 0, 1 }  /* vertical scroll bar for split */
};

/* Every Qt window reports to the one handler, which finds the view from the packed id given at creation */

static void
li_window_event(
    QTH_WINDOW * p_qth_window,
    const QTH_EVENT * p_event,
    void * client);

/* Make the Qt window for an event handler's slot in the view and record it there, as WM_CREATE does on Windows */

_Check_return_
static HOST_WND
li_window_create(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     int kind,
    _InVal_     HOST_WND parent,
    _In_        const QTH_RECT * p_bounds)
{
    QTH_WINDOW_DESC desc;
    HOST_WND hwnd;

    memset(&desc, 0, sizeof(desc));
    desc.kind = kind;
    desc.p_parent = parent;
    desc.bounds = *p_bounds;
    desc.proc = li_window_event;
    desc.client = (void *) (uintptr_t) viewid_pack(p_docu, p_view, (U16) event_handler);

    if(NULL == (hwnd = qth_window_create(&desc)))
        return(HOST_WND_NONE);

    * PtrAddBytes(HWND *, p_view, host_event_desc_table[event_handler].offset_of_hwnd) = hwnd;

    return(hwnd);
}

static void
li_window_destroy(
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    HWND * const p_hwnd = PtrAddBytes(HWND *, p_view, host_event_desc_table[event_handler].offset_of_hwnd);

    if(HOST_WND_NONE != *p_hwnd)
    {
        qth_window_destroy(*p_hwnd);
        *p_hwnd = HOST_WND_NONE;
    }
}

static void
li_metrics_init(void)
{
    if(0 == ho_win_state.metrics.scroll.x)
    {
        ho_win_state.metrics.scroll.x = qth_metric(QTH_METRIC_SCROLLBAR_WIDTH);
        ho_win_state.metrics.scroll.y = qth_metric(QTH_METRIC_SCROLLBAR_HEIGHT);
        ho_win_state.metrics.split.x = 4; /* the split grab area, in pixels */
        ho_win_state.metrics.split.y = 4;
    }
}

extern void
host_get_pixel_size(
    _HwndRef_opt_ HWND hwnd /* NULL for screen */,
    _OutRef_opt_  P_GDI_COORD p_size_cx,
    _OutRef_opt_  P_GDI_COORD p_size_cy)
{
    int32_t dpi_x, dpi_y;

    (void) hwnd;

    qth_screen_dpi(&dpi_x, &dpi_y);

    if(NULL != p_size_cx) *p_size_cx = dpi_x;
    if(NULL != p_size_cy) *p_size_cy = dpi_y;
}

extern void
host_view_query_posn(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _OutRef_    P_PIXIT_POINT p_tl,
    _OutRef_    P_PIXIT_SIZE p_size)
{
    QTH_RECT window_rect;
    GDI_RECT gdi_rect;
    PIXIT_RECT pixit_rect;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    qth_window_get_bounds(p_view->main[WIN_BACK].hwnd, &window_rect);
    gdi_rect.tl.x = window_rect.left;
    gdi_rect.tl.y = window_rect.top;
    gdi_rect.br.x = window_rect.right;
    gdi_rect.br.y = window_rect.bottom;
    pixit_rect_from_window_rect(&pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_BACK]);

    p_tl->x = pixit_rect.tl.x;
    p_tl->y = pixit_rect.tl.y;
    p_size->cx = (pixit_rect.br.x - pixit_rect.tl.x);
    p_size->cy = (pixit_rect.br.y - pixit_rect.tl.y);
}

extern void
host_set_extent_this_view(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    GDI_POINT pane_extent;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    window_point_from_pixit_point(&pane_extent, &p_view->paneextent, &p_view->host_xform[XFORM_PANE]);

    { /* Modify the extent in the pane descriptions */
    PANE_ID pane_id = NUMPANE;
    do  {
        PANE_ID_DECR(pane_id);

        p_view->pane[pane_id].extent.x = pane_extent.x;
        p_view->pane[pane_id].extent.y = pane_extent.y;

    } while(PANE_ID_START != pane_id);
    } /*block*/

    { /* Cycle over the event handlers, looking for ones which are scroll widgets */
    EVENT_HANDLER event_handler;
    for(event_handler = 0; event_handler < NUMEVENTS; ++event_handler)
    {
        const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];

        if(p_host_event_desc->set_scroll_x)
        {
            const HWND hwnd = p_view->edge[p_host_event_desc->slave_id].hwnd;
            if(HOST_WND_NONE != hwnd)
            {
                const PC_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
                qth_window_set_scroll_value(hwnd, point_to_blip_pos(p_pane->scroll.x, p_pane->extent.x - p_pane->size.x));
            }
        }

        if(p_host_event_desc->set_scroll_y)
        {
            const HWND hwnd = p_view->edge[p_host_event_desc->slave_id].hwnd;
            if(HOST_WND_NONE != hwnd)
            {
                const PC_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
                qth_window_set_scroll_value(hwnd, point_to_blip_pos(p_pane->scroll.y, p_pane->extent.y - p_pane->size.y));
            }
        }
    }
    } /*block*/
}

extern void
host_recache_visible_row_range(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    PANE_ID pane_id = NUMPANE;

    do  {
        P_PANE p_pane;

        PANE_ID_DECR(pane_id);

        p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE != p_pane->hwnd)
            skel_visible_row_range(p_docu, p_view, &p_pane->visible_skel_rect, &p_pane->visible_row_range);

    } while(PANE_ID_START != pane_id);
}

_Check_return_
static STATUS
view_pane_create(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id)
{
    STATUS status = STATUS_OK;
    EVENT_HANDLER event_handler = pane_to_event_handler_table[pane_id].event_handler;
    HWND hwnd;

    trace_1(TRACE_WINDOWS_HOST, TEXT("view_pane_create: Called to make a view pane, id = ") S32_TFMT, pane_id);

    {
    const QTH_RECT bounds = { 0, 0, 100, 100 };
    hwnd = li_window_create(p_docu, p_view, event_handler, QTH_WINDOW_PANE, p_view->main[WIN_BACK].hwnd, &bounds);
    } /*block*/
    if(HOST_WND_NONE == hwnd)
        return(create_error(ERR_TOOMANYWINDOWS));

    assert(hwnd == p_view->pane[pane_id].hwnd);

    switch(pane_id)
    {
    case WIN_PANE_SPLIT_DIAG:
        break;

    case WIN_PANE_SPLIT_HORZ:
        status = slave_window_create(p_docu, p_view, WIN_HSCROLL_SPLIT_HORZ);
        break;

    case WIN_PANE_SPLIT_VERT:
        status = slave_window_create(p_docu, p_view, WIN_VSCROLL_SPLIT_VERT);
        break;

    default: default_unhandled();
    case WIN_PANE:
        status_break(status = slave_window_create(p_docu, p_view, WIN_HSCROLL));
        status_break(status = slave_window_create(p_docu, p_view, WIN_VSCROLL));
        break;
    }

    if(status_fail(status))
        view_pane_destroy(p_view, pane_id);

    return(status);
}

static void
view_pane_destroy(
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id)
{
    P_PANE p_pane;

    trace_1(TRACE_WINDOWS_HOST, TEXT("view_pane_destroy: called to destroy view pane %d"), pane_id);

    if(!IS_PANE_ID_VALID(pane_id))
        return;

    p_pane = &p_view->pane[pane_id];

    assert(p_pane->hwnd);

    switch(pane_id)
    {
    case WIN_PANE_SPLIT_DIAG:
        break;

    case WIN_PANE_SPLIT_HORZ:
        slave_window_destroy(p_view, WIN_HSCROLL_SPLIT_HORZ);
        break;

    case WIN_PANE_SPLIT_VERT:
        slave_window_destroy(p_view, WIN_VSCROLL_SPLIT_VERT);
        break;

    default: default_unhandled();
    case WIN_PANE:
        slave_window_destroy(p_view, WIN_HSCROLL);
        slave_window_destroy(p_view, WIN_VSCROLL);
        break;
    }

    if(p_view->flags.horz_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
        slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_HORZ : WIN_BORDER_HORZ_SPLIT);
    if(p_view->flags.vert_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
        slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_VERT : WIN_BORDER_VERT_SPLIT);

    if(p_view->flags.horz_ruler_on  && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
        slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_RULER_HORZ : WIN_RULER_HORZ_SPLIT);
    if(p_view->flags.vert_ruler_on  && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
        slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_RULER_VERT : WIN_RULER_VERT_SPLIT);

    li_window_destroy(p_view, pane_to_event_handler_table[pane_id].event_handler);
}

static void
show_view_pane_set(
    _ViewRef_   P_VIEW p_view)
{
    PANE_ID pane_id;
    EDGE_ID edge_id;

    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
        if(HOST_WND_NONE != p_view->pane[pane_id].hwnd)
            qth_window_show(p_view->pane[pane_id].hwnd, 1);

    for(edge_id = EDGE_ID_START; edge_id < NUMEDGE; EDGE_ID_INCR(edge_id))
        if(HOST_WND_NONE != p_view->edge[edge_id].hwnd)
            qth_window_show(p_view->edge[edge_id].hwnd, 1);
}

static void
calc_pane_posn(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    POINT ruler = { 0, 0 };
    POINT border = { 0, 0 };
    POINT start, inner, outer, max;
    PANE_ID pane_id;

    {
    RECT client_rect;
    QTH_RECT bounds;

    li_metrics_init();

    qth_window_get_bounds(p_view->main[WIN_BACK].hwnd, &bounds);
    client_rect.left = client_rect.top = 0;
    client_rect.right = bounds.right - bounds.left;
    client_rect.bottom = bounds.bottom - bounds.top;
    p_view->outer_frame_gdi_rect.tl.x = client_rect.left;
    p_view->outer_frame_gdi_rect.tl.y = client_rect.top;
    p_view->outer_frame_gdi_rect.br.x = client_rect.right;
    p_view->outer_frame_gdi_rect.br.y = client_rect.bottom;
    } /*block*/

    {
    MSG_FRAME_WINDOW msg_frame_window;
    msg_frame_window.p_view = p_view;
    msg_frame_window.gdi_rect = p_view->outer_frame_gdi_rect;
    status_assert(object_call_all(p_docu, T5_MSG_FRAME_WINDOWS_DESCRIBE, &msg_frame_window));
    p_view->inner_frame_gdi_rect = msg_frame_window.gdi_rect;
    } /*block*/

    if(p_view->flags.horz_border_on)
        border.y = p_view->horz_border_gdi_height;
    if(p_view->flags.vert_border_on)
        border.x = p_view->vert_border_gdi_width;

    if(p_view->flags.horz_ruler_on)
        ruler.y = p_view->horz_ruler_gdi_height;
    if(p_view->flags.vert_ruler_on)
        ruler.x = p_view->vert_ruler_gdi_width;

    inner.x = inner.y = 0;

    start.x = p_view->inner_frame_gdi_rect.tl.x + (ruler.x + border.x);
    start.y = p_view->inner_frame_gdi_rect.tl.y + (ruler.y + border.y);
    outer.x = max.x = MAX(0, (p_view->inner_frame_gdi_rect.br.x - start.x) - ho_win_state.metrics.scroll.x);
    outer.y = max.y = MAX(0, (p_view->inner_frame_gdi_rect.br.y - start.y) - ho_win_state.metrics.scroll.y);

    if(p_view->flags.horz_split_on || p_view->flags.vert_split_on)
    {
        GDI_POINT split_point;
        PIXIT_POINT split_pixit_point;

        split_pixit_point.x = p_view->horz_split_pos;
        split_pixit_point.y = p_view->vert_split_pos;

        window_point_from_pixit_point(&split_point, &split_pixit_point, &p_view->host_xform[XFORM_BACK]);

        if(p_view->flags.horz_split_on)
        {
            outer.x = MAX(0, max.x - ((int) split_point.x + ho_win_state.metrics.split.x));
            inner.x = MIN(max.x, (int) split_point.x);
        }

        if(p_view->flags.vert_split_on)
        {
            outer.y = MAX(0, max.y - ((int) split_point.y + ho_win_state.metrics.split.y));
            inner.y = MIN(max.y, (int) split_point.y);
        }
    }

    p_view->pane_posn.split.x = (max.x - outer.x) + start.x;
    p_view->pane_posn.split.y = (max.y - outer.y) + start.y;

    p_view->pane_posn.grab.x = MAX(start.x, (p_view->pane_posn.split.x - (ho_win_state.metrics.split.x /* *2*/)));
    p_view->pane_posn.grab.y = MAX(start.y, (p_view->pane_posn.split.y - (ho_win_state.metrics.split.y /* *2*/)));

    p_view->pane_posn.start = start;

    p_view->pane_posn.inner_size = inner;
    p_view->pane_posn.outer_size = outer;

    p_view->pane_posn.max.x = start.x + max.x;
    p_view->pane_posn.max.y = start.y + max.y;

    p_view->pane_posn.grab_size = ho_win_state.metrics.split;

    p_view->flags.pseudo_horz_split_on = (outer.x <= 0);
    p_view->flags.pseudo_vert_split_on = (outer.y <= 0);

    if(p_view->flags.pseudo_horz_split_on)
        p_view->pane_posn.grab_size.x += PIXEL_HORZ;
    if(p_view->flags.pseudo_vert_split_on)
        p_view->pane_posn.grab_size.y += PIXEL_VERT;

    /* Compute the bounding boxes for the four pane windows */
    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        switch(pane_id)
        {
        default: default_unhandled();
        case WIN_PANE:
            set_rect(&p_pane->rect, p_view->pane_posn.split.x, p_view->pane_posn.split.y, p_view->pane_posn.outer_size.x, p_view->pane_posn.outer_size.y);

            if(!p_view->flags.horz_split_on)
                p_pane->rect.left += PIXEL_HORZ;
            if(!p_view->flags.vert_split_on)
                p_pane->rect.top += PIXEL_VERT;

            break;

        case WIN_PANE_SPLIT_DIAG:
            set_rect(&p_pane->rect, p_view->pane_posn.start.x, p_view->pane_posn.start.y, p_view->pane_posn.inner_size.x, p_view->pane_posn.inner_size.y);

            p_pane->rect.left += PIXEL_HORZ;
            p_pane->rect.top += PIXEL_VERT;
            break;

        case WIN_PANE_SPLIT_HORZ:
            set_rect(&p_pane->rect, p_view->pane_posn.start.x, p_view->pane_posn.split.y, p_view->pane_posn.inner_size.x, p_view->pane_posn.outer_size.y);

            if(!p_view->flags.vert_split_on)
                p_pane->rect.top += PIXEL_VERT;

            p_pane->rect.left += PIXEL_HORZ;
            break;

        case WIN_PANE_SPLIT_VERT:
            set_rect(&p_pane->rect, p_view->pane_posn.split.x, p_view->pane_posn.start.y, p_view->pane_posn.outer_size.x, p_view->pane_posn.inner_size.y);

            if(!p_view->flags.horz_split_on)
                 p_pane->rect.left += PIXEL_HORZ;

            p_pane->rect.top += PIXEL_VERT;
            break;
        }
    }

    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        p_pane->size.x = p_pane->rect.right  - p_pane->rect.left;
        p_pane->size.y = p_pane->rect.bottom - p_pane->rect.top;

        {
        GDI_RECT gdi_rect;
        gdi_rect.tl = p_pane->scroll;
        gdi_rect.br.x = gdi_rect.tl.x + p_pane->size.x;
        gdi_rect.br.y = gdi_rect.tl.y + p_pane->size.y;
        pixit_rect_from_window_rect(&p_pane->visible_pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_PANE]);
        } /*block*/

        view_visible_skel_rect(p_docu, p_view, &p_pane->visible_pixit_rect, &p_pane->visible_skel_rect);
        skel_visible_row_range(p_docu, p_view, &p_pane->visible_skel_rect, &p_pane->visible_row_range);
    }

    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        scroll_pane_absolute(p_docu, p_view, pane_to_event_handler_table[pane_id].event_handler, &p_pane->scroll, TRUE);
    }

    {
    VIEWEVENT_VISIBLEAREA_CHANGED event_info;

    event_info.p_view = event_info.visible_area.p_view = p_view;

    event_info.visible_area.rect.tl.x =
    event_info.visible_area.rect.tl.y = +HUGENUMBER;
    event_info.visible_area.rect.br.x =
    event_info.visible_area.rect.br.y = -HUGENUMBER;

    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        event_info.visible_area.rect.tl.x = MIN(event_info.visible_area.rect.tl.x, p_pane->visible_pixit_rect.tl.x);
        event_info.visible_area.rect.tl.y = MIN(event_info.visible_area.rect.tl.y, p_pane->visible_pixit_rect.tl.y);

        event_info.visible_area.rect.br.x = MAX(event_info.visible_area.rect.br.x, p_pane->visible_pixit_rect.br.x);
        event_info.visible_area.rect.br.y = MAX(event_info.visible_area.rect.br.y, p_pane->visible_pixit_rect.br.y);
    }

    view_event_pane_window(p_docu, T5_EVENT_VISIBLEAREA_CHANGED, &event_info);
    } /*block*/
}

static void
set_pane_posn(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    POINT border_tl, ruler_tl;
    PANE_ID pane_id;

    {
    MSG_FRAME_WINDOW msg_frame_window;
    msg_frame_window.p_view = p_view;
    msg_frame_window.gdi_rect = p_view->outer_frame_gdi_rect;
    status_assert(object_call_all(p_docu, T5_MSG_FRAME_WINDOWS_POSN, &msg_frame_window));
    } /*block*/

    ruler_tl.x = border_tl.x = p_view->inner_frame_gdi_rect.tl.x;
    ruler_tl.y = border_tl.y = p_view->inner_frame_gdi_rect.tl.y;

    if(p_view->flags.horz_ruler_on)
        border_tl.y += p_view->horz_ruler_gdi_height;
    if(p_view->flags.vert_ruler_on)
        border_tl.x += p_view->vert_ruler_gdi_width;

    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        move_pane(p_view, pane_id, p_pane->size.x && p_pane->size.y);

        if(p_view->flags.horz_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
            move_slave(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_HORZ : WIN_BORDER_HORZ_SPLIT, p_pane->rect.left, border_tl.y, (int) p_pane->size.x, p_view->horz_border_gdi_height);
        if(p_view->flags.vert_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
            move_slave(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_VERT : WIN_BORDER_VERT_SPLIT, border_tl.x, p_pane->rect.top, p_view->vert_border_gdi_width, (int) p_pane->size.y);

        if(p_view->flags.horz_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
            move_slave(p_view, (pane_id == WIN_PANE) ? WIN_RULER_HORZ : WIN_RULER_HORZ_SPLIT, p_pane->rect.left, ruler_tl.y, (int) p_pane->size.x, p_view->horz_ruler_gdi_height);
        if(p_view->flags.vert_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
            move_slave(p_view, (pane_id == WIN_PANE) ? WIN_RULER_VERT : WIN_RULER_VERT_SPLIT, ruler_tl.x, p_pane->rect.top, p_view->vert_ruler_gdi_width, (int) p_pane->size.y);
    }
}

static void
move_pane(
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id,
    _InVal_     BOOL show)
{
    const P_PANE p_pane = &p_view->pane[pane_id];
    POINT edge;

    if(HOST_WND_NONE == p_pane->hwnd)
        return;

    edge.x = p_view->pane[p_view->flags.pseudo_horz_split_on ? WIN_PANE_SPLIT_VERT : WIN_PANE].rect.right;
    edge.y = p_view->pane[p_view->flags.pseudo_vert_split_on ? WIN_PANE_SPLIT_HORZ:  WIN_PANE].rect.bottom;

    switch(pane_id)
    {
    default: default_unhandled();
#if CHECKING
    case WIN_PANE_SPLIT_DIAG:
#endif
        break;

    case WIN_PANE:
        {
        POINT start;

        start.x = p_view->pane_posn.grab.x + p_view->pane_posn.grab_size.x;
        start.y = p_view->pane_posn.grab.y + p_view->pane_posn.grab_size.y;

        move_slave(p_view, WIN_HSCROLL, start.x /*- PIXEL_HORZ*/, edge.y, (p_pane->rect.right - start.x) + PIXEL_HORZ2, ho_win_state.metrics.scroll.y);
        move_slave(p_view, WIN_VSCROLL, edge.x, start.y /*- PIXEL_VERT*/, ho_win_state.metrics.scroll.x, (p_pane->rect.bottom - start.y) + PIXEL_VERT2);

        break;
        }

    case WIN_PANE_SPLIT_HORZ:
        move_slave(p_view, WIN_HSCROLL_SPLIT_HORZ, p_view->pane_posn.start.x, edge.y, (p_view->pane_posn.grab.x - p_view->pane_posn.start.x) + PIXEL_HORZ, ho_win_state.metrics.scroll.y);
        break;

    case WIN_PANE_SPLIT_VERT:
        move_slave(p_view, WIN_VSCROLL_SPLIT_VERT, edge.x, p_view->pane_posn.start.y, ho_win_state.metrics.scroll.x, (p_view->pane_posn.grab.y - p_view->pane_posn.start.y) + PIXEL_VERT);
        break;
    }

    {
    const QTH_RECT bounds = { p_pane->rect.left, p_pane->rect.top, p_pane->rect.left + (int) p_pane->size.x, p_pane->rect.top + (int) p_pane->size.y };
    qth_window_set_bounds(p_pane->hwnd, &bounds);
    qth_window_show(p_pane->hwnd, show);
    } /*block*/
}

static void
move_slave(
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id,
    _In_        int x,
    _In_        int y,
    _In_        int w,
    _In_        int h)
{
    if(HOST_WND_NONE == p_view->edge[slave_id].hwnd)
        return;

    {
    const QTH_RECT bounds = { x, y, x + w, y + h };
    qth_window_set_bounds(p_view->edge[slave_id].hwnd, &bounds);
    } /*block*/
}

static void
scroll_and_recache(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        PANE_ID pane_id,
    _InRef_     PC_GDI_POINT p_offset)
{
    const P_PANE p_pane = &p_view->pane[pane_id];

    {
    GDI_RECT gdi_rect;
    gdi_rect.tl = p_pane->scroll;
    gdi_rect.br.x = gdi_rect.tl.x + p_pane->size.x;
    gdi_rect.br.y = gdi_rect.tl.y + p_pane->size.y;
    pixit_rect_from_window_rect(&p_pane->visible_pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_PANE]);
    } /*block*/

    view_visible_skel_rect(p_docu, p_view, &p_pane->visible_pixit_rect, &p_pane->visible_skel_rect);
    skel_visible_row_range(p_docu, p_view, &p_pane->visible_skel_rect, &p_pane->visible_row_range);

    {
    VIEWEVENT_VISIBLEAREA_CHANGED event_info;
    PANE_ID pane_id_loop;

    event_info.p_view = event_info.visible_area.p_view = p_view;

    event_info.visible_area.rect.tl.x =
    event_info.visible_area.rect.tl.y = +HUGENUMBER;
    event_info.visible_area.rect.br.x =
    event_info.visible_area.rect.br.y = -HUGENUMBER;

    for(pane_id_loop = PANE_ID_START; pane_id_loop < NUMPANE; PANE_ID_INCR(pane_id_loop))
    {
        const PC_PANE p_pane_loop = &p_view->pane[pane_id_loop];

        if(HOST_WND_NONE == p_pane_loop->hwnd)
            continue;

        event_info.visible_area.rect.tl.x = MIN(event_info.visible_area.rect.tl.x, p_pane_loop->visible_pixit_rect.tl.x);
        event_info.visible_area.rect.tl.y = MIN(event_info.visible_area.rect.tl.y, p_pane_loop->visible_pixit_rect.tl.y);
        event_info.visible_area.rect.br.x = MAX(event_info.visible_area.rect.br.x, p_pane_loop->visible_pixit_rect.br.x);
        event_info.visible_area.rect.br.y = MAX(event_info.visible_area.rect.br.y, p_pane_loop->visible_pixit_rect.br.y);
    }

    view_event_pane_window(p_docu, T5_EVENT_VISIBLEAREA_CHANGED, &event_info);
    } /*block*/

    assert(NULL != p_pane->hwnd);
    qth_window_scroll_contents(p_pane->hwnd, (int) p_offset->x, (int) p_offset->y);
    qth_window_update_now(p_pane->hwnd);
}

static void
scroll_pane(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     S32 new_scroll)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const P_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    GDI_POINT point;
    GDI_POINT offset;

    point.x = (p_host_event_desc->set_scroll_x) ? MIN(new_scroll, p_pane->extent.x - p_pane->size.x) : p_pane->scroll.x;
    point.y = (p_host_event_desc->set_scroll_y) ? MIN(new_scroll, p_pane->extent.y - p_pane->size.y) : p_pane->scroll.y;
    point.x = MAX(point.x, 0);
    point.y = MAX(point.y, 0);

    offset.x = p_pane->scroll.x - point.x;
    offset.y = p_pane->scroll.y - point.y;

    trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane: event_handler ") U32_TFMT TEXT(", new scroll posn ") S32_TFMT, (U32) event_handler, new_scroll);
    trace_1(TRACE_WINDOWS_HOST, TEXT("scroll_pane: event handler yields slave id %d"), p_host_event_desc->slave_id);
    trace_1(TRACE_WINDOWS_HOST, TEXT("scroll_pane: event handler yields HWND ") HOST_WND_XTFMT, p_view->edge[p_host_event_desc->slave_id].hwnd);

    if(offset.x || offset.y)
    {
        if(p_host_event_desc->set_scroll_x)
            p_pane->scroll.x = p_view->pane[p_host_event_desc->pane_id ^ 2].scroll.x = point.x;
        if(p_host_event_desc->set_scroll_y)
            p_pane->scroll.y = p_view->pane[p_host_event_desc->pane_id ^ 1].scroll.y = point.y;

        scroll_and_recache(p_docu, p_view, p_host_event_desc->pane_id, &offset);

        if(p_host_event_desc->set_scroll_x)
        {
            if(HOST_WND_NONE != p_view->pane[p_host_event_desc->pane_id ^ 2].hwnd)
                scroll_and_recache(p_docu, p_view, (PANE_ID) (p_host_event_desc->pane_id ^ 2), &offset);

            if(p_view->flags.horz_border_on)
            {
                ScrollWindow(p_view->edge[p_host_event_desc->border_id].hwnd, (int) offset.x, 0, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_event_desc->border_id].hwnd);
            }

            if(p_view->flags.horz_ruler_on)
            {
                ScrollWindow(p_view->edge[p_host_event_desc->ruler_id].hwnd, (int) offset.x, 0, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_event_desc->ruler_id].hwnd);
            }
        }

        if(p_host_event_desc->set_scroll_y)
        {
            if(HOST_WND_NONE != p_view->pane[p_host_event_desc->pane_id ^ 1].hwnd)
                scroll_and_recache(p_docu, p_view, (PANE_ID) (p_host_event_desc->pane_id ^ 1), &offset);

            if(p_view->flags.vert_border_on)
            {
                ScrollWindow(p_view->edge[p_host_event_desc->border_id].hwnd, 0, (int) offset.y, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_event_desc->border_id].hwnd);
            }

            if(p_view->flags.vert_ruler_on)
            {
                ScrollWindow(p_view->edge[p_host_event_desc->ruler_id].hwnd, 0, (int) offset.y, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_event_desc->ruler_id].hwnd);
            }
        }

        SetScrollPos(p_view->edge[p_host_event_desc->slave_id].hwnd,
                     SB_CTL,
                     point_to_blip_pos(p_host_event_desc->set_scroll_x ? p_pane->scroll.x : p_pane->scroll.y,
                                       p_host_event_desc->set_scroll_x ? p_pane->extent.x - p_pane->size.x : p_pane->extent.y - p_pane->size.y),
                     TRUE);
    }
}

static void
scroll_pane_absolute(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InRef_     PC_GDI_POINT p_gdi_point,
    _InVal_     BOOL repaint_scroll_bars)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const PC_HOST_PANE_DESC p_host_pane_desc = &host_pane_desc_table[p_host_event_desc->pane_id];
    const P_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    GDI_POINT scroll = *p_gdi_point;
    GDI_POINT offset;

    trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: scroll x = ") S32_TFMT TEXT(", y = ") S32_TFMT, scroll.x, scroll.y);
    trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: top value x = ") S32_TFMT TEXT(", y = ") S32_TFMT, p_pane->extent.x - p_pane->size.x, p_pane->extent.y - p_pane->size.y);

    scroll.x = MIN(scroll.x, p_pane->extent.x - p_pane->size.x);
    scroll.x = MAX(scroll.x, 0);
    scroll.y = MIN(scroll.y, p_pane->extent.y - p_pane->size.y);
    scroll.y = MAX(scroll.y, 0);

    trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: scroll position x = ") S32_TFMT TEXT(", y = ") S32_TFMT, scroll.x, scroll.y);

    offset.x = p_pane->scroll.x - scroll.x;
    offset.y = p_pane->scroll.y - scroll.y;

    trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: offset x = ") S32_TFMT TEXT(", y = ") S32_TFMT, offset.x, offset.y);

    if(offset.x || offset.y)
    {
        p_pane->scroll.x = p_view->pane[p_host_event_desc->pane_id ^ 2].scroll.x = scroll.x;
        p_pane->scroll.y = p_view->pane[p_host_event_desc->pane_id ^ 1].scroll.y = scroll.y;

        trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: new x positions: ") S32_TFMT TEXT(" ") S32_TFMT, p_pane->scroll.x, p_view->pane[p_host_event_desc->pane_id ^ 2].scroll.x);
        trace_2(TRACE_WINDOWS_HOST, TEXT("scroll_pane_absolute: new y positions: ") S32_TFMT TEXT(" ") S32_TFMT, p_pane->scroll.y, p_view->pane[p_host_event_desc->pane_id ^ 1].scroll.y);

        scroll_and_recache(p_docu, p_view, p_host_event_desc->pane_id, &offset);

        if(offset.x)
        {
            GDI_POINT movement;
            int pos;

            movement.x = offset.x;
            movement.y = 0;

            if(HOST_WND_NONE != p_view->pane[p_host_event_desc->pane_id ^ 2].hwnd)
                scroll_and_recache(p_docu, p_view, (PANE_ID) (p_host_event_desc->pane_id ^ 2), &movement);

            if(p_view->flags.horz_border_on)
            {
                ScrollWindow(p_view->edge[p_host_pane_desc->hborder_id].hwnd, (int) movement.x, 0, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_pane_desc->hborder_id].hwnd);
            }

            if(p_view->flags.horz_ruler_on)
            {
                ScrollWindow(p_view->edge[p_host_pane_desc->hruler_id].hwnd, (int) movement.x, 0, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_pane_desc->hruler_id].hwnd);
            }

            pos = point_to_blip_pos(p_gdi_point->x, p_pane->extent.x - p_pane->size.x);

            SetScrollPos(p_view->edge[p_host_pane_desc->hscroll_id].hwnd, SB_CTL, pos, repaint_scroll_bars);
        }

        if(offset.y)
        {
            GDI_POINT movement;
            int pos;

            movement.x = 0;
            movement.y = offset.y;

            if(HOST_WND_NONE != p_view->pane[p_host_event_desc->pane_id ^ 1].hwnd)
                scroll_and_recache(p_docu, p_view, (PANE_ID) (p_host_event_desc->pane_id ^ 1), &movement);

            if(p_view->flags.vert_border_on)
            {
                ScrollWindow(p_view->edge[p_host_pane_desc->vborder_id].hwnd, 0, (int) movement.y, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_pane_desc->vborder_id].hwnd);
            }

            if(p_view->flags.vert_ruler_on)
            {
                ScrollWindow(p_view->edge[p_host_pane_desc->vruler_id].hwnd, 0, (int) movement.y, NULL, NULL);
                UpdateWindow(p_view->edge[p_host_pane_desc->vruler_id].hwnd);
            }

            pos = point_to_blip_pos(p_gdi_point->y, p_pane->extent.y - p_pane->size.y);

            SetScrollPos(p_view->edge[p_host_pane_desc->vscroll_id].hwnd, SB_CTL, pos, repaint_scroll_bars);
        }
    }
}

_Check_return_
static int
point_to_blip_pos(
    _InVal_     S32 point,
    _InVal_     S32 range)
{
    int pos;

    trace_2(TRACE_WINDOWS_HOST, TEXT("point_to_blip_pos: point ") S32_TFMT TEXT(", range ") S32_TFMT, point, range);

    if(range)
    {
        pos = (int) muldiv64(point, SCROLL_RANGE, range);
        pos = MIN(pos, SCROLL_RANGE);
        pos = MAX(pos, 0);
    }
    else
        pos = 0;

    trace_1(TRACE_WINDOWS_HOST, TEXT("point_to_blip_pos: result %d"), pos);
    return(pos);
}

_Check_return_
static S32
point_from_blip_pos(
    _InVal_     int pos,
    _InVal_     S32 range)
{
    S32 point;

    trace_2(TRACE_WINDOWS_HOST, TEXT("point_from_blip_pos: pos %d, range ") S32_TFMT, pos, range);

    if(range)
    {
        point = muldiv64(range, pos, SCROLL_RANGE);
        point = MIN(point, range);
        point = MAX(point, 0);
    }
    else
        point = 0;

    trace_1(TRACE_WINDOWS_HOST, TEXT("point_from_blip_pos: result ") S32_TFMT, point);
    return(point);
}

extern void
host_scroll_pane(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     T5_MESSAGE t5_message,
    P_VIEW_POINT p_view_point)
{
    const P_PANE p_pane = &p_view->pane[p_view->cur_pane];
    GDI_POINT scroll = p_pane->scroll;
    S32 offset;
    GDI_POINT gdi_point;
    PIXIT_POINT pixit_point;

    DOCU_ASSERT(p_docu);
    VIEW_ASSERT(p_view);
    assert((t5_message == T5_CMD_PAGE_DOWN) || (t5_message == T5_CMD_PAGE_UP) || (t5_message == T5_CMD_SHIFT_PAGE_DOWN) || (t5_message == T5_CMD_SHIFT_PAGE_UP));

    if(HOST_WND_NONE == p_pane->hwnd)
        return;

    offset = ((t5_message == T5_CMD_PAGE_DOWN) || (t5_message == T5_CMD_SHIFT_PAGE_DOWN)) ? p_pane->size.y : -p_pane->size.y;

    scroll.y += offset;
    scroll.y = MAX(scroll.y, 0);

    scroll_pane_absolute(p_docu, p_view, pane_to_event_handler_table[p_view->cur_pane].event_handler, &scroll, TRUE);

    gdi_point.x = 0;
    gdi_point.y = offset;
    pixit_point_from_window_point(&pixit_point, &gdi_point, &p_view->host_xform[XFORM_PANE]);
    p_view_point->pixit_point.x += pixit_point.x;
    p_view_point->pixit_point.y += pixit_point.y;
    p_view_point->pixit_point.x = MAX(0, p_view_point->pixit_point.x);
    p_view_point->pixit_point.y = MAX(0, p_view_point->pixit_point.y);
}

_Check_return_
static S32
host_HScroll_get_new_scroll(
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InRef_     P_PANE p_pane)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const HWND hwnd = p_view->edge[p_host_event_desc->slave_id].hwnd;
    return(point_from_blip_pos(GetScrollPos(hwnd, SB_CTL), p_pane->extent.x - p_pane->size.x));
}

_Check_return_
static S32
host_VScroll_get_new_scroll(
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InRef_     P_PANE p_pane)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const HWND hwnd = p_view->edge[p_host_event_desc->slave_id].hwnd;
    return(point_from_blip_pos(GetScrollPos(hwnd, SB_CTL), p_pane->extent.y - p_pane->size.y));
}

static void
refresh_back_window(
    _ViewRef_   P_VIEW p_view)
{
    const HWND hwnd = p_view->main[WIN_BACK].hwnd;
    POINT split_inner, split_outer;
    RECT rect;

    split_inner.x = (p_view->flags.vert_split_on && (!p_view->flags.pseudo_horz_split_on)) ? p_view->inner_frame_gdi_rect.tl.x : p_view->pane_posn.max.x;
    split_inner.y = (p_view->flags.horz_split_on && (!p_view->flags.pseudo_vert_split_on)) ? p_view->inner_frame_gdi_rect.tl.y : p_view->pane_posn.max.y;

    split_outer.x = p_view->pane_posn.grab.x + p_view->pane_posn.grab_size.x;
    split_outer.y = p_view->pane_posn.grab.y + p_view->pane_posn.grab_size.y;

    SetRect(&rect, p_view->inner_frame_gdi_rect.tl.x, p_view->inner_frame_gdi_rect.tl.y, p_view->pane_posn.start.x, p_view->pane_posn.start.y);
    InvalidateRect(hwnd, &rect, FALSE);
    SetRect(&rect, p_view->pane_posn.max.x, p_view->inner_frame_gdi_rect.tl.y, p_view->inner_frame_gdi_rect.br.x, p_view->pane_posn.start.y);
    InvalidateRect(hwnd, &rect, FALSE);
    SetRect(&rect, p_view->pane_posn.max.x, p_view->pane_posn.max.y, p_view->inner_frame_gdi_rect.br.x, p_view->inner_frame_gdi_rect.br.y);
    InvalidateRect(hwnd, &rect, FALSE);
    SetRect(&rect, p_view->inner_frame_gdi_rect.tl.x, p_view->pane_posn.max.y, p_view->pane_posn.start.x, p_view->inner_frame_gdi_rect.br.y);
    InvalidateRect(hwnd, &rect, FALSE);

    SetRect(&rect, p_view->pane_posn.grab.x, split_inner.y, split_outer.x, p_view->inner_frame_gdi_rect.br.y);
    InvalidateRect(hwnd, &rect, FALSE);
    SetRect(&rect, split_inner.x, p_view->pane_posn.grab.y, p_view->inner_frame_gdi_rect.br.x, split_outer.y);
    InvalidateRect(hwnd, &rect, FALSE);

    SetRect(&rect, p_view->pane_posn.start.x, p_view->pane_posn.start.y, p_view->pane_posn.max.x, p_view->pane_posn.start.y + PIXEL_VERT);
    InvalidateRect(hwnd, &rect, FALSE);
    SetRect(&rect, p_view->pane_posn.start.x, p_view->pane_posn.start.y, p_view->pane_posn.start.x + PIXEL_HORZ, p_view->pane_posn.max.y);
    InvalidateRect(hwnd, &rect, FALSE);
}

static void
update_pane_window(
    _HwndRef_   HWND hwnd,
    _InVal_     REDRAW_FLAGS redraw_flags)
{
    // update using a special set of flags defined by our fine selves
    ho_win_state.update.redraw_flags_valid = TRUE;
    ho_win_state.update.redraw_flags = redraw_flags;
    UpdateWindow(hwnd);
    ho_win_state.update.redraw_flags_valid = FALSE;
}

_Check_return_
static STATUS
ensure_pane_set(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    {
    PANE_ID pane_id = WIN_PANE;
    const HWND hwnd = p_view->pane[pane_id].hwnd;
    if(HOST_WND_NONE == hwnd)
        status_return(view_pane_create(p_docu, p_view, pane_id));
    } /*block*/

    { /* Ensure pane set for horizontally split views */
    PANE_ID pane_id = WIN_PANE_SPLIT_HORZ;
    const HWND hwnd = p_view->pane[pane_id].hwnd;
    if(p_view->flags.horz_split_on)
    {
        if(HOST_WND_NONE == hwnd)
            status_return(view_pane_create(p_docu, p_view, pane_id));
    }
    else if(HOST_WND_NONE != hwnd)
        view_pane_destroy(p_view, pane_id);
    } /*block*/

    { /* Ensure pane window for vertically split views */
    PANE_ID pane_id = WIN_PANE_SPLIT_VERT;
    const HWND hwnd = p_view->pane[pane_id].hwnd;
    if(p_view->flags.vert_split_on)
    {
        if(HOST_WND_NONE == hwnd)
            status_return(view_pane_create(p_docu, p_view, pane_id));
    }
    else if(HOST_WND_NONE != hwnd)
        view_pane_destroy(p_view, pane_id);
    } /*block*/

    { /* If both vertically and horizontally split then make the extra window */
    PANE_ID pane_id = WIN_PANE_SPLIT_DIAG;
    const HWND hwnd = p_view->pane[pane_id].hwnd;
    if(p_view->flags.horz_split_on && p_view->flags.vert_split_on)
    {
        if(HOST_WND_NONE == hwnd)
            status_return(view_pane_create(p_docu, p_view, pane_id));
    }
    else if(HOST_WND_NONE != hwnd)
        view_pane_destroy(p_view, pane_id);
    } /*block*/

    { /* Now make the slave windows for rulers and borders */
    PANE_ID pane_id = NUMPANE;

    do  {
        PANE_ID_DECR(pane_id);

        if(HOST_WND_NONE != p_view->pane[pane_id].hwnd)
        {
            /* Loop through destroying the rulers and borders as required */
            if(!p_view->flags.horz_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
                slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_HORZ : WIN_BORDER_HORZ_SPLIT);
            if(!p_view->flags.vert_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
                slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_BORDER_VERT : WIN_BORDER_VERT_SPLIT);

            if(!p_view->flags.horz_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
                slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_RULER_HORZ : WIN_RULER_HORZ_SPLIT);
            if(!p_view->flags.vert_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
                slave_window_destroy(p_view, (pane_id == WIN_PANE) ? WIN_RULER_VERT : WIN_RULER_VERT_SPLIT);

            /* Loop through ensuring that rulers etc. have been created */
            if(p_view->flags.horz_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
                status_return(slave_window_create(p_docu, p_view, (pane_id == WIN_PANE) ? WIN_BORDER_HORZ : WIN_BORDER_HORZ_SPLIT));
            if(p_view->flags.vert_border_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
                status_return(slave_window_create(p_docu, p_view, (pane_id == WIN_PANE) ? WIN_BORDER_VERT : WIN_BORDER_VERT_SPLIT));

            if(p_view->flags.horz_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
                status_return(slave_window_create(p_docu, p_view, (pane_id == WIN_PANE) ? WIN_RULER_HORZ : WIN_RULER_HORZ_SPLIT));
            if(p_view->flags.vert_ruler_on && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
                status_return(slave_window_create(p_docu, p_view, (pane_id == WIN_PANE) ? WIN_RULER_VERT : WIN_RULER_VERT_SPLIT));
        }

    } while(PANE_ID_START != pane_id);
    } /*block*/

    return(STATUS_OK);
}

_Check_return_
static BOOL
preprocess_key_event_ESCAPE(
    _DocuRef_   P_DOCU p_docu_for_key)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu_for_key);

    host_key_buffer_flush();

    /* If dragging then terminate the drag, otherwise we can pass the event down! */
    if(ho_win_state.drag.enabled)
    {
        const P_DOCU p_docu = p_docu_from_docno(ho_win_state.drag.docno);
        const P_VIEW p_view = p_view_from_viewno(p_docu, ho_win_state.drag.viewno);
        GDI_POINT point;

        GetMessagePosAsClient(ho_win_state.drag.hwnd, &point.x, &point.y);

        ReleaseCapture();

        trace_2(TRACE__NULL, TEXT("send_key_to_docu(docno=") DOCNO_TFMT TEXT(") - terminating drag monitor - *** null_events_stop(docno=") DOCNO_TFMT TEXT(")"), docno_from_p_docu(p_docu_for_key), ho_win_state.drag.docno);
        null_events_stop(ho_win_state.drag.docno, T5_MSG_WIN_HOST_NULL, null_event_ho_win, HO_WIN_DRAG_NULL_CLIENT_HANDLE);

        /* inform the current owner of the pending doom */
        trace_2(TRACE_WINDOWS_HOST, TEXT("send_key_to_docu(docno=") DOCNO_TFMT TEXT("): *** ABORTING DRAG in docno=") DOCNO_TFMT TEXT(" ***"), docno_from_p_docu(p_docu_for_key), ho_win_state.drag.docno);
        send_mouse_event(p_docu, p_view, T5_EVENT_CLICK_DRAG_ABORTED, ho_win_state.drag.hwnd, point.x, point.y, FALSE, FALSE, ho_win_state.drag.event_handler);
        ho_win_state.drag.enabled = FALSE;

        return(1 /*processed*/);
    }

    return(FALSE);
}

_Check_return_
static STATUS
send_key_to_docu(
    _DocuRef_   P_DOCU p_docu,
    _In_        KMAP_CODE key_code)
{
    ARRAY_HANDLE h_commands;
    T5_MESSAGE t5_message;
    BOOL fn_key = FALSE;

    if((h_commands = command_array_handle_from_key_code(p_docu, key_code, &t5_message)) == 0)
    {
        if(t5_message == T5_CMD_ESCAPE)
            if(preprocess_key_event_ESCAPE(p_docu))
                return(1 /*processed*/);

        fn_key = (T5_EVENT_NONE != t5_message);

        if(!fn_key)
        {
            if((key_code < 0x20) || (key_code > 0xFF))
            {
                trace_1(TRACE_WINDOWS_HOST, TEXT("key kmap=") U32_XTFMT TEXT(" unprocessed"), (U32) key_code);
                return(0 /*unprocessed*/);
            }

            docu_modify(p_docu);
        }
    }
    else
        fn_key = TRUE;

    status_assert(host_key_cache_event(docno_from_p_docu(p_docu), key_code, fn_key, 1));

    return(1 /*processed*/);
}

static void
enable_triple_click(
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y)
{
    if(HOST_WND_NONE != ho_win_state.triple.hwnd)
        return;

    (void) SetTimer(hwnd, TRIPLE_CLICK_TIMER_ID, GetDoubleClickTime(), NULL);
    ho_win_state.triple.hwnd = hwnd;
    ho_win_state.triple.start.x = x;
    ho_win_state.triple.start.y = y;
    ho_win_state.triple.dist.x = GetSystemMetrics(SM_CXDOUBLECLK);
    ho_win_state.triple.dist.y = GetSystemMetrics(SM_CYDOUBLECLK);
}

static void
send_click_to_view(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _HwndRef_   HWND hwnd,
    _InVal_     UINT uiMsg,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed)
{
    const BOOL emulate_right_button = FALSE; /* ctrl_pressed || shift_pressed; */ /* handled on case-by-case basis now */
    T5_MESSAGE t5_message;

    switch(uiMsg)
    {
    default:
        t5_message = T5_EVENT_NONE;
        break;

    case WM_LBUTTONDOWN:
        if(HOST_WND_NONE != ho_win_state.triple.hwnd)
        {
            /* Only generate triple click within same range as double */
            int dx = abs(ho_win_state.triple.start.x - x);
            int dy = abs(ho_win_state.triple.start.y - y);
            if((dx <= ho_win_state.triple.dist.x) && (dy <= ho_win_state.triple.dist.y))
            {
                t5_message = (emulate_right_button) ? T5_EVENT_CLICK_RIGHT_TRIPLE : T5_EVENT_CLICK_LEFT_TRIPLE;
                start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, emulate_right_button);
                break;
            }
        }

        t5_message = (emulate_right_button) ? T5_EVENT_CLICK_RIGHT_SINGLE : T5_EVENT_CLICK_LEFT_SINGLE;
        start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, emulate_right_button);
        break;

    case WM_RBUTTONDOWN:
        if(HOST_WND_NONE != ho_win_state.triple.hwnd)
        {
            /* Only generate triple click within same range as double */
            int dx = abs(ho_win_state.triple.start.x - x);
            int dy = abs(ho_win_state.triple.start.y - y);
            if((dx <= ho_win_state.triple.dist.x) && (dy <= ho_win_state.triple.dist.y))
            {
                t5_message = T5_EVENT_CLICK_RIGHT_TRIPLE;
                start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, TRUE);
                break;
            }
        }

        t5_message = T5_EVENT_CLICK_RIGHT_SINGLE;
        start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, TRUE);
        break;

    case WM_LBUTTONDBLCLK:
        t5_message = (emulate_right_button) ? T5_EVENT_CLICK_RIGHT_DOUBLE : T5_EVENT_CLICK_LEFT_DOUBLE;
        start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, emulate_right_button);
        break;

    case WM_RBUTTONDBLCLK:
        t5_message = T5_EVENT_CLICK_RIGHT_DOUBLE;
        start_drag_monitor(p_docu, p_view, hwnd, x, y, event_handler, TRUE);
        break;
    }

    send_mouse_event(p_docu, p_view, t5_message, hwnd, x, y, ctrl_pressed, shift_pressed, event_handler);
}

static void
send_mouse_event(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     T5_MESSAGE t5_message,
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    GDI_POINT gdi_point;
    VIEWEVENT_CLICK viewevent_click;
    zero_struct(viewevent_click);

    if((T5_EVENT_NONE == t5_message) || (NULL == p_host_event_desc->p_proc_event))
        return;

    gdi_point.x = x;
    gdi_point.y = y;

    /* setup the click context information and mouse point */
    viewevent_click.click_context.hwnd = hwnd;
    viewevent_click.click_context.ctrl_pressed = ctrl_pressed;
    viewevent_click.click_context.shift_pressed = shift_pressed;
    viewevent_click.click_context.gdi_org.x = 0; /* window-relative */
    viewevent_click.click_context.gdi_org.y = 0;
    {
    const P_PANE p_pane = (p_host_event_desc->pane_id >= 0) ? &p_view->pane[p_host_event_desc->pane_id] : NULL;
    if(p_pane && p_host_event_desc->scroll_x) viewevent_click.click_context.gdi_org.x = p_pane->scroll.x;
    if(p_pane && p_host_event_desc->scroll_y) viewevent_click.click_context.gdi_org.y = p_pane->scroll.y;
    } /*block*/
    host_set_click_context(p_docu, p_view, &viewevent_click.click_context, &p_view->host_xform[p_host_event_desc->xform_index]);

    host_modify_click_point(&gdi_point);

    view_point_from_window_point_and_context(&viewevent_click.view_point, &gdi_point, &viewevent_click.click_context);

    /* compute the extra drag information, including the delta x/y */
    viewevent_click.data.drag.p_reason_data = ho_win_state.drag.p_reason_data;
    viewevent_click.data.drag.pixit_delta.x = viewevent_click.view_point.pixit_point.x - ho_win_state.drag.start_pixit_point.x;
    viewevent_click.data.drag.pixit_delta.y = viewevent_click.view_point.pixit_point.y - ho_win_state.drag.start_pixit_point.y;

    hard_assert(TRUE);

    /* pass the event down to the event handler */
    (* p_host_event_desc->p_proc_event) (p_docu, t5_message, &viewevent_click);

    hard_assert(FALSE);

    /* start a triple click timer if double just broadcast */
    if((T5_EVENT_CLICK_LEFT_DOUBLE == t5_message) || (T5_EVENT_CLICK_RIGHT_DOUBLE == t5_message))
        enable_triple_click(hwnd, x, y);
    else if(T5_EVENT_POINTER_MOVEMENT != t5_message)
        disable_triple_click();

    /* if its a drag starting then tell the client about it */
    if(((T5_EVENT_CLICK_LEFT_DRAG == t5_message) || (T5_EVENT_CLICK_RIGHT_DRAG == t5_message)) && (ho_win_state.drag.enabled))
    {
        ho_win_state.drag.start_pixit_point = viewevent_click.view_point.pixit_point;
        send_mouse_event(p_docu, p_view, T5_EVENT_CLICK_DRAG_STARTED, hwnd, x, y, ctrl_pressed, shift_pressed, event_handler);
    }
}

static void
start_drag_monitor(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     BOOL right_button)
{
    DOCU_ASSERT(p_docu);
    VIEW_ASSERT(p_view);
    assert(hwnd);

    ho_win_state.drag.right_button = right_button;
    ho_win_state.drag.docno = docno_from_p_docu(p_docu);
    ho_win_state.drag.viewno = viewno_from_p_view(p_view);
    ho_win_state.drag.hwnd = hwnd;
    ho_win_state.drag.start.x = x;
    ho_win_state.drag.start.y = y;
    ho_win_state.drag.dist.x = GetSystemMetrics(SM_CXDRAG);
    ho_win_state.drag.dist.y = GetSystemMetrics(SM_CYDRAG);
    ho_win_state.drag.event_handler = event_handler;

    if(SetTimer(ho_win_state.drag.hwnd, DRAG_TIMER_ID, GetProfileInt(TEXT("windows"), TEXT("DragDelay"), DD_DEFDRAGDELAY), NULL))
    {
        SetCapture(ho_win_state.drag.hwnd);
        ho_win_state.drag.pending = TRUE;
    }
}

static void
maybe_scroll(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const PC_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    const HWND hwnd = p_host_event_desc->is_slave ? p_view->edge[p_host_event_desc->slave_id].hwnd : p_pane->hwnd;
    POINT point;
    RECT window_rect;
    POINT offset;

    if(!p_host_event_desc->scroll_x && !p_host_event_desc->scroll_y)
        return;

    point.x = x;
    point.y = y;

    ClientToScreen(hwnd, &point);
    GetWindowRect(hwnd, &window_rect);

    trace_2(TRACE_WINDOWS_HOST, TEXT("post_movement_message: screen point %d,%d"), point.x, point.y);
    trace_4(TRACE_WINDOWS_HOST, TEXT("post_movement_message: bounding rectangle tl == %d,%d; br == %d,%d"), window_rect.left, window_rect.top, window_rect.right, window_rect.bottom);

    offset.x = offset.y = 0;

    if(point.x < window_rect.left)
        offset.x = point.x - window_rect.left;
    if(point.x > window_rect.right)
        offset.x = point.x - window_rect.right;
    if(point.y < window_rect.top)
        offset.y = point.y - window_rect.top;
    if(point.y > window_rect.bottom)
        offset.y = point.y - window_rect.bottom;

    trace_2(TRACE_WINDOWS_HOST, TEXT("post_movement_message: offset after calculations %d,%d"), offset.x, offset.y);

    if(offset.x || offset.y)
    {
        GDI_POINT scroll = p_pane->scroll;

        trace_2(TRACE_WINDOWS_HOST, TEXT("post_movement_message: (before modify) scroll x == ") S32_TFMT TEXT(", y == ") S32_TFMT, scroll.x, scroll.y);

        if(p_host_event_desc->scroll_x)
            scroll.x += offset.x;
        if(p_host_event_desc->scroll_y)
            scroll.y += offset.y;

        trace_2(TRACE_WINDOWS_HOST, TEXT("post_movement_message: (after modify) scroll x == ") S32_TFMT TEXT(", y == ") S32_TFMT, scroll.x, scroll.y);

        scroll.x = MIN(scroll.x, p_pane->extent.x - p_pane->size.x); /* SKS notes that if extent is smaller than size then we tried to -ve scroll */
        scroll.x = MAX(scroll.x, 0);
        scroll.y = MIN(scroll.y, p_pane->extent.y - p_pane->size.y);
        scroll.y = MAX(scroll.y, 0);

        scroll_pane_absolute(p_docu, p_view, event_handler, &scroll, TRUE);
    }
}

static void
view_point_from_window_point_and_context(
    _OutRef_    P_VIEW_POINT p_view_point,
    _InRef_     PC_GDI_POINT p_gdi_point,
    _InRef_     PC_CLICK_CONTEXT p_click_context)
{
    GDI_POINT gdi_point = *p_gdi_point;

    gdi_point.x += p_click_context->gdi_org.x;
    gdi_point.y += p_click_context->gdi_org.y;

    VIEW_ASSERT(p_click_context->p_view);
    p_view_point->p_view = p_click_context->p_view;
    pixit_point_from_window_point(&p_view_point->pixit_point, &gdi_point, &p_click_context->host_xform);
}

static void
rect_from_view_rect(
    _OutRef_    PRECT p_rect,
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEW_RECT p_view_rect,
    _InRef_opt_ PC_RECT_FLAGS p_rect_flags,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const PC_PANE p_pane = &p_view_rect->p_view->pane[p_host_event_desc->pane_id];
    GDI_RECT gdi_rect;
    PC_PIXIT_RECT p_pixit_rect = &p_view_rect->rect;
    PIXIT_RECT t_pixit_rect;

    trace_4(TRACE_WINDOWS_HOST, TEXT("rect_from_view_rect: (pixit rectangle) tl = ") S32_TFMT TEXT(",") S32_TFMT TEXT("; br = ") S32_TFMT TEXT(",") S32_TFMT,
                                p_pixit_rect->tl.x, p_pixit_rect->tl.y, p_pixit_rect->br.x, p_pixit_rect->br.y);

    if(NULL != p_rect_flags)
    {
        RECT_FLAGS rect_flags = *p_rect_flags;

        t_pixit_rect = * p_pixit_rect;

      /*t_pixit_rect.tl.x -= rect_flags.extend_left_currently_unused     * p_docu->page_def.grid_size;*/
        if(rect_flags.reduce_left_by_2)
            t_pixit_rect.tl.x += 2 * p_docu->page_def.grid_size;

      /*t_pixit_rect.tl.y -= rect_flags.extend_up_currently_unused       * p_docu->page_def.grid_size;*/
        if(rect_flags.reduce_up_by_2)
            t_pixit_rect.tl.y += 2 * p_docu->page_def.grid_size;

        if(rect_flags.extend_right_by_1)
            t_pixit_rect.br.x += p_docu->page_def.grid_size;
        if(rect_flags.reduce_right_by_1)
            t_pixit_rect.br.x -= p_docu->page_def.grid_size;

        if(rect_flags.extend_down_by_1)
            t_pixit_rect.br.y += p_docu->page_def.grid_size;
        if(rect_flags.reduce_down_by_1)
            t_pixit_rect.br.y -= p_docu->page_def.grid_size;

        p_pixit_rect = &t_pixit_rect;
    }

    trace_4(TRACE_WINDOWS_HOST, TEXT("rect_from_view_rect: (pixit - after adjust) tl = ") S32_TFMT TEXT(",") S32_TFMT TEXT("; br = ") S32_TFMT TEXT(",") S32_TFMT,
        p_pixit_rect->tl.x, p_pixit_rect->tl.y, p_pixit_rect->br.x, p_pixit_rect->br.y);

    status_consume(window_rect_from_pixit_rect(&gdi_rect, p_pixit_rect, &p_view_rect->p_view->host_xform[p_host_event_desc->xform_index]));

    trace_4(TRACE_WINDOWS_HOST, TEXT("rect_from_view-rect: (converted to window units) tl = ") S32_TFMT TEXT(",") S32_TFMT TEXT("; br = ") S32_TFMT TEXT(",") S32_TFMT,
            gdi_rect.tl.x, gdi_rect.tl.y, gdi_rect.br.x, gdi_rect.br.y);

    if(NULL != p_rect_flags)
    {
        RECT_FLAGS rect_flags = *p_rect_flags;

        gdi_rect.tl.x -= rect_flags.extend_left_ppixels  * DU_PER_PROGRAM_PIXEL_X; /* SKS 17may94 */
        gdi_rect.tl.y -= rect_flags.extend_up_ppixels    * DU_PER_PROGRAM_PIXEL_Y;
        gdi_rect.br.x += rect_flags.extend_right_ppixels * DU_PER_PROGRAM_PIXEL_X;
        gdi_rect.br.y += rect_flags.extend_down_ppixels  * DU_PER_PROGRAM_PIXEL_Y;
    }

    if(p_host_event_desc->scroll_x)
    {
        gdi_rect.tl.x -= p_pane->scroll.x;
        gdi_rect.br.x -= p_pane->scroll.x;
    }

    if(p_host_event_desc->scroll_y)
    {
        gdi_rect.tl.y -= p_pane->scroll.y;
        gdi_rect.br.y -= p_pane->scroll.y;
    }

    p_rect->left   = (int) MAX(T5_GDI_MIN_X, gdi_rect.tl.x);
    p_rect->top    = (int) MAX(T5_GDI_MIN_Y, gdi_rect.tl.y);
    p_rect->right  = (int) MIN(T5_GDI_MAX_X, gdi_rect.br.x);
    p_rect->bottom = (int) MIN(T5_GDI_MAX_Y, gdi_rect.br.y);

    trace_4(TRACE_WINDOWS_HOST, TEXT("rect_from_view-rect: (final result) tl = %d,%d; br = %d,%d"), p_rect->left, p_rect->top, p_rect->right, p_rect->bottom);
}

extern void
host_drag_start(
    _In_opt_    P_ANY p_reason_data)
{
    static S32 default_drag_reason_data = CB_CODE_NOREASON;

    trace_0(TRACE_WINDOWS_HOST, TEXT("host_drag_start: **** CALLED TO START A DRAG! ****"));

    /* must be threaded, so that the state information is valid */
    assert(ho_win_state.drag.threaded == TRUE);

    if(NULL == p_reason_data)
        p_reason_data = &default_drag_reason_data;

    /* ensure we capture the mouse, ensures we get the mouse events */
    SetCapture(ho_win_state.drag.hwnd);

    trace_1(TRACE_OUT | TRACE_ANY, TEXT("host_drag_start() - starting drag monitor - *** null_events_start(docno=") DOCNO_TFMT TEXT(")"), ho_win_state.drag.docno);
    if(status_fail(status_wrap(null_events_start(ho_win_state.drag.docno, T5_MSG_WIN_HOST_NULL, null_event_ho_win, HO_WIN_DRAG_NULL_CLIENT_HANDLE))))
    {
        ReleaseCapture();
        return;
    }

    ho_win_state.drag.p_reason_data = p_reason_data;
    ho_win_state.drag.enabled = TRUE;
}

_Check_return_
extern BOOL
host_drag_in_progress(
    _DocuRef_   P_DOCU p_docu,
    _OutRef_    P_P_ANY p_p_reason_data)
{
    if(ho_win_state.drag.enabled)
    {
        if(docno_from_p_docu(p_docu) == ho_win_state.drag.docno)
        {
            *p_p_reason_data = ho_win_state.drag.p_reason_data;
            return(TRUE);
        }
    }

    *p_p_reason_data = NULL;
    return(FALSE);
}

extern void
host_update_all(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     REDRAW_TAG redraw_tag)
{
    int index[2], i;
    HWND hwnd;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    switch(redraw_tag)
    {
    default: default_unhandled();
#if CHECKING
    case UPDATE_PANE_MARGIN_HEADER:
    case UPDATE_PANE_MARGIN_FOOTER:
    case UPDATE_PANE_MARGIN_COL:
    case UPDATE_PANE_MARGIN_ROW:
    case UPDATE_PANE_CELLS_AREA:
    case UPDATE_PANE_PRINT_AREA:
    case UPDATE_PANE_PAPER:
#endif
        {
        PANE_ID pane_id = NUMPANE;

        do  {
            PANE_ID_DECR(pane_id);

            hwnd = p_view->pane[pane_id].hwnd;
            if(HOST_WND_NONE != hwnd)
                InvalidateRect(hwnd, NULL, FALSE);

        } while(PANE_ID_START != pane_id);

        return;
        }

    case UPDATE_BORDER_HORZ:
        index[0] = WIN_BORDER_HORZ_SPLIT;
        index[1] = WIN_BORDER_HORZ;
        break;

    case UPDATE_RULER_HORZ:
        index[0] = WIN_RULER_HORZ_SPLIT;
        index[1] = WIN_RULER_HORZ;
        break;

    case UPDATE_BORDER_VERT:
        index[0] = WIN_BORDER_VERT_SPLIT;
        index[1] = WIN_BORDER_VERT;
        break;

    case UPDATE_RULER_VERT:
        index[0] = WIN_RULER_VERT_SPLIT;
        index[1] = WIN_RULER_VERT;
        break;
    }

    for(i = 0; i <= 1; ++i)
    {
        hwnd = p_view->edge[index[i]].hwnd;
        if(HOST_WND_NONE != hwnd)
            InvalidateRect(hwnd, NULL, FALSE);
    }
}

extern void
host_all_update_later(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEW_RECT p_view_rect)
{
    REDRAW_FLAGS redraw_flags;
    REDRAW_FLAGS_CLEAR(redraw_flags);

    host_update(p_docu, p_view_rect, NULL, redraw_flags, UPDATE_PANE_PAPER);
    host_update(p_docu, p_view_rect, NULL, redraw_flags, UPDATE_BORDER_HORZ);
    host_update(p_docu, p_view_rect, NULL, redraw_flags, UPDATE_BORDER_VERT);
    host_update(p_docu, p_view_rect, NULL, redraw_flags, UPDATE_RULER_HORZ);
    host_update(p_docu, p_view_rect, NULL, redraw_flags, UPDATE_RULER_VERT);
}

extern void
host_update(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEW_RECT p_view_rect,
    _InRef_opt_ PC_RECT_FLAGS p_rect_flags,
    _InVal_     REDRAW_FLAGS redraw_flags,
    _InVal_     REDRAW_TAG redraw_tag)
{
    const P_VIEW p_view = p_view_rect->p_view;
    int index[2], i;
    RECT rect;
    HWND hwnd;
    RECT update_rect;

    trace_5(TRACE_WINDOWS_HOST, TEXT("host_update: view for update ") PTR_XTFMT TEXT("; rect = tl = %d, %d; br = %d, %d"), p_view_rect->p_view, p_view_rect->rect.tl.x, p_view_rect->rect.tl.y, p_view_rect->rect.br.x, p_view_rect->rect.br.y);

    switch(redraw_tag)
    {
    default: default_unhandled();
#if CHECKING
    case UPDATE_PANE_MARGIN_HEADER:
    case UPDATE_PANE_MARGIN_FOOTER:
    case UPDATE_PANE_MARGIN_COL:
    case UPDATE_PANE_MARGIN_ROW:
    case UPDATE_PANE_CELLS_AREA:
    case UPDATE_PANE_PRINT_AREA:
    case UPDATE_PANE_PAPER:
#endif
        {
        PANE_ID pane_id = NUMPANE;

        do  {
            PANE_ID_DECR(pane_id);

            hwnd = p_view->pane[pane_id].hwnd;

            if(HOST_WND_NONE != hwnd)
            {
                rect_from_view_rect(&rect, p_docu, p_view_rect, p_rect_flags, pane_to_event_handler_table[pane_id].event_handler);

                trace_5(TRACE_WINDOWS_HOST, TEXT("host_update: (pane %d) rectangle tl %d,%d, br %d,%d"), pane_id, rect.left, rect.top, rect.right, rect.bottom);

                if(redraw_flags.update_now)
                {
                    // check for dangerous redraw situation where bombing out is a good idea
                    if(GetUpdateRect(hwnd, &update_rect, FALSE)
                       &&
                       (update_rect.left   < rect.left   ||
                        update_rect.right  > rect.right  ||
                        update_rect.top    < rect.top    ||
                        update_rect.bottom > rect.bottom))
                    {
                        trace_0(TRACE_WINDOWS_HOST, TEXT("host_update: *** INTENSE REDRAW SITUATION *** "));
                        InvalidateRect(hwnd, &rect, FALSE);
                        UpdateWindow(hwnd);
                        continue;

                    }
                    trace_1(TRACE_WINDOWS_HOST, TEXT("host_update: *** UPDATE NOW FOR PANE %d ***"), pane_id);
                    InvalidateRect(hwnd, &rect, FALSE);
                    update_pane_window(hwnd, redraw_flags);
                }
                else
                    InvalidateRect(hwnd, &rect, FALSE);
            }

        } while(PANE_ID_START != pane_id);

        return;
        }

    case UPDATE_BORDER_HORZ:
        index[0] = WIN_BORDER_HORZ_SPLIT;
        index[1] = WIN_BORDER_HORZ;
        break;

    case UPDATE_BORDER_VERT:
        index[0] = WIN_BORDER_VERT_SPLIT;
        index[1] = WIN_BORDER_VERT;
        break;

    case UPDATE_RULER_HORZ:
        index[0] = WIN_RULER_HORZ_SPLIT;
        index[1] = WIN_RULER_HORZ;
        break;

    case UPDATE_RULER_VERT:
        index[0] = WIN_RULER_VERT_SPLIT;
        index[1] = WIN_RULER_VERT;
        break;
    }

    /* Invalidate the rectangle and then force updates of those areas */
    for(i = 0; i <= 1; ++i)
    {
        hwnd = p_view->edge[index[i]].hwnd;

        if(HOST_WND_NONE != hwnd)
        {
            rect_from_view_rect(&rect, p_docu, p_view_rect, p_rect_flags, slave_to_event_handler_table[index[i]].event_handler);

            trace_4(TRACE_WINDOWS_HOST, TEXT("host_update (edge): rectangle tl %d,%d, br %d,%d"), rect.left, rect.top, rect.right, rect.bottom);

            if(redraw_flags.update_now)
            {
                // check for dangerous redraw situation where bombing out is a good idea
                if(GetUpdateRect(hwnd, &update_rect, FALSE)
                   &&
                   (update_rect.left   < rect.left  ||
                    update_rect.right  > rect.right ||
                    update_rect.top    < rect.top   ||
                    update_rect.bottom > rect.bottom))
                {
                    trace_0(TRACE_WINDOWS_HOST, TEXT("host_update: *** INTENSE REDRAW SITUATION ON INDEX 1 *** "));
                    InvalidateRect(hwnd, &rect, FALSE);
                    UpdateWindow(hwnd);
                }
                else
                {
                    trace_0(TRACE_WINDOWS_HOST, TEXT("edge_update: *** UPDATE NOW FOR INDEX 1 ***"));
                    InvalidateRect(hwnd, &rect, FALSE);
                    update_pane_window(hwnd, redraw_flags);
                }
            }
            else
                InvalidateRect(hwnd, &rect, FALSE);
        }
    }
}

_Check_return_
extern BOOL
host_update_fast_continue(
    _DocuRef_   P_DOCU p_docu,
    _InoutRef_  P_VIEWEVENT_REDRAW p_viewevent_redraw)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER_InoutRef_(p_viewevent_redraw);

    /* host_update_fast_start() never starts a fast update on Linux, so there is nothing to continue */
    return(FALSE);
}

_Check_return_
extern BOOL
host_update_fast_start(
    _DocuRef_   P_DOCU p_docu,
    _OutRef_    P_VIEWEVENT_REDRAW p_viewevent_redraw,
    _InVal_     REDRAW_TAG redraw_tag,
    _InRef_     PC_VIEW_RECT p_view_rect,
    RECT_FLAGS rect_flags)
{
    const P_VIEW p_view = p_view_rect->p_view;
    PANE_ID pane_id = NUMPANE;

    UNREFERENCED_PARAMETER_InVal_(redraw_tag);

    zero_struct_ptr(p_viewevent_redraw);

    /* Windows draws straight into the current pane here. Qt can only paint inside a paint event,
     * so ask for the whole of every pane's rectangle to be painted in the ordinary way instead,
     * and report that there is no fast update to run */
    do  {
        PANE_ID_DECR(pane_id);

        if(HOST_WND_NONE != p_view->pane[pane_id].hwnd)
        {
            RECT rect;
            rect_from_view_rect(&rect, p_docu, p_view_rect, &rect_flags, pane_to_event_handler_table[pane_id].event_handler);
            InvalidateRect(p_view->pane[pane_id].hwnd, &rect, FALSE);
        }
    }
    while(PANE_ID_START != pane_id);

    return(FALSE);
}

extern BOOL
host_show_caret(
    _HwndRef_   HWND hwnd,
    _In_        int width,
    _In_        int height,
    _In_        int x,
    _In_        int y)
{
    static const QTH_RECT no_caret = { 0, 0, 0, 0 };

    // discard existing caret
    if( (HOST_WND_NONE != ho_win_state.caret.visible.hwnd) && (ho_win_state.caret.visible.hwnd != hwnd) )
        qth_window_set_caret(ho_win_state.caret.visible.hwnd, &no_caret, 0);

    // initialise the state for the caret
    ho_win_state.caret.visible.hwnd = hwnd;
    ho_win_state.caret.visible.dimensions.x = width;
    ho_win_state.caret.visible.dimensions.y = height;
    ho_win_state.caret.visible.point.x = x;
    ho_win_state.caret.visible.point.y = y;

    // show a caret in this viewer, or none if it has no height
    {
    const QTH_RECT caret = { x, y, x + width, y + height };
    qth_window_set_caret(hwnd, (0 != height) ? &caret : &no_caret, 0 != height);
    } /*block*/

    return(TRUE);
}

extern void
host_main_show_caret(
    _DocuRef_   P_DOCU p_docu,
    P_VIEW_POINT p_view_point,
    _InVal_     PIXIT pixit_height)
{
    const P_VIEW p_view = p_view_point->p_view;
    const P_PANE p_pane = &p_view->pane[p_view->cur_pane];
    PIXIT_RECT pixit_rect;

    // get the bounding information for the caret
    pixit_rect.br = pixit_rect.tl = p_view_point->pixit_point;
    pixit_rect.br.y += pixit_height;
    status_consume(window_rect_from_pixit_rect(&ho_win_state.caret.gdi_rect, &pixit_rect, &p_view->host_xform[XFORM_PANE]));
    ho_win_state.caret.gdi_rect.br.x += GetSystemMetrics(SM_CXBORDER);

    if(HOST_WND_NONE != p_pane->hwnd)
    {
        (void) host_show_caret(p_pane->hwnd,
                        (ho_win_state.caret.gdi_rect.br.x - ho_win_state.caret.gdi_rect.tl.x),
                        (ho_win_state.caret.gdi_rect.br.y - ho_win_state.caret.gdi_rect.tl.y),
                        (ho_win_state.caret.gdi_rect.tl.x - p_pane->scroll.x),
                        (ho_win_state.caret.gdi_rect.tl.y - p_pane->scroll.y));

        if(!p_docu->flags.is_current)
        {
            DOCNO docno = DOCNO_NONE;

            while(DOCNO_NONE != (docno = docno_enum_docs(docno)))
                p_docu_from_docno_valid(docno)->flags.is_current = 0;
        }

        p_docu->flags.is_current = 1;
    }
}

extern void
host_main_scroll_caret(
    _DocuRef_   P_DOCU p_docu,
    P_VIEW_POINT p_view_point,
    _InVal_     PIXIT left,
    _InVal_     PIXIT right,
    _InVal_     PIXIT top,
    _InVal_     PIXIT bottom)
{
    const P_VIEW p_view = p_view_point->p_view;
    const P_PANE p_pane = &p_view->pane[p_view->cur_pane];

    if(HOST_WND_NONE != p_pane->hwnd)
    {
        GDI_POINT scroll = p_pane->scroll;
        PIXIT_RECT pixit_rect;
        GDI_RECT gdi_rect;
        GDI_POINT gdi_point;

        /* left/right are relative to p_view_point position -
         * a rectangle which the caller wants:
         * a) kept on screen if possible
         * b) if caret is nearer to RHS, keep RHS on screen if possible
         * c) if caret is nearer to LHS, keep LHS on screen if possible
         * d) caret must be on screen whatever
         */

        pixit_rect.br = pixit_rect.tl = p_view_point->pixit_point;
        pixit_rect.tl.x += left;
        pixit_rect.br.x += right;
        pixit_rect.tl.y += top;
        pixit_rect.br.y += bottom;

        status_consume(window_rect_from_pixit_rect(&gdi_rect, &pixit_rect, &p_view->host_xform[XFORM_PANE]));

        window_point_from_pixit_point(&gdi_point, &p_view_point->pixit_point, &p_view->host_xform[XFORM_PANE]);

        // diz: 19th May 1994, ensure that it is all visible
        gdi_rect.tl.x -= GetSystemMetrics(SM_CXBORDER);
        gdi_rect.br.x += GetSystemMetrics(SM_CXBORDER) * 2;
        gdi_rect.tl.y -= GetSystemMetrics(SM_CYBORDER);
        gdi_rect.br.y += GetSystemMetrics(SM_CYBORDER) * 2;

        /* MRJC 11.7.94 */
        if(gdi_rect.br.x - gdi_rect.tl.x > p_pane->size.x)
        {
            if(gdi_point.x < p_pane->scroll.x + 1 * p_pane->size.x / 16)
                scroll.x = gdi_point.x - 1 * p_pane->size.x / 8;
            else if(gdi_point.x > p_pane->scroll.x + 15 * p_pane->size.x / 16)
                scroll.x = gdi_point.x - 7 * p_pane->size.x / 8;
        }
        else if(gdi_rect.br.x > p_pane->scroll.x + p_pane->size.x)
            scroll.x = gdi_rect.br.x - p_pane->size.x;
        else if(gdi_rect.tl.x < p_pane->scroll.x)
            scroll.x = gdi_rect.tl.x;

        scroll.x = MAX(scroll.x, 0);

        /* MRJC 16.3.95 */
        if(gdi_rect.br.y - gdi_rect.tl.y > p_pane->size.y)
        {
            if(gdi_point.y < p_pane->scroll.y + 1 * p_pane->size.y / 16)
                scroll.y = gdi_point.y - 1 * p_pane->size.y / 8;
            else if(gdi_point.y > p_pane->scroll.y + 15 * p_pane->size.y / 16)
                scroll.y = gdi_point.y - 7 * p_pane->size.y / 8;
        }
        else if(gdi_rect.br.y > p_pane->scroll.y + p_pane->size.y)
            scroll.y = gdi_rect.br.y - p_pane->size.y;
        else if(gdi_rect.tl.y < p_pane->scroll.y)
            scroll.y = gdi_rect.tl.y;

        scroll.y = MAX(scroll.y, 0);

        /* If we must then perform a nice little scroll */
        trace_2(TRACE_WINDOWS_HOST, TEXT("host_main_scroll_caret: new scroll x=") S32_TFMT TEXT(", y=") S32_TFMT, scroll.x, scroll.y);
        trace_2(TRACE_WINDOWS_HOST, TEXT("host_main_scroll_caret: current scroll x=") S32_TFMT TEXT(", y=") S32_TFMT, p_pane->scroll.x, p_pane->scroll.y);

        if((scroll.x != p_pane->scroll.x) || (scroll.y != p_pane->scroll.y))
            scroll_pane_absolute(p_docu, p_view, pane_to_event_handler_table[p_view->cur_pane].event_handler, &scroll, TRUE);
    }
}

_Check_return_
extern DOCNO
resolve_hwnd(
    _HwndRef_   HWND hwnd,
    _OutRef_    P_P_VIEW p_p_view,
    _Out_       EVENT_HANDLER * const p_event_handler)
{
    U32 u32 = (U32) (uintptr_t) qth_window_get_client(hwnd);

    if(u32)
    {
        const P_DOCU p_docu = viewid_unpack(u32, p_p_view, p_event_handler);
        return(docno_from_p_docu(p_docu));
    }

    *p_p_view = NULL;
    *p_event_handler = 0xFFFF;
    return(DOCNO_NONE);
}

_Check_return_
static STATUS
slave_window_create(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id)
{
    assert_EQ(NUMEDGE, elemof32(internal_slave_table));

    if( IS_ARRAY_INDEX_VALID(slave_id, elemof32(internal_slave_table)) &&
        (HOST_WND_NONE == p_view->edge[slave_id].hwnd) )
    {
        const QTH_RECT bounds = { 0, 0, 0, 0 };
        const int kind = internal_slave_table[slave_id].is_horz_scroll_widget ? QTH_WINDOW_HSCROLL
                       : internal_slave_table[slave_id].is_vert_scroll_widget ? QTH_WINDOW_VSCROLL
                       : QTH_WINDOW_PANE;
        const HWND hwnd = li_window_create(p_docu, p_view, internal_slave_table[slave_id].event_handler, kind, p_view->main[WIN_BACK].hwnd, &bounds);

        if(HOST_WND_NONE == hwnd)
            return(create_error(ERR_TOOMANYWINDOWS));

        if(kind != QTH_WINDOW_PANE)
            qth_window_set_scroll_range(hwnd, 0, SCROLL_RANGE, SCROLL_RANGE / 16);
    }

    return(STATUS_OK);
}

static void
slave_window_destroy(
    _ViewRef_   P_VIEW p_view,
    _In_        int slave_id)
{
    if(HOST_WND_NONE == p_view->edge[slave_id].hwnd)
        return;

    li_window_destroy(p_view, internal_slave_table[slave_id].event_handler);
}

static void
send_drag_movement_and_maybe_scroll(
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed)
{
    const P_DOCU p_docu = p_docu_from_docno(ho_win_state.drag.docno);
    const P_VIEW p_view = p_view_from_viewno(p_docu, ho_win_state.drag.viewno);

    assert(ho_win_state.drag.enabled);

    send_mouse_event(p_docu, p_view, T5_EVENT_CLICK_DRAG_MOVEMENT, ho_win_state.drag.hwnd, x, y, ctrl_pressed, shift_pressed, ho_win_state.drag.event_handler);

    maybe_scroll(p_docu, p_view, x, y, ho_win_state.drag.event_handler);
}

_Check_return_
static STATUS
back_window_event_pointer(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Ensure point reflects current state of play */
    get_split_info(&p_viewevent_click->view_point);

    host_set_pointer_shape(split_info.pointer_shape);

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_left_double(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Double click toggles split, if enabling enables to half way into area */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;

    get_split_info(&p_viewevent_click->view_point);

    if((split_info.flags.horz) || (split_info.flags.vert))
    {
        GDI_RECT gdi_rect;
        PIXIT_RECT pixit_rect;

        gdi_rect.tl.x = p_view->pane_posn.start.x;
        gdi_rect.tl.y = p_view->pane_posn.start.y;
        gdi_rect.br.x = p_view->pane_posn.max.x;
        gdi_rect.br.y = p_view->pane_posn.max.y;

        pixit_rect_from_window_rect(&pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_BACK]);

        p_view->flags.pseudo_horz_split_on &= ~split_info.flags.horz;
        p_view->flags.pseudo_vert_split_on &= ~split_info.flags.vert;

        p_view->flags.horz_split_on ^= split_info.flags.horz;
        p_view->flags.vert_split_on ^= split_info.flags.vert;

        if(split_info.flags.horz && p_view->flags.horz_split_on)
            p_view->horz_split_pos = (pixit_rect.br.x - pixit_rect.tl.x) / 2;
        if(split_info.flags.vert && p_view->flags.vert_split_on)
            p_view->vert_split_pos = (pixit_rect.br.y - pixit_rect.tl.y) / 2;

        host_view_reopen(p_docu, p_view);
    }

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_left_drag(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Drag occuring on the back window, look and see if over split points */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;

    get_split_info(&p_viewevent_click->view_point);

    if((split_info.flags.horz) || (split_info.flags.vert))
    {
        p_view->flags.pseudo_horz_split_on &= ~split_info.flags.horz;
        p_view->flags.pseudo_vert_split_on &= ~split_info.flags.vert;

        p_view->flags.horz_split_on |= split_info.flags.horz;
        p_view->flags.vert_split_on |= split_info.flags.vert;

        host_drag_start(NULL);
    }

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_drag_started(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Drag is starting, so initialise the dragging state */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;
    GDI_RECT gdi_rect;

    zero_struct(split_info.dragging);

    gdi_rect.tl.x = p_view->pane_posn.start.x;
    gdi_rect.tl.y = p_view->pane_posn.start.y;
    gdi_rect.br.x = p_view->pane_posn.max.x;
    gdi_rect.br.y = p_view->pane_posn.max.y;

    pixit_rect_from_window_rect(&split_info.dragging.clip_pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_BACK]);

    gdi_rect.tl.x = ho_win_state.metrics.split.x;
    gdi_rect.tl.y = ho_win_state.metrics.split.y;
    gdi_rect.br.x = (p_view->pane_posn.max.x - ho_win_state.metrics.split.x) - p_view->pane_posn.start.x;
    gdi_rect.br.y = (p_view->pane_posn.max.y - ho_win_state.metrics.split.y) - p_view->pane_posn.start.y;

    pixit_rect_from_window_rect(&split_info.dragging.valid_pixit_rect, &gdi_rect, &p_view->host_xform[XFORM_BACK]);

    get_split_drag_info(p_viewevent_click, &split_info.dragging.previous_pixit_point);

    render_split_drag_info(p_view, &split_info.dragging.previous_pixit_point);

    host_set_pointer_shape(split_info.pointer_shape);

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_drag_movement(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Pointer is moving within the window.  Render a suitable set of cross hairs */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;
    PIXIT_POINT pixit_point;

    get_split_drag_info(p_viewevent_click, &pixit_point);

    if(!split_info.flags.horz)
        pixit_point.x = p_view->horz_split_pos;
    if(!split_info.flags.vert)
        pixit_point.y = p_view->vert_split_pos;

    if((pixit_point.x != split_info.dragging.previous_pixit_point.x) || (pixit_point.y != split_info.dragging.previous_pixit_point.y))
    {
        render_split_drag_info(p_view, &pixit_point);

        split_info.dragging.previous_pixit_point = pixit_point;
    }

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_drag_finished(
    _DocuRef_   P_DOCU p_docu,
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Finish dragging, ensure that we should have the split points enabled */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;
    PIXIT_POINT pixit_point;

    get_split_drag_info(p_viewevent_click, &pixit_point);

    if(split_info.flags.horz)
    {
        if((pixit_point.x < split_info.dragging.valid_pixit_rect.tl.x) || (pixit_point.x >= split_info.dragging.valid_pixit_rect.br.x))
            p_view->flags.horz_split_on = FALSE;
        else
            p_view->horz_split_pos = pixit_point.x;
    }

    if(split_info.flags.vert)
    {
        if((pixit_point.y < split_info.dragging.valid_pixit_rect.tl.y) || (pixit_point.y >= split_info.dragging.valid_pixit_rect.br.y))
            p_view->flags.vert_split_on = FALSE;
        else
            p_view->vert_split_pos = pixit_point.y;
    }

    render_split_drag_info(p_view, NULL /* don't define new point */);

    host_view_reopen(p_docu, p_view);

    return(STATUS_OK);
}

_Check_return_
static STATUS
back_window_event_click_drag_aborted(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click)
{
    /* Drag has been aborted, erase the current cross hair stuff */
    const P_VIEW p_view = p_viewevent_click->view_point.p_view;

    render_split_drag_info(p_view, NULL /* don't define new point */);

    return(STATUS_OK);
}

static void
get_split_info(
    _InRef_     PC_VIEW_POINT p_view_point)
{
    const P_VIEW p_view = p_view_point->p_view;
    GDI_POINT cursor;
    POINT split_area;
    RECT grab;

    zero_struct(split_info);

    window_point_from_pixit_point(&cursor, &p_view_point->pixit_point, &p_view->host_xform[XFORM_BACK]);

    split_area.x = (p_view->flags.vert_split_on && (!p_view->flags.pseudo_horz_split_on)) ? p_view->inner_frame_gdi_rect.tl.x : p_view->pane_posn.max.x;
    split_area.y = (p_view->flags.horz_split_on && (!p_view->flags.pseudo_vert_split_on)) ? p_view->inner_frame_gdi_rect.tl.y : p_view->pane_posn.max.y;

    grab.right = grab.left = p_view->pane_posn.grab.x;
    grab.right += p_view->pane_posn.grab_size.x;
    grab.bottom = grab.top = p_view->pane_posn.grab.y;
    grab.bottom += p_view->pane_posn.grab_size.y;

    grab.left   = MAX(grab.left - ho_win_state.metrics.split.x, p_view->pane_posn.start.x);
    grab.top    = MAX(p_view->pane_posn.grab.y - ho_win_state.metrics.split.y, p_view->pane_posn.start.y);
    grab.right  = MIN(grab.right + ho_win_state.metrics.split.x, p_view->pane_posn.max.x);
    grab.bottom = MIN(grab.bottom + ho_win_state.metrics.split.y, p_view->pane_posn.max.y);

    trace_2(TRACE_WINDOWS_HOST, TEXT("get_split_info: grab          x/y == %d,%d"), grab.left, grab.top);
    trace_2(TRACE_WINDOWS_HOST, TEXT("get_split_info: grab size     x/y == %d,%d"), grab.right - grab.left, grab.bottom - grab.top);
    trace_2(TRACE_WINDOWS_HOST, TEXT("get_split_info: split metrics x/y == %d,%d"), ho_win_state.metrics.split.x, ho_win_state.metrics.split.y);
    trace_2(TRACE_WINDOWS_HOST, TEXT("get_split_info: co-ords       x/y == %d,%d"), (int) cursor.x, (int) cursor.y);

    if(((int) cursor.x >= grab.left) &&
       ((int) cursor.x  < grab.right) &&
       ((int) cursor.y >= split_area.y) &&
       ((int) cursor.y  < p_view->inner_frame_gdi_rect.br.y) )
    {
        split_info.flags.horz = TRUE;
    }

    if(((int) cursor.x >= grab.left) &&
       ((int) cursor.x  < grab.right) &&
       ((int) cursor.y >= p_view->pane_posn.max.y) &&
       ((int) cursor.y  < p_view->inner_frame_gdi_rect.br.y) )
    {
        split_info.flags.horz_box = TRUE;
    }

    if(((int) cursor.x >= split_area.x) &&
       ((int) cursor.x  < p_view->inner_frame_gdi_rect.br.x) &&
       ((int) cursor.y >= grab.top) &&
       ((int) cursor.y  < grab.bottom) )
    {
        split_info.flags.vert = TRUE;
    }

    if(((int) cursor.x >= p_view->pane_posn.max.x) &&
       ((int) cursor.x  < p_view->inner_frame_gdi_rect.br.x) &&
       ((int) cursor.y >= grab.top) &&
       ((int) cursor.y  < grab.bottom) )
    {
        split_info.flags.vert_box = TRUE;
    }

    split_info.pointer_shape = POINTER_DEFAULT;

    if(split_info.flags.horz || split_info.flags.horz_box)
    {
        if(split_info.flags.vert || split_info.flags.vert_box)
            split_info.pointer_shape = POINTER_SPLIT_ALL;
        else
            split_info.pointer_shape = POINTER_SPLIT_LEFTRIGHT;
    }
    else
        if(split_info.flags.vert || split_info.flags.vert_box)
            split_info.pointer_shape = POINTER_SPLIT_UPDOWN;
}

static void
get_split_drag_info(
    _InRef_     PC_VIEWEVENT_CLICK p_viewevent_click,
    _OutRef_    P_PIXIT_POINT p_pixit_point)
{
    *p_pixit_point = p_viewevent_click->view_point.pixit_point;

    p_pixit_point->x = MAX(p_pixit_point->x, split_info.dragging.clip_pixit_rect.tl.x);
    p_pixit_point->x = MIN(p_pixit_point->x, split_info.dragging.clip_pixit_rect.br.x);
    p_pixit_point->y = MAX(p_pixit_point->y, split_info.dragging.clip_pixit_rect.tl.y);
    p_pixit_point->y = MIN(p_pixit_point->y, split_info.dragging.clip_pixit_rect.br.y);

    p_pixit_point->x -= split_info.dragging.clip_pixit_rect.tl.x;
    p_pixit_point->y -= split_info.dragging.clip_pixit_rect.tl.y;
}

static void
render_split_drag_info(
    _ViewRef_   P_VIEW p_view,
    P_PIXIT_POINT p_pixit_point)
{
    const HWND hwnd = p_view->main[WIN_BACK].hwnd;
    GDI_POINT point = { 0, 0 };

    if(NULL != p_pixit_point)
    {
        PIXIT_POINT pixit_point = *p_pixit_point;

        pixit_point.x = MIN(pixit_point.x, split_info.dragging.valid_pixit_rect.br.x);
        pixit_point.y = MIN(pixit_point.y, split_info.dragging.valid_pixit_rect.br.y);

        pixit_point.x += split_info.dragging.clip_pixit_rect.tl.x;
        pixit_point.y += split_info.dragging.clip_pixit_rect.tl.y;

        window_point_from_pixit_point(&point, &pixit_point, &p_view->host_xform[XFORM_BACK]);
    }

    /* the lines are inverted markers over the back window, one for each way the split can go */

    if(split_info.flags.horz)
    {
        const QTH_RECT rect = { (int) point.x, p_view->inner_frame_gdi_rect.tl.y, (int) (point.x + ho_win_state.metrics.split.x), p_view->pane_posn.max.y };
        qth_window_set_marker(hwnd, 0, &rect, NULL != p_pixit_point);
    }

    if(split_info.flags.vert)
    {
        const QTH_RECT rect = { p_view->inner_frame_gdi_rect.tl.x, (int) point.y, p_view->pane_posn.max.x, (int) (point.y + ho_win_state.metrics.split.y) };
        qth_window_set_marker(hwnd, 1, &rect, NULL != p_pixit_point);
    }
}

extern void
host_view_minimize(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    qth_window_set_state(p_view->main[WIN_BACK].hwnd, QTH_STATE_MINIMIZED);
}

extern void
host_view_maximize(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    qth_window_set_state(p_view->main[WIN_BACK].hwnd, QTH_STATE_MAXIMIZED);
}

extern void
host_settitle(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _In_z_      PCTSTR p_new_title)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    trace_1(TRACE_WINDOWS_HOST, TEXT("host_settitle: set title string for window: '%s'"), p_new_title);
    qth_window_set_title(p_view->main[WIN_BACK].hwnd, p_new_title);
}

/* ---- Event handling: the Linux counterpart of the window procedures ----------------- */

/* Windows messages whose numbers the shared code mentions */

#define MK_SHIFT    0x0004
#define MK_CONTROL  0x0008

#define WHEEL_DELTA 120

/* scroll codes: a Qt scroll bar tells us only where its thumb now is */
#define SB_THUMBPOSITION    4

/* scroll bar codes, as on Windows. Qt only ever reports the thumb position, which is the one Linux sends. */
#define SB_LINEUP           0
#define SB_LINELEFT         0
#define SB_LINEDOWN         1
#define SB_LINERIGHT        1
#define SB_PAGEUP           2
#define SB_PAGELEFT         2
#define SB_PAGEDOWN         3
#define SB_PAGERIGHT        3
#define SB_THUMBTRACK       5
#define SB_TOP              6
#define SB_LEFT             6
#define SB_BOTTOM           7
#define SB_RIGHT            7

#define IsWindow(hwnd)      (HOST_WND_NONE != (hwnd))
#define IsIconic(hwnd)      FALSE /* TODO: ask the Qt layer; only used to count windows that are not minimised */

#define KillTimer(hwnd, id) qth_window_kill_timer(hwnd, id)

_Check_return_
extern BOOL
host_set_tracking_for_window(
    _HwndRef_   HWND hwnd);

static void
host_view_scroll_wheel_v(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     int zDelta);

static void
host_view_scroll_wheel_h(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     int zDelta);

#define PostQuitMessage(code) qth_app_quit(code)


static void
do_drag_start(
    _InVal_     BOOL ctrl_pressed,
    _InVal_     BOOL shift_pressed)
{
    const P_DOCU p_docu = p_docu_from_docno(ho_win_state.drag.docno);
    const P_VIEW p_view = p_view_from_viewno(p_docu, ho_win_state.drag.viewno);

    const T5_MESSAGE t5_message = ho_win_state.drag.right_button ? T5_EVENT_CLICK_RIGHT_DRAG : T5_EVENT_CLICK_LEFT_DRAG;

    stop_drag_monitor();

    ho_win_state.drag.threaded = TRUE;
    send_mouse_event(p_docu, p_view, t5_message, ho_win_state.drag.hwnd, ho_win_state.drag.start.x, ho_win_state.drag.start.y, ctrl_pressed, shift_pressed, ho_win_state.drag.event_handler);
    ho_win_state.drag.threaded = FALSE;
}

static void
stop_drag_monitor(void)
{
    trace_0(TRACE_WINDOWS_HOST, TEXT("stop_drag_monitor: killing the drag monitor / drag canceled"));

    if(ho_win_state.drag.pending)
    {
        ReleaseCapture();
        KillTimer(ho_win_state.drag.hwnd, DRAG_TIMER_ID);
        ho_win_state.drag.pending = FALSE;
    }
}

static void
disable_triple_click(void)
{
    if(HOST_WND_NONE == ho_win_state.triple.hwnd)
        return;

    (void) KillTimer(ho_win_state.triple.hwnd, TRIPLE_CLICK_TIMER_ID);
    ho_win_state.triple.hwnd = NULL;
}

static void
host_onTimer_drag(void)
{
    /*POINT point;*/

    /*GetMessagePosAsClient(ho_win_state.drag.hwnd, &point);*/

    if(ho_win_state.drag.pending)
    {
        const BOOL ctrl_pressed = host_ctrl_pressed(); /* these may change between click and drag start */
        const BOOL shift_pressed = host_shift_pressed();

        trace_0(TRACE_WINDOWS_HOST, TEXT("WM_TIMER: **** POSTING DRAG START ****"));
        do_drag_start(ctrl_pressed, shift_pressed);
    }
}

_Check_return_
static STATUS
ho_win_maeve_service_win_host_null(void)
{
    if(ho_win_state.drag.enabled)
    {
        const BOOL ctrl_pressed = host_ctrl_pressed(); /* these may change during drag */
        const BOOL shift_pressed = host_shift_pressed();
        int x, y;

        GetMessagePosAsClient(ho_win_state.drag.hwnd, &x, &y);

        send_drag_movement_and_maybe_scroll(x, y, ctrl_pressed, shift_pressed);
    }

    return(STATUS_OK);
}

extern void
host_reenter_window(void)
{
#if 0
    const HWND hwnd = CurrentTrackWindow(); /*???*/
    DOCNO docno;
    P_VIEW p_view;
    EVENT_HANDLER event_handler;

    if(!IsWindow(hwnd))
        return;

    if(DOCNO_NONE != (docno = resolve_hwnd(hwnd, &p_view, &event_handler)))
    {
        int x, y;

        GetMessagePosAsClient(hwnd, &x, &y);

        send_mouse_event(p_docu_from_docno_valid(docno), p_view, T5_EVENT_POINTER_ENTERS_WINDOW, hwnd, x, y, event_handler);
    }
#endif
}

_Check_return_
extern BOOL
some_document_windows(void)
{
    S32 n_windows = 0;
    S32 n_iconic_windows = 0;
    DOCNO docno = DOCNO_NONE;

    while(DOCNO_NONE != (docno = docno_enum_docs(docno)))
    {
        const P_DOCU p_docu = p_docu_from_docno_valid(docno);
        VIEWNO viewno = VIEWNO_NONE;
        P_VIEW p_view;

        while(P_VIEW_NONE != (p_view = docu_enum_views(p_docu, &viewno)))
        {
            const HWND hwnd = p_view->main[WIN_BACK].hwnd;

            if(!IsWindow(hwnd))
                continue;

            ++n_windows;

            if(IsIconic(hwnd))
                ++n_iconic_windows;
        }
    }

    if(0 == n_windows)
    {
        return(FALSE);
    }

    return(TRUE);
}

static void
host_onButtonDown(
    _HwndRef_   HWND hwnd,
    _InVal_     UINT uiMsg,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     UINT keyFlags,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    const BOOL ctrl_pressed = (0 != (keyFlags & MK_CONTROL));
    const BOOL shift_pressed = (0 != (keyFlags & MK_SHIFT));

    status_line_auto_clear(p_docu);

    status_assert(host_key_cache_emit_events());

    switch(event_handler)
    {
    case EVENT_HANDLER_PANE:
    case EVENT_HANDLER_PANE_SPLIT_HORZ:
    case EVENT_HANDLER_PANE_SPLIT_VERT:
    case EVENT_HANDLER_PANE_SPLIT_DIAG:
        {
        const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
        const PANE_ID pane_id = p_host_event_desc->pane_id;
        //const P_PANE p_pane = &p_view->pane[pane_id];

        p_docu->viewno_caret = viewno_from_p_view(p_view);
        p_view->cur_pane = pane_id;

        send_click_to_view(p_docu, p_view, hwnd, uiMsg, x, y, event_handler, ctrl_pressed, shift_pressed);
        break;
        }

    default: default_unhandled();
#if CHECKING
    case EVENT_HANDLER_BACK_WINDOW:
    case EVENT_HANDLER_BORDER_HORZ_SPLIT:
    case EVENT_HANDLER_BORDER_HORZ:
    case EVENT_HANDLER_BORDER_VERT_SPLIT:
    case EVENT_HANDLER_BORDER_VERT:
    case EVENT_HANDLER_RULER_HORZ_SPLIT:
    case EVENT_HANDLER_RULER_HORZ:
    case EVENT_HANDLER_RULER_VERT_SPLIT:
    case EVENT_HANDLER_RULER_VERT:
#endif
        send_click_to_view(p_docu, p_view, hwnd, uiMsg, x, y, event_handler, ctrl_pressed, shift_pressed);
        break;
    }
}

static void
host_onButtonUp_dragging(
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     UINT keyFlags)
{
    const BOOL ctrl_pressed = (0 != (keyFlags & MK_CONTROL)); /* these may change during drag */
    const BOOL shift_pressed = (0 != (keyFlags & MK_SHIFT));
    P_DOCU p_docu = p_docu_from_docno(ho_win_state.drag.docno);
    P_VIEW p_view = p_view_from_viewno(p_docu, ho_win_state.drag.viewno);
    GDI_POINT point;

    assert(ho_win_state.drag.hwnd == hwnd);
    UNREFERENCED_PARAMETER_HwndRef_(hwnd);

    GetMessagePosAsClient(ho_win_state.drag.hwnd, &point.x, &point.y);
    UNREFERENCED_PARAMETER_InVal_(x);
    UNREFERENCED_PARAMETER_InVal_(y);
    assert(point.x == x);
    assert(point.y == y);

    ReleaseCapture();

    trace_1(TRACE__NULL, TEXT("WM_xBUTTONUP - terminating drag monitor - *** null_events_stop(docno=") DOCNO_TFMT TEXT(")"), ho_win_state.drag.docno);
    null_events_stop(ho_win_state.drag.docno, T5_MSG_WIN_HOST_NULL, null_event_ho_win, HO_WIN_DRAG_NULL_CLIENT_HANDLE);

    /* inform the current owner of the pending doom */
    trace_1(TRACE_WINDOWS_HOST, TEXT("WM_xBUTTONUP: *** TERMINATING DRAG in docno=") DOCNO_TFMT TEXT(" ***"), ho_win_state.drag.docno);
    send_mouse_event(p_docu, p_view, T5_EVENT_CLICK_DRAG_FINISHED, ho_win_state.drag.hwnd, point.x, point.y, ctrl_pressed, shift_pressed, ho_win_state.drag.event_handler);
    ho_win_state.drag.enabled = FALSE;
}

static void
host_onMouseMove(
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     UINT keyFlags,
    _DocuRef_   P_DOCU p_docu,
    /*_ViewRef_*/ P_VIEW p_view,
    /*_InVal_*/   EVENT_HANDLER event_handler)
{
    const BOOL ctrl_pressed = (0 != (keyFlags & MK_CONTROL)); /* these may change between click and drag start / during drag */
    const BOOL shift_pressed = (0 != (keyFlags & MK_SHIFT));

    if(host_set_tracking_for_window(hwnd))
    {
        GDI_POINT point;

        GetMessagePosAsClient(hwnd, &point.x, &point.y);
        assert(x == point.x);
        assert(y == point.y);

        trace_1(TRACE_WINDOWS_HOST, TEXT("wndproc_host: entering window ") HOST_WND_XTFMT, hwnd);

        send_mouse_event(p_docu, p_view, T5_EVENT_POINTER_ENTERS_WINDOW, hwnd, point.x, point.y, FALSE, FALSE, event_handler);
    }

    {
    T5_MESSAGE t5_message;

    if(ho_win_state.drag.pending)
    {
        /* If not forcing the drag to occur then check the distance between start and current */
        int dx = abs(ho_win_state.drag.start.x - x);
        int dy = abs(ho_win_state.drag.start.y - y);

        if((dx >= ho_win_state.drag.dist.x) || (dy >= ho_win_state.drag.dist.y))
        {
            trace_0(TRACE_WINDOWS_HOST, TEXT("WM_MOUSEMOVE: **** POSTING DRAG START ****"));
            do_drag_start(ctrl_pressed, shift_pressed);
        }
    }

    if(ho_win_state.drag.enabled)
    {
        send_drag_movement_and_maybe_scroll(x, y, ctrl_pressed, shift_pressed);
    }
    else
    {
        t5_message = T5_EVENT_POINTER_MOVEMENT;

        send_mouse_event(p_docu, p_view, t5_message, hwnd, x, y, FALSE, FALSE, event_handler);
    }
    } /*block*/
}

static void
host_onMouseLeave(
    _HwndRef_   HWND hwnd,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    int x, y;

    GetMessagePosAsClient(hwnd, &x, &y);

    send_mouse_event(p_docu, p_view, T5_EVENT_POINTER_LEAVES_WINDOW, hwnd, x, y, FALSE, FALSE, event_handler);
}

static void
host_onHScroll(
    _InVal_     UINT code,
    _InVal_     int pos,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    PANE_ID pane_id = p_host_event_desc->pane_id;
    const P_PANE p_pane = &p_view->pane[pane_id];
    S32 new_scroll = 0;
    BOOL do_it = TRUE;

    switch(code)
    {
    case SB_PAGERIGHT:
        new_scroll = host_HScroll_get_new_scroll(p_view, event_handler, p_pane) + p_pane->size.x;
        break;

    case SB_PAGELEFT:
        new_scroll = host_HScroll_get_new_scroll(p_view, event_handler, p_pane) - p_pane->size.x;
        break;

    case SB_LINERIGHT:
        new_scroll = host_HScroll_get_new_scroll(p_view, event_handler, p_pane) + ho_win_state.metrics.scroll.x *2;
        break;

    case SB_LINELEFT:
        new_scroll = host_HScroll_get_new_scroll(p_view, event_handler, p_pane) - ho_win_state.metrics.scroll.x *2;
        break;

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        new_scroll = point_from_blip_pos(pos, p_pane->extent.x - p_pane->size.x);
        break;

    case SB_RIGHT:
        new_scroll = p_pane->extent.x;
        break;

    case SB_LEFT:
        new_scroll = 0;
        break;

    default:
        do_it = FALSE;
        break;
    }

    if(!do_it)
        return;

    trace_1(TRACE_WINDOWS_HOST, TEXT("WM_HSCROLL: scroll position (post modify) %d"), new_scroll);
    scroll_pane(p_docu, p_view, event_handler, new_scroll);
}

static void
host_onVScroll(
    _InVal_     UINT code,
    _InVal_     int pos,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    PANE_ID pane_id = p_host_event_desc->pane_id;
    const P_PANE p_pane = &p_view->pane[pane_id];
    S32 new_scroll = 0;
    BOOL do_it = TRUE;

    switch(code)
    {
    case SB_PAGEDOWN:
        new_scroll = host_VScroll_get_new_scroll(p_view, event_handler, p_pane) + p_pane->size.y;
        break;

    case SB_PAGEUP:
        new_scroll = host_VScroll_get_new_scroll(p_view, event_handler, p_pane) - p_pane->size.y;
        break;

    case SB_LINEDOWN:
        new_scroll = host_VScroll_get_new_scroll(p_view, event_handler, p_pane) + ho_win_state.metrics.scroll.y *2;
        break;

    case SB_LINEUP:
        new_scroll = host_VScroll_get_new_scroll(p_view, event_handler, p_pane) - ho_win_state.metrics.scroll.y *2;
        break;

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        new_scroll = point_from_blip_pos(pos, p_pane->extent.y - p_pane->size.y);
        break;

    case SB_BOTTOM:
        new_scroll = p_pane->extent.y;
        break;

    case SB_TOP:
        new_scroll = 0;
        break;

    default:
        do_it = FALSE;
        break;
    }

    if(!do_it)
        return;

    trace_1(TRACE_WINDOWS_HOST, TEXT("WM_VSCROLL: scroll position (post modify) %d"), new_scroll);
    scroll_pane(p_docu, p_view, event_handler, new_scroll);
}

_Check_return_
static BOOL
host_onKeyDown(
    _InVal_     UINT vk,
    _DocuRef_   P_DOCU p_docu)
{
    const KMAP_CODE kmap_code = kmap_convert(vk);

    if(0 == kmap_code)
        return(FALSE);

    status_line_auto_clear(p_docu);

    status_assert(send_key_to_docu(p_docu, kmap_code)); /* don't pass on unprocessed keys */

    /* there will be a WM_KEYUP to come so flush there in any case */
    return(TRUE);
}

static void
host_onChar(
    _InVal_     TCHAR ch,
    _DocuRef_   P_DOCU p_docu)
{
    KMAP_CODE kmap_code = (KMAP_CODE) ch;

    if((kmap_code >= 0x20) && (kmap_code <= 0xFF) && (kmap_code != 0x7F))
    {
        docu_modify(p_docu);

        status_assert(host_key_cache_event(docno_from_p_docu(p_docu), kmap_code, FALSE, 1));
    }

    if(!host_keys_in_buffer())
        status_assert(host_key_cache_emit_events());
}

static void
host_onSetFocus(
    _DocuRef_   P_DOCU p_docu)
{
    { /* ask objects if they're happy for a new document focus */
    const DOCNO docno = docno_from_p_docu(p_docu);
    OBJECT_ID object_id = OBJECT_ID_ENUM_START;
    DOCU_FOCUS_QUERY docu_focus_query;
    docu_focus_query.processed = FALSE;
    docu_focus_query.docno = docno;
    while(!docu_focus_query.processed && status_ok(object_next(&object_id)))
        status_break(object_call_id(object_id, P_DOCU_NONE, T5_MSG_DOCU_FOCUS_QUERY, &docu_focus_query));

    p_docu = p_docu_from_docno(docu_focus_query.docno);
    DOCU_ASSERT(p_docu);
    } /*block*/

    caret_show_claim(p_docu, p_docu->focus_owner, FALSE); /* MRJC 3.8.94 */
}

static void
host_onMouseWheel(
    _InVal_     int xPos,
    _InVal_     int yPos,
    _InVal_     int zDelta,
    _InVal_     UINT fwKeys,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    POINT point;

    point.x = xPos;
    point.y = yPos;

#if 1
    /* I can see this being potentially useful when drag is enabled too */
#else
    if(ho_win_state.drag.enabled)
    {   /* This can be provoked by wheeling when the mouse button is still down during a drag. */
        return;
    }
#endif

    if(fwKeys & MK_CONTROL)
    {   /* Zoom the whole view (button away from screen -> negative delta -> smaller text, i.e. lower scale factor) */
        view_set_scale_from_wheel_delta(p_docu, p_view, (zDelta * 10) / WHEEL_DELTA);
    }
    else
    {   /* Scroll the corresponding vertical scroll bar */
        host_view_scroll_wheel_v(p_docu, p_view, event_handler, zDelta);
    }
}

static void
host_view_scroll_wheel_v(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     int zDelta)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const PC_HOST_PANE_DESC p_host_pane_desc = &host_pane_desc_table[p_host_event_desc->pane_id];
    const P_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    EVENT_HANDLER event_handler_scroll = p_host_pane_desc->vscroll_event_handler;
    S32 new_scroll, inc_scroll;

    inc_scroll = (ho_win_state.metrics.scroll.y * zDelta) / WHEEL_DELTA;

    new_scroll = p_pane->scroll.y - inc_scroll;

    scroll_pane(p_docu, p_view, event_handler_scroll, new_scroll);
}

static void
host_view_scroll_wheel_h(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InVal_     int zDelta)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const PC_HOST_PANE_DESC p_host_pane_desc = &host_pane_desc_table[p_host_event_desc->pane_id];
    const P_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    EVENT_HANDLER event_handler_scroll = p_host_pane_desc->hscroll_event_handler;
    S32 new_scroll, inc_scroll;

    new_scroll = p_pane->scroll.x;

    inc_scroll = (ho_win_state.metrics.scroll.x * zDelta) / WHEEL_DELTA;

    new_scroll -= inc_scroll;

    scroll_pane(p_docu, p_view, event_handler_scroll, new_scroll);
}

_Check_return_
static BOOL
host_onMouseHWheel(
    _InVal_     int xPos,
    _InVal_     int yPos,
    _InVal_     int zDelta,
    _InVal_     UINT fwKeys,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler)
{
    POINT point;

    point.x = xPos;
    point.y = yPos;

    if(ho_win_state.drag.enabled)
        return(FALSE);

    if(fwKeys & MK_CONTROL)
    {   /* Zoom the whole view (button away from screen -> negative delta -> smaller text, i.e. lower scale factor) */
        view_set_scale_from_wheel_delta(p_docu, p_view, (zDelta * 10) / WHEEL_DELTA);
    }
    else
    {   /* Scroll the corresponding horizontal scroll bar */
        host_view_scroll_wheel_h(p_docu, p_view, event_handler, zDelta);
    }

    return(TRUE); /* MUST indicate this to stop OS doing emulation */
}

static void
host_onClose(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
#if defined(USE_GLOBAL_CLIPBOARD)
    host_onClose_process_clipboard(p_docu, p_view);
#endif /* USE_GLOBAL_CLIPBOARD */

    process_close_request(p_docu, p_view, FALSE, TRUE /*closing_a_view*/, FALSE);

    if(!some_document_windows())
    {
        PostQuitMessage(0);
    }
}

/* The pointer is tracked from when it first moves in a window until it leaves, so a window
 * is told once when the pointer enters it */

static HWND g_current_track_window = HOST_WND_NONE;

extern void
host_clear_tracking_for_window(
    _HwndRef_   HWND hwnd)
{
    if(g_current_track_window == hwnd)
        g_current_track_window = HOST_WND_NONE;
}

_Check_return_
extern BOOL
host_set_tracking_for_window(
    _HwndRef_   HWND hwnd)
{
    if(g_current_track_window == hwnd)
        return(FALSE);

    g_current_track_window = hwnd;
    return(TRUE);
}

/* the window that last saw the pointer, which is where a cursor shape is set */
static HWND g_hwnd_pointer = HOST_WND_NONE;

PROC_EVENT_PROTO(static, null_event_ho_win)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER(p_data);

    switch(t5_message)
    {
    case T5_MSG_WIN_HOST_NULL:
        return(ho_win_maeve_service_win_host_null());

    default: default_unhandled();
        return(STATUS_OK);
    }
}

/* Qt keys that are not characters, as the Windows virtual key that keys.c converts */

static UINT
vk_from_qt_key(
    _InVal_     int32_t qt_key)
{
    switch(qt_key)
    {
    case 0x01000000: return(VK_ESCAPE);
    case 0x01000001: return(VK_TAB);
    case 0x01000002: return(VK_TAB);    /* backtab: shift is also down */
    case 0x01000003: return(VK_BACK);
    case 0x01000004: return(VK_RETURN);
    case 0x01000005: return(VK_RETURN); /* keypad enter */
    case 0x01000006: return(VK_INSERT);
    case 0x01000007: return(VK_DELETE);
    case 0x01000010: return(VK_HOME);
    case 0x01000011: return(VK_END);
    case 0x01000012: return(VK_LEFT);
    case 0x01000013: return(VK_UP);
    case 0x01000014: return(VK_RIGHT);
    case 0x01000015: return(VK_DOWN);
    case 0x01000016: return(VK_PRIOR);
    case 0x01000017: return(VK_NEXT);

    default:
        if((qt_key >= 0x01000030) && (qt_key <= 0x0100003e)) /* F1..F15 */
            return(VK_F1 + (UINT) (qt_key - 0x01000030));
        return(0);
    }
}

/* ---- painting --------------------------------------------------------------------------- */

static inline void
li_fill(
    _InoutRef_  QTH_PAINTER * p_painter,
    _InVal_     int left,
    _InVal_     int top,
    _InVal_     int right,
    _InVal_     int bottom,
    _InVal_     QTH_RGB rgb)
{
    const QTH_RECT rect = { left, top, right, bottom };

    if((left < right) && (top < bottom))
        qth_paint_fill_rect(p_painter, &rect, rgb);
}

/* the back window shows only what is around the panes: margins in grey, split grab points, and a frame */

static void
li_paint_back_window(
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  QTH_PAINTER * p_painter)
{
    const QTH_RGB ltgrey = { 192, 192, 192 };
    const QTH_RGB black = { 0, 0, 0 };
    POINT split_outer;
    PANE_ID pane_id;

    split_outer.x = p_view->pane_posn.grab.x + p_view->pane_posn.grab_size.x;
    split_outer.y = p_view->pane_posn.grab.y + p_view->pane_posn.grab_size.y;

    li_fill(p_painter, p_view->inner_frame_gdi_rect.tl.x, p_view->inner_frame_gdi_rect.tl.y, p_view->pane_posn.start.x, p_view->pane_posn.start.y, ltgrey);
    li_fill(p_painter, p_view->pane_posn.max.x, p_view->inner_frame_gdi_rect.tl.y, p_view->inner_frame_gdi_rect.br.x, p_view->pane_posn.start.y, ltgrey);
    li_fill(p_painter, p_view->pane_posn.max.x, p_view->pane_posn.max.y, p_view->inner_frame_gdi_rect.br.x, p_view->inner_frame_gdi_rect.br.y, ltgrey);
    li_fill(p_painter, p_view->inner_frame_gdi_rect.tl.x, p_view->pane_posn.max.y, p_view->pane_posn.start.x, p_view->inner_frame_gdi_rect.br.y, ltgrey);

    if(p_view->flags.horz_split_on && (!p_view->flags.pseudo_horz_split_on))
        li_fill(p_painter, p_view->pane_posn.grab.x, p_view->inner_frame_gdi_rect.tl.y, split_outer.x, p_view->pane_posn.max.y, ltgrey);

    if(p_view->flags.vert_split_on && (!p_view->flags.pseudo_vert_split_on))
        li_fill(p_painter, p_view->inner_frame_gdi_rect.tl.x, p_view->pane_posn.grab.y, p_view->pane_posn.max.x, split_outer.y, ltgrey);

    /* Black the split pick up points */
    li_fill(p_painter, p_view->pane_posn.grab.x, p_view->pane_posn.max.y, split_outer.x, p_view->inner_frame_gdi_rect.br.y, black);
    li_fill(p_painter, p_view->pane_posn.max.x, p_view->pane_posn.grab.y, p_view->inner_frame_gdi_rect.br.x, split_outer.y, black);

    /* Frame around all the panes in the view */
    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
    {
        const P_PANE p_pane = &p_view->pane[pane_id];
        const QTH_RECT frame = { p_pane->rect.left - PIXEL_HORZ, p_pane->rect.top - PIXEL_VERT, p_pane->rect.right + PIXEL_HORZ, p_pane->rect.bottom + PIXEL_VERT };

        if(HOST_WND_NONE == p_pane->hwnd)
            continue;

        if((p_view->flags.horz_border_on || p_view->flags.horz_ruler_on) && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_HORZ)))
        {
            li_fill(p_painter, p_pane->rect.left - PIXEL_HORZ, p_view->inner_frame_gdi_rect.tl.y, p_pane->rect.left, p_view->pane_posn.start.y, black);
            li_fill(p_painter, p_pane->rect.right, p_view->inner_frame_gdi_rect.tl.y, p_pane->rect.right + PIXEL_HORZ, p_view->pane_posn.start.y, black);
        }

        if((p_view->flags.vert_border_on || p_view->flags.vert_ruler_on) && ((pane_id == WIN_PANE) || (pane_id == WIN_PANE_SPLIT_VERT)))
        {
            li_fill(p_painter, p_view->inner_frame_gdi_rect.tl.x, p_pane->rect.top - PIXEL_VERT, p_view->pane_posn.start.x, p_pane->rect.top, black);
            li_fill(p_painter, p_view->inner_frame_gdi_rect.tl.x, p_pane->rect.bottom, p_view->pane_posn.start.x, p_pane->rect.bottom + PIXEL_VERT, black);
        }

        qth_paint_frame_rect(p_painter, &frame, black);
    }
}

/* Paint a pane, ruler or border: ask the object that owns it to redraw, with a redraw context for this painter */

static void
li_paint_pane_window(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  QTH_PAINTER * p_painter,
    _In_        const QTH_RECT * p_exposed,
    _InVal_     EVENT_HANDLER event_handler)
{
    const PC_HOST_EVENT_DESC p_host_event_desc = &host_event_desc_table[event_handler];
    const P_PANE p_pane = &p_view->pane[p_host_event_desc->pane_id];
    REDRAW_CONTEXT_CACHE redraw_context_cache;
    VIEWEVENT_REDRAW viewevent_redraw;
    const P_VIEWEVENT_REDRAW p_viewevent_redraw = &viewevent_redraw;
    const P_REDRAW_CONTEXT p_redraw_context = &p_viewevent_redraw->redraw_context;

    zero_struct_ptr(p_viewevent_redraw);
    zero_struct_fn(redraw_context_cache);
    p_redraw_context->p_redraw_context_cache = &redraw_context_cache;

    // define the redraw flags from the pending update
    if(ho_win_state.update.redraw_flags_valid)
        p_viewevent_redraw->flags = ho_win_state.update.redraw_flags;
    else
    {
        p_viewevent_redraw->flags.show_content = TRUE;
        p_viewevent_redraw->flags.show_selection = TRUE;
    }

    p_viewevent_redraw->p_view = p_view;
    p_viewevent_redraw->p_pane = p_pane;
    p_viewevent_redraw->area.p_view = p_view;

    p_redraw_context->p_docu = p_docu;
    p_redraw_context->p_view = p_view;

    p_redraw_context->linux_ctx.painter = p_painter;
    p_redraw_context->linux_ctx.paint_rect.tl.x = p_exposed->left;
    p_redraw_context->linux_ctx.paint_rect.tl.y = p_exposed->top;
    p_redraw_context->linux_ctx.paint_rect.br.x = p_exposed->right;
    p_redraw_context->linux_ctx.paint_rect.br.y = p_exposed->bottom;

    p_redraw_context->display_mode = p_view->display_mode;
    p_redraw_context->border_width.x = p_redraw_context->border_width.y = p_docu->page_def.grid_size;

    host_redraw_context_set_host_xform(p_redraw_context, &p_view->host_xform[p_host_event_desc->xform_index]);
    host_redraw_context_fillin(p_redraw_context);

    // Convert the scroll posn to a suitable origin
    p_redraw_context->gdi_org.x = (p_host_event_desc->scroll_x) ? p_pane->scroll.x : 0;
    p_redraw_context->gdi_org.y = (p_host_event_desc->scroll_y) ? p_pane->scroll.y : 0;

    host_paint_start(p_redraw_context);

    p_redraw_context->linux_ctx.host_machine_clip_rect = p_redraw_context->linux_ctx.paint_rect;

    { // Convert the redraw rectangle to a suitable view rectangle
    GDI_RECT gdi_rect;

    gdi_rect.tl.x = p_exposed->left   + p_redraw_context->gdi_org.x;
    gdi_rect.tl.y = p_exposed->top    + p_redraw_context->gdi_org.y;
    gdi_rect.br.x = p_exposed->right  + p_redraw_context->gdi_org.x;
    gdi_rect.br.y = p_exposed->bottom + p_redraw_context->gdi_org.y;

    pixit_rect_from_window_rect(&p_viewevent_redraw->area.rect, &gdi_rect, &p_redraw_context->host_xform);
    } /*block*/

    (* p_host_event_desc->p_proc_event) (p_docu, T5_EVENT_REDRAW, &viewevent_redraw);

    host_paint_end(p_redraw_context);
}

static void
li_on_paint(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _InoutRef_  QTH_PAINTER * p_painter,
    _In_        const QTH_RECT * p_exposed)
{
    BOOL call_maeve = TRUE;

    switch(event_handler)
    {
    case EVENT_HANDLER_BACK_WINDOW:
        li_paint_back_window(p_view, p_painter);
        break;

    default: default_unhandled();
#if CHECKING
    case EVENT_HANDLER_BORDER_HORZ:
    case EVENT_HANDLER_BORDER_VERT:
    case EVENT_HANDLER_BORDER_HORZ_SPLIT:
    case EVENT_HANDLER_BORDER_VERT_SPLIT:
    case EVENT_HANDLER_RULER_HORZ:
    case EVENT_HANDLER_RULER_VERT:
    case EVENT_HANDLER_RULER_HORZ_SPLIT:
    case EVENT_HANDLER_RULER_VERT_SPLIT:
#endif
        call_maeve = FALSE;
        /*FALLTHRU*/

    case EVENT_HANDLER_PANE:
    case EVENT_HANDLER_PANE_SPLIT_VERT:
    case EVENT_HANDLER_PANE_SPLIT_HORZ:
    case EVENT_HANDLER_PANE_SPLIT_DIAG:
        li_paint_pane_window(p_docu, p_view, p_painter, p_exposed, event_handler);

        if(call_maeve)
            status_assert(maeve_event(p_docu, T5_EVENT_REDRAW_AFTER, P_DATA_NONE));
        break;
    }
}

/* ---- the one handler ---------------------------------------------------------------------- */

static void
li_window_event(
    QTH_WINDOW * p_qth_window,
    const QTH_EVENT * p_event,
    void * client)
{
    const HWND hwnd = p_qth_window;
    P_VIEW p_view;
    U16 event_handler_id;
    const P_DOCU p_docu = viewid_unpack((U32) (uintptr_t) client, &p_view, &event_handler_id);
    const EVENT_HANDLER event_handler = (EVENT_HANDLER) event_handler_id;

    if(P_DOCU_NONE == p_docu)
        return;

    if(!VIEW_NOT_NONE(p_view) || (event_handler >= NUMEVENTS))
        return;

    g_hwnd_pointer = hwnd;

    switch(p_event->type)
    {
    case QTH_EVENT_PAINT:
        li_on_paint(p_docu, p_view, event_handler, p_event->u.paint.p_painter, &p_event->u.paint.exposed);
        break;

    case QTH_EVENT_RESIZE:
        if(EVENT_HANDLER_BACK_WINDOW == event_handler)
        {
            calc_pane_posn(p_docu, p_view);
            set_pane_posn(p_docu, p_view);
            refresh_back_window(p_view);
        }
        break;

    case QTH_EVENT_MOUSE:
        {
        const int x = p_event->u.mouse.x;
        const int y = p_event->u.mouse.y;
        const UINT keyFlags = ((p_event->u.mouse.modifiers & QTH_MOD_CTRL) ? MK_CONTROL : 0) | ((p_event->u.mouse.modifiers & QTH_MOD_SHIFT) ? MK_SHIFT : 0);
        const BOOL right = (0 != (p_event->u.mouse.buttons & QTH_BUTTON_RIGHT));

        switch(p_event->u.mouse.action)
        {
        case QTH_MOUSE_PRESS:
        case QTH_MOUSE_DOUBLE:
            if(0 != (p_event->u.mouse.buttons & (QTH_BUTTON_LEFT | QTH_BUTTON_RIGHT)))
            {
                const BOOL double_click = (QTH_MOUSE_DOUBLE == p_event->u.mouse.action);
                const UINT uiMsg = double_click ? (right ? WM_RBUTTONDBLCLK : WM_LBUTTONDBLCLK)
                                                : (right ? WM_RBUTTONDOWN   : WM_LBUTTONDOWN);

                if(ho_win_state.drag.enabled)
                    break;

                host_onButtonDown(hwnd, uiMsg, x, y, keyFlags, p_docu, p_view, event_handler);
            }
            break;

        case QTH_MOUSE_RELEASE:
            stop_drag_monitor();

            if(ho_win_state.drag.enabled)
                host_onButtonUp_dragging(hwnd, x, y, keyFlags);
            break;

        case QTH_MOUSE_MOVE:
            host_onMouseMove(hwnd, x, y, keyFlags, p_docu, p_view, event_handler);
            break;

        default:
            break;
        }
        break;
        }

    case QTH_EVENT_LEAVE:
        host_onMouseLeave(hwnd, p_docu, p_view, event_handler);
        break;

    case QTH_EVENT_WHEEL:
        {
        /* the wheel goes to the pane that has the caret, as the focus window gets it on Windows */
        const HWND hwnd_caret = ho_win_state.caret.visible.hwnd;
        P_VIEW p_view_caret = p_view;
        U16 event_handler_caret = event_handler_id;
        const UINT fwKeys = (p_event->u.wheel.modifiers & QTH_MOD_CTRL) ? MK_CONTROL : 0;
        P_DOCU p_docu_caret = p_docu;

        if(HOST_WND_NONE != hwnd_caret)
        {
            DOCNO docno = resolve_hwnd(hwnd_caret, &p_view_caret, &event_handler_caret);

            if(DOCNO_NONE != docno)
                p_docu_caret = p_docu_from_docno_valid(docno);
            else
            {
                p_view_caret = p_view;
                event_handler_caret = event_handler_id;
            }
        }

        if(0 != p_event->u.wheel.delta_y)
            host_onMouseWheel(p_event->u.wheel.x, p_event->u.wheel.y, p_event->u.wheel.delta_y, fwKeys, p_docu_caret, p_view_caret, (EVENT_HANDLER) event_handler_caret);

        if(0 != p_event->u.wheel.delta_x)
            host_onMouseHWheel(p_event->u.wheel.x, p_event->u.wheel.y, -p_event->u.wheel.delta_x, fwKeys, p_docu_caret, p_view_caret, (EVENT_HANDLER) event_handler_caret);
        break;
        }

    case QTH_EVENT_KEY:
        {
        const UINT vk = vk_from_qt_key(p_event->u.key.code);

        if(0 != vk)
            (void) host_onKeyDown(vk, p_docu);
        else if(0 != p_event->u.key.latin1)
            host_onChar((TCHAR) p_event->u.key.latin1, p_docu);

        /* a Windows key up would flush any keys still waiting */
        if(!host_keys_in_buffer())
            status_assert(host_key_cache_emit_events());
        break;
        }

    case QTH_EVENT_SCROLL:
        if(p_event->u.scroll.vertical)
            host_onVScroll(SB_THUMBPOSITION, p_event->u.scroll.value, p_docu, p_view, event_handler);
        else
            host_onHScroll(SB_THUMBPOSITION, p_event->u.scroll.value, p_docu, p_view, event_handler);
        break;

    case QTH_EVENT_FOCUS:
        if(p_event->u.focus.gained)
            host_onSetFocus(p_docu);
        else
            status_assert(host_key_cache_emit_events_for(docno_from_p_docu(p_docu)));
        break;

    case QTH_EVENT_TIMER:
        switch(p_event->u.timer.id)
        {
        case DRAG_TIMER_ID:
            host_onTimer_drag();
            break;

        case TRIPLE_CLICK_TIMER_ID:
            disable_triple_click(); /* triple click is timing out */
            break;

        default:
            break;
        }
        break;

    case QTH_EVENT_CLOSE:
        host_onClose(p_docu, p_view);
        break;

    default:
        break;
    }
}

/* ---- views ----------------------------------------------------------------------------- */

/* Setup the structure for the specified view port.  We are given its opening position
 * and size. Our job is to make the back window, the pane windows are attached at open time.
 */

_Check_return_
extern STATUS
host_view_init(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InRef_     PC_PIXIT_POINT p_tl,
    _InRef_     PC_PIXIT_SIZE p_size)
{
    GDI_POINT tl = { 0, 0 };
    GDI_SIZE size;
    QTH_RECT bounds;
    HWND hwnd;

    /* TODO: menus: ho_create_docu_menus() and the menu bar, once menus exist on Linux */

    /* Convert from pixits to pixels for the top left of the view. Windows lets the system pick
     * where a window opens, and so do we */
    if((p_tl->x) && (p_tl->y))
        window_point_from_pixit_point(&tl, p_tl, &p_view->host_xform[XFORM_BACK]);

    if((p_size->cx) && (p_size->cy))
        window_point_from_pixit_point((P_GDI_POINT) &size, (PC_PIXIT_POINT) p_size, &p_view->host_xform[XFORM_BACK]);
    else
    {
        GDI_SIZE pixels_per_inch;
        host_get_pixel_size(NULL /*screen*/, &pixels_per_inch.cx, &pixels_per_inch.cy);
        size.cx = (89 * pixels_per_inch.cx) / 10;
        size.cy =   7 * pixels_per_inch.cy;
    }

    bounds.left = tl.x;
    bounds.top = tl.y;
    bounds.right = tl.x + size.cx;
    bounds.bottom = tl.y + size.cy;

    if(HOST_WND_NONE == (hwnd = li_window_create(p_docu, p_view, EVENT_HANDLER_BACK_WINDOW, QTH_WINDOW_DOCUMENT, HOST_WND_NONE, &bounds)))
    {
        host_view_destroy(p_docu, p_view);
        return(create_error(ERR_TOOMANYWINDOWS));
    }

    return(STATUS_OK);
}

/* Attempt to tidy up the viewer window.  This is called when the view onto the document is closed.
 * We must attempt to tidy up (close the background) and then remove all the panes as required.
 */

extern void
host_view_destroy(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    if(NULL == p_view)
        return;

    {
    PANE_ID pane_id;
    for(pane_id = PANE_ID_START; pane_id < NUMPANE; PANE_ID_INCR(pane_id))
        if(HOST_WND_NONE != p_view->pane[pane_id].hwnd)
            view_pane_destroy(p_view, pane_id);
    } /*block*/

    if(HOST_WND_NONE == p_view->main[WIN_BACK].hwnd)
        return;

    host_clear_tracking_for_window(p_view->main[WIN_BACK].hwnd);

    li_window_destroy(p_view, EVENT_HANDLER_BACK_WINDOW);
}

/* Show the viewer.  In this case we show the background window and then the panes as required. */

extern void
host_view_show(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    if(HOST_WND_NONE == p_view->main[WIN_BACK].hwnd)
        return;

    status_assert(ensure_pane_set(p_docu, p_view));
    calc_pane_posn(p_docu, p_view);
    set_pane_posn(p_docu, p_view);
    show_view_pane_set(p_view);
    qth_window_show(p_view->main[WIN_BACK].hwnd, 1);
}

/* Attempt to re-open a viewer.  This is called as part of the viewer has been changed - i.e. a resize,
 * or, the viewer information has been updated (split etc!)
 */

extern void
host_view_reopen(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    if(HOST_WND_NONE == p_view->main[WIN_BACK].hwnd)
        return;

    status_assert(ensure_pane_set(p_docu, p_view));
    calc_pane_posn(p_docu, p_view);
    set_pane_posn(p_docu, p_view);
    show_view_pane_set(p_view);
    refresh_back_window(p_view);
    qth_window_show(p_view->main[WIN_BACK].hwnd, 1);
}

/* The mouse pointer: the shape is set on the window that last saw the pointer */

extern void
host_set_pointer_shape(
    _In_        POINTER_SHAPE pointer_shape)
{
    int cursor = QTH_CURSOR_ARROW;

    switch(pointer_shape)
    {
    case POINTER_DRAG_ALL:
    case POINTER_SPLIT_ALL:             cursor = QTH_CURSOR_SIZE_ALL; break;
    case POINTER_DRAG_LEFTRIGHT:
    case POINTER_DRAG_COLUMN:
    case POINTER_DRAG_COLUMN_LEFT:
    case POINTER_DRAG_COLUMN_RIGHT:
    case POINTER_SPLIT_LEFTRIGHT:       cursor = QTH_CURSOR_SIZE_H; break;
    case POINTER_DRAG_UPDOWN:
    case POINTER_DRAG_ROW:
    case POINTER_DRAG_ROW_UP:
    case POINTER_DRAG_ROW_DOWN:
    case POINTER_SPLIT_UPDOWN:          cursor = QTH_CURSOR_SIZE_V; break;
    default:                            break;
    }

    if(HOST_WND_NONE != g_hwnd_pointer)
        qth_window_set_cursor(g_hwnd_pointer, cursor);
}

/* end of ho_win.c */
