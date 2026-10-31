/* qth_qt.cpp */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The Qt 6 implementation of qt_host.h */

#include "ob_skel/linux/qt_host.h"
#include "ob_skel/linux/qt/qth_qt.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QScreen>
#include <QString>
#include <QWheelEvent>
#include <QWidget>

#include <map>

static int modifiers_of(Qt::KeyboardModifiers modifiers);

/* ---- Application ---------------------------------------------------------- */

static QApplication *
app(void)
{
    static int argc = 1;
    static char arg0[] = "fireworkz";
    static char * argv[] = { arg0, nullptr };
    static QApplication * p_app = nullptr;

    if(nullptr == p_app)
        p_app = new QApplication(argc, argv);

    return(p_app);
}

extern "C" int
qth_app_run(void)
{
    return(app()->exec());
}

extern "C" void
qth_process_events(int wait)
{
    app()->processEvents(wait ? QEventLoop::WaitForMoreEvents : QEventLoop::AllEvents, wait ? 50 : 0);
}

extern "C" int
qth_keyboard_modifiers(void)
{
    return(modifiers_of(QGuiApplication::keyboardModifiers()));
}

extern "C" void
qth_beep(void)
{
    app()->beep();
}

extern "C" void
qth_app_quit(int exit_code)
{
    app()->exit(exit_code);
}

