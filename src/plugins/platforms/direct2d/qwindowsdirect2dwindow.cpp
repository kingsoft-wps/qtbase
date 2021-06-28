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
#include "qwindowsdirect2dbitmap.h"
#include "qwindowsdirect2dwindow.h"
#include "qwindowsdirect2ddevicecontext.h"
#include "qwindowsdirect2dhelpers.h"
#include "qwindowsdirect2dplatformpixmap.h"

#include <QtGui/qguiapplication.h>

#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>

using Microsoft::WRL::ComPtr;

QT_BEGIN_NAMESPACE

void enableDirect2DBrushCache(ID2D1DeviceContext*);
void cleanDirect2DBrushCache(ID2D1DeviceContext*);
void clearDirect2DBrushCache(ID2D1DeviceContext*);

QWindowsDirect2DWindow::QWindowsDirect2DWindow(QWindow *window, const QWindowsWindowData &data, bool directRenderingEnable)
    : QWindowsWindow(window, data)
    , m_isTranslucent(data.flags & Qt::FramelessWindowHint && window->format().hasAlpha())
    , m_directRenderingEnabled(directRenderingEnable)
{
    if (window->type() == Qt::Desktop)
        return; // No further handling for Qt::Desktop

    if (isDirectRendering())
        setupSwapChain();

    HRESULT hr = QWindowsDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                m_deviceContext.GetAddressOf());
    if (FAILED(hr))
        qWarning("%s: Couldn't create Direct2D Device context: %#lx", __FUNCTION__, hr);
    if (!this->window()->property("_q_platform_Direct2DDisableBrushCache").toBool())
        enableDirect2DBrushCache(m_deviceContext.Get());
}

QWindowsDirect2DWindow::~QWindowsDirect2DWindow()
{
    if (!this->window()->property("_q_platform_Direct2DDisableBrushCache").toBool())
        cleanDirect2DBrushCache(m_deviceContext.Get());
}

void QWindowsDirect2DWindow::updateDirectRendering(Qt::WindowFlags flags, const QSurfaceFormat& format)
{
    m_isTranslucent = (flags & Qt::FramelessWindowHint && format.hasAlpha());

    if (!isDirectRendering())
        m_swapChain.Reset(); // No need for the swap chain; release from memory
    else if (!m_swapChain)
        setupSwapChain();
}


bool QWindowsDirect2DWindow::isDirectRendering() const
{
    return m_directRenderingEnabled && !m_isTranslucent;
}

void QWindowsDirect2DWindow::setWindowFlags(Qt::WindowFlags flags)
{
    updateDirectRendering(flags, window()->format());

    QWindowsWindow::setWindowFlags(flags);
}

void QWindowsDirect2DWindow::setFormat(const QSurfaceFormat& format)
{
    updateDirectRendering(windowFlags(), format);

    QWindowsWindow::setFormat(format);
}

bool QWindowsDirect2DWindow::isPixmapAvaliable() const
{
    return m_pixmap && !m_pixmap->isNull();
}


bool QWindowsDirect2DWindow::isTranslucent() const
{
    return m_isTranslucent;
}

QPixmap *QWindowsDirect2DWindow::pixmap()
{
    setupBitmap();

    return m_pixmap.data();
}


QWindowsDirect2DBitmap* QWindowsDirect2DWindow::bitmap() const
{
    Q_ASSERT(m_bitmap);
    return m_bitmap.data();
}

void QWindowsDirect2DWindow::flush(QWindowsDirect2DBitmap *bitmap, const QRegion &region, const QPoint &offset)
{
    if (!bitmap)
        return;

    if (!m_directRenderingEnabled) {
        qWarning("QWindowsDirect2DWindow::flush is not expected to call when direct rendering is disabled");
        return;
    }

    QSize size;
    if (!m_isTranslucent) {
        DXGI_SWAP_CHAIN_DESC1 desc;
        HRESULT hr = m_swapChain->GetDesc1(&desc);
        QRect geom = geometry();

        if (FAILED(hr) || (desc.Width != UINT(geom.width()) || (desc.Height != UINT(geom.height())))) {
            resizeSwapChain(geom.size());
            m_swapChain->GetDesc1(&desc);
        }
        size.setWidth(int(desc.Width));
        size.setHeight(int(desc.Height));
    } else {
        size = geometry().size();
    }

    setupBitmap();
    if (!m_bitmap)
        return;

    if (bitmap != m_bitmap.data()) {
        m_bitmap->deviceContext()->begin();

        ID2D1DeviceContext *dc = m_bitmap->deviceContext()->get();
        if (!m_needsFullFlush) {
            QRegion clipped = region;
            clipped &= QRect(QPoint(), size);

            for (const QRect &rect : clipped) {
                QRectF rectF(rect);
                dc->DrawBitmap(bitmap->bitmap(),
                               to_d2d_rect_f(rectF),
                               1.0,
                               D2D1_INTERPOLATION_MODE_LINEAR,
                               to_d2d_rect_f(rectF.translated(offset.x(), offset.y())));
            }
        } else {
            QRectF rectF(QPoint(), size);
            dc->DrawBitmap(bitmap->bitmap(),
                           to_d2d_rect_f(rectF),
                           1.0,
                           D2D1_INTERPOLATION_MODE_LINEAR,
                           to_d2d_rect_f(rectF.translated(offset.x(), offset.y())));
            m_needsFullFlush = false;
        }

        m_bitmap->deviceContext()->end();
    }
}

