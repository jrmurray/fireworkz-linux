/* ho_color.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Host colours */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

/* The RGB stash defines the RGB values used for painting the colour patches,
 * which are the RISC OS Wimp's 16 standard colours as they are on Windows.
 * It also holds some of the colours used to paint borders, rulers and the desktop,
 * which are indexes into it (COLOUR_OF_xxx in xp_skel.h).
 *
 * TODO: take the greys from the desktop's palette, so that borders suit a dark theme
 */

RGB
rgb_stash[COLOUR_STASH_MAX] =
{
    RGB_INIT(255, 255, 255), /* white */
    RGB_INIT(221, 221, 221),
    RGB_INIT(187, 187, 187),
    RGB_INIT(153, 153, 153), /* greyscale */
    RGB_INIT(117, 117, 117),
    RGB_INIT( 85,  85,  85),
    RGB_INIT( 51,  51,  51),
    RGB_INIT(  0,   0,   0), /* black */

    RGB_INIT(  0, 136,  68), /* dark blue */
    RGB_INIT(255, 238,   0), /* mellow yellow */
    RGB_INIT(  0, 221,   0), /* light green */
    RGB_INIT(221,   0,   0), /* simply red */
    RGB_INIT(238, 238, 204), /* fresh cream */
    RGB_INIT( 68, 136,   0), /* dark green */
    RGB_INIT(255, 204,   0), /* orange */
    RGB_INIT(  0, 255, 255)  /* light blue */
};

/* Always initialised statically, so nothing to do: kept for the skeleton, which calls it when the palette changes */

extern void
host_rgb_stash(void)
{
}

/* end of ho_color.c */