extern "C" void
qth_qt_process_events(void)
{
    app()->processEvents();

    /* outside an event loop, windows closed with deleteLater() are otherwise only
     * deleted at exit, after the platform has gone */
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

extern "C" void
qth_screen_dpi(int32_t * p_dpi_x, int32_t * p_dpi_y)
{
    QScreen * p_screen = app()->primaryScreen();

    *p_dpi_x = (int32_t) (p_screen->logicalDotsPerInchX() + 0.5);
    *p_dpi_y = (int32_t) (p_screen->logicalDotsPerInchY() + 0.5);
}

extern "C" void
qth_screen_work_area(int32_t * p_width, int32_t * p_height)
{
    const QRect r = app()->primaryScreen()->availableGeometry();

    *p_width = r.width();
    *p_height = r.height();
}

/* ---- Fonts ---------------------------------------------------------------- */

struct QTH_FONT
{
    QFont font;
};

static bool
is_latin1(const QString & s)
{
    for(const QChar c : s)
        if(c.unicode() > 0xFF)
            return(false);

    return(true);
}

static uint8_t
family_class_of(const QString & family, bool fixed_pitch)
{
    const QString lower = family.toLower();

    if(fixed_pitch || lower.contains(QLatin1String("mono")) || lower.contains(QLatin1String("courier")))
        return(QTH_FAMILY_MODERN);
    if(lower.contains(QLatin1String("script")) || lower.contains(QLatin1String("hand")))
        return(QTH_FAMILY_SCRIPT);
    if(lower.contains(QLatin1String("sans")) || lower.contains(QLatin1String("arial")) || lower.contains(QLatin1String("helvetica")) || lower.contains(QLatin1String("gothic")))
        return(QTH_FAMILY_SWISS);
    if(lower.contains(QLatin1String("serif")) || lower.contains(QLatin1String("times")) || lower.contains(QLatin1String("roman")) || lower.contains(QLatin1String("georgia")))
        return(QTH_FAMILY_ROMAN);

    return(QTH_FAMILY_DONTCARE);
}

extern "C" void
qth_font_enumerate(QTH_FONT_ENUM_PROC proc, void * client)
{
    app();

    const QStringList families = QFontDatabase::families();

    for(const QString & family : families)
    {
        if(!is_latin1(family) || (family.size() >= QTH_FACENAME_LEN))
            continue;

        const QByteArray family_latin1 = family.toLatin1();

        for(const QString & style : QFontDatabase::styles(family))
        {
            QTH_FONT_DESC desc;
            memset(&desc, 0, sizeof(desc));

            memcpy(desc.family, family_latin1.constData(), (size_t) family_latin1.size() + 1);
            desc.weight = QFontDatabase::weight(family, style);
            desc.italic = QFontDatabase::italic(family, style) ? 1 : 0;
            desc.fixed_pitch = QFontDatabase::isFixedPitch(family, style) ? 1 : 0;
            desc.family_class = family_class_of(family, desc.fixed_pitch);
            desc.symbol = QFontDatabase::writingSystems(family).contains(QFontDatabase::Symbol) ? 1 : 0;

            (*proc)(&desc, client);
        }
    }
}

extern "C" QTH_FONT *
qth_font_create(const QTH_FONT_DESC * p_desc, int32_t size_x, int32_t size_y, uint8_t underline)
{
    app();

    if(size_y <= 0)
        return(nullptr);

    QTH_FONT * p_font = new QTH_FONT;

    p_font->font.setFamily(QString::fromLatin1(p_desc->family));
    p_font->font.setWeight(static_cast<QFont::Weight>(p_desc->weight));
    p_font->font.setItalic(0 != p_desc->italic);
    p_font->font.setUnderline(0 != underline);
    p_font->font.setPixelSize(size_y);
    p_font->font.setHintingPreference(QFont::PreferNoHinting); /* widths must not depend on the pixel grid */

    if((size_x > 0) && (size_x != size_y))
        p_font->font.setStretch((int) ((100 * (int64_t) size_x) / size_y));

    return(p_font);
}

extern "C" void
qth_font_destroy(QTH_FONT * p_font)
{
    delete p_font;
}

extern "C" void
qth_font_metrics(const QTH_FONT * p_font, QTH_FONT_METRICS * p_metrics)
{
    const QFontMetricsF metrics(p_font->font);

    p_metrics->ascent = (int32_t) (metrics.ascent() + 0.5);
    p_metrics->descent = (int32_t) (metrics.descent() + 0.5);
    p_metrics->space_width = (int32_t) (metrics.horizontalAdvance(QLatin1Char(' ')) + 0.5);
}

extern "C" int32_t
qth_font_text_width(const QTH_FONT * p_font, const uint8_t * p_latin1, uint32_t n)
{
    const QFontMetricsF metrics(p_font->font);

    return((int32_t) (metrics.horizontalAdvance(QString::fromLatin1(reinterpret_cast<const char *>(p_latin1), (qsizetype) n)) + 0.5));
}

extern "C" int32_t
qth_font_text_width16(const QTH_FONT * p_font, const uint16_t * p_utf16, uint32_t n)
{
    const QFontMetricsF metrics(p_font->font);

    return((int32_t) (metrics.horizontalAdvance(QString::fromUtf16(reinterpret_cast<const char16_t *>(p_utf16), (qsizetype) n)) + 0.5));
}

/* ---- Painting ------------------------------------------------------------- */

struct QTH_PAINTER
{
    QPainter * p_painter;
};

static inline QColor
colour_of(QTH_RGB rgb)
{
    return(QColor(rgb.r, rgb.g, rgb.b));
}

static inline QRect
rect_of(const QTH_RECT * p_rect)
{
    return(QRect(p_rect->left, p_rect->top, p_rect->right - p_rect->left, p_rect->bottom - p_rect->top));
}

extern "C" void
qth_painter_dpi(const QTH_PAINTER * p_painter, int32_t * p_dpi_x, int32_t * p_dpi_y)
{
    *p_dpi_x = p_painter->p_painter->device()->logicalDpiX();
    *p_dpi_y = p_painter->p_painter->device()->logicalDpiY();
}

extern "C" void
qth_paint_fill_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb)
{
    p_painter->p_painter->fillRect(rect_of(p_rect), colour_of(rgb));
}

extern "C" void
qth_paint_frame_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect, QTH_RGB rgb)
{
    QPainter & painter = *p_painter->p_painter;
    const QColor colour = colour_of(rgb);
    const int w = p_rect->right - p_rect->left;
    const int h = p_rect->bottom - p_rect->top;

    if((w <= 0) || (h <= 0))
        return;

    painter.fillRect(QRect(p_rect->left, p_rect->top, w, 1), colour);
    painter.fillRect(QRect(p_rect->left, p_rect->bottom - 1, w, 1), colour);
    painter.fillRect(QRect(p_rect->left, p_rect->top, 1, h), colour);
    painter.fillRect(QRect(p_rect->right - 1, p_rect->top, 1, h), colour);
}

extern "C" void
qth_paint_line(QTH_PAINTER * p_painter, int32_t x0, int32_t y0, int32_t x1, int32_t y1, QTH_RGB rgb, int dashed)
{
    QPainter & painter = *p_painter->p_painter;
    QPen pen(colour_of(rgb), 0, dashed ? Qt::DashLine : Qt::SolidLine); /* width 0: one device pixel */

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(pen);
    painter.drawLine(x0, y0, x1, y1);
    painter.restore();
}

