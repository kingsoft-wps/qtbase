/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwindowsdrag.h"
#include "qwindowscontext.h"
#include "qwindowsscreen.h"
#if QT_CONFIG(clipboard)
#  include "qwindowsclipboard.h"
#endif
#include "qwindowsintegration.h"
#include "qwindowsdropdataobject.h"
#include "qwindowdragdrophelper.h"
#include <QtCore/qt_windows.h>
#include "qwindowswindow.h"
#include "qwindowsmousehandler.h"
#include "qwindowscursor.h"
#include "qwindowskeymapper.h"
#include "qwindowsdescription.h"

#include <QtGui/qevent.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qguiapplication.h>
#include <qpa/qwindowsysteminterface_p.h>
#include <QtGui/private/qdnd_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qhighdpiscaling_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qpoint.h>
#include <QtCore/qoperatingsystemversion.h>
#include <shlobj.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsDragCursorWindow
    \brief A toplevel window showing the drag icon in case of touch drag.

    \sa QWindowsOleDropSource
    \internal
    \ingroup qt-lighthouse-win
*/

class QWindowsDragCursorWindow : public QRasterWindow
{
public:
    explicit QWindowsDragCursorWindow(QWindow *parent = nullptr);

    void setPixmap(const QPixmap &p);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_pixmap);
    }

private:
    QPixmap m_pixmap;
};

