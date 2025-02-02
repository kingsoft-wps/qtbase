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

#ifndef QWINDOWSDIRECT2DBACKINGSTORE_H
#define QWINDOWSDIRECT2DBACKINGSTORE_H

#include <QtGui/qpa/qplatformbackingstore.h>
#include <wrl.h>

struct IDXGISurface1;

QT_BEGIN_NAMESPACE

class QWindowsDirect2DWindow;

class QWindowsDirect2DBackingStore : public QPlatformBackingStore
{
    Q_DISABLE_COPY(QWindowsDirect2DBackingStore)

public:
    QWindowsDirect2DBackingStore(QWindow *window);
    ~QWindowsDirect2DBackingStore();

    bool beginPaint(const QRegion &) override;
    void endPaint() override;

    QPaintDevice *paintDevice() override;
    void flush(QWindow *targetWindow, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;

    QImage toImage() const override;
};

class QWindowsDirect2DBackingStoreNoPresenting : public QWindowsDirect2DBackingStore
{
    Q_DISABLE_COPY(QWindowsDirect2DBackingStoreNoPresenting)

public:
    QWindowsDirect2DBackingStoreNoPresenting(QWindow *window);
    ~QWindowsDirect2DBackingStoreNoPresenting();

    void flush(QWindow *targetWindow, const QRegion &region, const QPoint &offset) override;
    void beginFlush() override;
    void endFlush() override;

    void copyImageData(
        QImage& image);

    bool testGetDC();

private:
    Microsoft::WRL::ComPtr<IDXGISurface1> m_surface;
    HDC m_surfaceHdc = 0;

    HDC m_manualHdc = nullptr;
    HBITMAP m_manualBitmap = nullptr;
    uchar *m_manualPixels = nullptr;
    QSize m_manualSize = { 0, 0 };

    static bool m_needManualCopy;
};

QT_END_NAMESPACE

#endif // QWINDOWSDIRECT2DBACKINGSTORE_H