extern "C" void
qth_paint_invert_rect(QTH_PAINTER * p_painter, const QTH_RECT * p_rect)
{
    QPainter & painter = *p_painter->p_painter;

    painter.save();
    painter.setCompositionMode(QPainter::RasterOp_NotDestination);
    painter.fillRect(rect_of(p_rect), Qt::black);
    painter.restore();
}

extern "C" void
qth_paint_set_clip(QTH_PAINTER * p_painter, const QTH_RECT * p_rect)
{
    p_painter->p_painter->setClipRect(rect_of(p_rect), Qt::ReplaceClip);
}

extern "C" void
qth_paint_text(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint8_t * p_latin1, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align)
{
    QPainter & painter = *p_painter->p_painter;
    const QString text = QString::fromLatin1(reinterpret_cast<const char *>(p_latin1), (qsizetype) n);
    const QFontMetricsF metrics(p_font->font);
    const qreal width = metrics.horizontalAdvance(text);

    if(QTH_ALIGN_CENTRE == align)
        x -= (int32_t) (width / 2 + 0.5);
    else if(QTH_ALIGN_RIGHT == align)
        x -= (int32_t) (width + 0.5);

    if(nullptr != p_background)
        painter.fillRect(QRectF(x, y - metrics.ascent(), width, metrics.ascent() + metrics.descent()), colour_of(*p_background));

    painter.save();
    painter.setFont(p_font->font);
    painter.setPen(colour_of(foreground));
    painter.drawText(QPointF(x, y), text);
    painter.restore();
}

extern "C" void
qth_paint_text16(QTH_PAINTER * p_painter, const QTH_FONT * p_font, int32_t x, int32_t y, const uint16_t * p_utf16, uint32_t n, QTH_RGB foreground, const QTH_RGB * p_background, int align)
{
    QPainter & painter = *p_painter->p_painter;
    const QString text = QString::fromUtf16(reinterpret_cast<const char16_t *>(p_utf16), (qsizetype) n);
    const QFontMetricsF metrics(p_font->font);
    const qreal width = metrics.horizontalAdvance(text);

    if(QTH_ALIGN_CENTRE == align)
        x -= (int32_t) (width / 2 + 0.5);
    else if(QTH_ALIGN_RIGHT == align)
        x -= (int32_t) (width + 0.5);

    if(nullptr != p_background)
        painter.fillRect(QRectF(x, y - metrics.ascent(), width, metrics.ascent() + metrics.descent()), colour_of(*p_background));

    painter.save();
    painter.setFont(p_font->font);
    painter.setPen(colour_of(foreground));
    painter.drawText(QPointF(x, y), text);
    painter.restore();
}

/* ---- Images --------------------------------------------------------------- */

struct QTH_IMAGE
{
    QImage image;
};

extern "C" QTH_IMAGE *
qth_image_load(const char * p_path)
{
    QImage image(QString::fromLocal8Bit(p_path));

    if(image.isNull())
        return(nullptr);

    QTH_IMAGE * p_image = new QTH_IMAGE;
    p_image->image = image;

    return(p_image);
}

extern "C" void
qth_image_size(const QTH_IMAGE * p_image, int32_t * p_width, int32_t * p_height)
{
    *p_width = p_image->image.width();
    *p_height = p_image->image.height();
}

extern "C" void
qth_image_destroy(QTH_IMAGE * p_image)
{
    delete p_image;
}

extern "C" void
qth_paint_image(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest)
{
    p_painter->p_painter->drawImage(rect_of(p_dest), p_image->image);
}

extern "C" void
qth_paint_image_keyed(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_dest, QTH_RGB transparent)
{
    QImage image = p_image->image.convertToFormat(QImage::Format_ARGB32);
    const QRgb key = qRgb(transparent.r, transparent.g, transparent.b);

    for(int y = 0; y < image.height(); ++y)
    {
        QRgb * p_line = reinterpret_cast<QRgb *>(image.scanLine(y));

        for(int x = 0; x < image.width(); ++x)
            if(qRgb(qRed(p_line[x]), qGreen(p_line[x]), qBlue(p_line[x])) == key)
                p_line[x] = 0;
    }

    p_painter->p_painter->drawImage(rect_of(p_dest), image);
}

