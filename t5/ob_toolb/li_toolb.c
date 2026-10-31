/* li_toolb.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The Linux counterpart of wi_toolb.c: the toolbar and status line as windows of the view.
 *
 * Where the shared code is the same as Windows' (placing the windows, and the mouse handling over the
 * toolbar buttons) it is taken from there, with the few Win32 calls it makes done over qt_host.h. */

#include "common/gflags.h"

#include "ob_toolb/ob_toolb.h"

#if LINUX

#include "ob_skel/linux/qt_host.h"

#define MK_CONTROL  0x0008

typedef GDI_SIZE SIZE;

#define SWP_NOZORDER 0

static inline void
GetClientRect(
    HOST_WND hwnd,
    RECT * p_rect)
{
    QTH_RECT bounds;

    qth_window_get_bounds(hwnd, &bounds);

    p_rect->left = p_rect->top = 0;
    p_rect->right = bounds.right - bounds.left;
    p_rect->bottom = bounds.bottom - bounds.top;
}

static inline void
SetWindowPos(
    HOST_WND hwnd,
    void * p_insert_after,
    int x,
    int y,
    int cx,
    int cy,
    int flags)
{
    const QTH_RECT bounds = { x, y, x + cx, y + cy };

    (void) p_insert_after;
    (void) flags;

    qth_window_set_bounds(hwnd, &bounds);
    qth_window_show(hwnd, (cx > 0) && (cy > 0));
}

static inline void
InvalidateRect(
    HOST_WND hwnd,
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

static HOST_WND g_hwnd_capture = HOST_WND_NONE;

static inline void
SetCapture(
    HOST_WND hwnd)
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

/* from ho_win.c */

_Check_return_
extern BOOL
host_set_tracking_for_window(
    _HwndRef_   HWND hwnd);

extern void
host_clear_tracking_for_window(
    _HwndRef_   HWND hwnd);

_Check_return_
extern STATUS
ho_create_main_window(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     EVENT_HANDLER event_handler,
    _Out_       HOST_WND * p_slot,
    _In_        const QTH_RECT * p_bounds);

extern void
ho_destroy_main_window(
    HOST_WND * p_slot);

/* There are no tooltips: the status line says what a button does */

static void
toolbar_set_tooltip_text(
    _ViewRef_   P_VIEW p_view,
    _InRef_     PC_UI_TEXT p_ui_text)
{
    UNREFERENCED_PARAMETER_ViewRef_(p_view);
    UNREFERENCED_PARAMETER_InRef_(p_ui_text);
}

/* ---- Placing the windows ------------------------------------------------------------------ */

static void
t5_msg_frame_windows_describe_toolbar(
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_TOOLBAR].hwnd;

    if(global_preferences.disable_toolbar)
        return;

    if(HOST_WND_NONE != hwnd)
        p_msg_frame_window->gdi_rect.tl.y += toolbar_height_pixels;
}

static void
t5_msg_frame_windows_describe_upper_status_line(
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_UPPER_STATUS_LINE].hwnd;

    if(global_preferences.disable_status_line)
        return;

    if(HOST_WND_NONE != hwnd)
        p_msg_frame_window->gdi_rect.tl.y += status_line_height_pixels;
}

static void
t5_msg_frame_windows_describe_lower_status_line(
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_LOWER_STATUS_LINE].hwnd;

    if(global_preferences.disable_status_line)
        return;

    if(HOST_WND_NONE != hwnd)
        p_msg_frame_window->gdi_rect.br.y -= status_line_height_pixels;
}

T5_MSG_PROTO(extern, t5_msg_frame_windows_describe, _InoutRef_ P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const P_VIEW p_view = p_msg_frame_window->p_view;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);
    UNREFERENCED_PARAMETER_InVal_(t5_message);

    t5_msg_frame_windows_describe_toolbar(p_view, p_msg_frame_window);

    t5_msg_frame_windows_describe_upper_status_line(p_view, p_msg_frame_window);

    t5_msg_frame_windows_describe_lower_status_line(p_view, p_msg_frame_window);

    return(STATUS_OK);
}