QWindowsDragCursorWindow::QWindowsDragCursorWindow(QWindow *parent)
    : QRasterWindow(parent)
{
    QSurfaceFormat windowFormat = format();
    windowFormat.setAlphaBufferSize(8);
    setFormat(windowFormat);
    setObjectName(QStringLiteral("QWindowsDragCursorWindow"));
    setFlags(Qt::Popup | Qt::NoDropShadowWindowHint
             | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
}

void QWindowsDragCursorWindow::setPixmap(const QPixmap &p)
{
    if (p.cacheKey() == m_pixmap.cacheKey())
        return;
    const QSize oldSize = m_pixmap.size();
    QSize newSize = p.size();
    qCDebug(lcQpaMime) << __FUNCTION__ << p.cacheKey() << newSize;
    m_pixmap = p;
    if (oldSize != newSize) {
        const qreal pixDevicePixelRatio = p.devicePixelRatio();
        if (pixDevicePixelRatio > 1.0 && qFuzzyCompare(pixDevicePixelRatio, devicePixelRatio()))
            newSize /= qRound(pixDevicePixelRatio);
        resize(newSize);
    }
    if (isVisible())
        update();
}

/*!
    \class QWindowsDropMimeData
    \brief Special mime data class for data retrieval from Drag operations.

    Implementation of QWindowsInternalMimeDataBase which retrieves the
    current drop data object from QWindowsDrag.

    \sa QWindowsDrag
    \internal
    \ingroup qt-lighthouse-win
*/

IDataObject *QWindowsDropMimeData::retrieveDataObject() const
{
    return QWindowsDrag::instance()->dropDataObject();
}

static inline Qt::DropActions translateToQDragDropActions(DWORD pdwEffects)
{
    Qt::DropActions actions = Qt::IgnoreAction;
    if (pdwEffects & DROPEFFECT_LINK)
        actions |= Qt::LinkAction;
    if (pdwEffects & DROPEFFECT_COPY)
        actions |= Qt::CopyAction;
    if (pdwEffects & DROPEFFECT_MOVE)
        actions |= Qt::MoveAction;
    return actions;
}

static inline Qt::DropAction translateToQDragDropAction(DWORD pdwEffect)
{
    if (pdwEffect & DROPEFFECT_LINK)
        return Qt::LinkAction;
    if (pdwEffect & DROPEFFECT_COPY)
        return Qt::CopyAction;
    if (pdwEffect & DROPEFFECT_MOVE)
        return Qt::MoveAction;
    return Qt::IgnoreAction;
}

static inline DWORD translateToWinDragEffects(Qt::DropActions action)
{
    DWORD effect = DROPEFFECT_NONE;
    if (action & Qt::LinkAction)
        effect |= DROPEFFECT_LINK;
    if (action & Qt::CopyAction)
        effect |= DROPEFFECT_COPY;
    if (action & Qt::MoveAction)
        effect |= DROPEFFECT_MOVE;
    return effect;
}

static inline Qt::KeyboardModifiers toQtKeyboardModifiers(DWORD keyState)
{
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;

    if (keyState & MK_SHIFT)
        modifiers |= Qt::ShiftModifier;
    if (keyState & MK_CONTROL)
        modifiers |= Qt::ControlModifier;
    if (keyState & MK_ALT)
        modifiers |= Qt::AltModifier;

    return modifiers;
}

static inline Qt::MouseButtons toQtMouseButtons(DWORD keyState)
{
    Qt::MouseButtons buttons = Qt::NoButton;

    if (keyState & MK_LBUTTON)
        buttons |= Qt::LeftButton;
    if (keyState & MK_RBUTTON)
        buttons |= Qt::RightButton;
    if (keyState & MK_MBUTTON)
        buttons |= Qt::MidButton;

    return buttons;
}

static Qt::KeyboardModifiers lastModifiers = Qt::NoModifier;
static Qt::MouseButtons lastButtons = Qt::NoButton;

/*!
    \class QWindowsOleDropSource
    \brief Implementation of IDropSource

    Used for drag operations.

    \sa QWindowsDrag
    \internal
    \ingroup qt-lighthouse-win
*/

class QWindowsOleDropSource : public QWindowsComBase<IDropSource>
{
public:
    enum Mode {
        MouseDrag,
        TouchDrag // Mouse cursor suppressed, use window as cursor.
    };

    explicit QWindowsOleDropSource(QWindowsDrag *drag);
    ~QWindowsOleDropSource() override;

    void createCursors();

    // IDropSource methods
    STDMETHOD(QueryContinueDrag)(BOOL fEscapePressed, DWORD grfKeyState);
    STDMETHOD(GiveFeedback)(DWORD dwEffect);

protected:
    struct CursorEntry {
        CursorEntry() : cacheKey(0) {}
        CursorEntry(const QPixmap &p, qint64 cK, const CursorHandlePtr &c, const QPoint &h) :
            pixmap(p), cacheKey(cK), cursor(c), hotSpot(h) {}

        QPixmap pixmap;
        qint64 cacheKey; // Cache key of cursor
        CursorHandlePtr cursor;
        QPoint hotSpot;
    };

    typedef QMap<Qt::DropAction, CursorEntry> ActionCursorMap;

    Mode m_mode;
    QWindowsDrag *m_drag;
    QPointer<QWindow> m_windowUnderMouse;
    Qt::MouseButtons m_currentButtons;
    ActionCursorMap m_cursors;
    QWindowsDragCursorWindow *m_touchDragWindow;

#ifndef QT_NO_DEBUG_STREAM
    friend QDebug operator<<(QDebug, const QWindowsOleDropSource::CursorEntry &);
#endif
};

QWindowsOleDropSource::QWindowsOleDropSource(QWindowsDrag *drag)
    : m_mode(QWindowsCursor::cursorState() != QWindowsCursor::State::Suppressed ? MouseDrag : TouchDrag)
    , m_drag(drag)
    , m_windowUnderMouse(QWindowsContext::instance()->windowUnderMouse())
    , m_currentButtons(Qt::NoButton)
    , m_touchDragWindow(nullptr)
{
    qCDebug(lcQpaMime) << __FUNCTION__ << m_mode;
}

QWindowsOleDropSource::~QWindowsOleDropSource()
{
    m_cursors.clear();
    delete m_touchDragWindow;
    qCDebug(lcQpaMime) << __FUNCTION__;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const QWindowsOleDropSource::CursorEntry &e)
{
    d << "CursorEntry:" << e.pixmap.size() << '#' << e.cacheKey
      << "HCURSOR" << e.cursor->handle() << "hotspot:" << e.hotSpot;
    return d;
}
#endif // !QT_NO_DEBUG_STREAM

/*!
    \brief Blend custom pixmap with cursors.
*/

void QWindowsOleDropSource::createCursors()
{
    const QDrag *drag = m_drag->currentDrag();
    const QPixmap pixmap = drag->pixmap();
    const bool hasPixmap = !pixmap.isNull();

    // Find screen for drag. Could be obtained from QDrag::source(), but that might be a QWidget.
    const QPlatformScreen *platformScreen = QWindowsContext::instance()->screenManager().screenAtDp(QWindowsCursor::mousePosition());
    if (!platformScreen) {
        if (const QScreen *primaryScreen = QGuiApplication::primaryScreen())
            platformScreen = primaryScreen->handle();
    }
    Q_ASSERT(platformScreen);

    if (GetSystemMetrics (SM_REMOTESESSION) != 0) {
        /* Workaround for RDP issues with large cursors.
         * Touch drag window seems to work just fine...
         * 96 pixel is a 'large' mouse cursor, according to RDP spec */
        const int rdpLargeCursor = qRound(qreal(96) / QHighDpiScaling::factor(platformScreen));
        if (pixmap.width() > rdpLargeCursor || pixmap.height() > rdpLargeCursor)
            m_mode = TouchDrag;
    }

    qreal pixmapScaleFactor = 1;
    qreal hotSpotScaleFactor = 1;
    if (m_mode != TouchDrag) { // Touch drag: pixmap is shown in a separate QWindow, which will be scaled.)
        hotSpotScaleFactor = QHighDpiScaling::factor(platformScreen);
        pixmapScaleFactor = hotSpotScaleFactor / pixmap.devicePixelRatio();
    }
    QPixmap scaledPixmap = qFuzzyCompare(pixmapScaleFactor, 1.0)
        ? pixmap
        :  pixmap.scaled((QSizeF(pixmap.size()) * pixmapScaleFactor).toSize(),
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaledPixmap.setDevicePixelRatio(1);

    Qt::DropAction actions[] = { Qt::MoveAction, Qt::CopyAction, Qt::LinkAction, Qt::IgnoreAction };
    int actionCount = int(sizeof(actions) / sizeof(actions[0]));
    if (!hasPixmap)
        --actionCount; // No Qt::IgnoreAction unless pixmap
    const QPoint hotSpot = qFuzzyCompare(hotSpotScaleFactor, 1.0)
        ?  drag->hotSpot()
        : (QPointF(drag->hotSpot()) * hotSpotScaleFactor).toPoint();
    for (int cnum = 0; cnum < actionCount; ++cnum) {
        const Qt::DropAction action = actions[cnum];
        QPixmap cursorPixmap = drag->dragCursor(action);

        if (cursorPixmap.isNull())
        {
            QPlatformCursor* platformCursor = platformScreen->cursor();
            if (!platformCursor)
                continue;

            //actually, it's necessary for getting cursorPixmap from QWindowsCursor, or setting pixmap to drag is useless.
            //And we need compatible with Qt4.
            if (drag->property("_q_platform_WindowsDragQt4CompatibleCursor").toBool())
                cursorPixmap = static_cast<QWindowsCursor *>(platformCursor)->dragQt4CompatibleCursor(action);
        }

        const qint64 cacheKey = cursorPixmap.cacheKey();
        const auto it = m_cursors.find(action);
        if (it != m_cursors.end() && it.value().cacheKey == cacheKey)
            continue;
        if (cursorPixmap.isNull()) {
            qWarning("%s: Unable to obtain drag cursor for %d.", __FUNCTION__, action);
            continue;
        }

        QPoint newHotSpot(0, 0);
        QPixmap newPixmap = cursorPixmap;

        if (hasPixmap) {
            const int x1 = qMin(-hotSpot.x(), 0);
            const int x2 = qMax(scaledPixmap.width() - hotSpot.x(), cursorPixmap.width());
            const int y1 = qMin(-hotSpot.y(), 0);
            const int y2 = qMax(scaledPixmap.height() - hotSpot.y(), cursorPixmap.height());
            QPixmap newCursor(x2 - x1 + 1, y2 - y1 + 1);
            newCursor.fill(Qt::transparent);
            QPainter p(&newCursor);
            const QPoint pmDest = QPoint(qMax(0, -hotSpot.x()), qMax(0, -hotSpot.y()));
            p.drawPixmap(pmDest, scaledPixmap);
            p.drawPixmap(qMax(0, hotSpot.x()),qMax(0, hotSpot.y()), cursorPixmap);
            newPixmap = newCursor;
            newHotSpot = QPoint(qMax(0, hotSpot.x()), qMax(0, hotSpot.y()));
        }

        if (const HCURSOR sysCursor = QWindowsCursor::createPixmapCursor(newPixmap, newHotSpot)) {
            const CursorEntry entry(newPixmap, cacheKey, CursorHandlePtr(new CursorHandle(sysCursor)), newHotSpot);
            if (it == m_cursors.end())
                m_cursors.insert(action, entry);
            else
                it.value() = entry;
        }
    }
#ifndef QT_NO_DEBUG_OUTPUT
    if (lcQpaMime().isDebugEnabled())
        qCDebug(lcQpaMime) << __FUNCTION__ << "pixmap" << pixmap.size() << m_cursors.size() << "cursors:\n" << m_cursors;
#endif // !QT_NO_DEBUG_OUTPUT
}

/*!
    \brief Check for cancel.
*/

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropSource::QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState)
{
    Qt::MouseButtons buttons = toQtMouseButtons(grfKeyState);
    Qt::MouseButtons currentButtons = buttons;

    SCODE result = S_OK;
    if (fEscapePressed || QWindowsDrag::isCanceled()) {
        result = DRAGDROP_S_CANCEL;
        buttons = Qt::NoButton;
    } else {
        if (buttons && !m_currentButtons) {
            m_currentButtons = buttons;
        } else if (!(m_currentButtons & buttons)) { // Button changed: Complete Drop operation.
            result = DRAGDROP_S_DROP;
        }
    }

    switch (result) {
        case DRAGDROP_S_DROP:
        case DRAGDROP_S_CANCEL:
            if (!m_windowUnderMouse.isNull() && m_mode != TouchDrag && fEscapePressed == FALSE
                && buttons != lastButtons) {
                // QTBUG 66447: Synthesize a mouse release to the window under mouse at
                // start of the DnD operation as Windows does not send any.
                const QPoint globalPos = QWindowsCursor::mousePosition();
                const QPoint localPos = m_windowUnderMouse->handle()->mapFromGlobal(globalPos);

                bool sendRelease = true;
                if (result == DRAGDROP_S_CANCEL && lastButtons == currentButtons) {
                    sendRelease = !QDragManager::self()->notifyCanceled(m_drag->currentDrag(), globalPos);
                }

                if (sendRelease) {
                    QWindowSystemInterface::handleMouseEvent(
                            m_windowUnderMouse.data(), QPointF(localPos), QPointF(globalPos),
                            QWindowsMouseHandler::queryMouseButtons(), Qt::LeftButton, QEvent::MouseButtonRelease);
                }
            }
            m_currentButtons = Qt::NoButton;
            break;

        default:
            QGuiApplication::processEvents();
            break;
    }

    if (QWindowsContext::verbose > 1 || result != S_OK) {
        qCDebug(lcQpaMime) << __FUNCTION__ << "fEscapePressed=" << fEscapePressed
            << "grfKeyState=" << grfKeyState << "buttons" << m_currentButtons
            << "returns 0x" << hex << int(result) << dec;
    }
    return ResultFromScode(result);
}

/*!
    \brief Give feedback: Change cursor accoding to action.
*/

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropSource::GiveFeedback(DWORD dwEffect)
{
    const Qt::DropAction action = translateToQDragDropAction(dwEffect);
    m_drag->updateAction(action);

    const qint64 currentCacheKey = m_drag->currentDrag()->dragCursor(action).cacheKey();
    auto it = m_cursors.constFind(action);
    // If a custom drag cursor is set, check its cache key to detect changes.
    if (currentCacheKey && currentCacheKey != it.value().cacheKey) {
        createCursors();
        it = m_cursors.constFind(action);
    }

    if (it != m_cursors.constEnd()) {
        const CursorEntry &e = it.value();
        switch (m_mode) {
        case MouseDrag:
            SetCursor(e.cursor->handle());
            break;
        case TouchDrag:
            // "Touch drag" with an unsuppressed cursor may happen with RDP (see createCursors())
            if (QWindowsCursor::cursorState() != QWindowsCursor::State::Suppressed)
                SetCursor(nullptr);
            if (!m_touchDragWindow)
                m_touchDragWindow = new QWindowsDragCursorWindow;
            m_touchDragWindow->setPixmap(e.pixmap);
            m_touchDragWindow->setFramePosition(QCursor::pos() - e.hotSpot);
            if (!m_touchDragWindow->isVisible())
                m_touchDragWindow->show();
            break;
        }
        return ResultFromScode(S_OK);
    }

    return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}

//---------------------------------------------------------------------
//                    QWindowsOleDropSourceEx
//---------------------------------------------------------------------
class QWindowsOleDropSourceEx : public QWindowsOleDropSource
{
public:
    QWindowsOleDropSourceEx(QWindowsDrag *drag,QWindowsOleDataObjectEx* iDataObj);
    virtual ~QWindowsOleDropSourceEx();

    // IDropSource methods
    STDMETHOD(GiveFeedback)(DWORD dwEffect);

private:
    bool SetDragImageCursor(DROPEFFECT dwEffect);

private:
    QWindowsOleDataObjectEx *m_pIDataObj;
};

QWindowsOleDropSourceEx::QWindowsOleDropSourceEx(QWindowsDrag *drag, QWindowsOleDataObjectEx* iDataObj)
    : QWindowsOleDropSource(drag)
    , m_pIDataObj(iDataObj)
{
    if (m_pIDataObj)
        m_pIDataObj->AddRef();
}

QWindowsOleDropSourceEx::~QWindowsOleDropSourceEx()
{
    if (m_pIDataObj) {
        m_pIDataObj->Release();
        m_pIDataObj = nullptr;
    }
}
//---------------------------------------------------------------------
//                    IDropSource Methods
//---------------------------------------------------------------------
QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP QWindowsOleDropSourceEx::GiveFeedback(DWORD dwEffect)
{
    /*
     bool bOldStyle =
            (0 == QDragDropHelper::GetGlobalDataDWord(m_pIDataObj, _T("IsShowingLayered")));
    if (m_pIDataObj->getDropDescription() && !bOldStyle) {
        FORMATETC FormatEtc = { 0 };
        STGMEDIUM StgMedium = { 0 };
        if (QDragDropHelper::GetGlobalData(m_pIDataObj, CFSTR_DROPDESCRIPTION, FormatEtc,
                                           StgMedium)) {
            bool bChangeDescription = false;
            DROPDESCRIPTION *pDropDescription =
                    static_cast<DROPDESCRIPTION *>(::GlobalLock(StgMedium.hGlobal));
            if (bOldStyle)
                bChangeDescription = QDragDropHelper::ClearDescription(pDropDescription);
            else if (pDropDescription->type <= DROPIMAGE_LINK) {
                DROPIMAGETYPE nImageType = QDragDropHelper::DropEffectToDropImage(dwEffect);
                if (DROPIMAGE_INVALID != nImageType && pDropDescription->type != nImageType) {
                    if (m_pIDataObj->getDropDescription()->HasText(nImageType)) {
                        bChangeDescription = true;
                        pDropDescription->type = nImageType;
                        m_pIDataObj->getDropDescription()->SetDescription(pDropDescription,
                                                                          nImageType);
                    } else
                        bChangeDescription = QDragDropHelper::ClearDescription(pDropDescription);
                }
            }
            ::GlobalUnlock(StgMedium.hGlobal);
            if (bChangeDescription) {
                if (S_OK != m_pIDataObj->SetData(&FormatEtc, &StgMedium, TRUE))
                    bChangeDescription = false;
            }
            if (!bChangeDescription)
                ::ReleaseStgMedium(&StgMedium);
        }
    }

    const Qt::DropAction action = translateToQDragDropAction(dwEffect);
    m_drag->updateAction(action); */
    if (SetDragImageCursor(dwEffect))
        return ResultFromScode(S_OK);
    return ResultFromScode(DRAGDROP_S_USEDEFAULTCURSORS);
}

#define DDWM_SETCURSOR (WM_USER + 2)
bool QWindowsOleDropSourceEx::SetDragImageCursor(DROPEFFECT dwEffect)
{
    HWND hWnd =
            (HWND)ULongToHandle(QDragDropHelper::GetGlobalDataDWord(m_pIDataObj, _T("DragWindow")));
    if (NULL == hWnd)
        return false;

    WPARAM wParam = 0;
    switch (dwEffect & ~DROPEFFECT_SCROLL) {
    case DROPEFFECT_NONE:
        wParam = 1;
        break;
    case DROPEFFECT_COPY:
        wParam = 3;
        break;
    case DROPEFFECT_MOVE:
        wParam = 2;
        break;
    case DROPEFFECT_LINK:
        wParam = 4;
        break;
    }
    ::PostMessage(hWnd, DDWM_SETCURSOR, wParam, 0);
    return IsWindowVisible(hWnd);
}

/*!
    \class QWindowsOleDropTarget
    \brief Implementation of IDropTarget

    To be registered for each window. Currently, drop sites
    are enabled for top levels. The child window handling
    (sending DragEnter/Leave, etc) is handled in here.

    \sa QWindowsDrag
    \internal
    \ingroup qt-lighthouse-win
*/

QWindowsOleDropTarget::QWindowsOleDropTarget(QWindow *w) : m_window(w)
{
    qCDebug(lcQpaMime) << __FUNCTION__ << this << w;
}

QWindowsOleDropTarget::~QWindowsOleDropTarget()
{
    qCDebug(lcQpaMime) << __FUNCTION__ <<  this;
}

void QWindowsOleDropTarget::handleDrag(QWindow *window, DWORD grfKeyState,
                                       const QPoint &point, LPDWORD pdwEffect)
{
    Q_ASSERT(window);
    m_lastPoint = point;
    m_lastKeyState = grfKeyState;

    QWindowsDrag *windowsDrag = QWindowsDrag::instance();
    const Qt::DropActions actions = translateToQDragDropActions(*pdwEffect);

    lastModifiers = toQtKeyboardModifiers(grfKeyState);
    lastButtons = toQtMouseButtons(grfKeyState);

    const QPlatformDragQtResponse response =
          QWindowSystemInterface::handleDrag(window, windowsDrag->dropData(),
                                             m_lastPoint, actions,
                                             lastButtons, lastModifiers);

    m_answerRect = response.answerRect();
    const Qt::DropAction action = response.acceptedAction();
    if (response.isAccepted()) {
        m_chosenEffect = translateToWinDragEffects(action);
    } else {
        m_chosenEffect = DROPEFFECT_NONE;
    }
    *pdwEffect = m_chosenEffect;
    qCDebug(lcQpaMime) << __FUNCTION__ << m_window
        << windowsDrag->dropData() << " supported actions=" << actions
        << " mods=" << lastModifiers << " mouse=" << lastButtons
        << " accepted: " << response.isAccepted() << action
        << m_answerRect << " effect" << *pdwEffect;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropTarget::DragEnter(LPDATAOBJECT pDataObj, DWORD grfKeyState,
                                 POINTL pt, LPDWORD pdwEffect)
{
    if (IDropTargetHelper* dh = QWindowsDrag::instance()->dropHelper())
        dh->DragEnter(reinterpret_cast<HWND>(m_window->winId()), pDataObj, reinterpret_cast<POINT*>(&pt), *pdwEffect);

    qCDebug(lcQpaMime) << __FUNCTION__ << "widget=" << m_window << " key=" << grfKeyState
        << "pt=" << pt.x << pt.y;

    QWindowsDrag::instance()->setDropDataObject(pDataObj);
    pDataObj->AddRef();
    const QPoint point = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x,pt.y));
    handleDrag(m_window, grfKeyState, point, pdwEffect);
    return NOERROR;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropTarget::DragOver(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect)
{
    if (IDropTargetHelper* dh = QWindowsDrag::instance()->dropHelper())
        dh->DragOver(reinterpret_cast<POINT*>(&pt), *pdwEffect);

    qCDebug(lcQpaMime) << __FUNCTION__ << "m_window" << m_window << "key=" << grfKeyState
        << "pt=" << pt.x << pt.y;
    const QPoint tmpPoint = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x,pt.y));
    // see if we should compress this event
    if ((tmpPoint == m_lastPoint || m_answerRect.contains(tmpPoint))
        && m_lastKeyState == grfKeyState) {
        *pdwEffect = m_chosenEffect;
        qCDebug(lcQpaMime) << __FUNCTION__ << "compressed event";
        return NOERROR;
    }

    handleDrag(m_window, grfKeyState, tmpPoint, pdwEffect);
    return NOERROR;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropTarget::DragLeave()
{
    if (IDropTargetHelper* dh = QWindowsDrag::instance()->dropHelper())
        dh->DragLeave();

    qCDebug(lcQpaMime) << __FUNCTION__ << ' ' << m_window;

    lastModifiers = QWindowsKeyMapper::queryKeyboardModifiers();
    lastButtons = QWindowsMouseHandler::queryMouseButtons();

    QWindowSystemInterface::handleDrag(m_window, nullptr, QPoint(), Qt::IgnoreAction,
                                       Qt::NoButton, Qt::NoModifier);

    if (!QDragManager::self()->source())
        m_lastKeyState = 0;
    QWindowsDrag::instance()->releaseDropDataObject();

    return NOERROR;
}

