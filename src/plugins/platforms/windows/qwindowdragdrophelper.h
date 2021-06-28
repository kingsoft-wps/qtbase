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

#ifndef QDRAG_DROP_HELPER_H
#define QDRAG_DROP_HELPER_H
#include <QtCore/qt_windows.h>
#include <QtCore/qglobal.h>

typedef DWORD DROPEFFECT;

QT_BEGIN_NAMESPACE

class QDragDropHelper
{
public:
    static inline CLIPFORMAT RegisterFormat(LPCTSTR lpszFormat)
    {
	    return static_cast<CLIPFORMAT>(::RegisterClipboardFormat(lpszFormat));
    }

    static bool	IsRegisteredFormat(CLIPFORMAT cfFormat, LPCWSTR lpszFormat, bool bRegister = false);
    static bool	GetGlobalData(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat, FORMATETC& FormatEtc, STGMEDIUM& StgMedium);
    static DWORD GetGlobalDataDWord(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat);
    static bool SetGlobalDataDWord(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat, DWORD dwData, bool bForce = true);
    static DROPIMAGETYPE DropEffectToDropImage(DROPEFFECT dwEffect);
    static void	ClearDescription(DROPDESCRIPTION *pDropDescription);
    static LPFORMATETC FillFormatEtc(LPFORMATETC lpFormatEtc, CLIPFORMAT cfFormat, LPFORMATETC lpFormatEtcFill);
    static BOOL CopyStgMedium(CLIPFORMAT cfFormat, LPSTGMEDIUM lpDest, LPSTGMEDIUM lpSource);
    static HGLOBAL CopyGlobalMemory(HGLOBAL hDest, HGLOBAL hSource);
    static void CopyFormatEtc(LPFORMATETC petcDest, LPFORMATETC petcSrc);
    static DVTARGETDEVICE* CopyTargetDevice(DVTARGETDEVICE* ptdSrc);
    static HBITMAP ReplaceBlackBit(HBITMAP hBitmap);
    static BOOL isAppThemed();
};

QT_END_NAMESPACE

#endif //QDRAG_DROP_HELPER_H