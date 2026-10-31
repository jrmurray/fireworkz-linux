/* qth_qt.cpp */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* The Qt 6 implementation of qt_host.h */

#include "ob_skel/linux/qt_host.h"
#include "ob_skel/linux/qt/qth_qt.h"

#include <QAbstractEventDispatcher>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFontMetrics>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QAction>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
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

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

static int modifiers_of(Qt::KeyboardModifiers modifiers);

static int g_screenshot_taken;

/* ---- Application ---------------------------------------------------------- */

static QApplication *
app(void)
{
    static int argc = 1;
    static char arg0[] = "fireworkz";
    static char * argv[] = { arg0, nullptr };
    static QApplication * p_app = nullptr;

    if(nullptr == p_app)
    {
        p_app = new QApplication(argc, argv);

        /* For testing and bug reports: FIREWORKZ_SCREENSHOT=base saves each top level window as
         * base-N.png after FIREWORKZ_SCREENSHOT_MS (default 1500), then quits */
        const QByteArray base = qgetenv("FIREWORKZ_SCREENSHOT");

        if(!base.isEmpty())
        {
            const int delay_ms = qEnvironmentVariableIntValue("FIREWORKZ_SCREENSHOT_MS") > 0 ? qEnvironmentVariableIntValue("FIREWORKZ_SCREENSHOT_MS") : 1500;

            /* FIREWORKZ_TEST_MENU="Page>Page layout..." chooses that menu item shortly before the screenshot, so
             * that a dialog can be seen without anyone at the keyboard */
            const QByteArray test_menu = qgetenv("FIREWORKZ_TEST_MENU");

            if(!test_menu.isEmpty())
                QTimer::singleShot(delay_ms / 3, p_app, [test_menu]()
                {
                    const QStringList path = QString::fromLatin1(test_menu).split(QLatin1Char('>'));

                    for(QWidget * p_widget : QApplication::topLevelWidgets())
                    {
                        QMainWindow * p_main = qobject_cast<QMainWindow *>(p_widget);
                        QMenuBar * p_bar = (nullptr != p_main) ? p_main->menuBar() : nullptr;
                        if(nullptr == p_bar)
                            continue;
                        QList<QAction *> actions = p_bar->actions();
                        QAction * p_found = nullptr;

                        for(const QString & name : path)
                        {
                            p_found = nullptr;
                            for(QAction * p_action : actions)
                            {
                                QString text = p_action->text();
                                text.remove(QLatin1Char('&'));
                                text = text.section(QLatin1Char('\t'), 0, 0);
                                if(text == name)
                                {
                                    p_found = p_action;
                                    break;
                                }
                            }
                            if(nullptr == p_found)
                            {
                                QStringList have;
                                for(QAction * p_action : actions)
                                    have << p_action->text();
                                fprintf(stderr, "FIREWORKZ_TEST_MENU: no \"%s\" among: %s\n", qPrintable(name), qPrintable(have.join(QLatin1String(" | "))));
                                break;
                            }
                            if(nullptr != p_found->menu())
                            {
                                emit p_found->menu()->aboutToShow(); /* so that the items are brought up to date */
                                actions = p_found->menu()->actions();
                            }
                        }

                        if((nullptr != p_found) && (nullptr == p_found->menu()))
                        {
                            p_found->trigger();
                            return;
                        }
                    }
                });

            QTimer::singleShot(delay_ms, p_app, [base]()
            {
                int n = 0;

                for(QWidget * p_widget : QApplication::topLevelWidgets())
                    if(p_widget->isVisible())
                        p_widget->grab().save(QString::fromLocal8Bit(base) + QStringLiteral("-") + QString::number(n++) + QStringLiteral(".png"));

                g_screenshot_taken = 1;
                QApplication::exit(0);
            });
        }
    }

    return(p_app);
}

extern "C" int
qth_app_run(void)
{
    return(app()->exec());
}

static int g_quit_requested;

extern "C" int
qth_process_events(int wait_ms)
{
    QAbstractEventDispatcher * p_dispatcher = QAbstractEventDispatcher::instance();

    app();

    if(nullptr == p_dispatcher)
        return(0);

    if(0 == wait_ms)
    {
        const int processed = p_dispatcher->processEvents(QEventLoop::AllEvents) ? 1 : 0;

        /* windows closed with deleteLater() are only deleted by an event loop that is running */
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        return(processed);
    }

    if(wait_ms > 0)
        QTimer::singleShot(wait_ms, [](){}); /* something to wake us if no input comes */

    return(p_dispatcher->processEvents(QEventLoop::WaitForMoreEvents) ? 1 : 0);
}

