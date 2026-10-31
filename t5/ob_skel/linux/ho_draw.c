/* linux/ho_draw.c */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* Draw files for Fireworkz for Linux: the objects of a RISC OS Draw file painted with Qt paths.
 *
 * Windows and RISC OS have a Draw renderer of their own (the Dial Solutions DLL and the Draw module): this
 * is the third. It handles what Fireworkz makes, and the common things in Draw files from elsewhere: paths
 * (lines, curves, fills, dashes, caps and joins), text (plain and transformed) with the font list,
 * and groups and tagged objects. It does not yet do sprites, JPEGs, Windows DIBs or text areas. */

#include "common/gflags.h"

#ifndef      SKEL_FLAGS_H
#include "ob_skel/flags.h"
#endif

#include "ob_skel/linux/qt_host.h"

#define DRAW_FILE_HEADER_BYTES 40 /* "Draw", two version words, the 12 byte name of the program, the 16 byte bounding box */

/* what the objects have to be drawn with */

typedef struct DRAW_RENDER
{
    QTH_PAINTER * p_painter;
    const uint8_t * p_data;
    uint32_t length;
    double ax, ay;           /* device x = ax + draw x * sx; device y = ay - draw y * sy */
    double sx, sy;           /* pixels per Draw unit */
    double thin;             /* the width of a thin line, in pixels */
    const char * font_name[256]; /* from the font list, by font reference */
}
DRAW_RENDER, * P_DRAW_RENDER;

static uint32_t
get_u32(const uint8_t * p)
{
    uint32_t u;
    memcpy(&u, p, sizeof(u)); /* Draw files are little-endian and so is this */
    return(u);
}

static int32_t
get_s32(const uint8_t * p)
{
    return((int32_t) get_u32(p));
}

static QTH_RGB
rgb_of_draw_colour(const uint32_t draw_colour)
{
    QTH_RGB rgb; /* &BBGGRR00 */
    rgb.r = (uint8_t) (draw_colour >> 8);
    rgb.g = (uint8_t) (draw_colour >> 16);
    rgb.b = (uint8_t) (draw_colour >> 24);
    return(rgb);
}

#define DRAW_COLOUR_TRANSPARENT 0xFFFFFFFFU

/* ---- the font list ---------------------------------------------------------------------- */

static void
draw_read_font_list(
    P_DRAW_RENDER p_render,
    const uint8_t * p_object,
    const uint32_t size)
{
    const uint8_t * p = p_object + 8;
    const uint8_t * const p_end = p_object + size;

    while(p < p_end)
    {
        const uint8_t font_ref = *p++;
        const uint8_t * const p_name = p;

        if(0 == font_ref)
            break;

        while((p < p_end) && (0 != *p))
            ++p;

        if(p >= p_end)
            break;

        p_render->font_name[font_ref] = (const char *) p_name;
        p += 1; /* the terminator, and then the entry is padded to a word at the end of the list only */
    }
}

/* RISC OS font names are Family.Weight.Style, e.g. Homerton.Bold.Oblique, or just Family */

static void
draw_font_desc_from_name(
    _Out_       QTH_FONT_DESC * p_desc,
    const char * name)
{
    memset(p_desc, 0, sizeof(*p_desc));
    p_desc->weight = QTH_WEIGHT_NORMAL;
    p_desc->family_class = QTH_FAMILY_SWISS;
    strcpy(p_desc->family, "Sans Serif");

    if(NULL == name)
        return;

    if(0 == strncmp(name, "Trinity", 7) || 0 == strncmp(name, "Times", 5))
    {
        p_desc->family_class = QTH_FAMILY_ROMAN;
        strcpy(p_desc->family, "Serif");
    }
    else if(0 == strncmp(name, "Corpus", 6) || 0 == strncmp(name, "Courier", 7))
    {
        p_desc->family_class = QTH_FAMILY_MODERN;
        p_desc->fixed_pitch = 1;
        strcpy(p_desc->family, "Monospace");
    }
    else if(0 == strncmp(name, "Homerton", 8) || 0 == strncmp(name, "Arial", 5) || 0 == strncmp(name, "Helvetica", 9))
    {   /* the default */
    }
    else
    {   /* a Windows font name, as in the Windows font list, or one we know nothing of: try it as it is */
        strncpy(p_desc->family, name, sizeof(p_desc->family) - 1);
        p_desc->family_class = QTH_FAMILY_DONTCARE;
    }

    if(NULL != strstr(name, "Bold"))
        p_desc->weight = QTH_WEIGHT_BOLD;

    if((NULL != strstr(name, "Italic")) || (NULL != strstr(name, "Oblique")))
        p_desc->italic = 1;
}

