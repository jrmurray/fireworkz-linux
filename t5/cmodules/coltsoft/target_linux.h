/* target_linux.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Native Linux target (GCC or Clang) */

#ifndef TARGET_LINUX_H
#define TARGET_LINUX_H

#if !defined(__CHAR_UNSIGNED__)
#error       __CHAR_UNSIGNED__ must be set (use -funsigned-char switch)
#endif

#include "cmodules/coltsoft/ns-sal.h"

#define F64_IS_64_BIT_ALIGNED 1

#define inline_when_fast_fp /*nothing*/

#define ___assert(e, s) 0 /* Norcroft specific */

#define __TFILE__ __FILE__
#define __Tfunc__ __func__

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-assign"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#endif /* TARGET_LINUX_H */

/* end of target_linux.h */