#define KEY_STATE_BUTTON_MASK (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON)

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP
QWindowsOleDropTarget::Drop(LPDATAOBJECT pDataObj, DWORD grfKeyState,
                            POINTL pt, LPDWORD pdwEffect)
{
    if (IDropTargetHelper* dh = QWindowsDrag::instance()->dropHelper())
        dh->Drop(pDataObj, reinterpret_cast<POINT*>(&pt), *pdwEffect);

    qCDebug(lcQpaMime) << __FUNCTION__ << ' ' << m_window
        << "keys=" << grfKeyState << "pt=" << pt.x << ',' << pt.y;

    m_lastPoint = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x,pt.y));

    QWindowsDrag *windowsDrag = QWindowsDrag::instance();

    lastModifiers = toQtKeyboardModifiers(grfKeyState);
    lastButtons = toQtMouseButtons(grfKeyState);

    const QPlatformDropQtResponse response =
        QWindowSystemInterface::handleDrop(m_window, windowsDrag->dropData(),
                                           m_lastPoint,
                                           translateToQDragDropActions(*pdwEffect),
                                           lastButtons,
                                           lastModifiers);

    m_lastKeyState = grfKeyState;

    if (response.isAccepted()) {
        const Qt::DropAction action = response.acceptedAction();
        if (action == Qt::MoveAction || action == Qt::TargetMoveAction) {
            if (action == Qt::MoveAction)
                m_chosenEffect = DROPEFFECT_MOVE;
            else
                m_chosenEffect = DROPEFFECT_COPY;
            HGLOBAL hData = GlobalAlloc(0, sizeof(DWORD));
            if (hData) {
                DWORD *moveEffect = reinterpret_cast<DWORD *>(GlobalLock(hData));
                *moveEffect = DROPEFFECT_MOVE;
                GlobalUnlock(hData);
                STGMEDIUM medium;
                memset(&medium, 0, sizeof(STGMEDIUM));
                medium.tymed = TYMED_HGLOBAL;
                medium.hGlobal = hData;
                FORMATETC format;
                format.cfFormat = CLIPFORMAT(RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT));
                format.tymed = TYMED_HGLOBAL;
                format.ptd = nullptr;
                format.dwAspect = 1;
                format.lindex = -1;
                windowsDrag->dropDataObject()->SetData(&format, &medium, true);
            }
        } else {
            m_chosenEffect = translateToWinDragEffects(action);
        }
    } else {
        m_chosenEffect = DROPEFFECT_NONE;
    }
    *pdwEffect = m_chosenEffect;

    windowsDrag->releaseDropDataObject();
    return NOERROR;
}

