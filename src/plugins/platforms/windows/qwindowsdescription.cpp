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

#include <Shlobj.h>
#include "qwindowsdescription.h"
#include "qwindowdragdrophelper.h"

QT_BEGIN_NAMESPACE

bool QDropDescription::SetDescText(DROPIMAGETYPE nType, QString qsInsert, QString qsInsertMirror)
{
    if (!IsValidType(nType))
        return false;
    m_qsDescList[nType] = qsInsert;
    m_qsDescMirrorList[nType] = qsInsertMirror;
    return true;
}

QString QDropDescription::GetDescText(DROPIMAGETYPE nType, bool bMirror) const
{
    if (!IsValidType(nType))
        return QString();

    if (bMirror)
        return m_qsDescMirrorList[nType];
    else
        return m_qsDescList[nType];
}

void QDropDescription::SetDescription(DROPDESCRIPTION *pDropDescription, DROPIMAGETYPE nType)
{
    SetDescription(pDropDescription, GetDescText(nType, HasInsert()));
}

void QDropDescription::SetDescription(DROPDESCRIPTION *pDropDescription, QString qsMsg)
{
    wcsncpy_s(pDropDescription->szInsert, ARRAYSIZE(pDropDescription->szInsert), 
        (WCHAR*)m_qsInsert.utf16(), _TRUNCATE);
    wcsncpy_s(pDropDescription->szMessage, ARRAYSIZE(pDropDescription->szMessage),
        (WCHAR *)qsMsg.utf16(), _TRUNCATE);
}
QT_END_NAMESPACE