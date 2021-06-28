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

#include "qwindowsdirect2dcontext.h"
#include "qwindowsdirect2dhelpers.h"
#include "qwindowsdirect2ddevicecontext.h"

#include <QtGui/qguiapplication.h>

#include <wrl.h>

using Microsoft::WRL::ComPtr;

QT_BEGIN_NAMESPACE

// The former HRESULT of EndDraw doesn't give a return value that could indicate a device lost error.
// Here we need to try to create a resource via this device context, if it returns D2DERR_RECREATE_TARGET,
// then a device lost error was detected.
static bool isGpuDeviceLost(ID2D1DeviceContext* dc)
{
    Q_ASSERT(dc);
    D2D1_PIXEL_FORMAT format = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
    D2D1_BITMAP_PROPERTIES1 properties = { format, 96.f, 96.f, D2D1_BITMAP_OPTIONS_TARGET,
                                           nullptr };
    D2D1_SIZE_U size = { 1, 1 };
    ComPtr<ID2D1Bitmap1> bmp;
    HRESULT hr = dc->CreateBitmap(size, nullptr, 0, properties, &bmp);
    return hr == D2DERR_RECREATE_TARGET;
}

class QWindowsDirect2DDeviceContextPrivate {
public:
    QWindowsDirect2DDeviceContextPrivate(ID2D1DeviceContext *dc)
        : deviceContext(dc)
    {
        if (!dc) {
            HRESULT hr = QWindowsDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
                        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                        &deviceContext);
            if (Q_UNLIKELY(FAILED(hr)))
                qFatal("%s: Couldn't create Direct2D Device Context: %#lx", __FUNCTION__, hr);
        }

        Q_ASSERT(deviceContext);
        deviceContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    }

    void begin()
    {
        Q_ASSERT(deviceContext);
        Q_ASSERT(refCount >= 0);

        if (refCount == 0)
            deviceContext->BeginDraw();

        refCount++;
    }

    bool end()
    {
        Q_ASSERT(deviceContext);
        Q_ASSERT(refCount > 0);

        bool success = true;
        refCount--;

        if (refCount == 0) {
            D2D1_TAG tag1, tag2;
            HRESULT hr = deviceContext->EndDraw(&tag1, &tag2);

            if (FAILED(hr)) {
                bool deviceLostError = isGpuDeviceLost(deviceContext.Get());
                success = false;
                qWarning("%s: EndDraw failed: %#lx, tag1: %lld, tag2: %lld",
                         __FUNCTION__, long(hr), tag1, tag2);
                if (deviceLostError) {
                    QGuiApplication::postEvent(QGuiApplication::instance(),
                                               new QEvent(QEvent::GpuDeviceLost),
                                               Qt::HighEventPriority);
                }
            }
        }

        return success;
    }

    ComPtr<ID2D1DeviceContext> deviceContext;
    int refCount = 0;
};

QWindowsDirect2DDeviceContext::QWindowsDirect2DDeviceContext(ID2D1DeviceContext *dc)
    : d_ptr(new QWindowsDirect2DDeviceContextPrivate(dc))
{
}

QWindowsDirect2DDeviceContext::~QWindowsDirect2DDeviceContext()
{

}

ID2D1DeviceContext *QWindowsDirect2DDeviceContext::get() const
{
    Q_D(const QWindowsDirect2DDeviceContext);
    Q_ASSERT(d->deviceContext);

    return d->deviceContext.Get();
}

void QWindowsDirect2DDeviceContext::begin()
{
    Q_D(QWindowsDirect2DDeviceContext);
    d->begin();
}

bool QWindowsDirect2DDeviceContext::end()
{
    Q_D(QWindowsDirect2DDeviceContext);
    return d->end();
}

void QWindowsDirect2DDeviceContext::suspend()
{
    Q_D(QWindowsDirect2DDeviceContext);
    if (d->refCount > 0)
        d->deviceContext->EndDraw();
}

void QWindowsDirect2DDeviceContext::resume()
{
    Q_D(QWindowsDirect2DDeviceContext);
    if (d->refCount > 0)
        d->deviceContext->BeginDraw();
}

QWindowsDirect2DDeviceContextSuspender::QWindowsDirect2DDeviceContextSuspender(QWindowsDirect2DDeviceContext *dc)
    : m_dc(dc)
{
    Q_ASSERT(m_dc);
    m_dc->suspend();
}

QWindowsDirect2DDeviceContextSuspender::~QWindowsDirect2DDeviceContextSuspender()
{
    resume();
}

void QWindowsDirect2DDeviceContextSuspender::resume()
{
    if (m_dc) {
        m_dc->resume();
        m_dc = nullptr;
    }
}

QT_END_NAMESPACE
