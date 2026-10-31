/* ho_menu.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Menu handling for the Linux version of Fireworkz: the counterpart of windows/ho_menu.c.
 *
 * The menus live in the skeleton as trees of MENU_ENTRYs hanging off the configuration document.
 * Here each view's window gets a menu bar built from the document's tree (and popups are built
 * when wanted), through qt_host.h. An item's client is its MENU_ENTRY, so choosing it needs no
 * command numbers; a submenu's client is its MENU_ROOT, so that when it is about to be shown its
 * items can be enabled and disabled to suit the document. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_toolb/xp_toolb.h"

#include "ob_skel/linux/qt_host.h"

static BOOL
disable[MENU_ACTIVE_STATE_COUNT];

/* the document and view that a window belongs to */

static P_DOCU
docu_of_window(
    _InRef_     QTH_WINDOW * p_window,
    _OutRef_    P_P_VIEW p_p_view)
{
    return(viewid_unpack((U32) (uintptr_t) qth_window_get_client(p_window), p_p_view, NULL));
}

/* Enquire with the application to see which menu items should be enabled and disabled.
 * Stored with each item is an index into the disable table, this flags which options should be
 * allowed and not and is generated in this function and used by the enable disable calls. */

static void
host_initmenu_for_docu(
    _DocuRef_   P_DOCU p_docu)
{
    MENU_OTHER_INFO menu_other_info;
    BOOL paste_valid;

    zero_struct(menu_other_info);

    /* ask our friends out there to fillin as much of this structure as they want */
    status_assert(object_call_all(p_docu, T5_MSG_MENU_OTHER_INFO, &menu_other_info));

    /* can only paste to certain target objects */
    paste_valid = ((OBJECT_ID_CELLS == p_docu->focus_owner) || (OBJECT_ID_NOTE == p_docu->focus_owner));

    if(paste_valid && (0 == local_clip_data_query()))
        paste_valid = FALSE; /* TODO: ask the desktop clipboard whether it has anything we can paste */

    /* now setup the disable array so that items can be enabled and disabled */
    disable[MENU_PASTE] = (!paste_valid);
    disable[MENU_SELECTION]       = (!menu_other_info.cells_selection) && (menu_other_info.note_selection_count == 0);
    disable[MENU_CELLS_SELECTION] = (!menu_other_info.cells_selection);
    disable[MENU_PICTURES]            = (menu_other_info.note_picture_count == 0);
    disable[MENU_PICTURE_SELECTION]   = (menu_other_info.note_selection_count == 0);
    disable[MENU_PICTURE_OR_BACKDROP] = (menu_other_info.note_selection_count != 1) && (menu_other_info.note_behind_count != 1);
    disable[MENU_CARET_IN_CELLS]      = (OBJECT_ID_CELLS != p_docu->focus_owner);
    disable[MENU_CARET_IN_TEXT]       = (OBJECT_ID_CELLS != p_docu->focus_owner) && (OBJECT_ID_HEADER != p_docu->focus_owner) && (OBJECT_ID_FOOTER != p_docu->focus_owner);
}

static void
host_initmenu_recurse(
    _DocuRef_maybenone_ PC_DOCU p_docu,
    P_MENU_ROOT p_menu_root,
    QTH_MENU * p_menu)
{
    ARRAY_INDEX index = array_elements(&p_menu_root->h_entry_list);

    while(--index >= 0)
    {
        P_MENU_ENTRY p_menu_entry = array_ptr(&p_menu_root->h_entry_list, MENU_ENTRY, index);
        PC_U8Z p_command = array_basec(&p_menu_entry->h_command, U8Z);
        BOOL enabled = !disable[p_menu_entry->flags.active_state];

        if(P_DATA_NONE != p_command)
        {
            const PC_U8Z p_colon = strchr(p_command, CH_COLON);

            if(NULL != p_colon)
            {
                const U32 n_bytes_command = PtrDiffBytesU32(p_colon, p_command) + 1;
                const U32 n_bytes_test = sizeof32("Button:")-1;

                if( (n_bytes_command == n_bytes_test) && (0 == memcmp("Button:", p_command, n_bytes_test)) )
                {   /* the entry does what a toolbar button does, so is enabled when that is */
                    const PC_U8Z p_button_name = PtrAddBytes(PC_USTR, p_command, n_bytes_command);
                    const PC_U8Z p_button_optional_args = strchr(p_button_name, CH_SEMICOLON);
                    const U32 button_name_len = (NULL != p_button_optional_args) ? PtrDiffBytesU32(p_button_optional_args, p_button_name) : ustrlen32(p_button_name);
                    T5_TOOLBAR_TOOL_ENABLE_QUERY t5_toolbar_tool_enable_query;

                    t5_toolbar_tool_enable_query.name = p_button_name;
                    t5_toolbar_tool_enable_query.name_len = button_name_len;
                    status_assert(object_call_id(OBJECT_ID_TOOLBAR, de_const_cast(P_DOCU, p_docu), T5_MSG_TOOLBAR_TOOL_ENABLE_QUERY, &t5_toolbar_tool_enable_query));
                    enabled = t5_toolbar_tool_enable_query.enabled;
                }
            }
        }

        qth_menu_set_item_enabled(p_menu, (int) index, enabled);

        if(p_menu_entry->sub_menu.h_entry_list)
            host_initmenu_recurse(p_docu, &p_menu_entry->sub_menu, qth_menu_get_submenu(p_menu, (int) index));
    }
}