extern "C" int
qth_quit_requested(void)
{
    return(g_quit_requested || g_screenshot_taken);
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
    g_quit_requested = 1;
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
qth_paint_image_part(QTH_PAINTER * p_painter, const QTH_IMAGE * p_image, const QTH_RECT * p_src, const QTH_RECT * p_dest, const QTH_RGB * p_transparent, int opacity)
{
    QPainter & painter = *p_painter->p_painter;
    QImage image = p_image->image.convertToFormat(QImage::Format_ARGB32);

    if(nullptr != p_transparent)
    {
        const QRgb key = qRgb(p_transparent->r, p_transparent->g, p_transparent->b);

        for(int y = 0; y < image.height(); ++y)
        {
            QRgb * p_line = reinterpret_cast<QRgb *>(image.scanLine(y));

            for(int x = 0; x < image.width(); ++x)
                if(qRgb(qRed(p_line[x]), qGreen(p_line[x]), qBlue(p_line[x])) == key)
                    p_line[x] = 0;
        }
    }

    painter.save();
    painter.setOpacity(opacity / 255.0);
    painter.drawImage(rect_of(p_dest), image, rect_of(p_src));
    painter.restore();
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

struct QTH_MENU;

struct QTH_WINDOW
{
    QMainWindow * p_main;     /* the frame of a document window: it has the menu bar, and p_widget is its client */
    QTH_MENU * p_menu_bar;
    int requested_client_height;
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

/* what is shown, titled, minimised and so on: the frame for a document, else the widget */

static inline QWidget *
top_of(const QTH_WINDOW * p_window)
{
    return((nullptr != p_window->p_main) ? static_cast<QWidget *>(p_window->p_main) : p_window->p_widget);
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
    p_window->p_main = nullptr;
    p_window->p_menu_bar = nullptr;

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
        if(QTH_WINDOW_DOCUMENT == p_desc->kind)
        {
            p_window->p_main = new QMainWindow();
            p_window->p_host = new HostWidget(nullptr, p_window, p_desc->proc, p_desc->client);
            p_window->p_main->setCentralWidget(p_window->p_host); /* the frame owns it now */
        }
        else
            p_window->p_host = new HostWidget(p_parent, p_window, p_desc->proc, p_desc->client);

        p_window->p_widget = p_window->p_host;
    }

    p_window->requested_client_height = p_desc->bounds.bottom - p_desc->bounds.top;

    if(nullptr != p_window->p_main)
        p_window->p_main->setGeometry(rect_from(p_desc->bounds)); /* until there is a menu bar, the client is all of it */
    else
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
    top_of(p_window)->hide();
    top_of(p_window)->deleteLater();
    delete p_window;
}

extern "C" void
qth_window_show(QTH_WINDOW * p_window, int visible)
{
    top_of(p_window)->setVisible(0 != visible);
}

extern "C" void
qth_window_set_title(QTH_WINDOW * p_window, const char * p_title_latin1)
{
    top_of(p_window)->setWindowTitle(QString::fromLatin1((nullptr != p_title_latin1) ? p_title_latin1 : ""));
}

extern "C" void
qth_window_set_bounds(QTH_WINDOW * p_window, const QTH_RECT * p_bounds)
{
    if(nullptr != p_window->p_main)
    {
        /* the bounds are of the client: the frame is as much bigger as the menu bar and so on make it */
        const int extra_h = p_window->p_main->height() - p_window->p_widget->height();
        const int extra_w = p_window->p_main->width() - p_window->p_widget->width();

        p_window->p_main->setGeometry(p_bounds->left, p_bounds->top, (p_bounds->right - p_bounds->left) + extra_w, (p_bounds->bottom - p_bounds->top) + extra_h);
    }
    else
        p_window->p_widget->setGeometry(rect_from(*p_bounds));
}

extern "C" void
qth_window_get_bounds(const QTH_WINDOW * p_window, QTH_RECT * p_bounds)
{
    /* for a document: where the frame is, with the size of the client */
    QRect g = p_window->p_widget->geometry();

    if(nullptr != p_window->p_main)
        g = QRect(p_window->p_main->pos(), p_window->p_widget->size());

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
    case QTH_STATE_MINIMIZED: top_of(p_window)->showMinimized(); break;
    case QTH_STATE_MAXIMIZED: top_of(p_window)->showMaximized(); break;
    default:                  top_of(p_window)->showNormal(); break;
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

/* ---- Menus ---------------------------------------------------------------------------- */

struct QTH_MENU
{
    QTH_WINDOW * p_window;
    QTH_MENU_PROC proc;
    QMenu * p_menu;                    /* null for the menu bar */
    QMenuBar * p_bar;                  /* null for a menu */
    std::vector<QAction *> actions;
    std::vector<QTH_MENU *> subs;      /* parallel to actions: the submenu of an item, or null */
};

static QString
text_of(const char * p_latin1)
{
    return(QString::fromLatin1((nullptr != p_latin1) ? p_latin1 : ""));
}

static QTH_MENU *
menu_new(QTH_WINDOW * p_window, QTH_MENU_PROC proc, QMenu * p_menu, QMenuBar * p_bar, void * client_about_to_show)
{
    QTH_MENU * p_qth_menu = new QTH_MENU();

    p_qth_menu->p_window = p_window;
    p_qth_menu->proc = proc;
    p_qth_menu->p_menu = p_menu;
    p_qth_menu->p_bar = p_bar;

    if(nullptr != p_menu)
        QObject::connect(p_menu, &QMenu::aboutToShow, p_menu, [p_qth_menu, client_about_to_show]()
        {
            if(nullptr != p_qth_menu->proc)
                (*p_qth_menu->proc)(p_qth_menu->p_window, QTH_MENU_ABOUT_TO_SHOW, client_about_to_show, p_qth_menu);
        });

    return(p_qth_menu);
}

extern "C" QTH_MENU *
qth_window_create_menu_bar(QTH_WINDOW * p_window, QTH_MENU_PROC proc)
{
    if(nullptr == p_window->p_main)
        return(nullptr);

    /* the client keeps its size: the frame grows by the bar's height */
    const int client_height = p_window->p_main->isVisible() ? p_window->p_widget->height() : p_window->requested_client_height;

    QMenuBar * p_bar = new QMenuBar(p_window->p_main);
    p_window->p_main->setMenuBar(p_bar); /* the frame owns it and any old one */

    p_window->p_main->resize(p_window->p_main->width(), client_height + p_bar->sizeHint().height());

    p_window->p_menu_bar = menu_new(p_window, proc, nullptr, p_bar, nullptr);

    return(p_window->p_menu_bar);
}

extern "C" QTH_MENU *
qth_menu_create_popup(QTH_WINDOW * p_window, QTH_MENU_PROC proc, void * client)
{
    return(menu_new(p_window, proc, new QMenu(p_window->p_widget), nullptr, client));
}

extern "C" void
qth_menu_add_item(QTH_MENU * p_menu, const char * p_text, const char * p_shortcut_text, void * client)
{
    QString text = text_of(p_text);
    QAction * p_action;

    if((nullptr != p_shortcut_text) && ('\0' != p_shortcut_text[0]))
        text += QLatin1Char('\t') + text_of(p_shortcut_text); /* shown in the shortcut column */

    p_action = (nullptr != p_menu->p_menu) ? p_menu->p_menu->addAction(text) : p_menu->p_bar->addAction(text);

    QObject::connect(p_action, &QAction::triggered, p_action, [p_menu, client]()
    {
        if(nullptr != p_menu->proc)
            (*p_menu->proc)(p_menu->p_window, QTH_MENU_ACTIVATE, client, p_menu);
    });

    p_menu->actions.push_back(p_action);
    p_menu->subs.push_back(nullptr);
}

extern "C" void
qth_menu_add_separator(QTH_MENU * p_menu)
{
    QAction * p_action = (nullptr != p_menu->p_menu) ? p_menu->p_menu->addSeparator() : p_menu->p_bar->addSeparator();

    p_menu->actions.push_back(p_action);
    p_menu->subs.push_back(nullptr);
}

extern "C" QTH_MENU *
qth_menu_add_submenu(QTH_MENU * p_menu, const char * p_text, void * client)
{
    QMenu * p_sub = (nullptr != p_menu->p_menu) ? p_menu->p_menu->addMenu(text_of(p_text)) : p_menu->p_bar->addMenu(text_of(p_text));
    QTH_MENU * p_qth_sub = menu_new(p_menu->p_window, p_menu->proc, p_sub, nullptr, client);

    p_menu->actions.push_back(p_sub->menuAction());
    p_menu->subs.push_back(p_qth_sub);

    return(p_qth_sub);
}

extern "C" void
qth_menu_set_item_enabled(QTH_MENU * p_menu, int index, int enabled)
{
    if((index >= 0) && ((size_t) index < p_menu->actions.size()))
        p_menu->actions[(size_t) index]->setEnabled(0 != enabled);
}

extern "C" QTH_MENU *
qth_menu_get_submenu(QTH_MENU * p_menu, int index)
{
    return(((index >= 0) && ((size_t) index < p_menu->subs.size())) ? p_menu->subs[(size_t) index] : nullptr);
}

extern "C" void
qth_menu_popup(QTH_MENU * p_menu)
{
    if(nullptr != p_menu->p_menu)
        p_menu->p_menu->exec(QCursor::pos());
}


/* ---- Dialog boxes ------------------------------------------------------------------ */

enum DLG_PLACE { DLG_PLACE_DEFAULT, DLG_PLACE_XY, DLG_PLACE_CENTRE_SCREEN, DLG_PLACE_CENTRE_PARENT, DLG_PLACE_POINTER };

struct DLG_CTL
{
    int kind = 0;
    QWidget * p_widget = nullptr;
    QAbstractButton * p_button = nullptr;
    QLineEdit * p_line = nullptr;
    QPlainTextEdit * p_multi = nullptr;
    QListWidget * p_list = nullptr;
    QComboBox * p_combo = nullptr;
    QLabel * p_label = nullptr;
    QGroupBox * p_group = nullptr;
};

/* Draws a list item's text in columns, split at tabs, the second starting at a fixed distance */
class TabDelegate : public QStyledItemDelegate
{
public:
    int tab = 0;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter * p_painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override
    {
        const QString text = index.data(Qt::DisplayRole).toString();
        const int split = text.indexOf(QLatin1Char('\t'));

        if((tab <= 0) || (split < 0))
        {
            QStyledItemDelegate::paint(p_painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, p_painter, opt.widget);

        p_painter->save();
        p_painter->setPen(opt.palette.color((opt.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
        QRect r = opt.rect.adjusted(3, 0, 0, 0);
        p_painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, text.left(split));
        r.setLeft(opt.rect.left() + tab);
        p_painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, text.mid(split + 1));
        p_painter->restore();
    }
};

struct QTH_DIALOG : public QDialog
{
    QTH_DIALOG(QWidget * p_parent) : QDialog(p_parent) {}

    QTH_DLG_PROC proc = nullptr;
    void * client = nullptr;
    std::map<int, DLG_CTL> ctls;
    std::map<int, TabDelegate *> delegates;
    DLG_PLACE place = DLG_PLACE_DEFAULT;
    int place_x = 0, place_y = 0;
    bool parented = false;
    bool updating = false;

    void send(int event, int id, int arg)
    {
        if(nullptr != proc)
            (*proc)(this, event, id, arg, client);
    }

    int id_of_widget(QObject * p_object)
    {
        for(auto & it : ctls)
        {
            const DLG_CTL & c = it.second;
            if((c.p_widget == p_object) || (c.p_line == p_object) || (c.p_multi == p_object) || ((nullptr != c.p_combo) && (c.p_combo->lineEdit() == p_object)))
                return(it.first);
        }
        return(0);
    }

    /* the engine decides when a dialog is over, so Enter and Escape and the close button just ask it */
    void accept() override {}
    void reject() override { send(QTH_DLG_EV_CLOSE, 0, 0); }

protected:
    void closeEvent(QCloseEvent * p_event) override
    {
        p_event->ignore();
        send(QTH_DLG_EV_CLOSE, 0, 0);
    }

    void keyPressEvent(QKeyEvent * p_event) override
    {
        if(p_event->key() == Qt::Key_F1)
        {
            send(QTH_DLG_EV_HELP, 0, 0);
            return;
        }
        QDialog::keyPressEvent(p_event);
    }

    bool eventFilter(QObject * p_object, QEvent * p_event) override
    {
        if(p_event->type() == QEvent::FocusIn)
        {
            const int id = id_of_widget(p_object);
            if(0 != id)
                send(QTH_DLG_EV_FOCUS, id, 0);
        }
        return(QDialog::eventFilter(p_object, p_event));
    }
};

/* Static text and group captions carry & for the hot key of the control that follows them, which is
 * not shown here; && is a real ampersand */
static QString
text_without_hot_key(const QString & text)
{
    QString out;

    for(int i = 0; i < text.length(); ++i)
    {
        if(text[i] == QLatin1Char('&'))
        {
            if((i + 1 < text.length()) && (text[i + 1] == QLatin1Char('&')))
                out += text[i++];
            continue;
        }
        out += text[i];
    }

    return(out);
}

static DLG_CTL *
dlg_ctl(QTH_DIALOG * p_dlg, int id)
{
    auto it = p_dlg->ctls.find(id);
    return((it != p_dlg->ctls.end()) ? &it->second : nullptr);
}

extern "C" void
qth_dlg_metrics(int32_t * p_four_h_du_px, int32_t * p_eight_v_du_px)
{
    static const QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    const QFontMetrics fm(QApplication::font());

    /* as the Windows dialog units are defined: the average of the alphabet, rounded */
    *p_four_h_du_px = (int32_t) (((fm.horizontalAdvance(alphabet) / 26) + 1) / 2);
    *p_eight_v_du_px = (int32_t) fm.height();
}

extern "C" int32_t
qth_dlg_text_width(const uint8_t * p_text, uint32_t n)
{
    const QFontMetrics fm(QApplication::font());

    return((int32_t) fm.horizontalAdvance(QString::fromLatin1((const char *) p_text, (int) n)));
}

extern "C" QTH_DIALOG *
qth_dlg_create(QTH_WINDOW * p_parent, const char * p_caption, int width, int height, QTH_DLG_PROC proc, void * client)
{
    QWidget * p_parent_widget = (nullptr != p_parent) ? ((nullptr != p_parent->p_main) ? static_cast<QWidget *>(p_parent->p_main) : p_parent->p_widget) : nullptr;
    QTH_DIALOG * p_dlg = new QTH_DIALOG(p_parent_widget);

    p_dlg->proc = proc;
    p_dlg->client = client;
    p_dlg->parented = (nullptr != p_parent_widget);
    p_dlg->setWindowTitle(text_of(p_caption));
    p_dlg->setSizeGripEnabled(false);
    p_dlg->setFixedSize(width, height);
    p_dlg->setAttribute(Qt::WA_DeleteOnClose, false);

    return(p_dlg);
}

extern "C" void
qth_dlg_destroy(QTH_DIALOG * p_dlg)
{
    if(nullptr == p_dlg)
        return;

    p_dlg->proc = nullptr; /* nothing more to the engine from here on */
    p_dlg->hide();
    p_dlg->deleteLater();
}

extern "C" void
qth_dlg_add(QTH_DIALOG * p_dlg, int kind, int id, int x, int y, int width, int height, int flags, int dropdown_height, const char * p_text)
{
    DLG_CTL c;
    QWidget * p_widget = nullptr;
    const QString text = text_of(p_text);
    const QRect r(x, y, width, height);

    (void) dropdown_height;

    c.kind = kind;

    switch(kind)
    {
    case QTH_DLG_LABEL:
    case QTH_DLG_FRAMED_TEXT:
        {
        QLabel * p_label = new QLabel(text_without_hot_key(text), p_dlg);
        Qt::Alignment align = Qt::AlignVCenter;
        align |= (flags & QTH_DLGF_CENTRE) ? Qt::AlignHCenter : (flags & QTH_DLGF_LEFT) ? Qt::AlignLeft : Qt::AlignRight;
        p_label->setAlignment(align);
        p_label->setTextFormat(Qt::PlainText);
        if(kind == QTH_DLG_FRAMED_TEXT)
        {
            p_label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
            p_label->setMargin(2);
        }
        c.p_label = p_label;
        p_widget = p_label;
        break;
        }

    case QTH_DLG_GROUP:
        c.p_group = new QGroupBox(text_without_hot_key(text), p_dlg);
        p_widget = c.p_group;
        break;

    case QTH_DLG_PUSH:
        {
        QPushButton * p_push = new QPushButton(text, p_dlg);
        p_push->setAutoDefault(false);
        p_push->setDefault(0 != (flags & QTH_DLGF_DEFAULT));
        c.p_button = p_push;
        p_widget = p_push;
        break;
        }

    case QTH_DLG_RADIO:
        {
        QRadioButton * p_radio = new QRadioButton(text, p_dlg);
        p_radio->setAutoExclusive(false); /* the engine says which one is on */
        c.p_button = p_radio;
        p_widget = p_radio;
        break;
        }

    case QTH_DLG_CHECK:
    case QTH_DLG_TRISTATE:
        {
        QCheckBox * p_check = new QCheckBox(text, p_dlg);
        p_check->setTristate(kind == QTH_DLG_TRISTATE);
        c.p_button = p_check;
        p_widget = p_check;
        break;
        }

    case QTH_DLG_EDIT:
    case QTH_DLG_SPIN:
        if((kind == QTH_DLG_EDIT) && (flags & QTH_DLGF_MULTILINE))
        {
            c.p_multi = new QPlainTextEdit(p_dlg);
            c.p_multi->setReadOnly(0 != (flags & QTH_DLGF_READONLY));
            c.p_multi->setTabChangesFocus(true);
            p_widget = c.p_multi;
        }
        else
        {
            QWidget * p_holder = (kind == QTH_DLG_SPIN) ? new QWidget(p_dlg) : nullptr;
            QLineEdit * p_line = new QLineEdit((nullptr != p_holder) ? p_holder : static_cast<QWidget *>(p_dlg));
            p_line->setReadOnly(0 != (flags & QTH_DLGF_READONLY));
            p_line->setAlignment((flags & QTH_DLGF_RIGHT) ? Qt::AlignRight : Qt::AlignLeft);
            c.p_line = p_line;
            p_widget = p_line;

            if(nullptr != p_holder)
            {   /* the arrows are two small buttons stacked at the right of the edit */
                const int arrow_w = qMax(14, height * 3 / 4);
                const int half = height / 2;
                QToolButton * p_up = new QToolButton(p_holder);
                QToolButton * p_down = new QToolButton(p_holder);
                const int id_for_arrows = id;

                p_up->setArrowType(Qt::UpArrow);
                p_down->setArrowType(Qt::DownArrow);
                p_up->setAutoRepeat(true);
                p_down->setAutoRepeat(true);
                p_up->setFocusPolicy(Qt::NoFocus);
                p_down->setFocusPolicy(Qt::NoFocus);
                p_line->setGeometry(0, 0, width - arrow_w, height);
                p_up->setGeometry(width - arrow_w, 0, arrow_w, half);
                p_down->setGeometry(width - arrow_w, half, arrow_w, height - half);

                QObject::connect(p_up,   &QToolButton::clicked, p_dlg, [p_dlg, id_for_arrows]() { p_dlg->send(QTH_DLG_EV_SPIN, id_for_arrows,  1); });
                QObject::connect(p_down, &QToolButton::clicked, p_dlg, [p_dlg, id_for_arrows]() { p_dlg->send(QTH_DLG_EV_SPIN, id_for_arrows, -1); });

                p_widget = p_holder;
            }
        }
        break;

    case QTH_DLG_LIST:
        {
        QListWidget * p_list = new QListWidget(p_dlg);
        TabDelegate * p_delegate = new TabDelegate(p_list);
        p_list->setItemDelegate(p_delegate);
        p_dlg->delegates[id] = p_delegate;
        c.p_list = p_list;
        p_widget = p_list;
        break;
        }

    case QTH_DLG_COMBO:
        c.p_combo = new QComboBox(p_dlg);
        c.p_combo->setEditable(0 == (flags & QTH_DLGF_READONLY));
        c.p_combo->setInsertPolicy(QComboBox::NoInsert);
        p_widget = c.p_combo;
        break;

    default:
    case QTH_DLG_BLANK:
        p_widget = new QWidget(p_dlg);
        break;
    }

    p_widget->setGeometry(r);
    p_widget->show();
    c.p_widget = p_widget;

    /* events from the controls */
    if(nullptr != c.p_button)
        QObject::connect(c.p_button, &QAbstractButton::clicked, p_dlg, [p_dlg, id]() { p_dlg->send(QTH_DLG_EV_CLICK, id, 0); });

    if(nullptr != c.p_line)
        QObject::connect(c.p_line, &QLineEdit::textEdited, p_dlg, [p_dlg, id]() { p_dlg->send(QTH_DLG_EV_TEXT, id, 0); });

    if(nullptr != c.p_multi)
        QObject::connect(c.p_multi, &QPlainTextEdit::textChanged, p_dlg, [p_dlg, id]()
        {
            if(!p_dlg->updating)
                p_dlg->send(QTH_DLG_EV_TEXT, id, 0);
        });

    if(nullptr != c.p_list)
    {
        QObject::connect(c.p_list, &QListWidget::itemClicked, p_dlg, [p_dlg, id, p_list = c.p_list](QListWidgetItem *)
        {
            p_dlg->send(QTH_DLG_EV_SELECT, id, p_list->currentRow());
        });
        QObject::connect(c.p_list, &QListWidget::currentRowChanged, p_dlg, [p_dlg, id](int row)
        {
            if(!p_dlg->updating)
                p_dlg->send(QTH_DLG_EV_SELECT, id, row);
        });
        QObject::connect(c.p_list, &QListWidget::itemDoubleClicked, p_dlg, [p_dlg, id](QListWidgetItem *) { p_dlg->send(QTH_DLG_EV_DBLCLK, id, 0); });
    }

    if(nullptr != c.p_combo)
    {
        QObject::connect(c.p_combo, QOverload<int>::of(&QComboBox::activated), p_dlg, [p_dlg, id](int index) { p_dlg->send(QTH_DLG_EV_SELECT, id, index); });
        if(nullptr != c.p_combo->lineEdit())
        {
            QObject::connect(c.p_combo->lineEdit(), &QLineEdit::textEdited, p_dlg, [p_dlg, id]() { p_dlg->send(QTH_DLG_EV_TEXT, id, 0); });
            c.p_combo->lineEdit()->installEventFilter(p_dlg);
        }
    }

    /* focus: tell the engine which one has it */
    p_widget->installEventFilter(p_dlg);
    if(nullptr != c.p_line)
        c.p_line->installEventFilter(p_dlg);

    p_dlg->ctls[id] = c;
}

extern "C" void
qth_dlg_place(QTH_DIALOG * p_dlg, int x, int y)
{
    p_dlg->place = DLG_PLACE_XY;
    p_dlg->place_x = x;
    p_dlg->place_y = y;
}

extern "C" void
qth_dlg_centre(QTH_DIALOG * p_dlg, int on_parent)
{
    p_dlg->place = (on_parent && p_dlg->parented) ? DLG_PLACE_CENTRE_PARENT : DLG_PLACE_CENTRE_SCREEN;
}

extern "C" void
qth_dlg_at_pointer(QTH_DIALOG * p_dlg)
{
    p_dlg->place = DLG_PLACE_POINTER;
}

static void
dlg_position(QTH_DIALOG * p_dlg)
{
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    QPoint tl;

    switch(p_dlg->place)
    {
    case DLG_PLACE_XY:
        tl = QPoint(p_dlg->place_x, p_dlg->place_y);
        break;

    case DLG_PLACE_CENTRE_PARENT:
        tl = p_dlg->parentWidget()->window()->frameGeometry().center() - QPoint(p_dlg->width() / 2, p_dlg->height() / 2);
        break;

    case DLG_PLACE_POINTER:
        tl = QCursor::pos() - QPoint(p_dlg->width() / 2, 16);
        break;

    default:
        tl = screen.center() - QPoint(p_dlg->width() / 2, p_dlg->height() / 2);
        break;
    }

    tl.setX(qMax(screen.left(), qMin(tl.x(), screen.right()  - p_dlg->width())));
    tl.setY(qMax(screen.top(),  qMin(tl.y(), screen.bottom() - p_dlg->height())));
    p_dlg->move(tl);
}

extern "C" void
qth_dlg_exec(QTH_DIALOG * p_dlg)
{
    dlg_position(p_dlg);
    p_dlg->setModal(true);
    p_dlg->QDialog::open();
    p_dlg->raise();
    p_dlg->activateWindow();

    QEventLoop loop;
    QObject::connect(p_dlg, &QDialog::finished, &loop, &QEventLoop::quit);
    QObject::connect(p_dlg, &QObject::destroyed, &loop, &QEventLoop::quit);
    loop.exec();
}

extern "C" void
qth_dlg_show(QTH_DIALOG * p_dlg)
{
    dlg_position(p_dlg);
    p_dlg->setModal(false);
    p_dlg->show();
    p_dlg->raise();
    p_dlg->activateWindow();
}

extern "C" void
qth_dlg_end(QTH_DIALOG * p_dlg)
{
    p_dlg->QDialog::done(1);
}

extern "C" void
qth_dlg_set_text(QTH_DIALOG * p_dlg, int id, const char * p_text)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);
    const QString text = text_of(p_text);

    if(nullptr == c)
        return;

    if(nullptr != c->p_label)
        c->p_label->setText(text_without_hot_key(text));
    else if(nullptr != c->p_group)
        c->p_group->setTitle(text_without_hot_key(text));
    else if(nullptr != c->p_button)
        c->p_button->setText(text);
    else if(nullptr != c->p_line)
    {
        if(c->p_line->text() != text)
        {
            const int cursor = c->p_line->cursorPosition();
            c->p_line->setText(text);
            c->p_line->setCursorPosition(qMin(cursor, (int) text.length()));
        }
    }
    else if(nullptr != c->p_multi)
    {
        if(c->p_multi->toPlainText() != text)
        {
            p_dlg->updating = true;
            c->p_multi->setPlainText(text);
            p_dlg->updating = false;
        }
    }
    else if(nullptr != c->p_combo)
    {
        if(c->p_combo->isEditable())
            c->p_combo->setEditText(text);
    }
}

extern "C" int
qth_dlg_get_text(QTH_DIALOG * p_dlg, int id, char * p_buf, int n)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);
    QString text;

    if(nullptr != c)
    {
        if(nullptr != c->p_line)
            text = c->p_line->text();
        else if(nullptr != c->p_multi)
            text = c->p_multi->toPlainText();
        else if(nullptr != c->p_combo)
            text = c->p_combo->currentText();
        else if(nullptr != c->p_button)
            text = c->p_button->text();
        else if(nullptr != c->p_label)
            text = c->p_label->text();
    }

    const QByteArray bytes = text.toLatin1(); /* anything not in Latin-1 becomes '?' */

    if((nullptr != p_buf) && (n > 0))
    {
        const int copy = qMin((int) bytes.size(), n - 1);
        memcpy(p_buf, bytes.constData(), (size_t) copy);
        p_buf[copy] = '\0';
    }

    return((int) bytes.size());
}

extern "C" void
qth_dlg_set_check(QTH_DIALOG * p_dlg, int id, int state)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if((nullptr == c) || (nullptr == c->p_button))
        return;

    QCheckBox * p_check = qobject_cast<QCheckBox *>(c->p_button);

    if((nullptr != p_check) && p_check->isTristate())
        p_check->setCheckState((state == 2) ? Qt::PartiallyChecked : (state ? Qt::Checked : Qt::Unchecked));
    else
        c->p_button->setChecked(0 != state);
}