//---------------------------------------------------------------------
//                    QWindowsOleDropTargetEx
//---------------------------------------------------------------------

QWindowsOleDropTargetEx::QWindowsOleDropTargetEx(QWindow *w) : QWindowsOleDropTarget(w)
{
    m_bUseDropDescription = false;
    m_bDescriptionUpdated = false;
    m_bHasDragImage = false;
    m_bTextAllowed = false;
    m_nDropEffects = DROPEFFECT_NONE;
    m_DropDescription = nullptr;
    m_bCanShowDescription = false;

    if (QSysInfo::windowsVersion() >= QSysInfo::WV_VISTA) {
#if defined(IID_PPV_ARGS)
        ::CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&m_pDropTargetHelper));
#else
        ::CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_IDropTargetHelper,
                           reinterpret_cast<LPVOID *>(&m_pDropTargetHelper));
#endif
        m_bCanShowDescription = (m_pDropTargetHelper != NULL);
        if (m_bCanShowDescription) {
#if (WINVER < 0x0600)
            OSVERSIONINFOEX VerInfo;
            DWORDLONG dwlConditionMask = 0;
            ::ZeroMemory(&VerInfo, sizeof(OSVERSIONINFOEX));
            VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
            VerInfo.dwMajorVersion = 6;
            VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
            m_bCanShowDescription =
                    ::VerifyVersionInfo(&VerInfo, VER_MAJORVERSION, dwlConditionMask)
                    && QDragDropHelper::IsAppThemed();
#else
            m_bCanShowDescription = (0 != QDragDropHelper::isAppThemed());
#endif
        }
        if (m_bCanShowDescription) {
            m_DropDescription = new QDropDescription;
            QString defaultDescText = QStringLiteral("open with %1");
            AddDropDescriptionText(DROPIMAGE_COPY, QString(),defaultDescText);
            AddDropDescriptionText(DROPIMAGE_MOVE, QString(),defaultDescText);
        }
    }
}

