/* li_dlg.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The Linux counterpart of wi_dlg.c: the dialog engine's host side.
 *
 * A dialog is built as a Qt dialog of ordinary widgets, one for each DIALOG_CONTROL (a spin
 * control is one widget here, where Windows has an edit and an up-down control), through the
 * plain-C qth_dlg_* functions. The widgets send their events back here, and they are fed to the
 * engine as the Windows messages are in wi_dlg.c. Picture buttons and user-drawn controls are
 * not yet drawn (see ReadMeL.txt). */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#if LINUX

#include "ob_skel/linux/qt_host.h"

#ifndef         UI_DLGIN_H
#include "ob_dlg/ui_dlgin.h"
#endif

/* Dialog units are defined from the average size of the dialog font's characters, as on Windows:
 * one horizontal dialog unit is a quarter of the average character width and one vertical dialog
 * unit an eighth of the character height. Measured at 96 dpi and scaled by the pixit transforms. */

static struct DIALOG_LINUX_METRICS
{
    int32_t pixels_per_four_h_du;
    int32_t pixels_per_eight_v_du;
}
dialog_metrics;

static void
dialog_metrics_init(void)
{
    if(0 == dialog_metrics.pixels_per_four_h_du)
        qth_dlg_metrics(&dialog_metrics.pixels_per_four_h_du, &dialog_metrics.pixels_per_eight_v_du);
}

/* The width of the text in the dialog font, rounded out and a wee bit more, as multiples of dialog units */

_Check_return_
extern PIXIT
ui_width_from_tstr_host(
    _In_z_      PCTSTR tstr)
{
    const U32 tchars_n = tstrlen32(tstr);
    const int32_t width = qth_dlg_text_width((const uint8_t *) tstr, tchars_n);

    dialog_metrics_init();

    return(PIXITS_PER_WDU_H * (2 + muldiv64_ceil(width, 4, dialog_metrics.pixels_per_four_h_du)));
}

extern void
ui_list_size_estimate(
    _InVal_     S32 num_elements,
    _OutRef_    P_PIXIT_SIZE p_pixit_size)
{
    S32 show_elements = num_elements;
    S32 max_elements;
    GDI_SIZE screen_gdi_size;

    dialog_metrics_init();

    /* always give the idea that there should be *some* list elements */
    show_elements = MAX(3, show_elements);

    /* always dynamically limit to a portion of the host's work area */
#define UI_LIST_LIMIT_NUMERATOR     3
#define UI_LIST_LIMIT_DENOMINATOR   5
    host_work_area_gdi_size_query(&screen_gdi_size); /* NB pixels */

    /* remembering to go from pixels via (multiples of) dialog units */
    max_elements = muldiv64(screen_gdi_size.cy, UI_LIST_LIMIT_NUMERATOR * 8, UI_LIST_LIMIT_DENOMINATOR * dialog_metrics.pixels_per_eight_v_du) / (DIALOG_STDLISTITEM_V / PIXITS_PER_WDU_V);

    p_pixit_size->cx = 0;

    if(show_elements > max_elements)
    {
        show_elements = max_elements;
        p_pixit_size->cx += DIALOG_STDLISTEXTRA_H;
    }

    p_pixit_size->cy = show_elements * DIALOG_STDLISTITEM_V;
}

/* ---- control positions ---------------------------------------------------------------- */

static int
px_h_pos(_InVal_ PIXIT x)
{
    int32_t four_h, eight_v;
    qth_dlg_metrics(&four_h, &eight_v);
    return((int) (idiv_floor(x, PIXITS_PER_WDU_H) * four_h / 4));
}

static int
px_v_pos(_InVal_ PIXIT y)
{
    int32_t four_h, eight_v;
    qth_dlg_metrics(&four_h, &eight_v);
    return((int) (idiv_floor(y, PIXITS_PER_WDU_V) * eight_v / 8));
}

static int
px_h_size(_InVal_ PIXIT x)
{
    int32_t four_h, eight_v;
    qth_dlg_metrics(&four_h, &eight_v);
    return((int) (((int64_t) idiv_ceil(x, PIXITS_PER_WDU_H) * four_h + 3) / 4));
}

static int
px_v_size(_InVal_ PIXIT y)
{
    int32_t four_h, eight_v;
    qth_dlg_metrics(&four_h, &eight_v);
    return((int) (((int64_t) idiv_ceil(y, PIXITS_PER_WDU_V) * eight_v + 7) / 8));
}

/* ---- the contents of lists and combos ---------------------------------------------------- */

extern void
dialog_windows_build_list(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    QTH_DIALOG * const p_dlg = p_dialog->lx.p_dlg;
    const int id = (int) p_dialog_ictl->dialog_control_id;
    PC_UI_SOURCE p_ui_source;
    const void /*UI_CONTROL*/ * p_ui_control = NULL;
    UI_DATA_TYPE ui_data_type = UI_DATA_TYPE_TEXT;

    if(NULL == p_dlg)
        return;

    switch(p_dialog_ictl->dialog_control_type)
    {
    default:
        return;

    case DIALOG_CONTROL_LIST_S32:
        ui_data_type = UI_DATA_TYPE_S32;
        p_ui_control = p_dialog_ictl->data.list_xx.list_xx.p_ui_control_s32;
        /*FALLTHRU*/
    case DIALOG_CONTROL_LIST_TEXT:
        p_ui_source = p_dialog_ictl->data.list_xx.list_xx.p_ui_source;
        if(p_dialog_ictl->p_dialog_control_data.list_text && p_dialog_ictl->p_dialog_control_data.list_text->list_xx.bits.tab_position)
            qth_dlg_list_set_tab(p_dlg, id, px_h_size(DIALOG_SYSCHARSL_H(p_dialog_ictl->p_dialog_control_data.list_text->list_xx.bits.tab_position)));
        break;

    case DIALOG_CONTROL_COMBO_S32:
        ui_data_type = UI_DATA_TYPE_S32;
        p_ui_control = p_dialog_ictl->data.combo_xx.list_xx.p_ui_control_s32;
        /*FALLTHRU*/
    case DIALOG_CONTROL_COMBO_TEXT:
        p_ui_source = p_dialog_ictl->data.combo_xx.list_xx.p_ui_source;
        break;
    }

    qth_dlg_list_clear(p_dlg, id);

    {
    const S32 n_items = ui_data_n_items_query(ui_data_type, p_ui_source);
    S32 itemno;

    for(itemno = 0; itemno < n_items; ++itemno)
    {
        UI_TEXT ui_text;

        if(status_ok(ui_data_query_as_text(ui_data_type, p_ui_source, itemno, p_ui_control, &ui_text)))
        {
            qth_dlg_list_add(p_dlg, id, ui_text_tstr(&ui_text));
            ui_text_dispose(&ui_text);
        }
    }
    } /*block*/
}