static void
toolbar_posn_user_buttons(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const ARRAY_INDEX view_rows = array_elements(&p_view->toolbar.h_toolbar_view_row_desc);
    ARRAY_INDEX view_row_idx;
    P_T5_TOOLBAR_VIEW_ROW_DESC p_t5_toolbar_view_row_desc = array_range(&p_view->toolbar.h_toolbar_view_row_desc, T5_TOOLBAR_VIEW_ROW_DESC, 0, view_rows);

    for(view_row_idx = 0; view_row_idx < view_rows; ++view_row_idx, ++p_t5_toolbar_view_row_desc)
    {
        const ARRAY_INDEX row_tools = array_elements(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc);
        ARRAY_INDEX row_tool_idx;
        P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc = array_range(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc, T5_TOOLBAR_VIEW_TOOL_DESC, 0, row_tools);

        for(row_tool_idx = 0; row_tool_idx < row_tools; ++row_tool_idx, ++p_t5_toolbar_view_tool_desc)
        {
            const P_T5_TOOLBAR_DOCU_TOOL_DESC p_t5_toolbar_docu_tool_desc = array_ptr(&p_docu->h_toolbar, T5_TOOLBAR_DOCU_TOOL_DESC, p_t5_toolbar_view_tool_desc->docu_tool_index);
            const PC_T5_TOOLBAR_TOOL_DESC p_t5_toolbar_tool_desc = p_t5_toolbar_docu_tool_desc->p_t5_toolbar_tool_desc;

            switch(ENUM_UNPACK(T5_TOOLBAR_TOOL_TYPE, p_t5_toolbar_tool_desc->bits.type))
            {
            default:
                break;

            case T5_TOOLBAR_TOOL_TYPE_USER:
                {
                if(p_t5_toolbar_view_tool_desc->client_handle)
                {
                    T5_TOOLBAR_TOOL_USER_POSN_SET t5_toolbar_tool_user_posn_set;

                    t5_toolbar_tool_user_posn_set.client_handle = p_t5_toolbar_view_tool_desc->client_handle;
                    t5_toolbar_tool_user_posn_set.p_t5_toolbar_tool_desc = p_t5_toolbar_tool_desc;
                    t5_toolbar_tool_user_posn_set.viewno = viewno_from_p_view(p_view);

                    { /* Ensure clipped to parent client area */
                    const HOST_WND hwnd = p_view->main[WIN_TOOLBAR].hwnd;
                    GDI_SIZE PixelsPerInch;

                    host_get_pixel_size(hwnd, &PixelsPerInch.cx, &PixelsPerInch.cy); /* Get current pixel size for this window e.g. 96 or 120 */

                    t5_toolbar_tool_user_posn_set.pixit_rect.tl.x = MAX(p_t5_toolbar_view_tool_desc->pixit_rect.tl.x, (p_msg_frame_window->gdi_rect.tl.x * PIXITS_PER_INCH) / PixelsPerInch.cx);
                    t5_toolbar_tool_user_posn_set.pixit_rect.tl.y = MAX(p_t5_toolbar_view_tool_desc->pixit_rect.tl.y, (p_msg_frame_window->gdi_rect.tl.y * PIXITS_PER_INCH) / PixelsPerInch.cy);
                    t5_toolbar_tool_user_posn_set.pixit_rect.br.x = MIN(p_t5_toolbar_view_tool_desc->pixit_rect.br.x, (p_msg_frame_window->gdi_rect.br.x * PIXITS_PER_INCH) / PixelsPerInch.cx);
                    t5_toolbar_tool_user_posn_set.pixit_rect.br.y = MIN(p_t5_toolbar_view_tool_desc->pixit_rect.br.y, (p_msg_frame_window->gdi_rect.br.y * PIXITS_PER_INCH) / PixelsPerInch.cy);
                    } /*block*/

                    status_assert(object_call_id(p_t5_toolbar_tool_desc->command_object_id, p_docu, T5_MSG_TOOLBAR_TOOL_USER_POSN_SET, &t5_toolbar_tool_user_posn_set));
                }

                break;
                }
            }
        }
    }
}