extern "C" void
qth_dlg_set_enabled(QTH_DIALOG * p_dlg, int id, int enabled)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr != c)
        c->p_widget->setEnabled(0 != enabled);
}

extern "C" void
qth_dlg_set_focus(QTH_DIALOG * p_dlg, int id)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr == c)
        return;

    if(nullptr != c->p_line)
        c->p_line->setFocus();
    else
        c->p_widget->setFocus();
}

extern "C" void
qth_dlg_set_default(QTH_DIALOG * p_dlg, int id)
{
    for(auto & it : p_dlg->ctls)
    {
        QPushButton * p_push = qobject_cast<QPushButton *>(it.second.p_button);
        if(nullptr != p_push)
            p_push->setDefault(it.first == id);
    }
}

extern "C" void
qth_dlg_select_all(QTH_DIALOG * p_dlg, int id)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr == c)
        return;

    if(nullptr != c->p_line)
        c->p_line->selectAll();
    else if(nullptr != c->p_multi)
        c->p_multi->selectAll();
    else if((nullptr != c->p_combo) && (nullptr != c->p_combo->lineEdit()))
        c->p_combo->lineEdit()->selectAll();
}

extern "C" void
qth_dlg_list_clear(QTH_DIALOG * p_dlg, int id)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr == c)
        return;

    p_dlg->updating = true;
    if(nullptr != c->p_list)
        c->p_list->clear();
    else if(nullptr != c->p_combo)
    {
        const QString keep = c->p_combo->isEditable() ? c->p_combo->currentText() : QString();
        c->p_combo->clear();
        if(c->p_combo->isEditable())
            c->p_combo->setEditText(keep);
    }
    p_dlg->updating = false;
}

