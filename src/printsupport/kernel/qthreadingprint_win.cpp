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

#include "qprintworker.h"
#include "qthreadingprint_win_p.h"
#include <qprintworker.h>

#ifndef QT_NO_PRINTER

Q_DECLARE_METATYPE(HDC)
Q_DECLARE_METATYPE(HANDLE)
Q_DECLARE_METATYPE(PDEVMODE)
Q_DECLARE_METATYPE(QVector<QVariant>)

QT_BEGIN_NAMESPACE

template<typename T>
static void arrayBufferDeleter(T *ptr)
{
    delete[] ptr;
}

static void *globalAlloc(DWORD size, HGLOBAL *hMem)
{
    *hMem = GlobalAlloc(GHND, size);
    if (*hMem)
        return GlobalLock(*hMem);
    return nullptr;
}

static void globalFree(HGLOBAL hMem)
{
    if (hMem) {
        GlobalUnlock(hMem);
        GlobalFree(hMem);
    }
}

int QThreadingPrint::startDoc(HDC hdc, const DOCINFO *info, bool *pInterrupted)
{
    int retVal = SP_ERROR;
    QString docName = QString::fromWCharArray(info->lpszDocName);
    QString output = QString::fromWCharArray(info->lpszOutput);
    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        DOCINFO di;
        memset(&di, 0, sizeof(DOCINFO));
        di.cbSize = sizeof(DOCINFO);
        di.lpszDocName = reinterpret_cast<const wchar_t *>(docName.utf16());
        di.lpszOutput = reinterpret_cast<const wchar_t *>(output.utf16());
        return QVariant(::StartDoc(hdc, &di));
    },
    [&](const QVariant &result, bool interrupt)->void {
        if (!interrupt) {
            retVal = result.toInt();
            if (pInterrupted)
                *pInterrupted = false;
        }
    });

    return retVal;
}

int QThreadingPrint::endDoc(HDC hdc, bool *pInterrupted)
{
    int retVal = 0;
    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        return QVariant(::EndDoc(hdc));
    },
    [&](const QVariant &result, bool interrupt)->void {
        if (!interrupt) {
            retVal = result.toInt();
            if (pInterrupted)
                *pInterrupted = false;
        }
    });

    return retVal;
}

int QThreadingPrint::abortDoc(HDC hdc)
{
    int retVal = 1; // Do not use the return value of this function
    QPrintWorker::instance()->runTask([=]()->QVariant {
        return QVariant(::AbortDoc(hdc));
    },
    [&](QVariant result, bool interrupt)->void {
        Q_UNUSED(result);
        Q_UNUSED(interrupt);
    }, 0);

    return retVal;
}

int QThreadingPrint::startPage(HDC hdc, bool *pInterrupted)
{
    int retVal = 0;
    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        return QVariant(::StartPage(hdc));
    },
    [&](const QVariant &result, bool interrupt)->void {
        if (!interrupt) {
            retVal = result.toInt();
            if (pInterrupted)
                *pInterrupted = false;
        }
    });

    return retVal;
}

int QThreadingPrint::endPage(HDC hdc, bool *pInterrupted)
{
    int retVal = 0;
    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        return QVariant(::EndPage(hdc));
    },
    [&](const QVariant &result, bool interrupt)->void {
        if (!interrupt) {
            retVal = result.toInt();
            if (pInterrupted)
                *pInterrupted = false;
        }
    });

    return retVal;
}

BOOL QThreadingPrint::openPrinter(const QString &printName, LPHANDLE pPrinterHandle)
{
    BOOL retVal = FALSE;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        HANDLE hPrinter = 0;
        BOOL ret = ::OpenPrinter((LPWSTR)printName.utf16(), &hPrinter, 0);

        QVector<QVariant> res;
        res.append(QVariant(ret));
        res.append(QVariant::fromValue<HANDLE>(hPrinter));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [&](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        BOOL apiRet = vecRes[0].toInt();
        HANDLE hPrinter = vecRes[1].value<HANDLE>();
        if (interrupt) {
            if (apiRet)
                ::ClosePrinter(hPrinter);
        } else {
            retVal = apiRet;
            *pPrinterHandle = hPrinter;
        }
    });

    return retVal;
}

