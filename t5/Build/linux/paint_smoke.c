/* paint_smoke.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Smoke test for the Linux host painting and window path, against the headless
 * backend, which records what is painted */

#include "common/gflags.h"

#include "ob_skel/flags.h"

#include "ob_skel/linux/qth_null.h"

static int n_failures = 0;

/* from ob_skel/view.c, which is not linked here: only set_host_xform_for_view() needs it */
extern void
set_host_xform_for_view_common(
    _OutRef_    P_HOST_XFORM p_host_xform,
    _ViewRef_   P_VIEW p_view,
    _InVal_     BOOL do_x_scale,
    _InVal_     BOOL do_y_scale)
{
    (void) p_host_xform;
    (void) p_view;
    (void) do_x_scale;
    (void) do_y_scale;
}

#define check(cond) \
    do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++n_failures; } } while(0)

#define check_log(p_painter, expected) \
    do { if(0 != strcmp(qth_null_painter_log(p_painter), expected)) { fprintf(stderr, "FAIL %s:%d: log is\n%sexpected\n%s", __FILE__, __LINE__, qth_null_painter_log(p_painter), expected); ++n_failures; } qth_null_painter_clear_log(p_painter); } while(0)

static void
context_init(
    _OutRef_    P_REDRAW_CONTEXT p_redraw_context,
    _InRef_     QTH_PAINTER * p_painter)
{
    HOST_XFORM host_xform;

    zero_struct_ptr(p_redraw_context);
    zero_struct(host_xform);

    host_xform.scale.t.x = host_xform.scale.t.y = 1; /* 1:1 */
    host_xform.scale.b.x = host_xform.scale.b.y = 1;
    host_xform.do_x_scale = host_xform.do_y_scale = TRUE;

    p_redraw_context->linux_ctx.painter = p_painter;
    p_redraw_context->linux_ctx.host_machine_clip_rect.br.x = 1000;
    p_redraw_context->linux_ctx.host_machine_clip_rect.br.y = 1000;
    p_redraw_context->border_width.x = p_redraw_context->border_width.y = 15; /* one pixel at 96 dpi */

    host_redraw_context_set_host_xform(p_redraw_context, &host_xform);
    host_redraw_context_fillin(p_redraw_context);
}

static void
test_xform(void)
{
    QTH_PAINTER * p_painter = qth_null_painter_new(96, 96);
    REDRAW_CONTEXT redraw_context;
    PIXIT_POINT pixit_point = { 3000, 1500 };
    GDI_POINT gdi_point;

    context_init(&redraw_context, p_painter);

    check(96 == redraw_context.pixels_per_inch.cx);

    gdi_point_from_pixit_point_and_context(&gdi_point, &pixit_point, &redraw_context);
    check((200 == gdi_point.x) && (100 == gdi_point.y)); /* 15 pixits per pixel */

    qth_null_painter_free(p_painter);
}

static void
test_paint(void)
{
    QTH_PAINTER * p_painter = qth_null_painter_new(96, 96);
    REDRAW_CONTEXT redraw_context;
    const PIXIT_RECT pixit_rect = { { 150, 300 }, { 1650, 1050 } }; /* 10,20 to 110,70 */
    const RGB red = RGB_INIT(255, 0, 0);
    const RGB blue = RGB_INIT(0, 0, 255);
    RGB clear = RGB_INIT(0, 0, 0);
    const RECT_FLAGS no_flags = { 0 };

    context_init(&redraw_context, p_painter);

    host_paint_rectangle_filled(&redraw_context, &pixit_rect, &red);
    check_log(p_painter, "fill 10,20,110,70 #ff0000\n");

    clear.transparent = 1;
    host_paint_rectangle_filled(&redraw_context, &pixit_rect, &clear);
    check_log(p_painter, ""); /* transparent: nothing */

    host_paint_rectangle_outline(&redraw_context, &pixit_rect, &blue);
    check_log(p_painter, "frame 10,20,110,70 #0000ff\n");

    host_paint_rectangle_crossed(&redraw_context, &pixit_rect, &blue);
    check_log(p_painter,
              "frame 10,20,110,70 #0000ff\n"
              "line 10,20,109,69 #0000ff\n"
              "line 10,69,109,20 #0000ff\n");

    host_invert_rectangle_outline(&redraw_context, &pixit_rect, &red, &blue);
    check_log(p_painter,
              "invert 10,20,109,21\n"
              "invert 109,20,110,69\n"
              "invert 11,69,110,70\n"
              "invert 10,21,11,70\n");

    { /* clipping narrows to the intersection with the current clip and is remembered */
    const PIXIT_RECT clip = { { 0, 0 }, { 750, 750 } }; /* 0,0 to 50,50 */
    check(host_set_clip_rectangle(&redraw_context, &clip, no_flags));
    check_log(p_painter, "clip 0,0,50,50\n");

    host_restore_clip_rectangle(&redraw_context);
    check_log(p_painter, "clip 0,0,50,50\n");

    { /* entirely outside: nothing to clip to */
    const PIXIT_RECT outside = { { 1500, 1500 }, { 3000, 3000 } };
    check(!host_set_clip_rectangle(&redraw_context, &outside, no_flags));
    check_log(p_painter, "");
    } /*block*/
    } /*block*/

    qth_null_painter_free(p_painter);
}

