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

#ifndef QWINDOWSDIRECT2DFUNCTIONS_H
#define QWINDOWSDIRECT2DFUNCTIONS_H

#include <QtGui/qtguiglobal.h>
#include <QtCore/QObject>
#include <QtPlatformHeaders/QPlatformHeaderHelper>

struct ID3D11Device;
struct ID2D1Device;
struct ID2D1Factory1;
struct IDXGIFactory2;
struct ID3D11DeviceContext;
struct ID2D1Bitmap1;
struct ID2D1DeviceContext;
struct ID3D11Texture2D;
struct IDWriteFontFace;
struct IDWriteFactory1;


QT_BEGIN_NAMESPACE

class QPlatformPixmap;
class QFontEngine;
class QString;

class QWindowsDirect2DFunctions
{
public:
    enum DeviceType {
        DeviceNone = 0,
        DeviceWarp,
        DeviceHardware,
    };

    enum AlphaMode {
        AlphaModeUnknown = 0,
        AlphaModePremultiplied,
        AlphaModeStraight,
        AlphaModeIgnore,
    };

    struct Direct2DContext
    {
        ID3D11Device *d3dDevice = nullptr;
        ID2D1Device *d2dDevice = nullptr;
        ID2D1Factory1 *d2dFactory = nullptr;
        IDXGIFactory2 *dxgiFactory = nullptr;
        ID3D11DeviceContext *d3dDeviceContext = nullptr;
        IDWriteFactory1 *dwFactroy1 = nullptr;
    };

public:
    typedef DeviceType (*PreferredDeviceType)();
    static const QByteArray preferredDeviceTypeIdentifier() { return QByteArrayLiteral("direct2dGetPreferredDeviceType"); }
    static DeviceType preferredDeviceType()
    {
        return QPlatformHeaderHelper::callPlatformFunction<DeviceType, CurrentDeviceType>(preferredDeviceTypeIdentifier());
    }

    typedef DeviceType (*CurrentDeviceType)();
    static const QByteArray currentDeviceTypeIdentifier() { return QByteArrayLiteral("direct2dGetDeviceType"); }
    static DeviceType currentDeviceType()
    {
        return QPlatformHeaderHelper::callPlatformFunction<DeviceType, CurrentDeviceType>(currentDeviceTypeIdentifier());
    }

    typedef Direct2DContext (*Context)();
    static const QByteArray contextIdentifier() { return QByteArrayLiteral("direct2dGetContext"); }
    static Direct2DContext context()
    {
        return QPlatformHeaderHelper::callPlatformFunction<Direct2DContext, Context>(contextIdentifier());
    }

    typedef QPlatformPixmap * (*CreatePixmap)(ID2D1Bitmap1 *, ID2D1DeviceContext *);
    static const QByteArray createPixmapIdentifier() { return QByteArrayLiteral("direct2dCreatePixmap"); }
    static QPlatformPixmap * createPixmap(ID2D1Bitmap1 *bm, ID2D1DeviceContext *dc)
    {
        return QPlatformHeaderHelper::callPlatformFunction<QPlatformPixmap*, CreatePixmap, ID2D1Bitmap1*, ID2D1DeviceContext*>(createPixmapIdentifier(), bm, dc);
    }

    typedef QPlatformPixmap * (*CreatePixmapFromTexture)(ID3D11Texture2D *, bool isTarget, AlphaMode);
    static const QByteArray createPixmapFromTextureIdentifier() { return QByteArrayLiteral("direct2dCreatePixmapFromTexture"); }
    static QPlatformPixmap * createPixmapFromTexture(ID3D11Texture2D *texture, bool isTarget, AlphaMode mode)
    {
        return QPlatformHeaderHelper::callPlatformFunction<QPlatformPixmap*, CreatePixmapFromTexture, ID3D11Texture2D *, bool, AlphaMode>(createPixmapFromTextureIdentifier(), texture, isTarget, mode);
    }

    typedef ID2D1DeviceContext * (*GetDeviceContext)(QPlatformPixmap *);
    static const QByteArray getDeviceContextIdentifier() { return QByteArrayLiteral("direct2dGetDeviceContext"); }
    static ID2D1DeviceContext * getDeviceContext(QPlatformPixmap *pm)
    {
        return QPlatformHeaderHelper::callPlatformFunction<ID2D1DeviceContext *, GetDeviceContext, QPlatformPixmap *>(getDeviceContextIdentifier(), pm);
    }

    typedef bool (*MatchDWriteFont)(const QFontEngine *, IDWriteFontFace **);
    static const QByteArray matchDWriteFontIdentifier() { return QByteArrayLiteral("matchDWriteFont"); }
    static bool matchDWriteFont(const QFontEngine *fontEngine, IDWriteFontFace **ppFontFace)
    {
        return QPlatformHeaderHelper::callPlatformFunction<bool, MatchDWriteFont, const QFontEngine *, IDWriteFontFace **>(matchDWriteFontIdentifier(), fontEngine, ppFontFace);
    }

    typedef QString (*GetDWriteFontPath)(IDWriteFontFace *);
    static const QByteArray getDWriteFontPathIdentifier()
    {
        return QByteArrayLiteral("getDWriteFontPath");
    }
    static QString getDWriteFontPath(IDWriteFontFace *pFontFace)
    {
        return QPlatformHeaderHelper::callPlatformFunction<QString, GetDWriteFontPath, IDWriteFontFace *>(getDWriteFontPathIdentifier(), pFontFace);
    }

    typedef void (*RebuildDirect2DContext)();
    static const QByteArray rebuildDirect2DContextIdentifier() { return QByteArrayLiteral("rebuildDirect2DContext"); }
    static void rebuildDirect2DContext()
    {
        return QPlatformHeaderHelper::callPlatformFunction<void, RebuildDirect2DContext>(rebuildDirect2DContextIdentifier());
    }
};

QT_END_NAMESPACE

#endif // QWINDOWSDIRECT2DFUNCTIONS_H