extern "C" void
qth_dlg_list_add(QTH_DIALOG * p_dlg, int id, const char * p_text)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr == c)
        return;

    p_dlg->updating = true;
    if(nullptr != c->p_list)
        c->p_list->addItem(text_of(p_text));
    else if(nullptr != c->p_combo)
    {
        const QString keep = c->p_combo->isEditable() ? c->p_combo->currentText() : QString();
        c->p_combo->addItem(text_of(p_text));
        if(c->p_combo->isEditable())
            c->p_combo->setEditText(keep);
    }
    p_dlg->updating = false;
}

extern "C" void
qth_dlg_list_set_tab(QTH_DIALOG * p_dlg, int id, int pixels)
{
    auto it = p_dlg->delegates.find(id);

    if(it != p_dlg->delegates.end())
        it->second->tab = pixels;
}

extern "C" void
qth_dlg_list_set_sel(QTH_DIALOG * p_dlg, int id, int index)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr == c)
        return;

    p_dlg->updating = true;
    if(nullptr != c->p_list)
        c->p_list->setCurrentRow(index, (index < 0) ? QItemSelectionModel::Clear : QItemSelectionModel::ClearAndSelect);
    else if(nullptr != c->p_combo)
    {
        const QString keep = (c->p_combo->isEditable() && (index < 0)) ? c->p_combo->currentText() : QString();
        c->p_combo->setCurrentIndex(index);
        if(c->p_combo->isEditable() && (index < 0))
            c->p_combo->setEditText(keep);
    }
    p_dlg->updating = false;
}

extern "C" int
qth_dlg_list_get_sel(QTH_DIALOG * p_dlg, int id)
{
    DLG_CTL * c = dlg_ctl(p_dlg, id);

    if(nullptr != c)
    {
        if(nullptr != c->p_list)
            return(c->p_list->currentRow());
        if(nullptr != c->p_combo)
            return(c->p_combo->currentIndex());
    }

    return(-1);
}

/* end of qth_qt.cpp */