void QWindowsDirect2DWindow::present(const QRegion &region)
{
    if (!m_directRenderingEnabled) {
        qWarning("QWindowsDirect2DWindow::present is not expected to call when direct rendering is disabled");
        return;
    }

    if (!m_isTranslucent) {
        HRESULT hr = m_swapChain->Present(0, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            QGuiApplication::postEvent(QGuiApplication::instance(),
                                       new QEvent(QEvent::GpuDeviceLost), Qt::HighEventPriority);
        return;
    }

    ComPtr<IDXGISurface> bitmapSurface;
    HRESULT hr = m_bitmap->bitmap()->GetSurface(&bitmapSurface);
    Q_ASSERT(SUCCEEDED(hr));
    ComPtr<IDXGISurface1> dxgiSurface;
    hr = bitmapSurface.As(&dxgiSurface);
    Q_ASSERT(SUCCEEDED(hr));

    HDC hdc;
    hr = dxgiSurface->GetDC(FALSE, &hdc);
    if (FAILED(hr)) {
        qErrnoWarning(hr, "Failed to get DC for presenting the surface");
        return;
    }

    const QRect bounds = window()->geometry();
    const SIZE size = { bounds.width(), bounds.height() };
    const POINT ptDst = { bounds.x(), bounds.y() };
    const POINT ptSrc = { 0, 0 };
    const BLENDFUNCTION blend = { AC_SRC_OVER, 0, BYTE(255.0 * opacity()), AC_SRC_ALPHA };
    const QRect r = region.boundingRect();
    const RECT dirty = { r.left(), r.top(), r.left() + r.width(), r.top() + r.height() };
    UPDATELAYEREDWINDOWINFO info = { sizeof(UPDATELAYEREDWINDOWINFO), nullptr,
                                     &ptDst, &size, hdc, &ptSrc, 0, &blend, ULW_ALPHA, &dirty };
    if (!UpdateLayeredWindowIndirect(handle(), &info))
        qErrnoWarning(int(GetLastError()), "Failed to update the layered window");

    hr = dxgiSurface->ReleaseDC(&RECT{0, 0, 0, 0});
    if (FAILED(hr))
        qErrnoWarning(hr, "Failed to release the DC for presentation");
}

void QWindowsDirect2DWindow::setupSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};

    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    HRESULT hr = QWindowsDirect2DContext::instance()->dxgiFactory()->CreateSwapChainForHwnd(
                QWindowsDirect2DContext::instance()->d3dDevice(), // [in]   IUnknown *pDevice
                handle(),                                         // [in]   HWND hWnd
                &desc,                                            // [in]   const DXGI_SWAP_CHAIN_DESC1 *pDesc
                nullptr,                                          // [in]   const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
                nullptr,                                          // [in]   IDXGIOutput *pRestrictToOutput
                m_swapChain.ReleaseAndGetAddressOf());            // [out]  IDXGISwapChain1 **ppSwapChain

    if (!QWindowsDirect2DContext::instance()->messageHookEnabled()) {
        QWindowsDirect2DContext::instance()->dxgiFactory()->MakeWindowAssociation(
            handle(), DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
    }

    if (FAILED(hr))
        qWarning("%s: Could not create swap chain: %#lx", __FUNCTION__, hr);

    m_needsFullFlush = true;
}

void QWindowsDirect2DWindow::resizeSwapChain(const QSize &size)
{
    m_pixmap.reset();
    m_bitmap.reset();
    m_deviceContext->SetTarget(nullptr);
    m_needsFullFlush = true;

    if (!m_swapChain)
        return;

    HRESULT hr = m_swapChain->ResizeBuffers(0,
                                            UINT(size.width()), UINT(size.height()),
                                            DXGI_FORMAT_UNKNOWN,
                                            0);
    if (FAILED(hr)) {
        qWarning("%s: Could not resize swap chain: %#lx", __FUNCTION__, hr);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            QGuiApplication::postEvent(QGuiApplication::instance(),
                                       new QEvent(QEvent::GpuDeviceLost),
                                       Qt::HighEventPriority);
    }
}