static void
t5_msg_frame_windows_posn_toolbar(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_TOOLBAR].hwnd;
    RECT client_rect;
    SIZE size;

    GetClientRect(hwnd, &client_rect);

    /* resize to whatever width we should now be */
    size.cx = p_msg_frame_window->gdi_rect.br.x - p_msg_frame_window->gdi_rect.tl.x;
    size.cy = !global_preferences.disable_toolbar ? toolbar_height_pixels : 0;
    SetWindowPos(hwnd, NULL, p_msg_frame_window->gdi_rect.tl.x, p_msg_frame_window->gdi_rect.tl.y, size.cx, size.cy, SWP_NOZORDER);

    if(size.cy && (size.cy != (client_rect.bottom - client_rect.top)))
        InvalidateRect(hwnd, NULL, TRUE);

    if(size.cy)
        toolbar_posn_user_buttons(p_docu, p_view, p_msg_frame_window);

    p_msg_frame_window->gdi_rect.tl.y += size.cy;
}

static void
t5_msg_frame_windows_posn_upper_status_line(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_UPPER_STATUS_LINE].hwnd;
    RECT client_rect;
    SIZE size;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    GetClientRect(hwnd, &client_rect);

    /* resize to whatever width we should now be */
    size.cx = p_msg_frame_window->gdi_rect.br.x - p_msg_frame_window->gdi_rect.tl.x;
    size.cy = !global_preferences.disable_status_line ? status_line_height_pixels : 0;
    SetWindowPos(hwnd, NULL, p_msg_frame_window->gdi_rect.tl.x, p_msg_frame_window->gdi_rect.tl.y, size.cx, size.cy, SWP_NOZORDER);

    if(size.cy && (size.cy != (client_rect.bottom - client_rect.top)))
        InvalidateRect(hwnd, NULL, TRUE);

    p_msg_frame_window->gdi_rect.tl.y += size.cy;
}

static void
t5_msg_frame_windows_posn_lower_status_line(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const HOST_WND hwnd = p_view->main[WIN_LOWER_STATUS_LINE].hwnd;
    RECT client_rect;
    SIZE size;

    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    GetClientRect(hwnd, &client_rect);

    /* resize to whatever width we should now be */
    size.cx = p_msg_frame_window->gdi_rect.br.x - p_msg_frame_window->gdi_rect.tl.x;
    size.cy = !global_preferences.disable_status_line ? status_line_height_pixels : 0;
    SetWindowPos(hwnd, NULL, p_msg_frame_window->gdi_rect.tl.x, p_msg_frame_window->gdi_rect.br.y - size.cy, size.cx, size.cy, SWP_NOZORDER);

    if(size.cy && (size.cy != (client_rect.bottom - client_rect.top)))
        InvalidateRect(hwnd, NULL, TRUE);

    p_msg_frame_window->gdi_rect.br.y -= size.cy;
}

T5_MSG_PROTO(extern, t5_msg_frame_windows_posn, _InoutRef_ P_MSG_FRAME_WINDOW p_msg_frame_window)
{
    const P_VIEW p_view = p_msg_frame_window->p_view;
    
    UNREFERENCED_PARAMETER_InVal_(t5_message);

    if(HOST_WND_NONE != p_view->main[WIN_TOOLBAR].hwnd)
        t5_msg_frame_windows_posn_toolbar(p_docu, p_view, p_msg_frame_window);

    if(HOST_WND_NONE != p_view->main[WIN_UPPER_STATUS_LINE].hwnd)
        t5_msg_frame_windows_posn_upper_status_line(p_docu, p_view, p_msg_frame_window);

    if(HOST_WND_NONE != p_view->main[WIN_LOWER_STATUS_LINE].hwnd)
        t5_msg_frame_windows_posn_lower_status_line(p_docu, p_view, p_msg_frame_window);

    return(STATUS_OK);
}

