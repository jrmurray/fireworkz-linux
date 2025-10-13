/* version.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 2019-2025 Stuart Swales */

#define HEX_VERSION_NUMBER          0x23300

/* for Windows RC file */

/*#define VER_FILEVERSION           2,xx,y,0*/
/*#define VER_FILEVERSION           2,xx,0,0*/
#define VER_FILEVERSION             2,33,0,0

#define VER_FILEVERSION_STR       L"2.33.00"
/*#define VER_FILEVERSION_STR     L"2.xx.00"*/
/*#define VER_FILEVERSION_STR     L"2.xx.yy"*/

/*
when changing version number for release, remember:
    common.mf_mid
    ob_skel.version.h
    ob_skel.resource.xx.msg
* RISC OS
    firewrkz.RelNotes/htm
    firewrkz.r.RiscPkg.Control
    firewrkz.r32.MakeRiscPkg
* Windows
    firewrkz\RelNotesW.htm
    firewrkz\Resources\Windows\UK\res00.rci (updated by RISC OS build)
    Build\windows\OUTx64\BLDCD2.BAT
    Build\windows\OUTx64\firewrkz-x64.iss
    Build\windows\OUTx86\BLDCD2.BAT
    Build\windows\OUTx86\firewrkz-x86.iss

* and for Fireworkz Pro:
    ^.fprowrkz.RelNotes/htm
    ^.fprowrkz.r32.Make_TS
    ^.fprowrkz.r32.MakeStore

when creating a new branch, remember:
* RISC OS
    Build.!!!Boot
    Build.configure
* Windows
    Build\windows\t5_tbt.bat
*/

/* end of version.h */