BOOL QThreadingPrint::closePrinter(HANDLE hPrinter)
{
    BOOL retVal = TRUE;
    QPrintWorker::instance()->runTask([=]()->QVariant {
        ::ClosePrinter(hPrinter);
        return QVariant(true);
    },
    [&](const QVariant &result, bool interrupt)->void {
        Q_UNUSED(result);
        Q_UNUSED(interrupt);
    }, 0);

    return retVal;
}



BOOL QThreadingPrint::getPrinter(HANDLE hPrinter, DWORD level, HGLOBAL pPrinter,
                                 DWORD cbBuf, LPDWORD pcbNeeded, bool *pInterrupted)
{
    BOOL retVal = FALSE;
    LPBYTE pPrinterBuf = nullptr;
    if (pPrinter) {
        pPrinterBuf = (LPBYTE)GlobalLock(pPrinter);
    }

    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        DWORD cbNeeded = 0;
        BOOL ret = ::GetPrinter(hPrinter, level, pPrinterBuf, cbBuf, &cbNeeded);

        QVector<QVariant> res;
        res.append(QVariant(ret));
        res.append(QVariant::fromValue<DWORD>(cbNeeded));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [&, pPrinter](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        BOOL apiRet = vecRes[0].toInt();
        DWORD cbNeeded = vecRes[1].value<DWORD>();
        if (!interrupt) {
            retVal = apiRet;
            *pcbNeeded = cbNeeded;
            if (pInterrupted)
                *pInterrupted = false;
        }
        if (pPrinter)
            GlobalUnlock(pPrinter);
    });

    return retVal;
}

BOOL QThreadingPrint::getPrinter(HANDLE hPrinter, DWORD level, DWORD cbBuf, LPDWORD pcbNeeded,
                                 QSharedPointer<BYTE> *retBuf, bool *pInterrupted)
{
    BOOL retVal = FALSE;
    QSharedPointer<BYTE> printerBuf;

    if (cbBuf) {
        printerBuf.reset(new BYTE[cbBuf], arrayBufferDeleter<BYTE>);
    }

    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        DWORD cbNeeded = 0;
        BOOL ret = ::GetPrinter(hPrinter, level, printerBuf.get(), cbBuf, &cbNeeded);

        QVector<QVariant> res;
        res.append(QVariant(ret));
        res.append(QVariant::fromValue<DWORD>(cbNeeded));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [&, printerBuf](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        BOOL apiRet = vecRes[0].toInt();
        DWORD cbNeeded = vecRes[1].value<DWORD>();
        if (!interrupt) {
            retVal = apiRet;
            if (apiRet && printerBuf && cbBuf && retBuf)
                *retBuf = printerBuf;
            *pcbNeeded = cbNeeded;
            if (pInterrupted)
                *pInterrupted = false;
        }
    });

    return retVal;
}

HDC QThreadingPrint::createDC(LPCWSTR pDriver, LPCWSTR pDevice, LPCWSTR pPort, const DEVMODE *pdm)
{
    HDC retVal = nullptr;
    QString driver = QString::fromWCharArray(pDriver);
    QString device = QString::fromWCharArray(pDevice);
    QString port = QString::fromWCharArray(pPort);
    HGLOBAL hMem = nullptr;
    void *localDevMode = nullptr;

    if (pdm) {
        const DWORD size = pdm->dmSize + pdm->dmDriverExtra;
        localDevMode = globalAlloc(size, &hMem);
        memcpy(localDevMode, pdm, size);
    }

    QPrintWorker::instance()->runTask([=]()->QVariant {
        HDC handle = ::CreateDC(driver.isEmpty() ? (LPCWSTR)nullptr : (LPCWSTR)driver.utf16(),
            device.isEmpty() ? (LPCWSTR)nullptr : (LPCWSTR)device.utf16(),
            port.isEmpty() ? (LPCWSTR)nullptr : (LPCWSTR)port.utf16(),
            (const DEVMODE *)localDevMode);

        return QVariant::fromValue<HDC>(handle);
    },
    [&, hMem](const QVariant &result, bool interrupt)->void {
        HDC handle = result.value<HDC>();
        if (interrupt) {
            if (handle)
                ::DeleteDC(handle);
        } else {
            retVal = handle;
        }
        globalFree(hMem);
    });

    return retVal;
}