static void
toolbar_onButtonDown(
    _HwndRef_   HWND hwnd,
    _InVal_     BOOL fDoubleClick,
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     UINT keyFlags,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     BOOL right_button_down)
{
    const BOOL ctrl_pressed = (0 != (keyFlags & MK_CONTROL));
    PIXIT_POINT pixit_point;
    P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc;

    {
    GDI_POINT gdi_point;
    gdi_point.x = x;
    gdi_point.y = y;
    pixit_point_from_window_point(&pixit_point, &gdi_point, &p_view->host_xform[XFORM_TOOLBAR]);
    } /*block*/

    status_assert(host_key_cache_emit_events());

    if(fDoubleClick)
        return;

    if(right_button_down)
        return;

    p_t5_toolbar_view_tool_desc = tool_from_point(p_view, &pixit_point);

    if(NULL != p_t5_toolbar_view_tool_desc)
    {
        const P_T5_TOOLBAR_DOCU_TOOL_DESC p_t5_toolbar_docu_tool_desc = array_ptr(&p_docu->h_toolbar, T5_TOOLBAR_DOCU_TOOL_DESC, p_t5_toolbar_view_tool_desc->docu_tool_index);

        if(tool_enabled_query(p_t5_toolbar_docu_tool_desc))
        {
            if(p_t5_toolbar_docu_tool_desc->p_t5_toolbar_tool_desc->bits.immediate)
            {
                const S32 alternate = /*right_button_down ||*/ ctrl_pressed;
                status_line_clear(p_docu, STATUS_LINE_LEVEL_BACKWINDOW_CONTROLS);
                execute_tool(p_docu, p_view, p_t5_toolbar_docu_tool_desc, alternate);
                return;
            }

            p_t5_toolbar_view_tool_desc->had_mouse_down = 1;
            p_t5_toolbar_view_tool_desc->button_down = 2;
            redraw_tool(p_docu, p_view, p_t5_toolbar_view_tool_desc);
            SetCapture(hwnd);
            /* and wait for the WM_LBUTTONUP */
        }
    }
}

static void
toolbar_onButtonUp(
    _InVal_     int x,
    _InVal_     int y,
    _InVal_     UINT keyFlags,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     BOOL right_button_up)
{
    const BOOL ctrl_pressed = (0 != (keyFlags & MK_CONTROL));
    PIXIT_POINT pixit_point;
    P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc;

    ReleaseCapture();

    {
    GDI_POINT gdi_point;
    gdi_point.x = x;
    gdi_point.y = y;
    pixit_point_from_window_point(&pixit_point, &gdi_point, &p_view->host_xform[XFORM_TOOLBAR]);
    } /*block*/

    if(right_button_up)
        return;

    p_t5_toolbar_view_tool_desc = tool_from_point(p_view, &pixit_point);

    if(NULL != p_t5_toolbar_view_tool_desc)
        /* only act on mouse up event for the tool that had the mouse down */
        if(!p_t5_toolbar_view_tool_desc->had_mouse_down)
            p_t5_toolbar_view_tool_desc = NULL;

    { /* ensure all buttons out of tracking mouse down state */
    const ARRAY_INDEX view_rows = array_elements(&p_view->toolbar.h_toolbar_view_row_desc);
    ARRAY_INDEX view_row_idx;
    P_T5_TOOLBAR_VIEW_ROW_DESC p_t5_toolbar_view_row_desc = array_range(&p_view->toolbar.h_toolbar_view_row_desc, T5_TOOLBAR_VIEW_ROW_DESC, 0, view_rows);

    for(view_row_idx = 0; view_row_idx < view_rows; ++view_row_idx, ++p_t5_toolbar_view_row_desc)
    {
        const ARRAY_INDEX row_tools = array_elements(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc);
        ARRAY_INDEX row_tool_idx;
        P_T5_TOOLBAR_VIEW_TOOL_DESC this_p_t5_toolbar_view_tool_desc = array_range(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc, T5_TOOLBAR_VIEW_TOOL_DESC, 0, row_tools);

        for(row_tool_idx = 0; row_tool_idx < row_tools; ++row_tool_idx, ++this_p_t5_toolbar_view_tool_desc)
        {
            BOOL redraw = FALSE;

            if(this_p_t5_toolbar_view_tool_desc->had_mouse_down)
            {
                this_p_t5_toolbar_view_tool_desc->had_mouse_down = 0;
                redraw = TRUE;
            }

            if(0 != this_p_t5_toolbar_view_tool_desc->button_down)
            {
                this_p_t5_toolbar_view_tool_desc->button_down = 0;
                redraw = TRUE;
            }

            if(redraw)
                redraw_tool(p_docu, p_view, this_p_t5_toolbar_view_tool_desc);
        }
    }
    } /*block*/

    if(NULL != p_t5_toolbar_view_tool_desc)
    {
        const P_T5_TOOLBAR_DOCU_TOOL_DESC p_t5_toolbar_docu_tool_desc = array_ptr(&p_docu->h_toolbar, T5_TOOLBAR_DOCU_TOOL_DESC, p_t5_toolbar_view_tool_desc->docu_tool_index);
        const S32 alternate = /*right_button_up ||*/ ctrl_pressed;
        status_line_clear(p_docu, STATUS_LINE_LEVEL_BACKWINDOW_CONTROLS);
        execute_tool(p_docu, p_view, p_t5_toolbar_docu_tool_desc, alternate);
        return;
    }
}