static void
dialog_ictls_make_lists(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL_GROUP p_ictl_group)
{
    ARRAY_INDEX i;

    for(i = 0; i < n_ictls_from_group(p_ictl_group); ++i)
    {
        const P_DIALOG_ICTL p_dialog_ictl = p_dialog_ictl_from(p_ictl_group, i);

        switch(p_dialog_ictl->dialog_control_type)
        {
        default:
            break;

        case DIALOG_CONTROL_GROUPBOX:
            dialog_ictls_make_lists(p_dialog, &p_dialog_ictl->data.groupbox.ictls);
            break;

        case DIALOG_CONTROL_LIST_S32:
        case DIALOG_CONTROL_LIST_TEXT:
        case DIALOG_CONTROL_COMBO_S32:
        case DIALOG_CONTROL_COMBO_TEXT:
            dialog_windows_build_list(p_dialog, p_dialog_ictl);
            break;
        }
    }
}

/* ---- showing the state of a control ---------------------------------------------------- */

extern void
dialog_windows_ictl_enable_here(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    if(!DIALOG_IS_OPEN(p_dialog))
        return;

    switch(p_dialog_ictl->dialog_control_type)
    {
    case DIALOG_CONTROL_GROUPBOX:
        if(p_dialog_ictl->p_dialog_control->bits.logical_group)
            return; /* there is no widget */
        break;

    default:
        break;
    }

    qth_dlg_set_enabled(p_dialog->lx.p_dlg, (int) p_dialog_ictl->dialog_control_id, !p_dialog_ictl->bits.disabled);
}

extern void
dialog_linux_ictl_encode_here(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    QTH_DIALOG * const p_dlg = p_dialog->lx.p_dlg;
    const int id = (int) p_dialog_ictl->dialog_control_id;

    if(NULL == p_dlg)
        return;

    switch(p_dialog_ictl->dialog_control_type)
    {
    default:
        break;

    case DIALOG_CONTROL_STATICTEXT:
        qth_dlg_set_text(p_dlg, id, ui_text_tstr(&p_dialog_ictl->state.statictext));
        break;

    case DIALOG_CONTROL_TEXTLABEL:
        if(p_dialog_ictl->p_dialog_control_data.textlabel->bits.windows_no_colon)
            qth_dlg_set_text(p_dlg, id, ui_text_tstr(&p_dialog_ictl->state.textlabel));
        else
        {
            QUICK_TBLOCK_WITH_BUFFER(quick_tblock, 128);
            quick_tblock_with_buffer_setup(quick_tblock);

            status_assert(ui_text_read(&quick_tblock, &p_dialog_ictl->state.textlabel));
            if(0 != quick_tblock_chars(&quick_tblock))
                status_assert(quick_tblock_tchar_add(&quick_tblock, CH_COLON));
            status_assert(quick_tblock_nullch_add(&quick_tblock));
            qth_dlg_set_text(p_dlg, id, quick_tblock_tstr(&quick_tblock));

            quick_tblock_dispose(&quick_tblock);
        }
        break;

    case DIALOG_CONTROL_TEXTFRAME:
        qth_dlg_set_text(p_dlg, id, ui_text_tstr(&p_dialog_ictl->state.textframe));
        break;

    case DIALOG_CONTROL_PUSHBUTTON:
        qth_dlg_set_text(p_dlg, id, ui_text_tstr(&p_dialog_ictl->state.pushbutton));
        break;

    case DIALOG_CONTROL_RADIOBUTTON:
        qth_dlg_set_check(p_dlg, id, p_dialog_ictl->bits.radiobutton_active);
        break;

    case DIALOG_CONTROL_CHECKBOX:
        qth_dlg_set_check(p_dlg, id, (p_dialog_ictl->state.checkbox != DIALOG_BUTTONSTATE_OFF));
        break;

#ifdef DIALOG_HAS_TRISTATE
    case DIALOG_CONTROL_TRISTATE:
        qth_dlg_set_check(p_dlg, id, (p_dialog_ictl->state.tristate == DIALOG_TRISTATE_DONT_CARE) ? 2 : (p_dialog_ictl->state.tristate != DIALOG_TRISTATE_OFF));
        break;
#endif

    case DIALOG_CONTROL_RADIOPICTURE:
    case DIALOG_CONTROL_CHECKPICTURE:
#ifdef DIALOG_HAS_TRISTATE
    case DIALOG_CONTROL_TRIPICTURE:
#endif
    case DIALOG_CONTROL_USER:
        qth_dlg_invalidate(p_dlg, id);
        break;

    case DIALOG_CONTROL_LIST_S32:
    case DIALOG_CONTROL_LIST_TEXT:
        qth_dlg_list_set_sel(p_dlg, id, (p_dialog_ictl->state.list_text.itemno >= 0) ? (int) p_dialog_ictl->state.list_text.itemno : -1);
        break;

    case DIALOG_CONTROL_COMBO_S32:
    case DIALOG_CONTROL_COMBO_TEXT:
        qth_dlg_list_set_sel(p_dlg, id, (p_dialog_ictl->state.combo_text.itemno >= 0) ? (int) p_dialog_ictl->state.combo_text.itemno : -1);
        break;
    }
}

