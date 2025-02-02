/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QAPPLICATION_P_H
#define QAPPLICATION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of qapplication_*.cpp, qwidget*.cpp, qcolor_x11.cpp, qfiledialog.cpp
// and many other.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "QtWidgets/qapplication.h"
#include "QtGui/qevent.h"
#include "QtGui/qfont.h"
#include "QtGui/qcursor.h"
#include "QtGui/qregion.h"
#include "QtGui/qwindow.h"
#include "qwidget.h"
#include <qpa/qplatformnativeinterface.h>
#include "QtCore/qmutex.h"
#include "QtCore/qtranslator.h"
#include "QtCore/qbasictimer.h"
#include "QtCore/qhash.h"
#include "QtCore/qpointer.h"
#include "private/qcoreapplication_p.h"
#include "QtCore/qpoint.h"
#include <QTime>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <qpa/qplatformintegration.h>
#include "private/qguiapplication_p.h"

QT_BEGIN_NAMESPACE

class QClipboard;
class QGraphicsScene;
class QObject;
class QWidget;
class QSocketNotifier;
class QTouchDevice;
#ifndef QT_NO_GESTURES
class QGestureManager;
#endif

extern Q_GUI_EXPORT bool qt_is_gui_used;
#ifndef QT_NO_CLIPBOARD
extern QClipboard *qt_clipboard;
#endif

typedef QHash<QByteArray, QFont> FontHash;
Q_WIDGETS_EXPORT FontHash *qt_app_fonts_hash();

typedef QHash<QByteArray, QPalette> PaletteHash;
PaletteHash *qt_app_palettes_hash();

#define QApplicationPrivateBase QGuiApplicationPrivate

class Q_WIDGETS_EXPORT QApplicationPrivate : public QApplicationPrivateBase
{
    Q_DECLARE_PUBLIC(QApplication)
public:
    QApplicationPrivate(int &argc, char **argv, int flags);
    ~QApplicationPrivate();

    virtual void notifyLayoutDirectionChange() override;
    virtual void notifyActiveWindowChange(QWindow *) override;
    virtual void updateWidgetFocus(QWindow *previous) override;

    virtual bool shouldQuit() override;
    bool tryCloseAllWindows() override;

    WId widgetEffectiveWinId(const QObject *o) const override;
    QPair<QObject*, WId> popupFocusEditableWidget() const override;
    QVariant objectImQuery(Qt::InputMethodQuery im, const QObject* o, WId nativeWid) const override;
    void changeKeyboard() const override;

    bool isInactiveWithoutFocus() const override;

#if 0 // Used to be included in Qt4 for Q_WS_X11
#if QT_CONFIG(settings)
    static bool x11_apply_settings();
#endif
    static void reset_instance_pointer();
#endif
    static bool autoSipEnabled;
    static QString desktopStyleKey();


    void createEventDispatcher() override;
    static void dispatchEnterLeave(QWidget *enter, QWidget *leave, const QPointF &globalPosF);

    void notifyWindowIconChanged() override;

    // modality
    bool isWindowBlocked(QWindow *window, QWindow **blockingWindow = 0) const override;
    static bool isBlockedByModal(QWidget *widget);
    static bool modalState();
    static bool tryModalHelper(QWidget *widget, QWidget **rettop = 0);
#if 0 // Used to be included in Qt4 for Q_WS_MAC
    static QWidget *tryModalHelper_sys(QWidget *top);
    bool canQuit();
#endif

    bool notify_helper(QObject *receiver, QEvent *e);

    void init(
#if 0 // Used to be included in Qt4 for Q_WS_X11
                   Display *dpy = 0, Qt::HANDLE visual = 0, Qt::HANDLE cmap = 0
#endif
    );
    void initialize();
    void process_cmdline();

#if 0 // Used to be included in Qt4 for Q_WS_X11
    static void x11_initialize_style();
#endif

    static bool inPopupMode();
    bool popupActive() override { return inPopupMode(); }
    void closeAllPopups() override;
    void closePopup(QWidget *popup);
    void openPopup(QWidget *popup);
    static void setFocusWidget(QWidget *focus, Qt::FocusReason reason, bool syncNative = true);
    static QWidget *focusNextPrevChild_helper(QWidget *toplevel, bool next,
                                              bool *wrappingOccurred = 0);

    bool isButtonDown() const override;
#if QT_CONFIG(graphicsview)
    // Maintain a list of all scenes to ensure font and palette propagation to
    // all scenes.
    QList<QGraphicsScene *> scene_list;
#endif

    QBasicTimer toolTipWakeUp, toolTipFallAsleep;
    QPoint toolTipPos, toolTipGlobalPos, hoverGlobalPos;
    QPointer<QWidget> toolTipWidget;

    static QSize app_strut;
    static QWidgetList *popupWidgets;
    static QStyle *app_style;
    static QPalette *sys_pal;
    static QPalette *set_pal;
#ifdef Q_OS_MAC
    static bool usingGlassEffect;
#endif
protected:
    void notifyThemeChanged() override;
    void sendApplicationPaletteChange(bool toAllWidgets = false,
                                      const char *className = nullptr) override;

#if QT_CONFIG(draganddrop)
    void notifyDragStarted(const QDrag *) override;
    bool notifyDragCanceled(const QDrag *drag, const QPoint &pos) override;
#endif // QT_CONFIG(draganddrop)

public:
    static QFont *sys_font;
    static QFont *set_font;
    static QWidget *main_widget;
    static QWidget *focus_widget;
    static QWidget *hidden_focus_widget;
    static bool hidden_focus_syncNative;
    static QWidget *active_window;
#if QT_CONFIG(wheelevent)
    static int wheel_scroll_lines;
    static QPointer<QWidget> wheel_widget;
#endif

#if QT_CONFIG(draganddrop)
    static QPointer<QWidget> saved_button_down;
#endif