QWindowsOleDropTargetEx::~QWindowsOleDropTargetEx()
{
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->Release();
    if (m_DropDescription)
        delete m_DropDescription;
}

void QWindowsOleDropTargetEx::ClearStateFlags()
{
    m_bDescriptionUpdated = false;
    m_bHasDragImage = false;
    m_bTextAllowed = false;
    m_nDropEffects = DROPEFFECT_NONE;
    m_nPreferredDropEffect = DROPEFFECT_NONE;
}

//---------------------------------------------------------------------
//                    IDropTarget Methods
//---------------------------------------------------------------------

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP QWindowsOleDropTargetEx::DragEnter(LPDATAOBJECT pDataObj,
                                                                         DWORD grfKeyState,
                                                                         POINTL pt,
                                                                         LPDWORD pdwEffect)
{
    QDrag::setDropDescriptionInsert(QString());
    ClearStateFlags();
    m_nDropEffects = *pdwEffect;

    QWindowsDrag::instance()->setDropDataObject(pDataObj);
    pDataObj->AddRef();

   // sendDragEnterEvent(m_window, grfKeyState, pt, pdwEffect);
    const QPoint point = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x,pt.y));
    handleDrag(m_window, grfKeyState, point, pdwEffect);

    POINT pt1 = { pt.x, pt.y };
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragEnter(NULL, pDataObj, &pt1, m_chosenEffect);

    m_bHasDragImage = (0 != QDragDropHelper::GetGlobalDataDWord(pDataObj, _T("DragWindow")));
    if (m_bHasDragImage && m_bCanShowDescription)
        m_bTextAllowed =
                (DSH_ALLOWDROPDESCRIPTIONTEXT
                 == QDragDropHelper::GetGlobalDataDWord(pDataObj, _T("DragSourceHelperFlags")));
    m_nPreferredDropEffect =
            QDragDropHelper::GetGlobalDataDWord(pDataObj, CFSTR_PREFERREDDROPEFFECT);

    QString descText = QDrag::getDropDescriptionText();
    if (descText.length())
    {
        AddDropDescriptionText(DROPIMAGE_MOVE, QString(), descText);
        AddDropDescriptionText(DROPIMAGE_COPY, QString(), descText);
    }

    QString insertText = QDrag::getDropDescriptionInsert();
    if (insertText.length())
    {
        AddDropInsertText(insertText);
        QDrag::setDropDescriptionInsert(QString());
    } else
        ClearDropDescription();

    if (m_bUseDropDescription && !m_bDescriptionUpdated)
        SetDropDescription(m_chosenEffect);

    return NOERROR;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP QWindowsOleDropTargetEx::DragOver(DWORD grfKeyState,
                                                                        POINTL pt,
                                                                        LPDWORD pdwEffect)
{
    m_bDescriptionUpdated = false;
    QString descText = QDrag::getDropDescriptionText();
    if (descText.length()) {
        AddDropDescriptionText(DROPIMAGE_MOVE, QString(), descText);
        AddDropDescriptionText(DROPIMAGE_COPY, QString(), descText);
    }

    QString insertText = QDrag::getDropDescriptionInsert();
    if (insertText.length()) {
        AddDropInsertText(insertText);
        QDrag::setDropDescriptionInsert(QString());
    } else
        ClearDropDescription();

    if (m_bUseDropDescription && !m_bDescriptionUpdated)
        SetDropDescription(*pdwEffect);

    POINT ptPoint = { pt.x, pt.y };
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragOver(&ptPoint, *pdwEffect);

    const QPoint tmpPoint = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x, pt.y));
    handleDrag(m_window, grfKeyState, tmpPoint, pdwEffect);
    return NOERROR;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP QWindowsOleDropTargetEx::DragLeave()
{
    __super::DragLeave();
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->DragLeave();
    ClearStateFlags();

    return NOERROR;
}