HDC QThreadingPrint::resetDC(HDC hdc, const DEVMODE *pdm, bool *pInterrupted)
{
    HDC retVal = nullptr;
    HGLOBAL hMem = nullptr;
    void *localDevMode = nullptr;

    if (pdm) {
        const DWORD size = pdm->dmSize + pdm->dmDriverExtra;
        localDevMode = globalAlloc(size, &hMem);
        memcpy(localDevMode, pdm, size);
    }

    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        HDC handle = ::ResetDC(hdc, (const DEVMODE *)localDevMode);

        return QVariant::fromValue<HDC>(handle);
    },
    [&, hMem](const QVariant &result, bool interrupt)->void {
        HDC handle = result.value<HDC>();
        if (!interrupt) {
            retVal = handle;
            if (pInterrupted)
                *pInterrupted = false;
        }
        globalFree(hMem);
    });

    return retVal;
}

BOOL QThreadingPrint::deleteDC(HDC hdc)
{
    BOOL retVal = TRUE;
    QPrintWorker::instance()->runTask([=]()->QVariant {
        ::DeleteDC(hdc);
        return QVariant(true);
    },
    [&](QVariant result, bool interrupt)->void {
        Q_UNUSED(result);
        Q_UNUSED(interrupt);
    }, 0);
    return retVal;
}

int QThreadingPrint::deviceCapabilities(LPCWSTR pDevice, LPCWSTR pPort, WORD fwCapability,
                                        LPWSTR pOutput, const DEVMODE *pdm, DWORD outputSize)
{
    int retVal = -1;
    QString device = QString::fromWCharArray(pDevice);
    QString port = QString::fromWCharArray(pPort);
    HGLOBAL hMem = nullptr;
    void *localDevMode = nullptr;

    if (pdm) {
        const DWORD size = pdm->dmSize + pdm->dmDriverExtra;
        localDevMode = globalAlloc(size, &hMem);
        memcpy(localDevMode, pdm, size);
    }

    QSharedPointer<BYTE> ownOutput;
    if (outputSize) {
        ownOutput.reset(new BYTE[outputSize], arrayBufferDeleter<BYTE>);
    }

    QPrintWorker::instance()->runTask([=]()->QVariant {
        int ret = ::DeviceCapabilities(device.isEmpty() ? (LPCWSTR)nullptr : (LPCWSTR)device.utf16(),
            port.isEmpty() ? (LPCWSTR)nullptr : (LPCWSTR)port.utf16(), fwCapability, (LPWSTR)ownOutput.get(),
            (const DEVMODE *)localDevMode);

        return QVariant(ret);
    },
    [&, hMem, ownOutput](const QVariant &result, bool interrupt)->void {
        int apiRet = result.toInt();
        if (!interrupt) {
            retVal = apiRet;
            memcpy(pOutput, ownOutput.get(), outputSize);
        }
        globalFree(hMem);
    });

    return retVal;
}

