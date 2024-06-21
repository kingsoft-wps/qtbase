/****************************************************************************
**
** Copyright (C) 2014 John Layt <jlayt@kde.org>
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QWINDOWSTHREADINGPRINTDEVICE_H
#define QWINDOWSTHREADINGPRINTDEVICE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <qwindowsprintdevice.h>

QT_BEGIN_NAMESPACE

class QWindowsThreadingPrintDevice : public QPlatformPrintDevice
{
public:
    QWindowsThreadingPrintDevice();
    explicit QWindowsThreadingPrintDevice(const QString &id);
    virtual ~QWindowsThreadingPrintDevice();

    bool isValid() const override;
    bool isDefault() const override;

    QPrint::DeviceState state() const override;

    QPageSize defaultPageSize() const override;

    QPageSize supportedPageSize(QPageSize::PageSizeId pageSizeId) const override;

    QMarginsF printableMargins(const QPageSize &pageSize, QPageLayout::Orientation orientation,
                               const QSize &resolution) const override;

    int defaultResolution() const override;

    QPrint::InputSlot defaultInputSlot() const override;

    QPrint::DuplexMode defaultDuplexMode() const override;

    QPrint::ColorMode defaultColorMode() const override;

    static QStringList availablePrintDeviceIds();
    static QString defaultPrintDeviceId();

    bool supportsCollateCopies() const override;
    bool supportsMultipleCopies() const override;
    bool supportsCustomPageSizes() const override;
    QSize minimumPhysicalPageSize() const override;
    QSize maximumPhysicalPageSize() const override;

protected:
    void loadPageSizes() const override;
    void loadResolutions() const override;
    void loadInputSlots() const override;
    void loadOutputBins() const override;
    void loadDuplexModes() const override;
    void loadColorModes() const override;
    void loadCopiesSupport() const;
    void loadMinMaxPageSizes() const;

private:
    LPCWSTR wcharId() const { return reinterpret_cast<LPCWSTR>(m_id.utf16()); }

    HANDLE m_hPrinter;
    mutable bool m_haveCopies;
    mutable bool m_haveMinMaxPageSizes;
    mutable bool m_interrupted;
    int m_infoIndex;
};

QT_END_NAMESPACE

#endif // QWINDOWSTHREADINGPRINTDEVICE_H