/* ---- drawing the controls the toolkit leaves to us ------------------------------------ */

static const RGB dialog_face_rgb   = RGB_INIT(0xEE, 0xEE, 0xEE);
static const RGB dialog_light_rgb  = RGB_INIT(0xFF, 0xFF, 0xFF);
static const RGB dialog_shadow_rgb = RGB_INIT(0x80, 0x80, 0x80);

static QTH_RGB
qth_rgb_of(const RGB * p_rgb)
{
    QTH_RGB qth_rgb;
    qth_rgb.r = p_rgb->r;
    qth_rgb.g = p_rgb->g;
    qth_rgb.b = p_rgb->b;
    return(qth_rgb);
}

static void
dialog_linux_paint_picture(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl,
    QTH_PAINTER * p_painter,
    _InRef_     const QTH_RECT * p_rect)
{
    const BOOL enabled = !p_dialog_ictl->bits.disabled;
    const BOOL pointing = enabled && !p_dialog_ictl->bits.nobbled;
    BOOL on = FALSE, plain_button = FALSE;
    RESOURCE_BITMAP_ID resource_bitmap_id;
    PC_RESOURCE_BITMAP_ID p_resource_bitmap_id = NULL;
    BOOL sunken, raised;
    const QTH_RGB face = qth_rgb_of(&dialog_face_rgb);
    const QTH_RGB light = qth_rgb_of(&dialog_light_rgb);
    const QTH_RGB shadow = qth_rgb_of(&dialog_shadow_rgb);
    const QTH_RGB key = { 192, 192, 192 }; /* the colour the pictures use for see-through */

    switch(p_dialog_ictl->dialog_control_type)
    {
    case DIALOG_CONTROL_PUSHPICTURE:
        plain_button = TRUE;
        resource_bitmap_id = p_dialog_ictl->p_dialog_control_data.pushpicture->picture_bitmap_id;
        p_resource_bitmap_id = &resource_bitmap_id;
        break;

    case DIALOG_CONTROL_RADIOPICTURE:
        on = p_dialog_ictl->bits.radiobutton_active;
        p_resource_bitmap_id = p_dialog_ictl->p_dialog_control_data.radiopicture->p_bitmap_id_offon;
        if(on && p_dialog_ictl->p_dialog_control_data.radiopicture->bits.has_n_bmp)
            p_resource_bitmap_id++;
        break;

    case DIALOG_CONTROL_CHECKPICTURE:
        on = (p_dialog_ictl->state.checkbox > 0);
        p_resource_bitmap_id = p_dialog_ictl->p_dialog_control_data.checkpicture->p_bitmap_id_offon;
        if(on && p_dialog_ictl->p_dialog_control_data.checkpicture->bits.has_n_bmp)
            p_resource_bitmap_id++;
        break;

#ifdef DIALOG_HAS_TRISTATE
    case DIALOG_CONTROL_TRIPICTURE:
        on = (p_dialog_ictl->state.tristate != DIALOG_TRISTATE_OFF);
        p_resource_bitmap_id = p_dialog_ictl->p_dialog_control_data.tripicture->p_bitmap_id_offondontcare;
        if(p_dialog_ictl->p_dialog_control_data.tripicture->bits.has_n_bmp)
        {
            if(p_dialog_ictl->state.tristate == DIALOG_TRISTATE_ON)
                p_resource_bitmap_id += 1;
            else if(p_dialog_ictl->state.tristate == DIALOG_TRISTATE_DONT_CARE)
                p_resource_bitmap_id += 2;
        }
        break;
#endif

    default:
        break;
    }

    /* a push picture is always a button; the others are flat until pointed at, and sunken while on */
    sunken = on || (pointing && p_dialog_ictl->bits.mouse_down);
    raised = !sunken && (plain_button || (pointing && p_dialog_ictl->bits.mouse_over));

    qth_paint_fill_rect(p_painter, p_rect, face);

    if(sunken || raised)
    {
        const QTH_RECT top = { p_rect->left, p_rect->top, p_rect->right, p_rect->top + 1 };
        const QTH_RECT left = { p_rect->left, p_rect->top, p_rect->left + 1, p_rect->bottom };
        const QTH_RECT bottom = { p_rect->left, p_rect->bottom - 1, p_rect->right, p_rect->bottom };
        const QTH_RECT right = { p_rect->right - 1, p_rect->top, p_rect->right, p_rect->bottom };

        qth_paint_fill_rect(p_painter, &top, sunken ? shadow : light);
        qth_paint_fill_rect(p_painter, &left, sunken ? shadow : light);
        qth_paint_fill_rect(p_painter, &bottom, sunken ? light : shadow);
        qth_paint_fill_rect(p_painter, &right, sunken ? light : shadow);
    }

    if(NULL != p_resource_bitmap_id)
    {
        RESOURCE_BITMAP_HANDLE resource_bitmap_handle;
        GDI_SIZE bm_grid_size, bm_size;
        UINT index;

        resource_bitmap_id = *p_resource_bitmap_id;

        if(resource_bitmap_find_new(&resource_bitmap_id, &resource_bitmap_handle, &bm_grid_size, &index))
        {
            resource_bitmap_gdi_size_query(resource_bitmap_handle, &bm_size);

            if(0 == bm_grid_size.cx)
                bm_grid_size.cx = bm_size.cx; /* a single picture fills its bitmap unless a strip says otherwise */
            if(0 == bm_grid_size.cy)
                bm_grid_size.cy = bm_size.cy;

            {
            const int shift = sunken ? 1 : 0;
            const int x = p_rect->left + (((p_rect->right - p_rect->left) - bm_grid_size.cx) / 2) + shift;
            const int y = p_rect->top + (((p_rect->bottom - p_rect->top) - bm_grid_size.cy) / 2) + shift;
            const QTH_RECT src = { (int) index * bm_grid_size.cx, 0, ((int) index + 1) * bm_grid_size.cx, bm_grid_size.cy };
            const QTH_RECT dest = { x, y, x + bm_grid_size.cx, y + bm_grid_size.cy };

            qth_paint_image_part(p_painter, (const QTH_IMAGE *) resource_bitmap_handle.i, &src, &dest, &key, enabled ? 255 : 90);
            } /*block*/

            resource_bitmap_lose(&resource_bitmap_handle);
        }
    }
}

