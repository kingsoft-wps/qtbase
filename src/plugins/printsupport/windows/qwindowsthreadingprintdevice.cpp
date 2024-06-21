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

#include "qwindowsthreadingprintdevice.h"
#include <private/qthreadingprint_win_p.h>

#ifndef DC_COLLATE
#  define DC_COLLATE 22
#endif

QT_BEGIN_NAMESPACE

QT_WARNING_DISABLE_GCC("-Wsign-compare")
typedef QVector<QWindowsPrinterInfo> WindowsPrinterLookup;
Q_GLOBAL_STATIC(WindowsPrinterLookup, windowsDeviceLookup);

extern qreal qt_pointMultiplier(QPageLayout::Unit unit);
extern QPrint::InputSlot paperBinToInputSlot(int windowsId, const QString &name);

static LPDEVMODE getDevmode(HANDLE hPrinter, const QString &printerId, bool *interrupted)
{
    if (interrupted && *interrupted)
        return nullptr;

    LPWSTR printerIdUtf16 = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(printerId.utf16()));
    // Allocate the required DEVMODE buffer
    LONG dmSize = QThreadingPrint::documentProperties(NULL, hPrinter, printerIdUtf16, NULL,
        NULL, 0, 0, interrupted);
    if (dmSize <= 0)
        return nullptr;

    LPDEVMODE pDevMode = reinterpret_cast<LPDEVMODE>(malloc(dmSize));
     // Get the default DevMode
    LONG result = QThreadingPrint::documentProperties(NULL, hPrinter, printerIdUtf16, pDevMode,
        NULL, DM_OUT_BUFFER, dmSize, interrupted);
    if (result != IDOK) {
        free(pDevMode);
        pDevMode = nullptr;
    }
    return pDevMode;
}

QWindowsThreadingPrintDevice::QWindowsThreadingPrintDevice()
    : QPlatformPrintDevice(),
      m_hPrinter(0),
      m_interrupted(false)
{
}

QWindowsThreadingPrintDevice::QWindowsThreadingPrintDevice(const QString &id)
    : QPlatformPrintDevice(id),
      m_hPrinter(0),
      m_interrupted(false)
{
    // First do a fast lookup to see if printer exists, if it does then open it
    if (!id.isEmpty() && QWindowsThreadingPrintDevice::availablePrintDeviceIds().contains(id)) {

        if (QThreadingPrint::openPrinter(m_id, &m_hPrinter)) {
            DWORD needed = 0;
            QThreadingPrint::getPrinter(m_hPrinter, 2, 0, &needed, nullptr, &m_interrupted);
            if (!m_interrupted) {
                QSharedPointer<BYTE> buffer;
                if (QThreadingPrint::getPrinter(m_hPrinter, 2, needed, &needed, &buffer, &m_interrupted) && buffer) {
                    PPRINTER_INFO_2 info = reinterpret_cast<PPRINTER_INFO_2>(buffer.data());
                    m_name = QString::fromWCharArray(info->pPrinterName);
                    m_location = QString::fromWCharArray(info->pLocation);
                    m_makeAndModel = QString::fromWCharArray(info->pDriverName); // TODO Check is not available elsewhere
                    m_isRemote = info->Attributes & PRINTER_ATTRIBUTE_NETWORK;
                }
            }

            QWindowsPrinterInfo m_info;
            m_info.m_id = m_id;
            m_info.m_name = m_name;
            m_info.m_location = m_location;
            m_info.m_makeAndModel = m_makeAndModel;
            m_info.m_isRemote = m_isRemote;
            m_infoIndex = windowsDeviceLookup()->indexOf(m_info);
            if (m_infoIndex != -1) {
                m_info = windowsDeviceLookup()->at(m_infoIndex);
                m_havePageSizes = m_info.m_havePageSizes;
                m_pageSizes = m_info.m_pageSizes;
                m_haveResolutions = m_info.m_haveResolutions;
                m_resolutions = m_info.m_resolutions;
                m_haveCopies = m_info.m_haveCopies;
                m_supportsMultipleCopies = m_info.m_supportsMultipleCopies;
                m_supportsCollateCopies = m_info.m_supportsCollateCopies;
                m_haveMinMaxPageSizes = m_info.m_haveMinMaxPageSizes;
                m_minimumPhysicalPageSize = m_info.m_minimumPhysicalPageSize;
                m_maximumPhysicalPageSize = m_info.m_maximumPhysicalPageSize;
                m_supportsCustomPageSizes = m_info.m_supportsCustomPageSizes;
                m_haveInputSlots = m_info.m_haveInputSlots;
                m_inputSlots = m_info.m_inputSlots;
                m_haveOutputBins = m_info.m_haveOutputBins;
                m_outputBins = m_info.m_outputBins;
                m_haveDuplexModes = m_info.m_haveDuplexModes;
                m_duplexModes = m_info.m_duplexModes;
                m_haveColorModes = m_info.m_haveColorModes;
                m_colorModes = m_info.m_colorModes;
                m_infoIndex = windowsDeviceLookup()->indexOf(m_info);
            } else {
                windowsDeviceLookup()->append(m_info);
                m_infoIndex = windowsDeviceLookup()->count() - 1;
            }
        }
    }
}

