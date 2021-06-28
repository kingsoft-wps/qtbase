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

#ifndef QDESCRIPTION_H
#define QDESCRIPTION_H
#include <QtCore/qt_windows.h>
#include <QtCore/qglobal.h>
#include <QString>
#include <QVector>
QT_BEGIN_NAMESPACE

class QDropDescription
{
public:
    static const DROPIMAGETYPE nMaxDropImage = DROPIMAGE_NOIMAGE;

    inline bool IsValidType(DROPIMAGETYPE nType) const
    {
	    return (nType >= DROPIMAGE_NONE && nType <= nMaxDropImage);
    }

    inline QString GetInsert() const { return m_qsInsert; }
    void SetInsert(QString qsInsert) { m_qsInsert = qsInsert;}

    QString GetDescText(DROPIMAGETYPE nType, bool bMirror) const;
    bool SetDescText(DROPIMAGETYPE nType, QString qsInsert, QString qsInsertMirror);
   
    void SetDescription(DROPDESCRIPTION *pDropDescription, DROPIMAGETYPE nType);
    void SetDescription(DROPDESCRIPTION *pDropDescription, QString qsMsg);


    inline bool HasText(DROPIMAGETYPE nType) const {
        return IsValidType(nType) && m_qsDescList[nType].length();
    }

    inline bool IsTextEmpty(DROPIMAGETYPE nType) const {
        return !HasText(nType) || m_qsDescList[nType].isEmpty();
    }

    inline bool HasInsert() const { return !m_qsInsert.isEmpty(); }
    inline bool HasInsert(const DROPDESCRIPTION *pDropDescription) const
    {
	    return pDropDescription && wcslen(pDropDescription->szInsert);
    }

protected:
    QString m_qsInsert;
    QVector<QString> m_qsDescList = QVector<QString>(nMaxDropImage);
    QVector<QString> m_qsDescMirrorList = QVector<QString>(nMaxDropImage);
};

QT_END_NAMESPACE
#endif // !QDESCRIPTION_H