static void
dialog_linux_paint_user(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl,
    QTH_PAINTER * p_painter,
    _InRef_     const QTH_RECT * p_rect)
{
    PC_RGB p_rgb = p_dialog_ictl->p_dialog_control_data.user->p_back_colour;
    RGB rgb;
    P_PROC_DIALOG_EVENT p_proc_client;
    DIALOG_MSG_CTL_USER_REDRAW dialog_msg_ctl_user_redraw;
    const QTH_RGB black = { 0, 0, 0 };

    if(NULL != p_rgb)
        rgb = *p_rgb;
    else if(p_dialog_ictl->p_dialog_control_data.user->bits.state_is_rgb)
        rgb = p_dialog_ictl->state.user.rgb;
    else if(p_dialog_ictl->p_dialog_control_data.user->bits.state_is_rgb_stash_index)
        rgb = rgb_stash[p_dialog_ictl->state.user.u32];
    else
        rgb_set(&rgb, 0xFF, 0xFF, 0xFF);

    if(p_dialog_ictl->bits.disabled)
    {   /* lighten up a bit: towards white by a third */
        rgb.r = (U8) (rgb.r + (255 - rgb.r) / 3);
        rgb.g = (U8) (rgb.g + (255 - rgb.g) / 3);
        rgb.b = (U8) (rgb.b + (255 - rgb.b) / 3);
    }

    qth_paint_fill_rect(p_painter, p_rect, qth_rgb_of(&rgb));
    qth_paint_frame_rect(p_painter, p_rect, black);

    msgclr(dialog_msg_ctl_user_redraw);

    if(NULL != (p_proc_client = dialog_find_handler(p_dialog, p_dialog_ictl->dialog_control_id, &dialog_msg_ctl_user_redraw.client_handle)))
    {
        REDRAW_CONTEXT_CACHE redraw_context_cache = { NULL };
        const P_REDRAW_CONTEXT p_redraw_context = &dialog_msg_ctl_user_redraw.redraw_context;
        GDI_RECT gdi_rect;

        zero_struct_ptr_fn(p_redraw_context);
        p_redraw_context->p_redraw_context_cache = &redraw_context_cache;

        p_redraw_context->linux_ctx.painter = p_painter;
        p_redraw_context->linux_ctx.paint_rect.tl.x = p_rect->left;
        p_redraw_context->linux_ctx.paint_rect.tl.y = p_rect->top;
        p_redraw_context->linux_ctx.paint_rect.br.x = p_rect->right;
        p_redraw_context->linux_ctx.paint_rect.br.y = p_rect->bottom;

        p_redraw_context->host_xform.scale.t.x = 1;
        p_redraw_context->host_xform.scale.t.y = 1;
        p_redraw_context->host_xform.scale.b.x = 1;
        p_redraw_context->host_xform.scale.b.y = 1;

        p_redraw_context->display_mode = DISPLAY_PRINT_AREA;

        if(DOCNO_NONE != p_dialog->docno)
            p_redraw_context->border_width.x = p_redraw_context->border_width.y = p_docu_from_docno(p_dialog->docno)->page_def.grid_size;

        host_redraw_context_set_host_xform(p_redraw_context, &p_redraw_context->host_xform);
        host_redraw_context_fillin(p_redraw_context);

        /* no origin worries */
        host_paint_start(p_redraw_context);

        p_redraw_context->linux_ctx.host_machine_clip_rect = p_redraw_context->linux_ctx.paint_rect;

        DIALOG_MSG_CTL_HDR_from_dialog_ictl(dialog_msg_ctl_user_redraw, p_dialog, p_dialog_ictl);

        gdi_rect = p_redraw_context->linux_ctx.paint_rect;
        pixit_rect_from_window_rect(&dialog_msg_ctl_user_redraw.control_inner_box, &gdi_rect, &p_redraw_context->host_xform);

        dialog_msg_ctl_user_redraw.enabled = !p_dialog_ictl->bits.disabled;

        status_assert(dialog_call_client(p_dialog, DIALOG_MSG_CODE_CTL_USER_REDRAW, &dialog_msg_ctl_user_redraw, p_proc_client));

        host_paint_end(p_redraw_context);
    }
}

static void
dialog_linux_paint(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    QTH_PAINTER * const p_painter = qth_dlg_painter(p_dialog->lx.p_dlg);
    QTH_RECT rect;
    int32_t width, height;

    if(NULL == p_painter)
        return;

    qth_dlg_ctl_size(p_dialog->lx.p_dlg, (int) p_dialog_ictl->dialog_control_id, &width, &height);
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;

    if(p_dialog_ictl->dialog_control_type == DIALOG_CONTROL_USER)
        dialog_linux_paint_user(p_dialog, p_dialog_ictl, p_painter, &rect);
    else
        dialog_linux_paint_picture(p_dialog, p_dialog_ictl, p_painter, &rect);
}

/* the text of an edit, spin or combo control */

_Check_return_
extern STATUS
ui_text_from_ictl_edit_xx(
    _OutRef_    P_UI_TEXT p_ui_text,
    P_DIALOG p_dialog,
    P_DIALOG_ICTL p_dialog_ictl)
{
    const int id = (int) p_dialog_ictl->dialog_control_id;
    const U32 tstr_len = (U32) qth_dlg_get_text(p_dialog->lx.p_dlg, id, NULL, 0) + 1 /*CH_NULL*/;
    STATUS status;
    PTSTR tstr;

    p_ui_text->type = UI_TEXT_TYPE_NONE;

    if(NULL != (tstr = al_array_alloc_TCHAR(&p_ui_text->text.array_handle_tstr, tstr_len, &array_init_block_tchar, &status)))
    {
        p_ui_text->type = UI_TEXT_TYPE_TSTR_ARRAY;

        (void) qth_dlg_get_text(p_dialog->lx.p_dlg, id, (char *) tstr, (int) tstr_len);
    }

    return(status);
}