QWindowsThreadingPrintDevice::~QWindowsThreadingPrintDevice()
{
    QThreadingPrint::closePrinter(m_hPrinter);
}

bool QWindowsThreadingPrintDevice::isValid() const
{
    return m_hPrinter;
}

bool QWindowsThreadingPrintDevice::isDefault() const
{
    return m_id == defaultPrintDeviceId();
}

QPrint::DeviceState QWindowsThreadingPrintDevice::state() const
{
    DWORD needed = 0;
    QThreadingPrint::getPrinter(m_hPrinter, 6, 0, &needed, nullptr, &m_interrupted);
    if (m_interrupted)
        return QPrint::Error;

    QSharedPointer<BYTE> buffer;
    if (QThreadingPrint::getPrinter(m_hPrinter, 6, needed, &needed, &buffer, &m_interrupted) && buffer) {
        PPRINTER_INFO_6 info = reinterpret_cast<PPRINTER_INFO_6>(buffer.data());
        // TODO Check mapping
        if (info->dwStatus == 0
            || (info->dwStatus & PRINTER_STATUS_WAITING) == PRINTER_STATUS_WAITING
            || (info->dwStatus & PRINTER_STATUS_POWER_SAVE) == PRINTER_STATUS_POWER_SAVE) {
            return QPrint::Idle;
        } else if ((info->dwStatus & PRINTER_STATUS_PRINTING) == PRINTER_STATUS_PRINTING
                   || (info->dwStatus & PRINTER_STATUS_BUSY) == PRINTER_STATUS_BUSY
                   || (info->dwStatus & PRINTER_STATUS_INITIALIZING) == PRINTER_STATUS_INITIALIZING
                   || (info->dwStatus & PRINTER_STATUS_IO_ACTIVE) == PRINTER_STATUS_IO_ACTIVE
                   || (info->dwStatus & PRINTER_STATUS_PROCESSING) == PRINTER_STATUS_PROCESSING
                   || (info->dwStatus & PRINTER_STATUS_WARMING_UP) == PRINTER_STATUS_WARMING_UP) {
            return QPrint::Active;
        }
    }

    return QPrint::Error;
}

void QWindowsThreadingPrintDevice::loadPageSizes() const
{
    // Get the number of paper sizes and check all 3 attributes have same count
    DWORD paperCount = QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERNAMES, NULL, NULL, 0);
    if (int(paperCount) > 0
        && QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERSIZE, NULL, NULL, 0) == paperCount
        && QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERS, NULL, NULL, 0) == paperCount) {

        QScopedArrayPointer<wchar_t> paperNames(new wchar_t[paperCount * 64]);
        QScopedArrayPointer<POINT> winSizes(new POINT[paperCount]);
        QScopedArrayPointer<wchar_t> papers(new wchar_t[paperCount]);

        // Get the details and match the default paper size
        if (QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERNAMES, paperNames.data(), NULL, paperCount * 64 *sizeof(wchar_t)) == paperCount
            && QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERSIZE, (wchar_t *)winSizes.data(), NULL, paperCount * sizeof(POINT)) == paperCount
            && QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_PAPERS, papers.data(), NULL, paperCount * sizeof(wchar_t)) == paperCount) {

            // Returned size is in tenths of a millimeter
            const qreal multiplier = qt_pointMultiplier(QPageLayout::Millimeter);
            for (int i = 0; i < int(paperCount); ++i) {
                QSize size = QSize(qRound((winSizes[i].x / 10.0) * multiplier), qRound((winSizes[i].y / 10.0) * multiplier));
                wchar_t *paper = paperNames.data() + (i * 64);
                QString name = QString::fromWCharArray(paper, wcsnlen_s(paper, 64));
                m_pageSizes.append(createPageSize(papers[i], size, name));
            }

        }
    }

    m_havePageSizes = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_havePageSizes = true;
    info[m_infoIndex].m_pageSizes = m_pageSizes;
}

