/* qth_null.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Headless backend for qt_host.h: a fixed set of fonts with simple, predictable
 * metrics (every character is half an em wide), so that the skeleton can be built
 * and tested without Qt. The Qt backend replaces this. */

#include "ob_skel/linux/qt_host.h"

#include <stdlib.h>
#include <string.h>

struct QTH_FONT
{
    int32_t size_x;
    int32_t size_y;
};

static const struct
{
    const char * family;
    uint8_t fixed_pitch;
    uint8_t family_class;
}
null_families[] =
{
    { "Null Sans",  0, QTH_FAMILY_SWISS },
    { "Null Serif", 0, QTH_FAMILY_ROMAN },
    { "Null Mono",  1, QTH_FAMILY_MODERN }
};

void
qth_font_enumerate(QTH_FONT_ENUM_PROC proc, void * client)
{
    unsigned int family, style;

    for(family = 0; family < sizeof(null_families) / sizeof(null_families[0]); ++family)
    {
        for(style = 0; style < 4; ++style)
        {
            QTH_FONT_DESC desc;

            memset(&desc, 0, sizeof(desc));
            strcpy(desc.family, null_families[family].family);
            desc.weight = (style & 1) ? QTH_WEIGHT_BOLD : QTH_WEIGHT_NORMAL;
            desc.italic = (style & 2) ? 1 : 0;
            desc.fixed_pitch = null_families[family].fixed_pitch;
            desc.family_class = null_families[family].family_class;

            (*proc)(&desc, client);
        }
    }
}

QTH_FONT *
qth_font_create(const QTH_FONT_DESC * p_desc, int32_t size_x_pixits, int32_t size_y_pixits, uint8_t underline)
{
    QTH_FONT * p_font;

    (void) p_desc;
    (void) underline;

    if(NULL == (p_font = malloc(sizeof(*p_font))))
        return(NULL);

    p_font->size_x = size_x_pixits;
    p_font->size_y = size_y_pixits;

    return(p_font);
}

void
qth_font_destroy(QTH_FONT * p_font)
{
    free(p_font);
}

void
qth_font_metrics(const QTH_FONT * p_font, QTH_FONT_METRICS * p_metrics)
{
    p_metrics->ascent = (p_font->size_y * 4) / 5;
    p_metrics->descent = p_font->size_y - p_metrics->ascent;
    p_metrics->space_width = p_font->size_x / 2;
}

int32_t
qth_font_text_width(const QTH_FONT * p_font, const uint8_t * p_latin1, uint32_t n)
{
    (void) p_latin1;

    return((int32_t) ((p_font->size_x / 2) * (int32_t) n));
}

/* end of qth_null.c */