_Check_return_
_Ret_maybenull_
static P_T5_TOOLBAR_VIEW_TOOL_DESC
toolbar_onMouseMove_find_view_tool_desc(
    _InVal_     int x,
    _InVal_     int y,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    PIXIT_POINT pixit_point;
    P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc;

    {
    GDI_POINT gdi_point;
    gdi_point.x = x;
    gdi_point.y = y;
    pixit_point_from_window_point(&pixit_point, &gdi_point, &p_view->host_xform[XFORM_TOOLBAR]);
    } /*block*/

    p_t5_toolbar_view_tool_desc = tool_from_point(p_view, &pixit_point);

    if(NULL != p_t5_toolbar_view_tool_desc)
    {
        const P_T5_TOOLBAR_DOCU_TOOL_DESC p_t5_toolbar_docu_tool_desc = array_ptr(&p_docu->h_toolbar, T5_TOOLBAR_DOCU_TOOL_DESC, p_t5_toolbar_view_tool_desc->docu_tool_index);
        PC_UI_TEXT p_ui_text = &p_t5_toolbar_docu_tool_desc->p_t5_toolbar_tool_desc->ui_text;
        if(0)
            toolbar_set_tooltip_text(p_view, p_ui_text);
        else
            status_line_set(p_docu, STATUS_LINE_LEVEL_DIALOG_CONTROLS, p_ui_text);
    }
    else
    {
        static const UI_TEXT ui_text = { UI_TEXT_TYPE_NONE };
        status_line_clear(p_docu, STATUS_LINE_LEVEL_DIALOG_CONTROLS);
        toolbar_set_tooltip_text(p_view, &ui_text);
    }

    return(p_t5_toolbar_view_tool_desc);
}

static void
toolbar_onMouseMove_update_buttons(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InRef_opt_ P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc)
{
    const ARRAY_INDEX view_rows = array_elements(&p_view->toolbar.h_toolbar_view_row_desc);
    ARRAY_INDEX view_row_idx;
    P_T5_TOOLBAR_VIEW_ROW_DESC p_t5_toolbar_view_row_desc = array_range(&p_view->toolbar.h_toolbar_view_row_desc, T5_TOOLBAR_VIEW_ROW_DESC, 0, view_rows);

    for(view_row_idx = 0; view_row_idx < view_rows; ++view_row_idx, ++p_t5_toolbar_view_row_desc)
    {
        const ARRAY_INDEX row_tools = array_elements(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc);
        ARRAY_INDEX row_tool_idx;
        P_T5_TOOLBAR_VIEW_TOOL_DESC this_p_t5_toolbar_view_tool_desc = array_range(&p_t5_toolbar_view_row_desc->h_toolbar_view_tool_desc, T5_TOOLBAR_VIEW_TOOL_DESC, 0, row_tools);

        for(row_tool_idx = 0; row_tool_idx < row_tools; ++row_tool_idx, ++this_p_t5_toolbar_view_tool_desc)
        {
            UINT required_state = 0;
            PTR_ASSERT(this_p_t5_toolbar_view_tool_desc);

            if(p_t5_toolbar_view_tool_desc == this_p_t5_toolbar_view_tool_desc)
            {
                if(this_p_t5_toolbar_view_tool_desc->had_mouse_down)
                    required_state = 2;
                else
                    required_state = 1;
            }
            else if(this_p_t5_toolbar_view_tool_desc->had_mouse_down)
                required_state = 1;
            else
                required_state = 0;

            if(required_state != this_p_t5_toolbar_view_tool_desc->button_down)
            {
                this_p_t5_toolbar_view_tool_desc->button_down = required_state;

                redraw_tool(p_docu, p_view, this_p_t5_toolbar_view_tool_desc);
            }
        }
    }
}