/* ---- what the user does ------------------------------------------------------------------ */

/* the text of an edit control, a spin control or a combo's edit has changed, so read and validate it */

static void
dialog_linux_text_changed(
    _InoutRef_  P_DIALOG p_dialog,
    _InoutRef_  P_DIALOG_ICTL p_dialog_ictl)
{
    STATUS status = STATUS_OK;
    P_DIALOG_ICTL_EDIT_XX p_dialog_ictl_edit_xx = p_dialog_ictl_edit_xx_from(p_dialog_ictl);
    DIALOG_CMD_CTL_STATE_SET dialog_cmd_ctl_state_set;
    UI_TEXT ui_text;

    if((NULL == p_dialog_ictl_edit_xx) || (0 != p_dialog_ictl->bits.in_update))
        return;

    msgclr(dialog_cmd_ctl_state_set);

    status_assert(ui_text_from_ictl_edit_xx(&ui_text, p_dialog, p_dialog_ictl));

    /* state change will want to rejig views iff chars rejected */
    p_dialog_ictl->bits.force_update = (ui_text_validate(&ui_text, p_dialog_ictl_edit_xx->p_bitmap_validation) != 0);

    if(p_dialog_ictl->bits.force_update)
        host_bleep();

    switch(p_dialog_ictl->dialog_control_type)
    {
    default: default_unhandled();
#if CHECKING
    case DIALOG_CONTROL_EDIT:
#endif
        dialog_cmd_ctl_state_set.state.edit.ui_text = ui_text;
        break;

    case DIALOG_CONTROL_COMBO_TEXT:
        dialog_cmd_ctl_state_set.state.combo_text.ui_text = ui_text;
        break;

    case DIALOG_CONTROL_BUMP_F64:
        {
        PC_USTR i_ustr = ui_text_ustr(&ui_text);
        PC_USTR e_ustr;
        SS_RECOG_CONTEXT ss_recog_context;
        ss_recog_context_push(&ss_recog_context);
        errno = 0;
        dialog_cmd_ctl_state_set.state.bump_f64 = ui_strtod(i_ustr, &e_ustr);
        ss_recog_context_pull(&ss_recog_context);
        break;
        }

    case DIALOG_CONTROL_BUMP_S32:
        {
        PC_USTR i_ustr = ui_text_ustr(&ui_text);
        P_USTR e_ustr;
        errno = 0;
        dialog_cmd_ctl_state_set.state.bump_s32 = ui_strtol(i_ustr, &e_ustr, 0);
        if(errno)
            dialog_cmd_ctl_state_set.state.bump_s32 = 0;
        break;
        }
    }

    /* command ourselves with a state change, with interlock against killer recursion */
    dialog_cmd_ctl_state_set.h_dialog = p_dialog_ictl_edit_xx->h_dialog;
    dialog_cmd_ctl_state_set.dialog_control_id = p_dialog_ictl_edit_xx->dialog_control_id;
    dialog_cmd_ctl_state_set.bits = 0;

    p_dialog_ictl->bits.in_update += 1;
    status = object_call_DIALOG(DIALOG_CMD_CODE_CTL_STATE_SET, &dialog_cmd_ctl_state_set);
    p_dialog_ictl->bits.in_update -= 1;

    if(status_fail(status))
        reperr_null(status);

    p_dialog_ictl->bits.force_update = 0;

    ui_text_dispose(&ui_text);
}

static void
dialog_linux_complete(
    _InoutRef_  P_DIALOG p_dialog,
    _InVal_     S32 completion_code)
{
    DIALOG_CMD_COMPLETE_DBOX dialog_cmd_complete_dbox;
    msgclr(dialog_cmd_complete_dbox);
    dialog_cmd_complete_dbox.h_dialog = p_dialog->h_dialog;
    dialog_cmd_complete_dbox.completion_code = completion_code;
    status_assert(object_call_DIALOG(DIALOG_CMD_CODE_COMPLETE_DBOX, &dialog_cmd_complete_dbox));
}