QT_ENSURE_STACK_ALIGNED_FOR_SSE STDMETHODIMP QWindowsOleDropTargetEx::Drop(LPDATAOBJECT pDataObj,
                                                                    DWORD grfKeyState, POINTL pt,
                                                                    LPDWORD pdwEffect)
{
    POINT ptPoint = { pt.x, pt.y };
    if (m_pDropTargetHelper)
        m_pDropTargetHelper->Drop(pDataObj, &ptPoint, m_chosenEffect);

    m_lastPoint = QWindowsGeometryHint::mapFromGlobal(m_window, QPoint(pt.x,pt.y));

    QWindowsDrag *windowsDrag = QWindowsDrag::instance();

    lastModifiers = toQtKeyboardModifiers(grfKeyState);
    lastButtons = toQtMouseButtons(grfKeyState);

    Qt::DropActions supportedDropActions = (DROPEFFECT_NONE == m_chosenEffect) 
        ? translateToQDragDropActions(*pdwEffect) : (Qt::DropAction)m_chosenEffect;
    const QPlatformDropQtResponse response =
        QWindowSystemInterface::handleDrop(m_window, windowsDrag->dropData(),
                                           m_lastPoint,
                                           supportedDropActions,
                                           lastButtons,
                                           lastModifiers);

    m_lastKeyState = grfKeyState;
    if (response.isAccepted()) {
        const Qt::DropAction action = response.acceptedAction();
        if (action == Qt::MoveAction || action == Qt::TargetMoveAction) {
            if (action == Qt::MoveAction)
                m_chosenEffect = DROPEFFECT_MOVE;
            else
                m_chosenEffect = DROPEFFECT_COPY;
            HGLOBAL hData = GlobalAlloc(0, sizeof(DWORD));
            if (hData) {
                DWORD *moveEffect = (DWORD *)GlobalLock(hData);
                ;
                *moveEffect = DROPEFFECT_MOVE;
                GlobalUnlock(hData);
                STGMEDIUM medium;
                memset(&medium, 0, sizeof(STGMEDIUM));
                medium.tymed = TYMED_HGLOBAL;
                medium.hGlobal = hData;
                FORMATETC format;
                format.cfFormat = RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT);
                format.tymed = TYMED_HGLOBAL;
                format.ptd = 0;
                format.dwAspect = 1;
                format.lindex = -1;
                windowsDrag->dropDataObject()->SetData(&format, &medium, true);
            }
        } else {
            m_chosenEffect = translateToWinDragEffects(action);
        }
    } else {
        m_chosenEffect = DROPEFFECT_NONE;
    }

    *pdwEffect = DROPEFFECT_NONE;
    windowsDrag->releaseDropDataObject();
    return NOERROR;
    // We won't get any mouserelease-event, so manually adjust qApp state:
    ///### test this        QApplication::winMouseButtonUp();
}