static void
toolbar_onMouseMove(
    _HwndRef_   HWND hwnd,
    _InVal_     int x,
    _InVal_     int y,
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    P_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc;

    if(host_set_tracking_for_window(hwnd))
    {
        trace_0(TRACE_APP_CLICK, TEXT("toolbar_onMouseMove T5_EVENT_POINTER_ENTERS_WINDOW"));
        status_line_clear(p_docu, STATUS_LINE_LEVEL_DIALOG_CONTROLS);
    }

    p_t5_toolbar_view_tool_desc = toolbar_onMouseMove_find_view_tool_desc(x, y, p_docu, p_view);

    toolbar_onMouseMove_update_buttons(p_docu, p_view, p_t5_toolbar_view_tool_desc);
}

/* ---- Events ------------------------------------------------------------------------------- */

/* paint, in ob_toolb.c, which has the toolbar's details */

extern void
paint_toolbar_linux(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  QTH_PAINTER * p_painter,
    _In_        const QTH_RECT * p_exposed,
    _InVal_     HOST_WND hwnd);

extern void
paint_status_line_linux(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InoutRef_  QTH_PAINTER * p_painter,
    _In_        const QTH_RECT * p_exposed,
    _InVal_     HOST_WND hwnd);

extern void
toolbar_window_event(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     HOST_WND hwnd,
    _InVal_     EVENT_HANDLER event_handler,
    _In_        const QTH_EVENT * p_event)
{
    const BOOL is_toolbar = (EVENT_HANDLER_TOOLBAR == event_handler);

    switch(p_event->type)
    {
    case QTH_EVENT_PAINT:
        if(is_toolbar)
            paint_toolbar_linux(p_docu, p_view, p_event->u.paint.p_painter, &p_event->u.paint.exposed, hwnd);
        else
            paint_status_line_linux(p_docu, p_view, p_event->u.paint.p_painter, &p_event->u.paint.exposed, hwnd);
        break;

    case QTH_EVENT_MOUSE:
        if(is_toolbar)
        {
            const int x = p_event->u.mouse.x;
            const int y = p_event->u.mouse.y;
            const UINT keyFlags = (p_event->u.mouse.modifiers & QTH_MOD_CTRL) ? MK_CONTROL : 0;
            const BOOL right = (0 != (p_event->u.mouse.buttons & QTH_BUTTON_RIGHT));

            switch(p_event->u.mouse.action)
            {
            case QTH_MOUSE_PRESS:
            case QTH_MOUSE_DOUBLE:
                toolbar_onButtonDown(hwnd, QTH_MOUSE_DOUBLE == p_event->u.mouse.action, x, y, keyFlags, p_docu, p_view, right);
                break;

            case QTH_MOUSE_RELEASE:
                toolbar_onButtonUp(x, y, keyFlags, p_docu, p_view, 0 != (p_event->u.mouse.buttons & QTH_BUTTON_RIGHT));
                break;

            case QTH_MOUSE_MOVE:
                toolbar_onMouseMove(hwnd, x, y, p_docu, p_view);
                break;

            default:
                break;
            }
        }
        break;

    case QTH_EVENT_LEAVE:
        host_clear_tracking_for_window(hwnd);

        if(is_toolbar)
        {
            static const UI_TEXT ui_text = { UI_TEXT_TYPE_NONE };

            status_line_clear(p_docu, STATUS_LINE_LEVEL_DIALOG_CONTROLS);
            toolbar_set_tooltip_text(p_view, &ui_text);
        }
        break;

    default:
        break;
    }
}

/* ---- Making and breaking the windows ------------------------------------------------------ */

static GDI_COORD
back_window_client_width(
    _ViewRef_   P_VIEW p_view)
{
    RECT client_rect;

    GetClientRect(p_view->main[WIN_BACK].hwnd, &client_rect);

    return(client_rect.right - client_rect.left);
}

extern void
li_toolbar_new(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     BOOL for_view_new)
{
    if(for_view_new)
    {
        const QTH_RECT bounds = { 0, 0, back_window_client_width(p_view), !global_preferences.disable_toolbar ? toolbar_height_pixels : 0 };

        status_assert(ho_create_main_window(p_docu, p_view, EVENT_HANDLER_TOOLBAR, &p_view->main[WIN_TOOLBAR].hwnd, &bounds));
    }
    else if(HOST_WND_NONE != p_view->main[WIN_TOOLBAR].hwnd)
        InvalidateRect(p_view->main[WIN_TOOLBAR].hwnd, NULL, TRUE);
}

extern void
li_toolbar_dispose(
    _ViewRef_   P_VIEW p_view)
{
    ho_destroy_main_window(&p_view->main[WIN_TOOLBAR].hwnd);
}