QPageSize QWindowsThreadingPrintDevice::defaultPageSize() const
{
    if (!m_havePageSizes)
        loadPageSizes();

    QPageSize pageSize;

    if (LPDEVMODE pDevMode = getDevmode(m_hPrinter, m_id, &m_interrupted)) {
        // Get the default paper size
        if (pDevMode->dmFields & DM_PAPERSIZE) {
            // Find the supported page size that matches, in theory default should be one of them
            foreach (const QPageSize &ps, m_pageSizes) {
                if (ps.windowsId() == pDevMode->dmPaperSize) {
                    pageSize = ps;
                    break;
                }
            }
        }
        // Clean-up
        free(pDevMode);
    }

    return pageSize;
}


QPageSize QWindowsThreadingPrintDevice::supportedPageSize(QPageSize::PageSizeId pageSizeId) const
{
    if (!m_havePageSizes)
        loadPageSizes();

    if (pageSizeId > QPageSize::LastPageSize) {
        for (const QPageSize &ps : m_pageSizes) {
            if (ps.windowsId() == pageSizeId)
                return ps;
        }
    }
    return QPlatformPrintDevice::supportedPageSize(pageSizeId);
}

QMarginsF QWindowsThreadingPrintDevice::printableMargins(const QPageSize &pageSize,
                                                         QPageLayout::Orientation orientation,
                                                         const QSize &resolution) const
{
    // TODO This is slow, need to cache values or find better way!
    // Modify the DevMode to get the DC printable margins in device pixels
    QMarginsF margins = QMarginsF(0, 0, 0, 0);
    DWORD needed = 0;
    QThreadingPrint::getPrinter(m_hPrinter, 2, 0, &needed, nullptr, &m_interrupted);
    if (m_interrupted)
        return margins;

    QSharedPointer<BYTE> buffer;
    if (QThreadingPrint::getPrinter(m_hPrinter, 2, needed, &needed, &buffer, &m_interrupted) && buffer) {
        PPRINTER_INFO_2 info = reinterpret_cast<PPRINTER_INFO_2>(buffer.data());
        LPDEVMODE devMode = info->pDevMode;
        bool separateDevMode = false;
        if (!devMode) {
            // GetPrinter() didn't include the DEVMODE. Get it a different way.
            devMode = getDevmode(m_hPrinter, m_id, &m_interrupted);
            if (!devMode)
                return margins;
            separateDevMode = true;
        }

        HDC pDC = QThreadingPrint::createDC(NULL, (LPWSTR)m_id.utf16(), NULL, devMode);
        if (pageSize.windowsId() <= 0) {
            devMode->dmPaperSize =  0;
            devMode->dmPaperWidth = pageSize.size(QPageSize::Millimeter).width() * 10.0;
            devMode->dmPaperLength = pageSize.size(QPageSize::Millimeter).height() * 10.0;
        } else {
            devMode->dmPaperSize =  pageSize.windowsId();
        }
        if (orientation == QPageLayout::Portrait) {
            devMode->dmPrintQuality = resolution.width();
            devMode->dmYResolution = resolution.height();
        } else {
            devMode->dmPrintQuality = resolution.height();
            devMode->dmYResolution = resolution.width();
        }
        devMode->dmOrientation = orientation == QPageLayout::Portrait ? DMORIENT_PORTRAIT : DMORIENT_LANDSCAPE;

        bool interrupted = false;
        QThreadingPrint::resetDC(pDC, devMode, &interrupted);
        const int dpiWidth = GetDeviceCaps(pDC, LOGPIXELSX);
        const int dpiHeight = GetDeviceCaps(pDC, LOGPIXELSY);
        const qreal wMult = 72.0 / dpiWidth;
        const qreal hMult = 72.0 / dpiHeight;
        const qreal physicalWidth = GetDeviceCaps(pDC, PHYSICALWIDTH) * wMult;
        const qreal physicalHeight = GetDeviceCaps(pDC, PHYSICALHEIGHT) * hMult;
        const qreal printableWidth = GetDeviceCaps(pDC, HORZRES) * wMult;
        const qreal printableHeight = GetDeviceCaps(pDC, VERTRES) * hMult;
        const qreal leftMargin = GetDeviceCaps(pDC, PHYSICALOFFSETX)* wMult;
        const qreal topMargin = GetDeviceCaps(pDC, PHYSICALOFFSETY) * hMult;
        const qreal rightMargin = physicalWidth - leftMargin - printableWidth;
        const qreal bottomMargin = physicalHeight - topMargin - printableHeight;
        margins = QMarginsF(leftMargin, topMargin, rightMargin, bottomMargin);
        if (separateDevMode)
            free(devMode);

        QThreadingPrint::deleteDC(pDC);
    }
    return margins;
}

