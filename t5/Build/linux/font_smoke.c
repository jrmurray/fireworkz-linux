/* font_smoke.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Smoke test for the Linux host font path, against the headless backend
 * (ob_skel/linux/qth_null.c, where every character is half an em wide) */

#include "common/gflags.h"

#include "ob_skel/flags.h"

#include "ob_skel/linux/qt_host.h"

static int n_failures = 0;

#define check(cond) \
    do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++n_failures; } } while(0)

static unsigned int n_enumerated;
static unsigned int n_bold_italic;

static void
count_font(const QTH_FONT_DESC * p_desc, void * client)
{
    check(&n_enumerated == client);
    ++n_enumerated;

    if((p_desc->weight >= QTH_WEIGHT_BOLD) && p_desc->italic)
        ++n_bold_italic;
}

static void
test_enumerate(void)
{
    qth_font_enumerate(count_font, &n_enumerated);

    check(12 == n_enumerated); /* 3 families, 4 styles each */
    check(3 == n_bold_italic);
}

static void
test_host_font(void)
{
    HOST_FONT_SPEC host_font_spec;
    HOST_FONT host_font;
    QTH_FONT_METRICS metrics;

    zero_struct(host_font_spec);
    xstrkpy(host_font_spec.logfont.lfFaceName, elemof32(host_font_spec.logfont.lfFaceName), "Null Sans");
    host_font_spec.logfont.lfWeight = QTH_WEIGHT_BOLD;
    host_font_spec.size_x = host_font_spec.size_y = 240; /* 12pt */

    host_font = host_font_find(&host_font_spec, P_REDRAW_CONTEXT_NONE);
    check(HOST_FONT_NONE != host_font);

    qth_font_metrics(host_font, &metrics);
    check(192 == metrics.ascent);
    check(48 == metrics.descent);
    check(120 == metrics.space_width);

    check(480 == host_fonty_uchars_width(host_font, ustr_bptr("abcd"), 4));
    check(0 == host_fonty_uchars_width(host_font, ustr_bptr(""), 0));

    host_font_dispose(&host_font, P_REDRAW_CONTEXT_NONE);
    check(HOST_FONT_NONE == host_font);
}

int
main(void)
{
    test_enumerate();
    test_host_font();

    if(0 != n_failures)
    {
        fprintf(stderr, "font_smoke: %d failure(s)\n", n_failures);
        return(EXIT_FAILURE);
    }

    printf("font_smoke: all checks passed\n");
    return(EXIT_SUCCESS);
}

/* end of font_smoke.c */
