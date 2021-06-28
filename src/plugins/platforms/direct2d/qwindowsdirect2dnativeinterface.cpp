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

#include "qwindowsdirect2dnativeinterface.h"
#include "qwindowsdirect2dcontext.h"
#include "qwindowsdirect2dbitmap.h"
#include "qwindowsdirect2dplatformpixmap.h"
#include "qwindowsdirect2ddevicecontext.h"
#include "qwindowsdirect2dpaintengine.h"

#include <QtGui/qbackingstore.h>
#include <QtPlatformHeaders/qwindowsdirect2dfunctions.h>

QT_BEGIN_NAMESPACE

void *QWindowsDirect2DNativeInterface::nativeResourceForBackingStore(const QByteArray &resource, QBackingStore *bs)
{
    if (!bs || !bs->handle()) {
        qWarning("%s: '%s' requested for null backingstore or backingstore without handle.", __FUNCTION__, resource.constData());
        return nullptr;
    }

    // getDC is so common we don't want to print an "invalid key" line for it
    if (resource == "getDC")
        return nullptr;

    qWarning("%s: Invalid key '%s' requested.", __FUNCTION__, resource.constData());
    return nullptr;

}

static QPlatformPixmap * createPixmap(ID2D1Bitmap1 *bm, ID2D1DeviceContext *dc)
{
    QScopedPointer<QWindowsDirect2DBitmap> bitmap(new QWindowsDirect2DBitmap(bm, dc));
    return new QWindowsDirect2DPlatformPixmap(
        QPlatformPixmap::PixmapType, QWindowsDirect2DPaintEngine::NoFlag, bitmap.take(), true);
}

static QPlatformPixmap * createPixmapFromTexture(ID3D11Texture2D *texture, bool isTarget, QWindowsDirect2DFunctions::AlphaMode mode)
{
    QScopedPointer<QWindowsDirect2DBitmap> bitmap(
        QWindowsDirect2DBitmap::fromTexture(texture, isTarget, static_cast<int>(mode)));
    if (bitmap)
        return new QWindowsDirect2DPlatformPixmap(
            QPlatformPixmap::PixmapType, QWindowsDirect2DPaintEngine::NoFlag, bitmap.take(), true);
    else
        return nullptr;
}

static ID2D1DeviceContext * getDeviceContext(QPlatformPixmap *pm)
{
    if (pm == nullptr || pm->classId() != QPlatformPixmap::Direct2DClass)
        return nullptr;
    auto d2dpm = static_cast<QWindowsDirect2DPlatformPixmap *>(pm);
    return d2dpm->bitmap()->deviceContext()->get();
}

static void rebuildDirect2DContext()
{
    QWindowsDirect2DContext::instance()->resetHardwareResources();
}

QFunctionPointer QWindowsDirect2DNativeInterface::platformFunction(const QByteArray &function) const
{
    if (function == QWindowsDirect2DFunctions::createPixmapIdentifier())
        return QFunctionPointer(createPixmap);
    if (function == QWindowsDirect2DFunctions::createPixmapFromTextureIdentifier())
        return QFunctionPointer(createPixmapFromTexture);
    if (function == QWindowsDirect2DFunctions::getDeviceContextIdentifier())
        return QFunctionPointer(getDeviceContext);
    if (function == QWindowsDirect2DFunctions::preferredDeviceTypeIdentifier())
        return QFunctionPointer(QWindowsDirect2DContext::preferredDeviceTypeStatic);
    if (function == QWindowsDirect2DFunctions::currentDeviceTypeIdentifier())
        return QFunctionPointer(QWindowsDirect2DContext::currentDeviceTypeStatic);
    if (function == QWindowsDirect2DFunctions::contextIdentifier())
        return QFunctionPointer(QWindowsDirect2DContext::contextStatic);
    if (function == QWindowsDirect2DFunctions::matchDWriteFontIdentifier())
        return QFunctionPointer(QWindowsDirect2DPaintEngine::matchDWriteFont);
    if (function == QWindowsDirect2DFunctions::getDWriteFontPathIdentifier())
        return QFunctionPointer(QWindowsDirect2DPaintEngine::getDWriteFontPath);
    if (function == QWindowsDirect2DFunctions::rebuildDirect2DContextIdentifier())
        return QFunctionPointer(rebuildDirect2DContext);
    return nullptr;
}

QT_END_NAMESPACE