BOOL QThreadingPrint::enumPrinters(DWORD flags, LPWSTR pName, DWORD level, LPBYTE pPrinterEnum,
                                   DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned)
{
    BOOL retVal = FALSE;
    QString name = QString::fromWCharArray(pName);
    QSharedPointer<BYTE> printersBuf;

    if (cbBuf) {
        printersBuf.reset(new BYTE[cbBuf], arrayBufferDeleter<BYTE>);
    }

    QPrintWorker::instance()->runTask([=]()->QVariant {
        DWORD cbNeeded = 0;
        DWORD cbReturned = 0;
        BOOL ret = ::EnumPrinters(flags, name.isEmpty() ? (LPWSTR)nullptr : (LPWSTR)name.utf16(),
            level, printersBuf.get(), cbBuf, &cbNeeded, &cbReturned);

        if (!ret && !cbBuf && cbNeeded && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            ret = TRUE;
        QVector<QVariant> res;
        res.append(QVariant(ret));
        res.append(QVariant::fromValue<DWORD>(cbNeeded));
        res.append(QVariant::fromValue<DWORD>(cbReturned));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [&, printersBuf](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        BOOL apiRet = vecRes[0].toInt();
        DWORD cbNeeded = vecRes[1].value<DWORD>();
        DWORD cbReturned = vecRes[2].value<DWORD>();
        if (!interrupt) {
            retVal = apiRet;
            if (apiRet && pPrinterEnum && printersBuf && cbBuf)
                memcpy(pPrinterEnum, printersBuf.get(), cbBuf);
            *pcbNeeded = cbNeeded;
            *pcReturned = cbReturned;
        }
    });

    return retVal;
}

BOOL QThreadingPrint::getDefaultPrinter(LPWSTR pszBuffer, LPDWORD pcchBuffer)
{
    BOOL retVal = FALSE;
    QSharedPointer<WCHAR> outputBuf;
    DWORD inputSize = *pcchBuffer;

    if (pszBuffer && inputSize) {
        outputBuf.reset(new WCHAR[inputSize], arrayBufferDeleter<WCHAR>);
    }

    QPrintWorker::instance()->runTask([=]()->QVariant {
        DWORD cbBuffer = inputSize;
        BOOL ret = ::GetDefaultPrinter(outputBuf.get(), &cbBuffer);

        QVector<QVariant> res;
        res.append(QVariant(ret));
        res.append(QVariant::fromValue<DWORD>(cbBuffer));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [&, outputBuf](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        BOOL apiRet = vecRes[0].toInt();
        DWORD cbBuffer = vecRes[1].value<DWORD>();
        if (!interrupt) {
            retVal = apiRet;
            if (apiRet && outputBuf && cbBuffer)
                memcpy(pszBuffer, outputBuf.get(), cbBuffer * sizeof(WCHAR));
            *pcchBuffer = cbBuffer;
        }
    });

    return retVal;
}


LONG QThreadingPrint::documentProperties(HWND hWnd, HANDLE hPrinter, LPWSTR pDeviceName,
                                         PDEVMODE pDevModeOutput, PDEVMODE pDevModeInput,
                                         DWORD fMode, DWORD outputDevModeSize, bool *pInterrupted)
{
    LONG retVal = -1;
    QString deviceName = QString::fromWCharArray(pDeviceName);
    HGLOBAL hMemIn = nullptr;
    void *localDevModeInput = nullptr;
    HGLOBAL hMemOut = nullptr;
    void *localDevModeOutput = nullptr;

    if (pDevModeInput) {
        const DWORD size = pDevModeInput->dmSize + pDevModeInput->dmDriverExtra;
        localDevModeInput = globalAlloc(size, &hMemIn);
        memcpy(localDevModeInput, pDevModeInput, size);
    }

    if (pDevModeOutput && outputDevModeSize) {
        localDevModeOutput = globalAlloc(outputDevModeSize, &hMemOut);
    }

    if (pInterrupted)
        *pInterrupted = true;

    QPrintWorker::instance()->runTask([=]()->QVariant {
        LONG ret = ::DocumentProperties(hWnd, hPrinter, deviceName.isEmpty() ? (LPWSTR)nullptr : (LPWSTR)deviceName.utf16(),
            (PDEVMODE)localDevModeOutput, (PDEVMODE)localDevModeInput, fMode);

        QVector<QVariant> res;
        res.append(QVariant(ret));
        return QVariant::fromValue<QVector<QVariant>>(res);
    },
    [=, &retVal](const QVariant &result, bool interrupt)->void {
        QVector<QVariant> vecRes = result.value<QVector<QVariant>>();
        LONG apiRet = vecRes[0].toInt();
        if (!interrupt) {
            retVal = apiRet;
            if (apiRet >= 0 && localDevModeOutput && pDevModeOutput)
                memcpy(pDevModeOutput, localDevModeOutput, outputDevModeSize);
            if (pInterrupted)
                *pInterrupted = false;
        }
        globalFree(hMemIn);
        globalFree(hMemOut);
    });

    return retVal;
}

QT_END_NAMESPACE

#endif // QT_NO_PRINTER