/* ---- Windows -------------------------------------------------------------- */

static int
modifiers_of(Qt::KeyboardModifiers modifiers)
{
    return(((modifiers & Qt::ShiftModifier)   ? QTH_MOD_SHIFT : 0) |
           ((modifiers & Qt::ControlModifier) ? QTH_MOD_CTRL  : 0) |
           ((modifiers & Qt::AltModifier)     ? QTH_MOD_ALT   : 0));
}

static int
buttons_of(Qt::MouseButtons buttons)
{
    return(((buttons & Qt::LeftButton)   ? QTH_BUTTON_LEFT   : 0) |
           ((buttons & Qt::MiddleButton) ? QTH_BUTTON_MIDDLE : 0) |
           ((buttons & Qt::RightButton)  ? QTH_BUTTON_RIGHT  : 0));
}

struct QTH_WINDOW;

class HostWidget : public QWidget
{
public:
    HostWidget(QWidget * p_parent, QTH_WINDOW * p_window, QTH_EVENT_PROC proc, void * client)
        : QWidget(p_parent), p_window(p_window), proc(proc), client(client)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

    QTH_WINDOW * p_window;
    QTH_EVENT_PROC proc;
    void * client;

    bool caret_visible = false;
    QRect caret;

    bool marker_visible[QTH_N_MARKERS] = { false, false, false, false };
    QRect marker[QTH_N_MARKERS];

protected:
    void send(const QTH_EVENT & event)
    {
        if(nullptr != proc)
            (*proc)(p_window, &event, client);
    }

    void paintEvent(QPaintEvent * p_event) override
    {
        QPainter painter(this);
        QTH_PAINTER qth_painter = { &painter };
        QTH_EVENT event;
        const QRect r = p_event->rect();

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_PAINT;
        event.u.paint.p_painter = &qth_painter;
        event.u.paint.exposed.left = r.left();
        event.u.paint.exposed.top = r.top();
        event.u.paint.exposed.right = r.right() + 1;
        event.u.paint.exposed.bottom = r.bottom() + 1;
        painter.setClipRect(r);
        send(event);

        painter.setClipping(false);
        painter.setCompositionMode(QPainter::RasterOp_NotDestination);

        if(caret_visible)
            painter.fillRect(caret, Qt::black);

        for(int i = 0; i < QTH_N_MARKERS; ++i)
            if(marker_visible[i])
                painter.fillRect(marker[i], Qt::black);
    }

    void mouse(int action, QMouseEvent * p_event)
    {
        QTH_EVENT event;

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_MOUSE;
        event.u.mouse.action = action;
        event.u.mouse.x = p_event->position().toPoint().x();
        event.u.mouse.y = p_event->position().toPoint().y();
        event.u.mouse.buttons = buttons_of((QTH_MOUSE_RELEASE == action) ? p_event->button() : p_event->buttons());
        event.u.mouse.modifiers = modifiers_of(p_event->modifiers());
        send(event);
    }

    void mousePressEvent(QMouseEvent * p_event) override { mouse(QTH_MOUSE_PRESS, p_event); }
    void mouseReleaseEvent(QMouseEvent * p_event) override { mouse(QTH_MOUSE_RELEASE, p_event); }
    void mouseMoveEvent(QMouseEvent * p_event) override { mouse(QTH_MOUSE_MOVE, p_event); }
    void mouseDoubleClickEvent(QMouseEvent * p_event) override { mouse(QTH_MOUSE_DOUBLE, p_event); }

    void keyPressEvent(QKeyEvent * p_event) override
    {
        QTH_EVENT event;
        const QString text = p_event->text();

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_KEY;
        event.u.key.code = p_event->key();
        event.u.key.modifiers = modifiers_of(p_event->modifiers());
        if((1 == text.size()) && (text[0].unicode() >= 0x20) && (text[0].unicode() <= 0xFF) && (0x7F != text[0].unicode()))
            event.u.key.latin1 = (uint8_t) text[0].unicode();
        send(event);
    }

    void resizeEvent(QResizeEvent * p_event) override
    {
        QTH_EVENT event;

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_RESIZE;
        event.u.resize.width = p_event->size().width();
        event.u.resize.height = p_event->size().height();
        send(event);
    }