void QWindowsDirect2DWindow::resetDeviceDependentResources()
{
    m_bitmap.reset();
    m_pixmap.reset();

    if (isDirectRendering())
    {
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        m_swapChain.Swap(swapChain);
        setupSwapChain();
    }

    {
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> devContext;
        m_deviceContext.Swap(devContext);
        HRESULT hr = QWindowsDirect2DContext::instance()->d2dDevice()->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_deviceContext.GetAddressOf());
        if (FAILED(hr))
            qWarning("%s: Couldn't create Direct2D Device context: %#lx", __FUNCTION__, hr);
    }

    if (!this->window()->property("_q_platform_Direct2DDisableBrushCache").toBool())
        clearDirect2DBrushCache(m_deviceContext.Get());

    setupBitmap();
}

QSharedPointer<QWindowsDirect2DBitmap> QWindowsDirect2DWindow::copyBackBuffer() const
{
    const QSharedPointer<QWindowsDirect2DBitmap> null_result;

    if (!m_bitmap)
        return null_result;

    D2D1_PIXEL_FORMAT format = m_bitmap->bitmap()->GetPixelFormat();
    D2D1_SIZE_U size = m_bitmap->bitmap()->GetPixelSize();

    FLOAT dpiX, dpiY;
    m_bitmap->bitmap()->GetDpi(&dpiX, &dpiY);

    D2D1_BITMAP_PROPERTIES1 properties = {
        format,                     // D2D1_PIXEL_FORMAT pixelFormat;
        dpiX,                       // FLOAT dpiX;
        dpiY,                       // FLOAT dpiY;
        D2D1_BITMAP_OPTIONS_TARGET, // D2D1_BITMAP_OPTIONS bitmapOptions;
        nullptr                   // _Field_size_opt_(1) ID2D1ColorContext *colorContext;
    };
    ComPtr<ID2D1Bitmap1> copy;
    HRESULT hr = m_deviceContext.Get()->CreateBitmap(size, nullptr, 0, properties, &copy);

    if (FAILED(hr)) {
        qWarning("%s: Could not create staging bitmap: %#lx", __FUNCTION__, hr);
        return null_result;
    }

    hr = copy.Get()->CopyFromBitmap(nullptr, m_bitmap->bitmap(), nullptr);
    if (FAILED(hr)) {
        qWarning("%s: Could not copy from bitmap! %#lx", __FUNCTION__, hr);
        return null_result;
    }

    return QSharedPointer<QWindowsDirect2DBitmap>(new QWindowsDirect2DBitmap(copy.Get(), nullptr));
}

void QWindowsDirect2DWindow::setupBitmap()
{
    if (m_bitmap)
        return;

    if (!m_deviceContext)
        return;

    bool directRendering = isDirectRendering();
    if (directRendering && !m_swapChain)
        return;

    HRESULT hr;
    ComPtr<IDXGISurface1> backBufferSurface;
    if (directRendering) {
        hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBufferSurface));
        if (FAILED(hr)) {
            qWarning("%s: Could not query backbuffer for DXGI Surface: %#lx", __FUNCTION__, hr);
            return;
        }
    } else {
        const QRect rect = geometry();
        CD3D11_TEXTURE2D_DESC backBufferDesc(DXGI_FORMAT_B8G8R8A8_UNORM, UINT(rect.width()), UINT(rect.height()), 1, 1);
        backBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        backBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
        ComPtr<ID3D11Texture2D> backBufferTexture;
        HRESULT hr = QWindowsDirect2DContext::instance()->d3dDevice()->CreateTexture2D(&backBufferDesc, nullptr, &backBufferTexture);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to create backing texture for indirect rendering");
            return;
        }

        hr = backBufferTexture.As(&backBufferSurface);
        if (FAILED(hr)) {
            qErrnoWarning(hr, "Failed to cast back buffer surface to DXGI surface");
            return;
        }
    }

    ComPtr<ID2D1Bitmap1> backBufferBitmap;
    hr = m_deviceContext->CreateBitmapFromDxgiSurface(backBufferSurface.Get(), nullptr, backBufferBitmap.GetAddressOf());
    if (FAILED(hr)) {
        qWarning("%s: Could not create Direct2D Bitmap from DXGI Surface: %#lx", __FUNCTION__, hr);
        return;
    }

    m_bitmap.reset(new QWindowsDirect2DBitmap(backBufferBitmap.Get(), m_deviceContext.Get()));

    QWindowsDirect2DPaintEngine::Flags flags = QWindowsDirect2DPaintEngine::NoFlag;
    if (!directRendering && m_isTranslucent)
        flags |= QWindowsDirect2DPaintEngine::TranslucentTopLevelWindow;
    QWindowsDirect2DPlatformPixmap *pp = new QWindowsDirect2DPlatformPixmap(QPlatformPixmap::PixmapType,
                                                                            flags,
                                                                            m_bitmap.data());
    m_pixmap.reset(new QPixmap(pp));
}

QT_END_NAMESPACE