void QWindowsThreadingPrintDevice::loadResolutions() const
{
    DWORD resCount = QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_ENUMRESOLUTIONS, NULL, NULL, 0);
    if (int(resCount) > 0) {
        QScopedArrayPointer<LONG> resolutions(new LONG[resCount*2]);
        // Get the details and match the default paper size
        if (QThreadingPrint::deviceCapabilities((LPWSTR)m_id.utf16(), NULL, DC_ENUMRESOLUTIONS,
            (LPWSTR)resolutions.data(), NULL, resCount * 2 * sizeof(LONG)) == resCount) {
            for (int i = 0; i < int(resCount * 2); i += 2)
                m_resolutions.append(resolutions[i+1]);
        }
    }
    m_haveResolutions = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveResolutions = true;
    info[m_infoIndex].m_resolutions = m_resolutions;
}

int QWindowsThreadingPrintDevice::defaultResolution() const
{
    int resolution = 72;  // TODO Set a sensible default?

    if (LPDEVMODE pDevMode = getDevmode(m_hPrinter, m_id, &m_interrupted)) {
        // Get the default resolution
        if (pDevMode->dmFields & DM_YRESOLUTION) {
            if (pDevMode->dmPrintQuality > 0)
                resolution = pDevMode->dmPrintQuality;
            else
                resolution = pDevMode->dmYResolution;
        }
        // Clean-up
        free(pDevMode);
    }
    return resolution;
}

void QWindowsThreadingPrintDevice::loadInputSlots() const
{
    const auto printerId = wcharId();
    DWORD binCount = QThreadingPrint::deviceCapabilities(printerId, nullptr, DC_BINS, nullptr, nullptr, 0);
    if (int(binCount) > 0
        && QThreadingPrint::deviceCapabilities(printerId, nullptr, DC_BINNAMES, nullptr, nullptr, 0) == binCount) {

        QScopedArrayPointer<WORD> bins(new WORD[binCount]);
        QScopedArrayPointer<wchar_t> binNames(new wchar_t[binCount*24]);

        // Get the details and match the default paper size
        if (QThreadingPrint::deviceCapabilities(printerId, nullptr, DC_BINS,
                               reinterpret_cast<LPWSTR>(bins.data()), nullptr, binCount*sizeof(WORD)) == binCount
            && QThreadingPrint::deviceCapabilities(printerId, nullptr, DC_BINNAMES, binNames.data(),
                                  nullptr, binCount * 24 *sizeof(wchar_t)) == binCount) {

            for (int i = 0; i < int(binCount); ++i) {
                wchar_t *binName = binNames.data() + (i * 24);
                QString name = QString::fromWCharArray(binName, wcsnlen_s(binName, 24));
                m_inputSlots.append(paperBinToInputSlot(bins[i], name));
            }

        }
    }

    m_haveInputSlots = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveInputSlots = true;
    info[m_infoIndex].m_inputSlots = m_inputSlots;
}