bool QWindowsOleDropTargetEx::AddDropDescriptionText(DWORD nType, 
                                                     QString qsText,
                                                     QString qsTextMirror,
                                                     QString qsInsert/* = QString()*/)
{
    bool bRet = false;
    if (m_bCanShowDescription) // Vista or later and themes are enabled
    {
        bRet = m_DropDescription->SetDescText((DROPIMAGETYPE)nType, qsText, qsTextMirror);
        if (bRet && qsInsert.length())
            m_DropDescription->SetInsert(qsInsert);
        m_bUseDropDescription |= bRet; // drop description text is available
    }
    return bRet;
}

bool QWindowsOleDropTargetEx::AddDropInsertText(QString qsInsertText)
{
    if (m_bCanShowDescription)
        m_DropDescription->SetInsert(qsInsertText);

    return m_bCanShowDescription;
}

bool QWindowsOleDropTargetEx::SetDropDescription(DROPEFFECT dwEffect)
{
    bool bHasDescription = false;
    if (m_bTextAllowed && m_DropDescription) {
        DROPIMAGETYPE nType =
                static_cast<DROPIMAGETYPE>(FilterDropEffect(dwEffect & ~DROPEFFECT_SCROLL));
        QString qsDescText = m_DropDescription->GetDescText(nType, m_DropDescription->HasInsert());
        if (qsDescText.length())
            bHasDescription = SetDropDescription(nType, qsDescText, true);
        else
            bHasDescription = ClearDropDescription();
    }
    m_bDescriptionUpdated = true;
    return bHasDescription;
}

bool QWindowsOleDropTargetEx::SetDropDescription(DWORD nImageType, QString qsDescText, bool bCreate)
{
    QWindowsDrag *windowsDrag = QWindowsDrag::instance();
    IDataObject *pDataObj = windowsDrag->dropDataObject();
    bool bHasDescription = false;
    if (m_bHasDragImage && m_bCanShowDescription) {
        STGMEDIUM StgMedium = { 0 };
        FORMATETC FormatEtc = { 0 };
        if (QDragDropHelper::GetGlobalData(pDataObj, CFSTR_DROPDESCRIPTION, FormatEtc, StgMedium)) {
            bHasDescription = true;
            bool bUpdate = false;
            DROPDESCRIPTION *pDropDescription =
                    static_cast<DROPDESCRIPTION *>(::GlobalLock(StgMedium.hGlobal));
            if (DROPIMAGE_INVALID == nImageType) {
                QDragDropHelper::ClearDescription(pDropDescription);
                bUpdate = true;
            } else if (m_bTextAllowed) {
                if (qsDescText.isEmpty())
                    qsDescText = m_DropDescription->GetDescText((DROPIMAGETYPE)nImageType,
                                                          m_DropDescription->HasInsert());
                m_DropDescription->SetDescription(pDropDescription, qsDescText);
                bUpdate = true;     
            }
            if (pDropDescription->type != nImageType) {
                bUpdate = true;
                pDropDescription->type = (DROPIMAGETYPE)nImageType;
            }
            ::GlobalUnlock(StgMedium.hGlobal);
            if (!bUpdate || (S_OK != pDataObj->SetData(&FormatEtc, &StgMedium, TRUE))) {
                ::ReleaseStgMedium(&StgMedium);
            }
        }

        if (!bHasDescription && bCreate && DROPIMAGE_INVALID != nImageType
            && (m_bTextAllowed || nImageType > DROPIMAGE_LINK)) {
            StgMedium.tymed = TYMED_HGLOBAL;
            StgMedium.hGlobal =
                    ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPDESCRIPTION));
            StgMedium.pUnkForRelease = NULL;
            if (StgMedium.hGlobal) {
                DROPDESCRIPTION *pDropDescription =
                        static_cast<DROPDESCRIPTION *>(::GlobalLock(StgMedium.hGlobal));
                pDropDescription->type = (DROPIMAGETYPE)nImageType;
                if (m_bTextAllowed) {
                    if (qsDescText.isEmpty())
                        qsDescText = m_DropDescription->GetDescText((DROPIMAGETYPE)nImageType,
                                                              m_DropDescription->HasInsert());
                    m_DropDescription->SetDescription(pDropDescription, qsDescText);
                }
                ::GlobalUnlock(StgMedium.hGlobal);
                bHasDescription = (S_OK == pDataObj->SetData(&FormatEtc, &StgMedium, TRUE));
                if (!bHasDescription)
                    ::GlobalFree(StgMedium.hGlobal);
            }
        }
    }

    m_bDescriptionUpdated = true;
    return bHasDescription;
}

bool QWindowsOleDropTargetEx::ClearDropDescription()
{
    return SetDropDescription(DROPIMAGE_INVALID, QString(), false);
}