/* ---- paths ------------------------------------------------------------------------------ */

static void
draw_paint_path(
    P_DRAW_RENDER p_render,
    const uint8_t * p_object,
    const uint32_t size)
{
    const uint32_t fill_colour = get_u32(p_object + 24);
    const uint32_t line_colour = get_u32(p_object + 28);
    const int32_t line_width = get_s32(p_object + 32);
    const uint32_t style = get_u32(p_object + 36);
    const uint8_t style_flags = (uint8_t) style;
    const uint8_t * p = p_object + 40;
    const uint8_t * const p_end = p_object + size;
    double dashes[6 + 1];
    int n_dashes = 0;
    double dash_offset = 0.0;
    QTH_PATH_STYLE path_style;
    QTH_PATH * p_path;

    memset(&path_style, 0, sizeof(path_style));

    if(style_flags & 0x80)
    {   /* dash pattern: where we start in it, how many elements, and the elements */
        uint32_t i, count;

        if(p_end - p < 8)
            return;

        dash_offset = get_s32(p) * p_render->sx;
        count = get_u32(p + 4);
        p += 8;

        if((count > 6) || ((uint32_t) (p_end - p) < 4 * count))
            return;

        for(i = 0; i < count; ++i, p += 4)
            dashes[n_dashes++] = get_s32(p) * p_render->sx;
    }

    if(DRAW_COLOUR_TRANSPARENT != fill_colour)
    {
        path_style.fill = 1;
        path_style.fill_rgb = rgb_of_draw_colour(fill_colour);
    }

    if((DRAW_COLOUR_TRANSPARENT != line_colour) && ((line_width > 0) || (DRAW_COLOUR_TRANSPARENT == fill_colour) || 1))
    {
        path_style.stroke = 1;
        path_style.line_rgb = rgb_of_draw_colour(line_colour);
        path_style.line_width = (line_width > 0) ? (line_width * p_render->sx) : p_render->thin;
        if(path_style.line_width < p_render->thin)
            path_style.line_width = p_render->thin;
    }

    path_style.join = (style_flags & 3) == 1 ? QTH_JOIN_ROUND : (style_flags & 3) == 2 ? QTH_JOIN_BEVEL : QTH_JOIN_MITRE;
    path_style.cap = ((style_flags >> 2) & 3) == 1 ? QTH_CAP_ROUND : ((style_flags >> 2) & 3) == 2 ? QTH_CAP_SQUARE : QTH_CAP_BUTT; /* triangle caps as butt */
    path_style.even_odd = (0 != (style_flags & 0x40));
    path_style.mitre_limit = 10.0;

    if(n_dashes > 0)
    {
        path_style.p_dashes = dashes;
        path_style.n_dashes = n_dashes;
        path_style.dash_offset = dash_offset;
    }

    if(!path_style.fill && !path_style.stroke)
        return;

    if(NULL == (p_path = qth_path_create()))
        return;

#define DX(x) (p_render->ax + (double) (x) * p_render->sx)
#define DY(y) (p_render->ay - (double) (y) * p_render->sy)

    while(p_end - p >= 4)
    {
        const uint32_t element = get_u32(p);
        p += 4;

        if(0 == element)
            break;

        if((2 == element) || (8 == element))
        {
            if(p_end - p < 8)
                break;
            if(2 == element)
                qth_path_move_to(p_path, DX(get_s32(p)), DY(get_s32(p + 4)));
            else
                qth_path_line_to(p_path, DX(get_s32(p)), DY(get_s32(p + 4)));
            p += 8;
        }
        else if(6 == element)
        {
            if(p_end - p < 24)
                break;
            qth_path_curve_to(p_path, DX(get_s32(p)), DY(get_s32(p + 4)), DX(get_s32(p + 8)), DY(get_s32(p + 12)), DX(get_s32(p + 16)), DY(get_s32(p + 20)));
            p += 24;
        }
        else if(5 == element)
            qth_path_close(p_path);
        else
            break; /* not a path element: the file is damaged */
    }

    qth_paint_path(p_render->p_painter, p_path, &path_style);

    qth_path_destroy(p_path);
}