static void
dialog_linux_event(
    QTH_DIALOG * p_dlg,
    int event,
    int id,
    int arg,
    void * client)
{
    const H_DIALOG h_dialog = (H_DIALOG) (intptr_t) client;
    const P_DIALOG p_dialog = p_dialog_from_h_dialog(h_dialog);
    P_DIALOG_ICTL p_dialog_ictl;
    STATUS status = STATUS_OK;

    UNREFERENCED_PARAMETER(p_dlg);

    if(NULL == p_dialog)
        return;

    if(event == QTH_DLG_EV_CLOSE)
    {   /* Escape or the close button: as if Cancel had been pressed, whether or not there is one */
        P_DIALOG_ICTL p_cancel = p_dialog_ictl_from_control_id(p_dialog, IDCANCEL);

        if((NULL != p_cancel) && (p_cancel->dialog_control_type == DIALOG_CONTROL_PUSHBUTTON) && !p_cancel->bits.disabled && !p_cancel->bits.nobbled)
            status_assert(status = dialog_click_pushbutton(p_dialog, p_cancel, 0, 0));
        else
            dialog_linux_complete(p_dialog, DIALOG_COMPLETION_CANCEL);

        if(status_fail(status))
            reperr_null(status);
        return;
    }

    if(event == QTH_DLG_EV_HELP)
    {
        status_assert(dialog_windows_help(p_dialog));
        return;
    }

    if(NULL == (p_dialog_ictl = p_dialog_ictl_from_control_id(p_dialog, (DIALOG_CONTROL_ID) id)))
        return;

    if(event == QTH_DLG_EV_PAINT)
    {
        dialog_linux_paint(p_dialog, p_dialog_ictl);
        return;
    }

    if((event == QTH_DLG_EV_HOVER) || (event == QTH_DLG_EV_PRESS))
    {   /* the look of a picture button follows the pointer */
        if(event == QTH_DLG_EV_HOVER)
            p_dialog_ictl->bits.mouse_over = (0 != arg);
        else
            p_dialog_ictl->bits.mouse_down = (0 != arg);
        qth_dlg_invalidate(p_dialog->lx.p_dlg, id);
        return;
    }

    if(event == QTH_DLG_EV_FOCUS)
    {
        p_dialog->current_dialog_control_id = p_dialog_ictl->dialog_control_id;
        return;
    }

    switch(p_dialog_ictl->dialog_control_type)
    {
    default:
        break;

    case DIALOG_CONTROL_PUSHBUTTON:
    case DIALOG_CONTROL_PUSHPICTURE:
        if((event == QTH_DLG_EV_CLICK) && !p_dialog_ictl->bits.nobbled)
            status_assert(status = dialog_click_pushbutton(p_dialog, p_dialog_ictl, 0 != (qth_keyboard_modifiers() & QTH_MOD_CTRL), 0));
        break;

    case DIALOG_CONTROL_USER:
        if(event == QTH_DLG_EV_CLICK)
        {
            P_PROC_DIALOG_EVENT p_proc_client;
            DIALOG_MSG_CTL_USER_MOUSE dialog_msg_ctl_user_mouse;
            msgclr(dialog_msg_ctl_user_mouse);

            if(NULL != (p_proc_client = dialog_find_handler(p_dialog, p_dialog_ictl->dialog_control_id, &dialog_msg_ctl_user_mouse.client_handle)))
            {
                DIALOG_MSG_CTL_HDR_from_dialog_ictl(dialog_msg_ctl_user_mouse, p_dialog, p_dialog_ictl);
                dialog_msg_ctl_user_mouse.click = (arg == 1) ? DIALOG_MSG_USER_MOUSE_CLICK_RIGHT_SINGLE : DIALOG_MSG_USER_MOUSE_CLICK_LEFT_SINGLE;
                status_assert(status = dialog_call_client(p_dialog, DIALOG_MSG_CODE_CTL_USER_MOUSE, &dialog_msg_ctl_user_mouse, p_proc_client));
            }
        }
        break;

    case DIALOG_CONTROL_RADIOBUTTON:
    case DIALOG_CONTROL_RADIOPICTURE:
        if((event == QTH_DLG_EV_CLICK) && !p_dialog_ictl->bits.nobbled)
            status_assert(status = dialog_click_radiobutton(p_dialog, p_dialog_ictl));
        break;

    case DIALOG_CONTROL_CHECKBOX:
    case DIALOG_CONTROL_CHECKPICTURE:
        if((event == QTH_DLG_EV_CLICK) && !p_dialog_ictl->bits.nobbled)
            status_assert(status = dialog_click_checkbox(p_dialog, p_dialog_ictl));
        break;

#ifdef DIALOG_HAS_TRISTATE
    case DIALOG_CONTROL_TRISTATE:
    case DIALOG_CONTROL_TRIPICTURE:
        if((event == QTH_DLG_EV_CLICK) && !p_dialog_ictl->bits.nobbled)
            status_assert(status = dialog_click_tristate(p_dialog, p_dialog_ictl));
        break;
#endif

    case DIALOG_CONTROL_EDIT:
    case DIALOG_CONTROL_BUMP_S32:
    case DIALOG_CONTROL_BUMP_F64:
        if(event == QTH_DLG_EV_TEXT)
            dialog_linux_text_changed(p_dialog, p_dialog_ictl);
        else if(event == QTH_DLG_EV_SPIN)
        {
            const BOOL decrement_button = (arg < 0);
            const BOOL ctrl_pressed = (0 != (qth_keyboard_modifiers() & QTH_MOD_CTRL));
            const UI_DATA_INC_TYPE ui_data_inc =
                decrement_button
                ? (ctrl_pressed ? UI_DATA_IDT_MINIMUM : UI_DATA_IDT_DECREMENT)
                : (ctrl_pressed ? UI_DATA_IDT_MAXIMUM : UI_DATA_IDT_INCREMENT);
            status_assert(status = dialog_click_bump_xx(p_dialog, p_dialog_ictl, ui_data_inc));
        }
        break;

    case DIALOG_CONTROL_LIST_S32:
    case DIALOG_CONTROL_LIST_TEXT:
    case DIALOG_CONTROL_COMBO_S32:
    case DIALOG_CONTROL_COMBO_TEXT:
        {
        const BOOL is_combo = (p_dialog_ictl->dialog_control_type == DIALOG_CONTROL_COMBO_S32) || (p_dialog_ictl->dialog_control_type == DIALOG_CONTROL_COMBO_TEXT);

        if(event == QTH_DLG_EV_TEXT)
        {
            if(is_combo)
                dialog_linux_text_changed(p_dialog, p_dialog_ictl);
        }
        else if(event == QTH_DLG_EV_DBLCLK)
        {
            const BOOL disable_double = is_combo
                ? (p_dialog_ictl->p_dialog_control_data.combo_text->combo_xx.list_xx.bits.disable_double)
                : (p_dialog_ictl->p_dialog_control_data.list_text && p_dialog_ictl->p_dialog_control_data.list_text->list_xx.bits.disable_double);

            if(!disable_double)
            {   /* a double click means OK to the containing dialog */
                DIALOG_CMD_DEFPUSHBUTTON dialog_cmd_defpushbutton;
                dialog_cmd_defpushbutton.h_dialog = p_dialog->h_dialog;
                dialog_cmd_defpushbutton.double_dialog_control_id = p_dialog_ictl->dialog_control_id;
                status_assert(object_call_DIALOG(DIALOG_CMD_CODE_DEFPUSHBUTTON, &dialog_cmd_defpushbutton));
            }
        }
        else if(event == QTH_DLG_EV_SELECT)
        {   /* reflect selection from list box into state, and for combos, the edit part of the control */
            DIALOG_CMD_CTL_STATE_SET dialog_cmd_ctl_state_set;

            msgclr(dialog_cmd_ctl_state_set);
            dialog_cmd_ctl_state_set.h_dialog = p_dialog->h_dialog;
            dialog_cmd_ctl_state_set.dialog_control_id = p_dialog_ictl->dialog_control_id;
            dialog_cmd_ctl_state_set.bits = DIALOG_STATE_SET_ALTERNATE;
            dialog_cmd_ctl_state_set.state.list_text.itemno = (arg >= 0) ? (S32) arg : DIALOG_CTL_STATE_LIST_ITEM_NONE;

            status_assert(object_call_DIALOG(DIALOG_CMD_CODE_CTL_STATE_SET, &dialog_cmd_ctl_state_set));
        }
        break;
        }
    }

    if(status_fail(status))
    {   /* report error locally */
        reperr_null(status);
    }
}

