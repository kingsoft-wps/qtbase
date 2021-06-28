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

#include "qwindowsdirect2dbackingstore.h"
#include "qwindowsdirect2dplatformpixmap.h"
#include "qwindowsdirect2dintegration.h"
#include "qwindowsdirect2dcontext.h"
#include "qwindowsdirect2dpaintdevice.h"
#include "qwindowsdirect2dbitmap.h"
#include "qwindowsdirect2ddevicecontext.h"
#include "qwindowsdirect2dwindow.h"

#include "qwindowscontext.h"

#include <QtGui/qpainter.h>
#include <QtGui/qwindow.h>
#include <QtCore/qdebug.h>

#include <d2d1_1.h>
#include <dxgi1_2.h>

using Microsoft::WRL::ComPtr;

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsDirect2DBackingStore
    \brief Backing store for windows.
    \internal
    \ingroup qt-lighthouse-win
*/

static inline QWindowsDirect2DPlatformPixmap *platformPixmap(QPixmap *p)
{
    return static_cast<QWindowsDirect2DPlatformPixmap *>(p->handle());
}

static inline QWindowsDirect2DBitmap *bitmap(QPixmap *p)
{
    return platformPixmap(p)->bitmap();
}

static inline QWindowsDirect2DWindow *nativeWindow(QWindow *window)
{
    return static_cast<QWindowsDirect2DWindow *>(window->handle());
}

QWindowsDirect2DBackingStore::QWindowsDirect2DBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
{
}

QWindowsDirect2DBackingStore::~QWindowsDirect2DBackingStore()
{
}

bool QWindowsDirect2DBackingStore::beginPaint(const QRegion &region)
{
    QPixmap *pixmap = nativeWindow(window())->pixmap();
    if (!pixmap) {
        nativeWindow(window())->setupBitmap();
        pixmap = nativeWindow(window())->pixmap();
    }
    if (!pixmap)
        return false;

    bitmap(pixmap)->deviceContext()->begin();

    QPainter painter(pixmap);
    QColor clear(Qt::transparent);

    painter.setCompositionMode(QPainter::CompositionMode_Source);

    for (const QRect &r : region)
        painter.fillRect(r, clear);

    return true;
}

void QWindowsDirect2DBackingStore::endPaint()
{
    QPixmap *pm = nativeWindow(window())->pixmap();
    if (!pm)
        return;
    bitmap(pm)->deviceContext()->end();
}

QPaintDevice *QWindowsDirect2DBackingStore::paintDevice()
{
    return nativeWindow(window())->pixmap();
}

void QWindowsDirect2DBackingStore::flush(QWindow *targetWindow, const QRegion &region, const QPoint &offset)
{
    if (targetWindow != window()) {
        QSharedPointer<QWindowsDirect2DBitmap> copy(nativeWindow(window())->copyBackBuffer());
        nativeWindow(targetWindow)->flush(copy.data(), region, offset);
    }

    nativeWindow(targetWindow)->present(region);
}

void QWindowsDirect2DBackingStore::resize(const QSize &size, const QRegion &region)
{
    QPixmap old;
    auto nw = nativeWindow(window());
    if (!region.isEmpty() && nw->isPixmapAvaliable())
        old = nw->pixmap()->copy();

    nw->resizeSwapChain(size);

    if (!old.isNull()) {
        QPixmap *newPixmap = nw->pixmap();
        for (const QRect &rect : region)
            platformPixmap(newPixmap)->copy(old.handle(), rect);
    }
}

QImage QWindowsDirect2DBackingStore::toImage() const
{
    return nativeWindow(window())->pixmap()->toImage();
}

QWindowsDirect2DBackingStoreNoPresenting::QWindowsDirect2DBackingStoreNoPresenting(QWindow* window)
    : QWindowsDirect2DBackingStore(window)
{

}

QWindowsDirect2DBackingStoreNoPresenting::~QWindowsDirect2DBackingStoreNoPresenting()
{
    if (m_surface || m_surfaceHdc)
        qCritical("Flushing is not ended before destroying the backing store");
}


void QWindowsDirect2DBackingStoreNoPresenting::flush(QWindow* targetWindow, const QRegion& region, const QPoint& offset)
{
    if (!m_surfaceHdc)
        return;

    const auto nw = nativeWindow(targetWindow);
    const QRect r = region.boundingRect();
    const HDC windc = nw->getDC();
    if (!nw->isTranslucent()) {
        if (!BitBlt(windc, r.x(), r.y(), r.width(), r.height(), m_surfaceHdc, r.x() + offset.x(), r.y() + offset.y(), SRCCOPY)) {
            const DWORD lastError =
                    GetLastError(); // QTBUG-35926, QTBUG-29716: may fail after lock screen.
            if (lastError != ERROR_SUCCESS && lastError != ERROR_INVALID_HANDLE)
                qErrnoWarning(int(lastError), "%s: BitBlt failed", __FUNCTION__);
        }
    } else {
        const QRect bounds = window()->geometry();
        const SIZE size = { bounds.width(), bounds.height() };
        const POINT ptDst = { bounds.x(), bounds.y() };
        const POINT ptSrc = { offset.x(), offset.y() };
        const BLENDFUNCTION blend = { AC_SRC_OVER, 0, BYTE(255.0 * nw->opacity()), AC_SRC_ALPHA };
        QRect dirtyRect = r;
        if (dirtyRect.x() < 0)
            dirtyRect.moveLeft(0);
        if (dirtyRect.y() < 0)
            dirtyRect.moveTop(0);
        const RECT dirty = { dirtyRect.left(), dirtyRect.top(), dirtyRect.left() + dirtyRect.width(),
                       dirtyRect.top() + dirtyRect.height() };
        UPDATELAYEREDWINDOWINFO info = { sizeof(UPDATELAYEREDWINDOWINFO), nullptr,
                                         &ptDst, &size, m_surfaceHdc, &ptSrc, 0, &blend, ULW_ALPHA, &dirty };
        if (!UpdateLayeredWindowIndirect(nw->handle(), &info))
            qErrnoWarning(int(GetLastError()), "Failed to update the layered window");
    }
    nw->releaseDC();
}

void QWindowsDirect2DBackingStoreNoPresenting::beginFlush()
{
    Q_ASSERT(m_surface == nullptr);
    Q_ASSERT(m_surfaceHdc == 0);

    QWindowsDirect2DBitmap* bmp = nativeWindow(window())->bitmap();
    if (!bmp)
        return;

    ComPtr<IDXGISurface> bitmapSurface;
    HRESULT hr = bmp->bitmap()->GetSurface(&bitmapSurface);
    Q_ASSERT(SUCCEEDED(hr));
    ComPtr<IDXGISurface1> dxgiSurface;
    hr = bitmapSurface.As(&dxgiSurface);
    Q_ASSERT(SUCCEEDED(hr));

    HDC hdc = 0;
    hr = dxgiSurface->GetDC(FALSE, &hdc);
    if (FAILED(hr)) {
        qErrnoWarning(hr, "Failed to get DC for flushing the surface");
        return;
    }

    m_surface.Attach(dxgiSurface.Detach());
    m_surfaceHdc = hdc;
}

void QWindowsDirect2DBackingStoreNoPresenting::endFlush()
{
    if (m_surface) {
        if (m_surfaceHdc) {
            HRESULT hr = m_surface->ReleaseDC(&RECT{0, 0, 0, 0});
            m_surfaceHdc = 0;
        }
        m_surface.Reset();
    }
}

QT_END_NAMESPACE