QPrint::InputSlot QWindowsThreadingPrintDevice::defaultInputSlot() const
{
    QPrint::InputSlot inputSlot = QPlatformPrintDevice::defaultInputSlot();;

    if (LPDEVMODE pDevMode = getDevmode(m_hPrinter, m_id, &m_interrupted)) {
        // Get the default input slot
        if (pDevMode->dmFields & DM_DEFAULTSOURCE) {
            QPrint::InputSlot tempSlot = paperBinToInputSlot(pDevMode->dmDefaultSource, QString());
            foreach (const QPrint::InputSlot &slot, supportedInputSlots()) {
                if (slot.key == tempSlot.key) {
                    inputSlot = slot;
                    break;
                }
            }
        }
        // Clean-up
        free(pDevMode);
    }
    return inputSlot;
}

void QWindowsThreadingPrintDevice::loadOutputBins() const
{
    m_outputBins.append(QPlatformPrintDevice::defaultOutputBin());
    m_haveOutputBins = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveOutputBins = true;
    info[m_infoIndex].m_outputBins = m_outputBins;
}

void QWindowsThreadingPrintDevice::loadDuplexModes() const
{
    m_duplexModes.append(QPrint::DuplexNone);
    DWORD duplex = QThreadingPrint::deviceCapabilities(wcharId(), nullptr, DC_DUPLEX, nullptr, nullptr, 0);
    if (int(duplex) == 1) {
        // TODO Assume if duplex flag supports both modes
        m_duplexModes.append(QPrint::DuplexAuto);
        m_duplexModes.append(QPrint::DuplexLongSide);
        m_duplexModes.append(QPrint::DuplexShortSide);
    }
    m_haveDuplexModes = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveDuplexModes = true;
    info[m_infoIndex].m_duplexModes = m_duplexModes;
}

QPrint::DuplexMode QWindowsThreadingPrintDevice::defaultDuplexMode() const
{
    QPrint::DuplexMode duplexMode = QPrint::DuplexNone;

    if (LPDEVMODE pDevMode = getDevmode(m_hPrinter, m_id, &m_interrupted)) {
        // Get the default duplex mode
        if (pDevMode->dmFields & DM_DUPLEX) {
            if (pDevMode->dmDuplex == DMDUP_VERTICAL)
                duplexMode = QPrint::DuplexLongSide;
            else if (pDevMode->dmDuplex == DMDUP_HORIZONTAL)
                duplexMode = QPrint::DuplexShortSide;
        }
        // Clean-up
        free(pDevMode);
    }
    return duplexMode;
}

void QWindowsThreadingPrintDevice::loadColorModes() const
{
    m_colorModes.append(QPrint::GrayScale);
    DWORD color = QThreadingPrint::deviceCapabilities(wcharId(), nullptr, DC_COLORDEVICE, nullptr, nullptr, 0);
    if (int(color) == 1)
        m_colorModes.append(QPrint::Color);
    m_haveColorModes = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveColorModes = true;
    info[m_infoIndex].m_colorModes = m_colorModes;
}

QPrint::ColorMode QWindowsThreadingPrintDevice::defaultColorMode() const
{
    if (!m_haveColorModes)
        loadColorModes();
    if (!m_colorModes.contains(QPrint::Color))
        return QPrint::GrayScale;

    QPrint::ColorMode colorMode = QPrint::GrayScale;

    if (LPDEVMODE pDevMode = getDevmode(m_hPrinter, m_id, &m_interrupted)) {
        // Get the default color mode
        if (pDevMode->dmFields & DM_COLOR && pDevMode->dmColor == DMCOLOR_COLOR)
            colorMode = QPrint::Color;
        // Clean-up
        free(pDevMode);
    }
    return colorMode;
}