/* ---- building and running a dialog ------------------------------------------------------ */

static void
dialog_linux_ictls_create(
    _InoutRef_  P_DIALOG p_dialog,
    P_DIALOG_ICTL_GROUP p_ictl_group,
    _OutRef_    P_BOOL p_focused)
{
    QTH_DIALOG * const p_dlg = p_dialog->lx.p_dlg;
    ARRAY_INDEX i;

    for(i = 0; i < n_ictls_from_group(p_ictl_group); ++i)
    {
        const P_DIALOG_ICTL p_dialog_ictl = p_dialog_ictl_from(p_ictl_group, i);
        const PC_DIALOG_CONTROL_DATA p_dialog_control_data = p_dialog_ictl->p_dialog_control_data;
        const int id = (int) p_dialog_ictl->dialog_control_id;
        PIXIT_RECT pixit_rect;
        int kind = 0, flags = 0;
        const char * caption = "";

        dialog_control_rect(p_dialog, p_dialog_ictl->dialog_control_id, &pixit_rect);

        switch(p_dialog_ictl->dialog_control_type)
        {
        case DIALOG_CONTROL_GROUPBOX:
            if(p_dialog_ictl->p_dialog_control->bits.logical_group)
                break; /* just for the layout */

            caption = ui_text_tstr(&p_dialog_ictl->caption);
            if(FRAMED_BOX_GROUP == p_dialog_control_data.groupbox->bits.border_style)
                kind = QTH_DLG_GROUP;
            else
            {   /* mutate into top left static text */
                kind = QTH_DLG_LABEL;
                flags = QTH_DLGF_LEFT;
            }
            break;

        case DIALOG_CONTROL_TEXTFRAME:
            kind = QTH_DLG_FRAMED_TEXT;
            flags = QTH_DLGF_LEFT;
            break;

        case DIALOG_CONTROL_STATICTEXT:
        case DIALOG_CONTROL_TEXTLABEL:
            kind = QTH_DLG_LABEL;
            if(p_dialog_control_data.statictext->bits.centre_text)
                flags = QTH_DLGF_CENTRE;
            else if(p_dialog_control_data.statictext->bits.left_text)
                flags = QTH_DLGF_LEFT;
            break;

        case DIALOG_CONTROL_PUSHBUTTON:
            kind = QTH_DLG_PUSH;
            caption = ui_text_tstr(&p_dialog_ictl->state.pushbutton);
            if((p_dialog_ictl->dialog_control_id == IDOK) || p_dialog_control_data.pushbutton->push_xx.def_pushbutton)
                flags = QTH_DLGF_DEFAULT;
            break;

        case DIALOG_CONTROL_RADIOBUTTON:
            kind = QTH_DLG_RADIO;
            caption = ui_text_tstr(&p_dialog_ictl->caption);
            break;

        case DIALOG_CONTROL_CHECKBOX:
            kind = QTH_DLG_CHECK;
            caption = ui_text_tstr(&p_dialog_ictl->caption);
            break;

#ifdef DIALOG_HAS_TRISTATE
        case DIALOG_CONTROL_TRISTATE:
            kind = QTH_DLG_TRISTATE;
            caption = ui_text_tstr(&p_dialog_ictl->caption);
            break;
#endif

        case DIALOG_CONTROL_EDIT:
            kind = QTH_DLG_EDIT;
            if(p_dialog_control_data.edit->edit_xx.bits.read_only)
                flags |= QTH_DLGF_READONLY;
            if(p_dialog_control_data.edit->edit_xx.bits.right_text)
                flags |= QTH_DLGF_RIGHT;
            if(p_dialog_control_data.edit->edit_xx.bits.multiline)
                flags |= QTH_DLGF_MULTILINE;
            break;

        case DIALOG_CONTROL_BUMP_S32:
        case DIALOG_CONTROL_BUMP_F64:
            kind = QTH_DLG_SPIN;
            if(p_dialog_control_data.bump_xx->edit_xx.bits.read_only)
                flags |= QTH_DLGF_READONLY;
            if(p_dialog_control_data.bump_xx->edit_xx.bits.right_text)
                flags |= QTH_DLGF_RIGHT;
            break;

        case DIALOG_CONTROL_LIST_S32:
        case DIALOG_CONTROL_LIST_TEXT:
            kind = QTH_DLG_LIST;
            break;

        case DIALOG_CONTROL_COMBO_S32:
        case DIALOG_CONTROL_COMBO_TEXT:
            kind = QTH_DLG_COMBO;
            if(p_dialog_control_data.combo_text && p_dialog_control_data.combo_text->combo_xx.edit_xx.bits.read_only)
                flags |= QTH_DLGF_READONLY;
            break;

        case DIALOG_CONTROL_PUSHPICTURE:
        case DIALOG_CONTROL_RADIOPICTURE:
        case DIALOG_CONTROL_CHECKPICTURE:
#ifdef DIALOG_HAS_TRISTATE
        case DIALOG_CONTROL_TRIPICTURE:
#endif
        case DIALOG_CONTROL_STATICPICTURE:
        case DIALOG_CONTROL_USER:
            kind = QTH_DLG_BLANK; /* owner-drawn on the other systems: to do */
            break;

        default:
            break;
        }

        if(0 != kind)
        {
            qth_dlg_add(p_dlg, kind, id,
                        px_h_pos(pixit_rect.tl.x - p_dialog->lx.offset_tl.x), px_v_pos(pixit_rect.tl.y - p_dialog->lx.offset_tl.y),
                        px_h_size(pixit_rect.br.x - pixit_rect.tl.x), px_v_size(pixit_rect.br.y - pixit_rect.tl.y),
                        flags, 0, caption);

            if(!*p_focused && p_dialog_ictl->p_dialog_control->bits.tabstop)
            {   /* the first control that can take the keyboard */
                switch(kind)
                {
                case QTH_DLG_EDIT: case QTH_DLG_SPIN: case QTH_DLG_LIST: case QTH_DLG_COMBO:
                    qth_dlg_set_focus(p_dlg, id);
                    *p_focused = TRUE;
                    break;
                default:
                    break;
                }
            }
        }

        if(p_dialog_ictl->dialog_control_type == DIALOG_CONTROL_GROUPBOX)
            dialog_linux_ictls_create(p_dialog, &p_dialog_ictl->data.groupbox.ictls, p_focused);
    }
}