    void focus(int gained)
    {
        QTH_EVENT event;

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_FOCUS;
        event.u.focus.gained = gained;
        send(event);
    }

    void wheelEvent(QWheelEvent * p_event) override
    {
        QTH_EVENT event;

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_WHEEL;
        event.u.wheel.x = p_event->position().toPoint().x();
        event.u.wheel.y = p_event->position().toPoint().y();
        event.u.wheel.delta_x = p_event->angleDelta().x();
        event.u.wheel.delta_y = p_event->angleDelta().y();
        event.u.wheel.modifiers = modifiers_of(p_event->modifiers());
        send(event);
    }

    void leaveEvent(QEvent *) override
    {
        QTH_EVENT event;

        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_LEAVE;
        send(event);
    }

    void focusInEvent(QFocusEvent *) override { focus(1); }
    void focusOutEvent(QFocusEvent *) override { focus(0); }

    void closeEvent(QCloseEvent * p_event) override
    {
        QTH_EVENT event;

        /* the handler decides, and agrees by calling qth_window_destroy() */
        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_CLOSE;
        send(event);
        p_event->ignore();
    }
};

struct QTH_WINDOW
{
    QWidget * p_widget;       /* the HostWidget for panes and documents, the QScrollBar for scroll bars */
    HostWidget * p_host;      /* NULL for scroll bars */
    QScrollBar * p_scroll;    /* NULL unless a scroll bar */
    QTH_EVENT_PROC proc;
    void * client;
    std::map<int, QTimer *> timers;
    int32_t extent_width, extent_height;
    int32_t scroll_x, scroll_y;
};

static QRect
rect_from(const QTH_RECT & r)
{
    return(QRect(r.left, r.top, r.right - r.left, r.bottom - r.top));
}

extern "C" QTH_WINDOW *
qth_window_create(const QTH_WINDOW_DESC * p_desc)
{
    app();

    QTH_WINDOW * p_window = new QTH_WINDOW();
    QWidget * p_parent = (nullptr != p_desc->p_parent) ? p_desc->p_parent->p_widget : nullptr;

    p_window->proc = p_desc->proc;
    p_window->client = p_desc->client;
    p_window->p_host = nullptr;
    p_window->p_scroll = nullptr;

    if((QTH_WINDOW_HSCROLL == p_desc->kind) || (QTH_WINDOW_VSCROLL == p_desc->kind))
    {
        QScrollBar * p_scroll = new QScrollBar((QTH_WINDOW_HSCROLL == p_desc->kind) ? Qt::Horizontal : Qt::Vertical, p_parent);
        const bool vertical = (QTH_WINDOW_VSCROLL == p_desc->kind);

        QObject::connect(p_scroll, &QScrollBar::valueChanged, p_scroll, [p_window, vertical](int value)
        {
            QTH_EVENT event;

            if(nullptr == p_window->proc)
                return;
            memset(&event, 0, sizeof(event));
            event.type = QTH_EVENT_SCROLL;
            event.u.scroll.vertical = vertical;
            event.u.scroll.value = value;
            (*p_window->proc)(p_window, &event, p_window->client);
        });

        p_window->p_scroll = p_scroll;
        p_window->p_widget = p_scroll;
    }
    else
    {
        p_window->p_host = new HostWidget(p_parent, p_window, p_desc->proc, p_desc->client);
        p_window->p_widget = p_window->p_host;

        if(QTH_WINDOW_DOCUMENT == p_desc->kind)
            p_window->p_widget->setWindowFlag(Qt::Window, true);
    }

    p_window->p_widget->setGeometry(rect_from(p_desc->bounds));

    qth_window_set_title(p_window, p_desc->p_title_latin1);

    return(p_window);
}

extern "C" void *
qth_window_get_client(const QTH_WINDOW * p_window)
{
    return(p_window->client);
}

extern "C" void
qth_window_destroy(QTH_WINDOW * p_window)
{
    if(nullptr == p_window)
        return;

    /* we may be inside one of the widget's own events (a close), so delete later */
    p_window->proc = nullptr; /* no more events for the handler */
    if(nullptr != p_window->p_host)
        p_window->p_host->proc = nullptr;
    p_window->p_widget->hide();
    p_window->p_widget->deleteLater();
    delete p_window;
}