QStringList QWindowsThreadingPrintDevice::availablePrintDeviceIds()
{
    QStringList list;
    DWORD needed = 0;
    DWORD returned = 0;

    if (!QThreadingPrint::enumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, 0, 0, &needed, &returned)
        || !needed) {
        return list;
    }

    QScopedArrayPointer<BYTE> buffer(new BYTE[needed]);
    if (!QThreadingPrint::enumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, buffer.data(), needed, &needed, &returned))
        return list;

    PPRINTER_INFO_4 infoList = reinterpret_cast<PPRINTER_INFO_4>(buffer.data());
    for (uint i = 0; i < returned; ++i)
        list.append(QString::fromWCharArray(infoList[i].pPrinterName));
    return list;
}

QString QWindowsThreadingPrintDevice::defaultPrintDeviceId()
{
    DWORD size = 0;

    if (QThreadingPrint::getDefaultPrinter(NULL, &size) == ERROR_FILE_NOT_FOUND || 0 == size)
       return QString();

    QScopedArrayPointer<wchar_t> name(new wchar_t[size]);
    QThreadingPrint::getDefaultPrinter(name.data(), &size);

    return QString::fromWCharArray(name.data());
}

void QWindowsThreadingPrintDevice::loadCopiesSupport() const
{
    auto printerId = wcharId();
    m_supportsMultipleCopies = (QThreadingPrint::deviceCapabilities(printerId, NULL, DC_COPIES, NULL, NULL, 0) > 1);
    m_supportsCollateCopies = QThreadingPrint::deviceCapabilities(printerId, NULL, DC_COLLATE, NULL, NULL, 0);
    m_haveCopies = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveCopies = true;
    info[m_infoIndex].m_supportsMultipleCopies = m_supportsMultipleCopies;
    info[m_infoIndex].m_supportsCollateCopies = m_supportsCollateCopies;
}

bool QWindowsThreadingPrintDevice::supportsCollateCopies() const
{
    if (!m_haveCopies)
        loadCopiesSupport();
    return m_supportsCollateCopies;
}

bool QWindowsThreadingPrintDevice::supportsMultipleCopies() const
{
    if (!m_haveCopies)
        loadCopiesSupport();
    return m_supportsMultipleCopies;
}

bool QWindowsThreadingPrintDevice::supportsCustomPageSizes() const
{
    if (!m_haveMinMaxPageSizes)
        loadMinMaxPageSizes();
    return m_supportsCustomPageSizes;
}

QSize QWindowsThreadingPrintDevice::minimumPhysicalPageSize() const
{
    if (!m_haveMinMaxPageSizes)
        loadMinMaxPageSizes();
    return m_minimumPhysicalPageSize;
}

QSize QWindowsThreadingPrintDevice::maximumPhysicalPageSize() const
{
    if (!m_haveMinMaxPageSizes)
        loadMinMaxPageSizes();
    return m_maximumPhysicalPageSize;
}

void QWindowsThreadingPrintDevice::loadMinMaxPageSizes() const
{
    // Min/Max custom size is in tenths of a millimeter
    const qreal multiplier = qt_pointMultiplier(QPageLayout::Millimeter);
    auto printerId = wcharId();
    DWORD min = QThreadingPrint::deviceCapabilities(printerId, NULL, DC_MINEXTENT, NULL, NULL, 0);
    m_minimumPhysicalPageSize = QSize((LOWORD(min) / 10.0) * multiplier, (HIWORD(min) / 10.0) * multiplier);
    DWORD max = QThreadingPrint::deviceCapabilities(printerId, NULL, DC_MAXEXTENT, NULL, NULL, 0);
    m_maximumPhysicalPageSize = QSize((LOWORD(max) / 10.0) * multiplier, (HIWORD(max) / 10.0) * multiplier);
    m_supportsCustomPageSizes = (m_maximumPhysicalPageSize.width() > 0 && m_maximumPhysicalPageSize.height() > 0);
    m_haveMinMaxPageSizes = true;
    QWindowsPrinterInfo *info = windowsDeviceLookup()->data();
    info[m_infoIndex].m_haveCopies = true;
    info[m_infoIndex].m_supportsMultipleCopies = m_supportsMultipleCopies;
    info[m_infoIndex].m_supportsCollateCopies = m_supportsCollateCopies;
}

QT_END_NAMESPACE