/* Work out how tall a status line should be for the font it uses, and make the window for it */

extern STATUS
li_status_line_new(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InRef_     PC_STYLE p_status_line_style)
{
    GDI_COORD status_line_font_height_pixels = 0;

    status_line_height_pixels = 0;

    {
    HOST_FONT host_font_formatting;
    HOST_FONT_SPEC host_font_spec;
    GDI_SIZE pixels_per_inch;

    status_assert(fontmap_host_font_spec_from_font_spec(&host_font_spec, &p_status_line_style->font_spec, FALSE));

    /* Em = dpiY * point_size / 72, in pixels */
    host_get_pixel_size(NULL /*screen*/, NULL /*cx*/, &pixels_per_inch.cy);
    host_font_spec.size_y = idiv_floor_u(host_font_spec.size_y * pixels_per_inch.cy, PIXITS_PER_INCH);

    host_font_formatting = host_font_find(&host_font_spec, P_REDRAW_CONTEXT_NONE);
    host_font_spec_dispose(&host_font_spec);

    if(HOST_FONT_NONE != host_font_formatting)
    {
        QTH_FONT_METRICS metrics;

        qth_font_metrics(host_font_formatting, &metrics);

        status_line_font_height_pixels = metrics.ascent + metrics.descent;
    }

    host_font_dispose(&host_font_formatting, P_REDRAW_CONTEXT_NONE);
    } /*block*/

    if(0 == status_line_font_height_pixels)
        return(STATUS_OK);

    status_line_height_pixels = status_line_font_height_pixels + 6; /* remember ideal height to open with */

    if(global_preferences.disable_status_line)
        return(STATUS_OK);

    { /* an upper status line, under the toolbar */
    const QTH_RECT bounds = { 0, 0, back_window_client_width(p_view), status_line_height_pixels };

    return(ho_create_main_window(p_docu, p_view, EVENT_HANDLER_STATUS_LINE, &p_view->main[WIN_UPPER_STATUS_LINE].hwnd, &bounds));
    } /*block*/
}

extern void
li_status_line_dispose(
    _ViewRef_   P_VIEW p_view)
{
    ho_destroy_main_window(&p_view->main[WIN_UPPER_STATUS_LINE].hwnd);
    ho_destroy_main_window(&p_view->main[WIN_LOWER_STATUS_LINE].hwnd);
}

extern void
li_status_line_update(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view)
{
    HOST_WND hwnd;
    RECT rect;

    if(HOST_WND_NONE != (hwnd = p_view->main[WIN_UPPER_STATUS_LINE].hwnd))
    {
    }
    else if(HOST_WND_NONE != (hwnd = p_view->main[WIN_LOWER_STATUS_LINE].hwnd))
    {
    }
    else
        return;

    GetClientRect(hwnd, &rect);

    /* try to avoid repainting the top and bottom lines */
    rect.top += 1;
    rect.bottom -= 1;

    InvalidateRect(hwnd, &rect, TRUE);

    if(p_view->viewno == p_docu->viewno_caret)
        qth_window_update_now(hwnd);
}

extern void
li_redraw_tool(
    _ViewRef_   P_VIEW p_view,
    _InRef_     PC_T5_TOOLBAR_VIEW_TOOL_DESC p_t5_toolbar_view_tool_desc)
{
    if(HOST_WND_NONE != p_view->main[WIN_TOOLBAR].hwnd)
    {
        GDI_RECT gdi_rect;
        RECT rect;

        status_consume(window_rect_from_pixit_rect(&gdi_rect, &p_t5_toolbar_view_tool_desc->pixit_rect, &p_view->host_xform[XFORM_TOOLBAR]));

        rect.left   = MAX(T5_GDI_MIN_X, gdi_rect.tl.x);
        rect.top    = MAX(T5_GDI_MIN_Y, gdi_rect.tl.y);
        rect.right  = MIN(T5_GDI_MAX_X, gdi_rect.br.x);
        rect.bottom = MIN(T5_GDI_MAX_Y, gdi_rect.br.y);

        if((rect.left < rect.right) && (rect.top < rect.bottom))
            InvalidateRect(p_view->main[WIN_TOOLBAR].hwnd, &rect, TRUE);
    }
}

#endif /* LINUX */

/* end of li_toolb.c */