extern "C" void
qth_window_show(QTH_WINDOW * p_window, int visible)
{
    p_window->p_widget->setVisible(0 != visible);
}

extern "C" void
qth_window_set_title(QTH_WINDOW * p_window, const char * p_title_latin1)
{
    p_window->p_widget->setWindowTitle(QString::fromLatin1((nullptr != p_title_latin1) ? p_title_latin1 : ""));
}

extern "C" void
qth_window_set_bounds(QTH_WINDOW * p_window, const QTH_RECT * p_bounds)
{
    p_window->p_widget->setGeometry(rect_from(*p_bounds));
}

extern "C" void
qth_window_get_bounds(const QTH_WINDOW * p_window, QTH_RECT * p_bounds)
{
    const QRect g = p_window->p_widget->geometry();

    p_bounds->left = g.left();
    p_bounds->top = g.top();
    p_bounds->right = g.left() + g.width();
    p_bounds->bottom = g.top() + g.height();
}

/* TODO: scroll bars. For now the extent and position are only remembered; the
 * skeleton paints at the scrolled origin itself. */

extern "C" void
qth_window_set_extent(QTH_WINDOW * p_window, int32_t width, int32_t height)
{
    p_window->extent_width = width;
    p_window->extent_height = height;
}

extern "C" void
qth_window_scroll_to(QTH_WINDOW * p_window, int32_t x, int32_t y)
{
    p_window->scroll_x = x;
    p_window->scroll_y = y;
}

extern "C" void
qth_window_invalidate(QTH_WINDOW * p_window, const QTH_RECT * p_rect_or_null)
{
    if(nullptr == p_rect_or_null)
        p_window->p_widget->update();
    else
        p_window->p_widget->update(rect_from(*p_rect_or_null));
}

extern "C" void
qth_window_update_now(QTH_WINDOW * p_window)
{
    p_window->p_widget->repaint();
}

extern "C" void
qth_window_set_caret(QTH_WINDOW * p_window, const QTH_RECT * p_rect, int visible)
{
    HostWidget * p_widget = p_window->p_host;

    if(nullptr == p_widget)
        return;

    p_widget->update(p_widget->caret); /* where it was */
    p_widget->caret = rect_from(*p_rect);
    p_widget->caret_visible = (0 != visible);
    p_widget->update(p_widget->caret);
}

extern "C" void
qth_window_set_scroll_range(QTH_WINDOW * p_window, int32_t min, int32_t max, int32_t page)
{
    if(nullptr != p_window->p_scroll)
    {
        p_window->p_scroll->setRange(min, max);
        p_window->p_scroll->setPageStep(page);
        p_window->p_scroll->setSingleStep((page > 4) ? page / 4 : 1);
    }
}

extern "C" void
qth_window_set_scroll_value(QTH_WINDOW * p_window, int32_t value)
{
    if(nullptr != p_window->p_scroll)
    {
        const QSignalBlocker blocker(p_window->p_scroll); /* setting from here does not send an event */
        p_window->p_scroll->setValue(value);
    }
}

extern "C" int32_t
qth_window_get_scroll_value(const QTH_WINDOW * p_window)
{
    return((nullptr != p_window->p_scroll) ? p_window->p_scroll->value() : 0);
}

extern "C" void
qth_window_mouse_position(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y)
{
    const QPoint p = p_window->p_widget->mapFromGlobal(QCursor::pos());

    *p_x = p.x();
    *p_y = p.y();
}

extern "C" void
qth_window_to_screen(const QTH_WINDOW * p_window, int32_t * p_x, int32_t * p_y)
{
    const QPoint p = p_window->p_widget->mapToGlobal(QPoint(*p_x, *p_y));

    *p_x = p.x();
    *p_y = p.y();
}

extern "C" void
qth_window_screen_rect(const QTH_WINDOW * p_window, QTH_RECT * p_rect)
{
    const QPoint tl = p_window->p_widget->mapToGlobal(QPoint(0, 0));

    p_rect->left = tl.x();
    p_rect->top = tl.y();
    p_rect->right = tl.x() + p_window->p_widget->width();
    p_rect->bottom = tl.y() + p_window->p_widget->height();
}

extern "C" void
qth_window_scroll_contents(QTH_WINDOW * p_window, int32_t dx, int32_t dy)
{
    p_window->p_widget->scroll(dx, dy);
}