/* ---- text ------------------------------------------------------------------------------- */

static void
draw_paint_text(
    P_DRAW_RENDER p_render,
    const uint8_t * p_object,
    const uint32_t size,
    const int transformed)
{
    const uint32_t first = transformed ? 52 : 24; /* the text colour */
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, e = 0.0, f = 0.0;
    uint32_t text_colour, fsize_x, fsize_y, text_offset;
    int32_t coord_x, coord_y;
    uint8_t font_ref;
    const uint8_t * p_text;
    uint32_t n;
    QTH_FONT_DESC desc;
    double px, py;

    text_offset = first + 4 + 4 + 4 + 4 + 4 + 8; /* colours, style, two sizes and the origin */
    if(size < text_offset + 1)
        return;

    if(transformed)
    {
        a = get_s32(p_object + 24) / 65536.0;
        b = get_s32(p_object + 28) / 65536.0;
        c = get_s32(p_object + 32) / 65536.0;
        d = get_s32(p_object + 36) / 65536.0;
        e = get_s32(p_object + 40);
        f = get_s32(p_object + 44);
    }

    text_colour = get_u32(p_object + first);
    font_ref = p_object[first + 8];
    fsize_x = get_u32(p_object + first + 12);
    fsize_y = get_u32(p_object + first + 16);
    coord_x = get_s32(p_object + first + 20);
    coord_y = get_s32(p_object + first + 24);

    if(DRAW_COLOUR_TRANSPARENT == text_colour)
        return;

    p_text = p_object + text_offset;
    n = 0;
    while(((text_offset + n) < size) && (0 != p_text[n]))
        ++n;

    if(0 == n)
        return;

    draw_font_desc_from_name(&desc, p_render->font_name[font_ref]);

    /* where the baseline starts, after the transformation's turn and move */
    px = p_render->ax + (a * coord_x + c * coord_y + e) * p_render->sx;
    py = p_render->ay - (b * coord_x + d * coord_y + f) * p_render->sy;

    /* sizes are in 1/640 of a point: Draw units to pixels is sx, and there are 46080 of those to an inch */
    qth_paint_text_scaled(p_render->p_painter, &desc,
                          (double) fsize_x / 640.0 / 72.0 * (46080.0 * p_render->sx),
                          (double) fsize_y / 640.0 / 72.0 * (46080.0 * p_render->sy),
                          px, py,
                          a, -b, -c, d,
                          p_text, n, rgb_of_draw_colour(text_colour));
}

/* ---- the objects ------------------------------------------------------------------------ */

static void
draw_paint_objects(
    P_DRAW_RENDER p_render,
    const uint8_t * p,
    const uint8_t * const p_end,
    int depth)
{
    while(p_end - p >= 8)
    {
        const uint32_t type = get_u32(p);
        const uint32_t size = get_u32(p + 4);

        if((size < 8) || (0 != (size & 3)) || (size > (uint32_t) (p_end - p)))
            return; /* damaged */

        switch(type)
        {
        case 0:
            draw_read_font_list(p_render, p, size);
            break;

        case 1:
            draw_paint_text(p_render, p, size, 0);
            break;

        case 2:
            draw_paint_path(p_render, p, size);
            break;

        case 6: /* group: the objects follow its box and name */
            if((size >= 36) && (depth < 16))
                draw_paint_objects(p_render, p + 36, p + size, depth + 1);
            break;

        case 7: /* tagged: the object follows its box and tag */
            if((size >= 32) && (depth < 16))
                draw_paint_objects(p_render, p + 28, p + size, depth + 1);
            break;

        case 12:
            draw_paint_text(p_render, p, size, 1);
            break;

        default: /* options, sprites, text areas, JPEGs and the rest: not drawn */
            break;
        }

        p += size;
    }
}