static unsigned int n_paint_events, n_mouse_events;

static void
window_proc(QTH_WINDOW * p_window, const QTH_EVENT * p_event, void * client)
{
    check(&n_paint_events == client);

    switch(p_event->type)
    {
    case QTH_EVENT_PAINT:
        ++n_paint_events;
        check(100 == p_event->u.paint.exposed.right);
        check(50 == p_event->u.paint.exposed.bottom);
        break;

    case QTH_EVENT_MOUSE:
        ++n_mouse_events;
        check((QTH_MOUSE_PRESS == p_event->u.mouse.action) && (7 == p_event->u.mouse.x) && (QTH_BUTTON_LEFT == p_event->u.mouse.buttons));
        break;

    default:
        break;
    }

    (void) p_window;
}

static void
test_window(void)
{
    QTH_WINDOW_DESC desc;
    QTH_WINDOW * p_window;
    QTH_NULL_WINDOW_STATE state;
    QTH_EVENT event;
    QTH_RECT caret = { 5, 6, 6, 20 };

    memset(&desc, 0, sizeof(desc));
    desc.kind = QTH_WINDOW_DOCUMENT;
    desc.bounds.right = 100;
    desc.bounds.bottom = 50;
    desc.p_title_latin1 = "Untitled";
    desc.proc = window_proc;
    desc.client = &n_paint_events;

    p_window = qth_window_create(&desc);
    check(NULL != p_window);
    check(1 == qth_null_n_windows());

    qth_window_show(p_window, 1);
    qth_window_set_title(p_window, "Sheet 1");
    qth_window_set_extent(p_window, 800, 600);
    qth_window_scroll_to(p_window, 10, 20);
    qth_window_set_caret(p_window, &caret, 1);
    qth_window_set_cursor(p_window, QTH_CURSOR_IBEAM);
    qth_window_capture_mouse(p_window, 1);
    qth_window_invalidate(p_window, NULL);
    qth_window_update_now(p_window);

    qth_null_window_state(p_window, &state);
    check(state.visible);
    check(0 == strcmp(state.title, "Sheet 1"));
    check((800 == state.extent_width) && (600 == state.extent_height));
    check((10 == state.scroll_x) && (20 == state.scroll_y));
    check(state.caret_visible && (20 == state.caret.bottom));
    check(QTH_CURSOR_IBEAM == state.cursor);
    check(state.mouse_captured);
    check(1 == state.n_invalidates);
    check(1 == n_paint_events);

    memset(&event, 0, sizeof(event));
    event.type = QTH_EVENT_MOUSE;
    event.u.mouse.action = QTH_MOUSE_PRESS;
    event.u.mouse.x = 7;
    event.u.mouse.buttons = QTH_BUTTON_LEFT;
    qth_null_post_event(p_window, &event);
    check(1 == n_mouse_events);

    qth_window_destroy(p_window);
    check(0 == qth_null_n_windows());
}

int
main(void)
{
    test_xform();
    test_paint();
    test_window();

    if(0 != n_failures)
    {
        fprintf(stderr, "paint_smoke: %d failure(s)\n", n_failures);
        return(EXIT_FAILURE);
    }

    printf("paint_smoke: all checks passed\n");
    return(EXIT_SUCCESS);
}

/* end of paint_smoke.c */
