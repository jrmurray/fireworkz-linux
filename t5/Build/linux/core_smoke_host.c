/* core_smoke_host.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Minimal stand-ins for the skeleton/host symbols that the core references,
 * so that the core can be linked and exercised before ob_skel is ported.
 * NOT part of the application: each one is replaced by the real thing
 * as ob_skel comes up on Linux.
 */

#include "common/gflags.h"

#include "ob_skel/flags.h"

GLOBAL_PREFERENCES global_preferences;

const PCTSTR tstr_objects_dll_store = TEXT("");

_Check_return_
_Ret_z_
extern PCTSTR
product_family_id(void)
{
    return(TEXT("Fireworkz"));
}

extern void __cdecl
messagef(
    _In_z_      PCTSTR format,
    /**/        ...)
{
    va_list va;

    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
    fputc('\n', stderr);
}

_Check_return_
extern STATUS
object_next(
    _InoutRef_  P_OBJECT_ID p_object_id)
{
    UNREFERENCED_PARAMETER_InoutRef_(p_object_id);
    return(STATUS_FAIL); /* no objects loaded */
}

_Check_return_
extern BOOL
object_present(
    _InVal_     OBJECT_ID object_id)
{
    UNREFERENCED_PARAMETER_InVal_(object_id);
    return(FALSE);
}

_Check_return_
extern STATUS /* size out */
inline_quick_ublock_from_data(
    _InoutRef_  P_QUICK_UBLOCK p_quick_ublock /*appended*/,
    _InVal_     S32 inline_id,
    _InVal_     S32 inline_type,
    _In_reads_bytes_opt_(size) PC_ANY p_any,
    _InVal_     U32 size /* only for TYPE_ANY */)
{
    UNREFERENCED_PARAMETER_InoutRef_(p_quick_ublock);
    UNREFERENCED_PARAMETER_InVal_(inline_id);
    UNREFERENCED_PARAMETER_InVal_(inline_type);
    UNREFERENCED_PARAMETER(p_any);
    UNREFERENCED_PARAMETER_InVal_(size);
    return(STATUS_FAIL);
}

/* end of core_smoke_host.c */