extern "C" void
qth_window_set_timer(QTH_WINDOW * p_window, int id, int32_t ms)
{
    qth_window_kill_timer(p_window, id);

    QTimer * p_timer = new QTimer(p_window->p_widget);

    QObject::connect(p_timer, &QTimer::timeout, p_timer, [p_window, id]()
    {
        QTH_EVENT event;

        if(nullptr == p_window->proc)
            return;
        memset(&event, 0, sizeof(event));
        event.type = QTH_EVENT_TIMER;
        event.u.timer.id = id;
        (*p_window->proc)(p_window, &event, p_window->client);
    });

    p_timer->start(ms);
    p_window->timers[id] = p_timer;
}

extern "C" void
qth_window_kill_timer(QTH_WINDOW * p_window, int id)
{
    auto it = p_window->timers.find(id);

    if(it != p_window->timers.end())
    {
        delete it->second;
        p_window->timers.erase(it);
    }
}

extern "C" int32_t
qth_metric(int which)
{
    app();

    switch(which)
    {
    case QTH_METRIC_SCROLLBAR_WIDTH:
    case QTH_METRIC_SCROLLBAR_HEIGHT:   return(app()->style()->pixelMetric(QStyle::PM_ScrollBarExtent));
    case QTH_METRIC_DRAG_X:
    case QTH_METRIC_DRAG_Y:             return(QApplication::startDragDistance());
    case QTH_METRIC_DOUBLECLICK_X:
    case QTH_METRIC_DOUBLECLICK_Y:      return(4);
    case QTH_METRIC_DOUBLECLICK_MS:     return(QApplication::doubleClickInterval());
    default:                            return(0);
    }
}

extern "C" void
qth_window_set_marker(QTH_WINDOW * p_window, int slot, const QTH_RECT * p_rect, int visible)
{
    HostWidget * p_widget = p_window->p_host;

    if((nullptr == p_widget) || (slot < 0) || (slot >= QTH_N_MARKERS))
        return;

    p_widget->update(p_widget->marker[slot]); /* where it was */
    p_widget->marker[slot] = rect_from(*p_rect);
    p_widget->marker_visible[slot] = (0 != visible);
    p_widget->update(p_widget->marker[slot]);
}

extern "C" void
qth_window_set_state(QTH_WINDOW * p_window, int state)
{
    switch(state)
    {
    case QTH_STATE_MINIMIZED: p_window->p_widget->showMinimized(); break;
    case QTH_STATE_MAXIMIZED: p_window->p_widget->showMaximized(); break;
    default:                  p_window->p_widget->showNormal(); break;
    }
}

extern "C" void
qth_window_set_focus(QTH_WINDOW * p_window)
{
    p_window->p_widget->setFocus();
}

extern "C" void
qth_window_capture_mouse(QTH_WINDOW * p_window, int capture)
{
    if(capture)
        p_window->p_widget->grabMouse();
    else
        p_window->p_widget->releaseMouse();
}

extern "C" void
qth_window_set_cursor(QTH_WINDOW * p_window, int cursor)
{
    Qt::CursorShape shape = Qt::ArrowCursor;

    switch(cursor)
    {
    case QTH_CURSOR_IBEAM:    shape = Qt::IBeamCursor; break;
    case QTH_CURSOR_CROSS:    shape = Qt::CrossCursor; break;
    case QTH_CURSOR_HAND:     shape = Qt::PointingHandCursor; break;
    case QTH_CURSOR_WAIT:     shape = Qt::WaitCursor; break;
    case QTH_CURSOR_SIZE_H:   shape = Qt::SizeHorCursor; break;
    case QTH_CURSOR_SIZE_V:   shape = Qt::SizeVerCursor; break;
    case QTH_CURSOR_SIZE_ALL: shape = Qt::SizeAllCursor; break;
    default: break;
    }

    p_window->p_widget->setCursor(QCursor(shape));
}

extern "C" int
qth_qt_window_pixel(QTH_WINDOW * p_window, int32_t x, int32_t y, uint32_t * p_argb)
{
    const QImage image = p_window->p_widget->grab().toImage();

    if((x < 0) || (y < 0) || (x >= image.width()) || (y >= image.height()))
        return(0);

    *p_argb = image.pixel(x, y);

    return(1);
}

/* end of qth_qt.cpp */