static void
li_menu_event(
    QTH_WINDOW * p_qth_window,
    int kind,
    void * client,
    QTH_MENU * p_menu)
{
    P_VIEW p_view;
    const P_DOCU p_docu = docu_of_window(p_qth_window, &p_view);

    if(P_DOCU_NONE == p_docu)
        return;

    switch(kind)
    {
    case QTH_MENU_ACTIVATE:
        {
        const P_MENU_ENTRY p_menu_entry = (P_MENU_ENTRY) client;

        /* the command is a macro script; any error has already been reported */
        if(p_menu_entry->h_command)
            status_consume(command_array_handle_execute(docno_from_p_docu(p_docu), p_menu_entry->h_command));
        break;
        }

    case QTH_MENU_ABOUT_TO_SHOW:
        {
        const P_MENU_ROOT p_menu_root = (P_MENU_ROOT) client;

        zero_array(disable);

        host_initmenu_for_docu(p_docu);

        host_initmenu_recurse(p_docu, p_menu_root, p_menu);
        break;
        }

    default:
        break;
    }
}

/* Build a menu from the tree: items, separators and submenus (the keys beside the items are
 * informational, as they are on Windows: the skeleton binds the keys itself) */

static void
make_host_menu(
    _DocuRef_   P_DOCU p_docu,
    P_MENU_ROOT p_menu_root,
    QTH_MENU * p_menu)
{
    const ARRAY_INDEX entry_count = array_elements(&p_menu_root->h_entry_list);
    ARRAY_INDEX index;

    for(index = 0; index < entry_count; ++index)
    {
        P_MENU_ENTRY p_menu_entry = array_ptr(&p_menu_root->h_entry_list, MENU_ENTRY, index);
        TCHARZ buffer[64];

        if(NULL == p_menu_entry->tstr_entry_text)
        {
            qth_menu_add_separator(p_menu);
            continue;
        }

        buffer[0] = CH_NULL;
        ui_string_to_text(p_docu, buffer, elemof32(buffer), p_menu_entry->tstr_entry_text);

        if(p_menu_entry->sub_menu.h_entry_list)
        {
            QTH_MENU * p_sub = qth_menu_add_submenu(p_menu, buffer, &p_menu_entry->sub_menu);

            if(NULL != p_sub)
                make_host_menu(p_docu, &p_menu_entry->sub_menu, p_sub);
        }
        else
            qth_menu_add_item(p_menu, buffer, key_ui_name_from_key_code(p_menu_entry->key_code, 0 /*short*/), p_menu_entry);
    }
}

/* The menus are made for each view's window when it is wanted */

_Check_return_
extern STATUS
ho_create_docu_menus(
    _DocuRef_   P_DOCU p_docu)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    return(STATUS_OK);
}

_Check_return_
extern STATUS
ho_menu_msg_close2(
    _DocuRef_   P_DOCU p_docu)
{
    UNREFERENCED_PARAMETER_DocuRef_(p_docu);

    return(STATUS_OK);
}

/* Give the view's window the menu bar for the given menu root (the document's, usually) */

extern void
ho_attach_menu_bar(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     S32 menu_root_id)
{
    const P_DOCU p_docu_config = p_docu_from_config_wr();
    P_MENU_ROOT p_menu_root;
    QTH_MENU * p_menu_bar;

    if(HOST_WND_NONE == p_view->main[WIN_BACK].hwnd)
        return;

    if(NULL == (p_menu_root = sk_menu_root(p_docu_config, (MENU_ROOT_ID) menu_root_id)))
        return;

    if(NULL != (p_menu_bar = qth_window_create_menu_bar(p_view->main[WIN_BACK].hwnd, li_menu_event)))
        make_host_menu(p_docu, p_menu_root, p_menu_bar);
}

/* Given a document change its menu bar to a new one.  This allows editing objects to change
 * their menus to reflect their state. */

extern void
ho_prefer_menu_bar(
    _DocuRef_   P_DOCU p_docu_in,
    _InVal_     S32 menu_id)
{
    VIEWNO viewno = VIEWNO_NONE;
    P_VIEW p_view;

    while(P_VIEW_NONE != (p_view = docu_enum_views(p_docu_in, &viewno)))
        ho_attach_menu_bar(p_docu_in, p_view, menu_id);
}

/* Given the index for a menu make a popup of it where the pointer is */

_Check_return_
extern STATUS
ho_menu_popup(
    _DocuRef_   P_DOCU p_docu,
    _ViewRef_   P_VIEW p_view,
    _InVal_     S32 menu_root_id)
{
    const P_DOCU p_docu_config = p_docu_from_config_wr();
    P_MENU_ROOT p_menu_root;
    QTH_MENU * p_popup;

    if(NULL == (p_menu_root = sk_menu_root(p_docu_config, (MENU_ROOT_ID) menu_root_id)))
        return(STATUS_FAIL);

    if(NULL == (p_popup = qth_menu_create_popup(p_view->main[WIN_BACK].hwnd, li_menu_event, p_menu_root)))
        return(STATUS_FAIL);

    make_host_menu(p_docu, p_menu_root, p_popup);

    /* bring the enabling up to date, as being about to be shown does for a menu bar */
    li_menu_event(p_view->main[WIN_BACK].hwnd, QTH_MENU_ABOUT_TO_SHOW, p_menu_root, p_popup);

    qth_menu_popup(p_popup);

    return(STATUS_OK);
}

extern void
ho_menu_dispose(void)
{
}

/* end of ho_menu.c */
