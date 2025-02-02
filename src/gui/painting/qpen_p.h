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

#ifndef QPEN_P_H
#define QPEN_P_H

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

#include <QtCore/qglobal.h>
#include <qcustomlineanchor.h>

QT_BEGIN_NAMESPACE

class QPenPrivate {
public:
    QPenPrivate(const QBrush &brush, qreal width, Qt::PenStyle, Qt::PenCapStyle,
                Qt::PenJoinStyle _joinStyle, bool defaultWidth = true);
    QAtomicInt ref;
    qreal width;
    QBrush brush;
    Qt::PenStyle style;
    Qt::PenCapStyle capStyle;
    Qt::PenJoinStyle joinStyle;
    mutable QVector<qreal> dashPattern;
    qreal dashOffset;
    qreal miterLimit;
    QVector<qreal> compoundArray;
    Qt::PenAnchorStyle startAnchorStyle;
    QCustomLineAnchor startAnchor;
    Qt::PenAnchorStyle endAnchorStyle;
    QCustomLineAnchor endAnchor;
    Qt::PenAlignment alignment;
    Qt::PenCapStyle startCap;
    Qt::PenCapStyle endCap;
    Qt::PenCapStyle dashCap;
    uint cosmetic : 1;
    uint defaultWidth : 1; // default-constructed width? used for cosmetic pen compatibility
    bool bDrawCustomTextBold;
    bool bSupportComoplex;
    uint bDrawPdfTextBoldContent : 1;
    qreal boldFactor;
};

inline bool qpen_is_complex(const QPen &pen)
{
    auto ptr = const_cast<QPen &>(pen).data_ptr();
    return (ptr->alignment != Qt::PenAlignmentCenter
        || ptr->startAnchorStyle != Qt::SquareAnchor
        || ptr->endAnchorStyle != Qt::SquareAnchor
        || ptr->startCap > Qt::RoundCap
        || ptr->endCap > Qt::RoundCap
        || ptr->dashCap >= Qt::RoundCap
        || ptr->startCap != ptr->endCap
        || ptr->startCap != ptr->dashCap
        || !ptr->compoundArray.isEmpty());
}

QT_END_NAMESPACE

#endif // QPEN_P_H
