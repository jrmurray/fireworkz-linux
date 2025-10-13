/* mathxtra.h */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Copyright (C) 1991-1998 Colton Software Limited
 * Copyright (C) 1998-2015 R W Colton */

/* SKS July 1991 */

#ifndef MATHXTRA_H
#define MATHXTRA_H

/*
exported functions
*/

#define PRAGMA_SIDE_EFFECTS_OFF
#include "coltsoft/pragma.h"
/* note that ANSI errno is volatile to enable this sort of CSE optimization */

_Check_return_
extern F64
mathxtra_acosh(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_acosec(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_acosech(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_acot(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_acoth(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_asec(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_asech(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_asinh(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_atanh(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_cosec(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_cosech(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_cot(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_coth(
    _InVal_     F64 x);

/* return the square of a number (or more likely, hard expression) */

_Check_return_
static inline F64
mathxtra_square(
    _InVal_     F64 x)
{
    return(x * x);
}

_Check_return_
extern F64
mathxtra_hypot(
    _InVal_     F64 x,
    _InVal_     F64 y);

_Check_return_
extern F64
mathxtra_sec(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_sech(
    _InVal_     F64 x);

_Check_return_
extern F64
mathxtra_gcd(
    _InVal_     F64 a,
    _InVal_     F64 b);

_Check_return_
extern F64
mathxtra_lcm(
    _InVal_     F64 a,
    _InVal_     F64 b);

#define PRAGMA_SIDE_EFFECTS
#include "coltsoft/pragma.h"


/* C99 has both erf and erfc; MSVC earlier than 2013 don't */

#if defined(_MSC_VER)
#if _MSC_VER < 1800 /* < VS2013 */

extern double FreeBSD_erf(double);

#define erf FreeBSD_erf

extern double FreeBSD_erfc(double);

#define erfc FreeBSD_erfc

#endif
#endif /* _MSC_VER */


_Check_return_
extern double FreeBSD_jn(int, double);

#define bessel_jn(n, x) FreeBSD_jn(n, x)

_Check_return_
extern double FreeBSD_yn(int, double);

#define bessel_yn(n, x) FreeBSD_yn(n, x)


#if __STDC_VERSION__ < 199901L

#define acosh(d) mathxtra_acosh(d)
#define asinh(d) mathxtra_asinh(d)
#define atanh(d) mathxtra_atanh(d)

#define hypot(x, y) mathxtra_hypot(x, y)

#define isgreater(x,y)      (!isunordered((x), (y)) && ((x) > (y)))
#define isgreaterequal(x,y) (!isunordered((x), (y)) && ((x) >= (y)))
#define isless(x,y)         (!isunordered((x), (y)) && ((x) < (y)))
#define islessequal(x,y)    (!isunordered((x), (y)) && ((x) <= (y)))
#define	islessgreater(x, y)	(!isunordered((x), (y)) && ((x) > (y) || (y) > (x)))
#define isunordered(x,y)    (isnan(x) || isnan(y))

_Check_return_
static inline double
fmax(_InVal_ double a, _InVal_ double b)
{
    if(isnan(a))
        return(b);

    return(isgreater(a, b) ? a : b);
}

_Check_return_
static inline double
fmin(_InVal_ double a, _InVal_ double b)
{
    if(isnan(a))
        return(b);

    return(isless(a, b) ? a : b);
}

_Check_return_
static inline double
exp2(_InVal_ double d)
{
    return(pow(2.0, d));
}

#if WINDOWS

#ifndef                     MATHNUMS_H
#include "cmodules/coltsoft/mathnums.h" /* for _log2_e */
#endif

_Check_return_
static inline double
log2(_InVal_ double d)
{
    return(log(d) * _log2_e);
}

_Check_return_
static inline double
fma(_InVal_ double x, _InVal_ double y, _InVal_ double z)
{
    return((x * y) + z);
}

#endif /* OS */

#endif /* __STDC_VERSION__ */


#endif /* MATHXTRA_H */

/* end of mathxtra.h  */
