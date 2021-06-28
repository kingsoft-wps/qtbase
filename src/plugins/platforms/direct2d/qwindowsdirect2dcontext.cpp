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
#include "qwindowsdirect2dintegration.h"
#include "qwindowscontext.h"

#include <d3d11_1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <dwrite.h>

using Microsoft::WRL::ComPtr;

QT_BEGIN_NAMESPACE

static int getNumberOfCores()
{
    auto glpi = QWindowsContext::kernel32dll.getLogicalProcessorInformation;
    if (nullptr == glpi)
        return -1;

    BOOL done = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = nullptr;
    DWORD returnLength = 0;
    while (!done) {
        DWORD rc = glpi(buffer, &returnLength);
        if (FALSE == rc) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                if (buffer)
                    free(buffer);
                buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
                if (nullptr == buffer) {
                    return -2;
                }
            } else {
                return -3;
            }
        } else {
            done = TRUE;
        }
    }
    ptr = buffer;
    DWORD byteOffset = 0;
    int processorCoreCount = 0;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
        switch (ptr->Relationship) {
        case RelationProcessorCore:
            processorCoreCount++;
            break;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    free(buffer);
    return processorCoreCount;
}

static bool isSupportedWarp()
{
    static int numberOfCores = getNumberOfCores();
    const int MinimumPhysicalCoreNumberForWarp = 4;
    return numberOfCores >= MinimumPhysicalCoreNumberForWarp;
}

class QWindowsDirect2DContextPrivate
{
public:
    bool init()
    {
        HRESULT hr = E_FAIL;

        D3D_FEATURE_LEVEL level;

        D3D_DRIVER_TYPE typeAttempts[] = {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP
        };

        // Currently only considering supporting DX11
        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };

        const int ntypes = int(sizeof(typeAttempts) / sizeof(typeAttempts[0]));
        const int startType = useWarp ? 1 : 0;
        const int toType = supportWarp ? ntypes : (ntypes - 1);
        Q_ASSERT(startType < toType);
        for (int i = startType; i < toType; i++) {
            if (typeAttempts[i] == D3D_DRIVER_TYPE_WARP && !isSupportedWarp())
                break;
            hr = D3D11CreateDevice(nullptr,
                                   typeAttempts[i],
                                   nullptr,
                                   D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   featureLevels,
                                   (sizeof(featureLevels) / sizeof(featureLevels[0])),
                                   D3D11_SDK_VERSION,
                                   &d3dDevice,
                                   &level,
                                   &d3dDeviceContext);

            if (SUCCEEDED(hr)) {
                d3dDriverType = typeAttempts[i];
                break;
            }
        }

        if (FAILED(hr)) {
            qWarning("%s: Could not create Direct3D Device: %#lx", __FUNCTION__, hr);
            return false;
        }

        ComPtr<IDXGIDevice1> dxgiDevice;
        ComPtr<IDXGIAdapter> dxgiAdapter;

        hr = d3dDevice.As(&dxgiDevice);
        if (FAILED(hr)) {
            qWarning("%s: DXGI Device interface query failed on D3D Device: %#lx", __FUNCTION__, hr);
            return false;
        }

        // Ensure that DXGI doesn't queue more than one frame at a time.
        dxgiDevice->SetMaximumFrameLatency(1);

        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (FAILED(hr)) {
            qWarning("%s: Failed to probe DXGI Device for parent DXGI Adapter: %#lx", __FUNCTION__, hr);
            return false;
        }

        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) {
            qWarning("%s: Failed to probe DXGI Adapter for parent DXGI Factory: %#lx", __FUNCTION__, hr);
            return false;
        }

        D2D1_FACTORY_OPTIONS options = {};
        if (enableDebug)
            options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;

#ifdef QT_D2D_DEBUG_OUTPUT
        qDebug("Turning on Direct2D debugging messages");
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif // QT_D2D_DEBUG_OUTPUT
        
        auto factoryType = multiThreaded ? D2D1_FACTORY_TYPE_MULTI_THREADED : D2D1_FACTORY_TYPE_SINGLE_THREADED;
        hr = D2D1CreateFactory(factoryType, options, d2dFactory.GetAddressOf());
        if (FAILED(hr)) {
            qWarning("%s: Could not create Direct2D Factory: %#lx", __FUNCTION__, hr);
            return false;
        }

        hr = d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
        if (FAILED(hr)) {
            qWarning("%s: Could not create D2D Device: %#lx", __FUNCTION__, hr);
            return false;
        }

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 static_cast<IUnknown **>(&directWriteFactory));
        if (FAILED(hr)) {
            qWarning("%s: Could not create DirectWrite factory: %#lx", __FUNCTION__, hr);
            return false;
        }

        hr = directWriteFactory->GetGdiInterop(&directWriteGdiInterop);
        if (FAILED(hr)) {
            qWarning("%s: Could not create DirectWrite GDI Interop: %#lx", __FUNCTION__, hr);
            return false;
        }

        return true;
    }

    void reset()
    {
        directWriteGdiInterop.Reset();
        d3dDeviceContext.Reset();
        directWriteFactory.Reset();
        dxgiFactory.Reset();
        d2dFactory.Reset();
        d2dDevice.Reset();
        d3dDevice.Reset();
        init();
    }

    ComPtr<ID3D11Device>  d3dDevice;
    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device>   d2dDevice;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    ComPtr<ID3D11DeviceContext> d3dDeviceContext;
    ComPtr<IDWriteFactory> directWriteFactory;
    ComPtr<IDWriteGdiInterop> directWriteGdiInterop;
    D3D_DRIVER_TYPE d3dDriverType = D3D_DRIVER_TYPE_NULL;
    bool multiThreaded = false;
    bool useWarp = false;
    bool supportWarp = false;
    bool enableDebug = false;
    bool enableMessageHook = true;
};