_Check_return_
extern STATUS
dialog_dbox_process_windows(
    _InoutRef_  P_DIALOG p_dialog,
    _InoutRef_  P_DIALOG_CMD_PROCESS_DBOX p_dialog_cmd_process_dbox,
    _InRef_     PC_PIXIT_RECT p_pixit_rect)
{
    DIALOG_POSITION_TYPE dialog_position_type;
    QTH_WINDOW * p_parent = NULL;
    BOOL focused = FALSE;

    if((DOCNO_NONE != p_dialog->docno) && !p_dialog->modeless)
    {
        const P_DOCU p_docu = p_docu_from_docno(p_dialog->docno);
        const P_VIEW p_view = p_view_from_viewno_caret(p_docu);
        if(VIEW_NOT_NONE(p_view))
            p_parent = p_view->main[WIN_BACK].hwnd;
    }

    if(DIALOG_POSITION_DEFAULT == (dialog_position_type = ENUM_UNPACK(DIALOG_POSITION_TYPE, p_dialog_cmd_process_dbox->bits.dialog_position_type)))
        dialog_position_type = DIALOG_POSITION_NEAR_MOUSE;

    p_dialog->lx.offset_tl.x = p_pixit_rect->tl.x; /* so that the controls start at the top left of the dialog */
    p_dialog->lx.offset_tl.y = p_pixit_rect->tl.y;

    if(NULL == (p_dialog->lx.p_dlg = qth_dlg_create(p_parent, ui_text_tstr(&p_dialog->caption),
                                                    px_h_size(pixit_rect_width(p_pixit_rect)), px_v_size(pixit_rect_height(p_pixit_rect)),
                                                    dialog_linux_event, (void *) (intptr_t) p_dialog->h_dialog)))
        return(status_nomem());

    switch(dialog_position_type)
    {
    case DIALOG_POSITION_CENTRE_SCREEN:
        qth_dlg_centre(p_dialog->lx.p_dlg, 0);
        break;

    case DIALOG_POSITION_CENTRE_WINDOW:
        qth_dlg_centre(p_dialog->lx.p_dlg, 1);
        break;

    default:
        qth_dlg_at_pointer(p_dialog->lx.p_dlg);
        break;
    }

    dialog_linux_ictls_create(p_dialog, &p_dialog->ictls, &focused);

    /* loop over these new control representations encoding them and enabling them as appropriate */
    dialog_ictls_make_lists(p_dialog, &p_dialog->ictls);
    status_assert(dialog_ictls_encode_in(p_dialog, &p_dialog->ictls, 0));
    status_assert(dialog_ictls_enable_in(p_dialog, &p_dialog->ictls));

    { /* tell client that dialog processing is about to start */
    CLIENT_HANDLE client_handle;
    const P_PROC_DIALOG_EVENT p_proc_client = dialog_main_handler(p_dialog, &client_handle);

    if(NULL != p_proc_client)
    {
        DIALOG_MSG_PROCESS_START dialog_msg_process_start;
        msgclr(dialog_msg_process_start);
        dialog_msg_process_start.h_dialog = p_dialog->h_dialog;
        dialog_msg_process_start.client_handle = client_handle;
        status_assert(dialog_call_client(p_dialog, DIALOG_MSG_CODE_PROCESS_START, &dialog_msg_process_start, p_proc_client));
    }
    } /*block*/

    if(p_dialog->modeless)
        qth_dlg_show(p_dialog->lx.p_dlg);
    else
        qth_dlg_exec(p_dialog->lx.p_dlg); /* returns when the dialog has been completed */

    return(STATUS_OK);
}

extern void
dialog_linux_end(
    _InoutRef_  P_DIALOG p_dialog)
{
    if(DIALOG_IS_OPEN(p_dialog))
        qth_dlg_end(p_dialog->lx.p_dlg);
}

extern void
dialog_linux_dispose(
    _InoutRef_  P_DIALOG p_dialog)
{
    if(DIALOG_IS_OPEN(p_dialog))
    {
        qth_dlg_destroy(p_dialog->lx.p_dlg);
        p_dialog->lx.p_dlg = NULL;
    }
}

extern void
dialog_windows_ui_len_init(void)
{
    /* the dialog units come from the font the toolkit uses, whenever they are needed */
}

_Check_return_
extern STATUS
dialog_windows_help(
    _InoutRef_  P_DIALOG p_dialog)
{
    UNREFERENCED_PARAMETER_InoutRef_(p_dialog);

    return(STATUS_OK); /* no help yet */
}

#endif /* LINUX */

/* end of li_dlg.c */