DROPEFFECT QWindowsOleDropTargetEx::FilterDropEffect(DROPEFFECT dropEffect) const
{
    DROPEFFECT dropScroll = dropEffect & DROPEFFECT_SCROLL;
    dropEffect &= ~DROPEFFECT_SCROLL;
    if (dropEffect) {
        dropEffect &= m_nDropEffects;
        if (dropEffect & m_nPreferredDropEffect)
            dropEffect &= m_nPreferredDropEffect;
        if (0 == dropEffect) {
            dropEffect = m_nPreferredDropEffect & m_nDropEffects;
            if (0 == dropEffect)
                dropEffect = m_nDropEffects;
        }
        if (dropEffect & DROPEFFECT_MOVE)
            dropEffect = DROPEFFECT_MOVE;
        else if (dropEffect & DROPEFFECT_COPY)
            dropEffect = DROPEFFECT_COPY;
        else if (dropEffect & DROPEFFECT_LINK)
            dropEffect = DROPEFFECT_LINK;
    }
    return dropEffect | dropScroll;
}

/*!
    \class QWindowsDrag
    \brief Windows drag implementation.
    \internal
    \ingroup qt-lighthouse-win
*/

bool QWindowsDrag::m_canceled = false;
bool QWindowsDrag::m_dragging = false;

QWindowsDrag::QWindowsDrag() = default;

QWindowsDrag::~QWindowsDrag()
{
    if (m_cachedDropTargetHelper)
        m_cachedDropTargetHelper->Release();
}

/*!
    \brief Return data for a drop in process. If it stems from a current drag, use a shortcut.
*/

QMimeData *QWindowsDrag::dropData()
{
    if (const QDrag *drag = currentDrag())
        return drag->mimeData();
    return &m_dropData;
}

/*!
    \brief May be used to handle extended cursors functionality for drags from outside the app.
*/
IDropTargetHelper* QWindowsDrag::dropHelper() {
#if 0
    // removed, no need
    if (!m_cachedDropTargetHelper) {
        CoCreateInstance(CLSID_DragDropHelper, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IDropTargetHelper,
                         reinterpret_cast<void**>(&m_cachedDropTargetHelper));
    }
#endif
    return m_cachedDropTargetHelper;
}

Qt::DropAction QWindowsDrag::drag(QDrag *drag)
{
    // TODO: Accessibility handling?
    QMimeData *dropData = drag->mimeData();
    Qt::DropAction dragResult = Qt::IgnoreAction;

    DWORD resultEffect;
    QWindowsDrag::m_canceled = false;
    QWindowsOleDropSource *windowDropSource = nullptr;
    QWindowsOleDataObject *dropDataObject = nullptr;
    if (drag->getUseDropDescription() &&
        (QOperatingSystemVersion::current() >= QOperatingSystemVersion::WindowsVista)) {
        QWindowsOleDataObjectEx* pdropDataObject = new QWindowsOleDataObjectEx(dropData);
        windowDropSource = new QWindowsOleDropSourceEx(this, pdropDataObject);
        dropDataObject = pdropDataObject;
    } else {
        windowDropSource = new QWindowsOleDropSource(this);
        dropDataObject = new QWindowsDropDataObject(dropData);
    }

    windowDropSource->createCursors();

    const Qt::DropActions possibleActions = drag->supportedActions();
    const DWORD allowedEffects = translateToWinDragEffects(possibleActions);
    qCDebug(lcQpaMime) << '>' << __FUNCTION__ << "possible Actions=0x"
        << hex << int(possibleActions) << "effects=0x" << allowedEffects << dec;
    // Indicate message handlers we are in DoDragDrop() event loop.
    QWindowsDrag::m_dragging = true;
    const HRESULT r = DoDragDrop(dropDataObject, windowDropSource, allowedEffects, &resultEffect);
    QWindowsDrag::m_dragging = false;
    const DWORD  reportedPerformedEffect = dropDataObject->reportedPerformedEffect();
    if (r == DRAGDROP_S_DROP) {
        if (reportedPerformedEffect == DROPEFFECT_MOVE && resultEffect != DROPEFFECT_MOVE) {
            dragResult = Qt::TargetMoveAction;
            resultEffect = DROPEFFECT_MOVE;
        } else {
            dragResult = translateToQDragDropAction(resultEffect);
        }
        // Force it to be a copy if an unsupported operation occurred.
        // This indicates a bug in the drop target.
        if (resultEffect != DROPEFFECT_NONE && !(resultEffect & allowedEffects)) {
            qWarning("%s: Forcing Qt::CopyAction", __FUNCTION__);
            dragResult = Qt::CopyAction;
        }
    }

    // after drag finished, we must reset mouse_buttons status, otherwise, ...
    // we miss to clean the status.
    QGuiApplicationPrivate::mouse_buttons = Qt::NoButton;
    if (GetAsyncKeyState(VK_LBUTTON))
        QGuiApplicationPrivate::mouse_buttons |= Qt::LeftButton;
    if (GetAsyncKeyState(VK_RBUTTON))
        QGuiApplicationPrivate::mouse_buttons |= Qt::RightButton;
    if (GetAsyncKeyState(VK_MBUTTON))
        QGuiApplicationPrivate::mouse_buttons |= Qt::MidButton;

    // clean up
    dropDataObject->releaseQt();
    dropDataObject->Release();        // Will delete obj if refcount becomes 0
    windowDropSource->Release();        // Will delete src if refcount becomes 0
    qCDebug(lcQpaMime) << '<' << __FUNCTION__ << hex << "allowedEffects=0x" << allowedEffects
        << "reportedPerformedEffect=0x" << reportedPerformedEffect
        <<  " resultEffect=0x" << resultEffect << "hr=0x" << int(r) << dec << "dropAction=" << dragResult;
    return dragResult;
}

QWindowsDrag *QWindowsDrag::instance()
{
    return static_cast<QWindowsDrag *>(QWindowsIntegration::instance()->drag());
}

void QWindowsDrag::releaseDropDataObject()
{
    qCDebug(lcQpaMime) << __FUNCTION__ << m_dropDataObject;
    if (m_dropDataObject) {
        m_dropDataObject->Release();
        m_dropDataObject = nullptr;
    }
}

QT_END_NAMESPACE