    static int enabledAnimations; // Combination of QPlatformTheme::UiEffect
    static bool widgetCount; // Coupled with -widgetcount switch

    static void setSystemPalette(const QPalette &pal);
    static void setPalette_helper(const QPalette &palette, const char* className, bool clearWidgetPaletteHash);
    static void initializeWidgetPaletteHash();
    static void initializeWidgetFontHash();
    static void setSystemFont(const QFont &font);

#if 0 // Used to be included in Qt4 for Q_WS_X11
    static void applyX11SpecificCommandLineArguments(QWidget *main_widget);
#endif

    static QApplicationPrivate *instance() { return self; }

#ifdef QT_KEYPAD_NAVIGATION
    static QWidget *oldEditFocus;
    static Qt::NavigationMode navigationMode;
#endif

#if 0 /* Used to be included in Qt4 for Q_WS_MAC */ || 0 /* Used to be included in Qt4 for Q_WS_X11 */
    void _q_alertTimeOut();
    QHash<QWidget *, QTimer *> alertTimerHash;
#endif
#ifndef QT_NO_STYLE_STYLESHEET
    static QString styleSheet;
#endif
    static QPointer<QWidget> leaveAfterRelease;
    static QWidget *pickMouseReceiver(QWidget *candidate, const QPoint &windowPos, QPoint *pos,
                                      QEvent::Type type, Qt::MouseButtons buttons,
                                      QWidget *buttonDown, QWidget *alienWidget);
    static bool sendMouseEvent(QWidget *receiver, QMouseEvent *event, QWidget *alienWidget,
                               QWidget *native, QWidget **buttonDown, QPointer<QWidget> &lastMouseReceiver,
                               bool spontaneous = true, bool onlyDispatchEnterLeave = false);
    void sendSyntheticEnterLeave(QWidget *widget);

    static QWindow *windowForWidget(const QWidget *widget)
    {
        if (QWindow *window = widget->windowHandle())
            return window;
        if (const QWidget *nativeParent = widget->nativeParentWidget())
            return nativeParent->windowHandle();
        return 0;
    }

#ifdef Q_OS_WIN
    static HWND getHWNDForWidget(const QWidget *widget)
    {
        if (QWindow *window = windowForWidget(widget))
            if (window->handle() && QGuiApplication::platformNativeInterface())
                return static_cast<HWND> (QGuiApplication::platformNativeInterface()->
                                          nativeResourceForWindow(QByteArrayLiteral("handle"), window));
        return 0;
    }

    static bool isWindowsDropMimeData(const QMimeData *mime)
    {
        if (nullptr == mime)
            return false;

        if (QGuiApplication::platformNativeInterface())
            return QGuiApplication::platformNativeInterface()->isWindowsDropMimeData(mime);

        return nullptr;
    }

    static void *getDropDataObjectForMimeData(const QMimeData *mime) 
    {
        if (nullptr == mime)
            return nullptr;
        if (QGuiApplication::platformNativeInterface())
            return QGuiApplication::platformNativeInterface()->nativeResourceForMimeData(
                            QByteArrayLiteral("dropdataobject"), mime);

        return nullptr;
    }
#endif

#ifndef QT_NO_GESTURES
    QGestureManager *gestureManager;
    QWidget *gestureWidget;
#endif
#if 0 /* Used to be included in Qt4 for Q_WS_X11 */ || 0 /* Used to be included in Qt4 for Q_WS_WIN */
    QPixmap *move_cursor;
    QPixmap *copy_cursor;
    QPixmap *link_cursor;
#endif
#if 0 // Used to be included in Qt4 for Q_WS_WIN
    QPixmap *ignore_cursor;
#endif

    static bool updateTouchPointsForWidget(QWidget *widget, QTouchEvent *touchEvent);
    void initializeMultitouch();
    void initializeMultitouch_sys();
    void cleanupMultitouch();
    void cleanupMultitouch_sys();
    QWidget *findClosestTouchPointTarget(QTouchDevice *device, const QTouchEvent::TouchPoint &touchPoint);
    void appendTouchPoint(const QTouchEvent::TouchPoint &touchPoint);
    void removeTouchPoint(int touchPointId);
    void activateImplicitTouchGrab(QWidget *widget, QTouchEvent *touchBeginEvent);
    static bool translateRawTouchEvent(QWidget *widget,
                                       QTouchDevice *device,
                                       const QList<QTouchEvent::TouchPoint> &touchPoints,
                                       ulong timestamp);
    static void translateTouchCancel(QTouchDevice *device, ulong timestamp);

    QPixmap applyQIconStyleHelper(QIcon::Mode mode, const QPixmap& base) const override;
private:
    static QApplicationPrivate *self;
    static bool tryCloseAllWidgetWindows(QWindowList *processedWindows);

    static void giveFocusAccordingToFocusPolicy(QWidget *w, QEvent *event, QPoint localPos);
    static bool shouldSetFocus(QWidget *w, Qt::FocusPolicy policy);


    static bool isAlien(QWidget *);
};

#if 0 // Used to be included in Qt4 for Q_WS_WIN
  extern void qt_win_set_cursor(QWidget *, bool);
#elif 0 // Used to be included in Qt4 for Q_WS_X11
  extern void qt_x11_enforce_cursor(QWidget *, bool);
  extern void qt_x11_enforce_cursor(QWidget *);
#else
  extern void qt_qpa_set_cursor(QWidget * w, bool force);
#endif

QT_END_NAMESPACE

#endif // QAPPLICATION_P_H