QWindowsDirect2DContext::QWindowsDirect2DContext(const QStringList &paramList)
    : d_ptr(new QWindowsDirect2DContextPrivate)
{
    for (auto& param : paramList) {
        if (param == QStringLiteral("multithreaded"))
            d_ptr->multiThreaded = true;
        if (param == QStringLiteral("usewarp"))
            d_ptr->useWarp = true;
        else if (param == QStringLiteral("enabledebug"))
            d_ptr->enableDebug = true;
        else if (param == QStringLiteral("fixmessagehook"))
            d_ptr->enableMessageHook = false;
        else if (param == QStringLiteral("supportwarp"))
            d_ptr->supportWarp = true;
    }
}

QWindowsDirect2DContext::~QWindowsDirect2DContext() = default;

bool QWindowsDirect2DContext::init()
{
    Q_D(QWindowsDirect2DContext);
    return d->init();
}

void QWindowsDirect2DContext::resetHardwareResources()
{
    Q_D(QWindowsDirect2DContext);
    d->reset();
}

QWindowsDirect2DContext *QWindowsDirect2DContext::instance()
{
    return QWindowsDirect2DIntegration::instance()->direct2DContext();
}

ID3D11Device *QWindowsDirect2DContext::d3dDevice() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->d3dDevice.Get();
}

ID2D1Device *QWindowsDirect2DContext::d2dDevice() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->d2dDevice.Get();
}

ID2D1Factory1 *QWindowsDirect2DContext::d2dFactory() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->d2dFactory.Get();
}

IDXGIFactory2 *QWindowsDirect2DContext::dxgiFactory() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->dxgiFactory.Get();
}

ID3D11DeviceContext *QWindowsDirect2DContext::d3dDeviceContext() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->d3dDeviceContext.Get();
}

IDWriteFactory *QWindowsDirect2DContext::dwriteFactory() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->directWriteFactory.Get();
}

IDWriteGdiInterop *QWindowsDirect2DContext::dwriteGdiInterop() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->directWriteGdiInterop.Get();
}

QWindowsDirect2DFunctions::DeviceType QWindowsDirect2DContext::preferredDeviceTypeStatic()
{
    return instance()->d_ptr->useWarp
        ? QWindowsDirect2DFunctions::DeviceWarp
        : QWindowsDirect2DFunctions::DeviceHardware;
}

float QWindowsDirect2DContext::dpiForSystem() const
{
    if (QWindowsContext::user32dll.getDpiForSystem == nullptr) {
        FLOAT dpiX, dpiY;
        d2dFactory()->GetDesktopDpi(&dpiX, &dpiY);
        Q_ASSERT(dpiX == dpiY);
        return dpiX;
    } else {
        auto dpi = QWindowsContext::user32dll.getDpiForSystem();
        return dpi;
    }
}

bool QWindowsDirect2DContext::messageHookEnabled() const
{
    Q_D(const QWindowsDirect2DContext);
    return d->enableMessageHook;
}

QWindowsDirect2DFunctions::DeviceType QWindowsDirect2DContext::currentDeviceTypeStatic()
{
    auto type = instance()->d_ptr->d3dDriverType;
    switch (type) {
    case D3D_DRIVER_TYPE_HARDWARE:
        return QWindowsDirect2DFunctions::DeviceHardware;
    case D3D_DRIVER_TYPE_WARP:
        return QWindowsDirect2DFunctions::DeviceWarp;
    default:
        return QWindowsDirect2DFunctions::DeviceNone;
    }
}

QWindowsDirect2DFunctions::Direct2DContext QWindowsDirect2DContext::contextStatic()
{
    auto context = instance();
    QWindowsDirect2DFunctions::Direct2DContext result; 
    result.d3dDevice = context->d3dDevice();
    result.d2dDevice = context->d2dDevice();
    result.d2dFactory = context->d2dFactory();
    result.dxgiFactory = context->dxgiFactory();
    result.d3dDeviceContext = context->d3dDeviceContext();
    return result;
}

QT_END_NAMESPACE