/* Render the given Draw file at the required position, scaled by the pair. */

extern void
host_paint_drawfile(
    _InRef_     PC_REDRAW_CONTEXT p_redraw_context,
    _InRef_     PC_PIXIT_POINT p_pixit_point,
    _InRef_     PC_GR_SCALE_PAIR p_gr_scale_pair,
    _InRef_     PC_DRAW_DIAG p_draw_diag,
    _InVal_     BOOL eor_paths)
{
    DRAW_RENDER render;
    GDI_POINT gdi_point;
    double scale_x, scale_y;
    int32_t x0, y1;
    QTH_PAINTER * const p_painter = (QTH_PAINTER *) p_redraw_context->linux_ctx.painter;

    UNREFERENCED_PARAMETER_InVal_(eor_paths); /* drawn as they are: there is no exclusive-or here */

    if((NULL == p_painter) || (0 == p_draw_diag->length) || (NULL == p_draw_diag->data))
        return;

    if(p_draw_diag->length < DRAW_FILE_HEADER_BYTES)
        return;

    gdi_point_from_pixit_point_and_context(&gdi_point, p_pixit_point, p_redraw_context);

    scale_x = (double) muldiv64(p_gr_scale_pair->x, p_redraw_context->host_xform.scale.t.x, p_redraw_context->host_xform.scale.b.x) / GR_SCALE_ONE;
    scale_y = (double) muldiv64(p_gr_scale_pair->y, p_redraw_context->host_xform.scale.t.y, p_redraw_context->host_xform.scale.b.y) / GR_SCALE_ONE;

    memset(&render, 0, sizeof(render));
    render.p_painter = p_painter;
    render.p_data = (const uint8_t *) p_draw_diag->data;
    render.length = p_draw_diag->length;

    render.sx = scale_x * p_redraw_context->host_xform.linux_xform.pixels_per_inch.x / 46080.0;
    render.sy = scale_y * p_redraw_context->host_xform.linux_xform.pixels_per_inch.y / 46080.0;

    /* the file's top left goes at the point */
    x0 = get_s32(render.p_data + 24);
    y1 = get_s32(render.p_data + 36);
    render.ax = gdi_point.x - x0 * render.sx;
    render.ay = gdi_point.y + y1 * render.sy;

    render.thin = (p_redraw_context->host_xform.linux_xform.pixels_per_inch.x > 200) ? p_redraw_context->host_xform.linux_xform.pixels_per_inch.x / 200.0 : 1.0;

    draw_paint_objects(&render, render.p_data + DRAW_FILE_HEADER_BYTES, render.p_data + render.length, 0);
}

/* The size of the Draw file's bounding box in pixits */

extern void
host_read_drawfile_pixit_size(
    /*_In_reads_bytes_(DRAW_FILE_HEADER)*/ PC_ANY p_any,
    _OutRef_    P_PIXIT_SIZE p_pixit_size)
{
    PC_DRAW_FILE_HEADER p_draw_file_header = (PC_DRAW_FILE_HEADER) p_any;
    DRAW_POINT draw_size;

    draw_size.x = p_draw_file_header->bbox.x1 - p_draw_file_header->bbox.x0;
    draw_size.y = p_draw_file_header->bbox.y1 - p_draw_file_header->bbox.y0;

    /* flooring ok 'cos paths got rounded out on loading to worst-case pixels */
    p_pixit_size->cx = muldiv64_floor(draw_size.x, 1, 2 * RISCDRAW_PER_RISCOS) * 2 * PIXITS_PER_RISCOS;
    p_pixit_size->cy = muldiv64_floor(draw_size.y, 1, 2 * RISCDRAW_PER_RISCOS) * 2 * PIXITS_PER_RISCOS;
}

/* end of linux/ho_draw.c */
