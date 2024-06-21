/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QTHREADINGPRINT_WIN_P_H
#define QTHREADINGPRINT_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtPrintSupport/private/qtprintsupportglobal_p.h>
#include <QtCore/qt_windows.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_PRINTER

class Q_PRINTSUPPORT_EXPORT QThreadingPrint
{
public:
    static int startDoc(HDC hdc, const DOCINFO *info, bool *pInterrupted);
    static int endDoc(HDC hdc, bool *pInterrupted);
    static int abortDoc(HDC hdc);
    static int startPage(HDC hdc, bool *pInterrupted);
    static int endPage(HDC hdc, bool *pInterrupted);
    static BOOL openPrinter(const QString &printName, LPHANDLE pPrinterHandle);
    static BOOL closePrinter(HANDLE hPrinter);
    static BOOL getPrinter(HANDLE hPrinter, DWORD level, HGLOBAL pPrinter,
                           DWORD cbBuf, LPDWORD pcbNeeded, bool *pInterrupted);
    static BOOL getPrinter(HANDLE hPrinter, DWORD level, DWORD cbBuf, LPDWORD pcbNeeded,
                           QSharedPointer<BYTE> *retBuf, bool *pInterrupted);
    static HDC createDC(LPCWSTR pDriver, LPCWSTR pDevice, LPCWSTR pPort, const DEVMODE *pdm);
    static HDC resetDC(HDC hdc, const DEVMODE *pdm, bool *pInterrupted);
    static BOOL deleteDC(HDC hdc);
    static int deviceCapabilities(LPCWSTR pDevice, LPCWSTR pPort, WORD fwCapability,
                                  LPWSTR pOutput, const DEVMODE *pdm, DWORD outputSize);
    static BOOL enumPrinters(DWORD flags, LPWSTR pName, DWORD level, LPBYTE pPrinterEnum,
                             DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned);
    static BOOL getDefaultPrinter(LPWSTR pszBuffer, LPDWORD pcchBuffer);
    static LONG documentProperties(HWND hWnd, HANDLE hPrinter, LPWSTR pDeviceName,
                                   PDEVMODE pDevModeOutput, PDEVMODE pDevModeInput,
                                   DWORD fMode, DWORD outputDevModeSize, bool *pInterrupted);
};


#endif // QT_NO_PRINTER

QT_END_NAMESPACE

#endif // QTHREADINGPRINT_WIN_P_H
