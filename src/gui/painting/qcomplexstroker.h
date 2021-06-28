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

#ifndef QCOMPLEXSTROKER_H
#define QCOMPLEXSTROKER_H

#include <QtGui/qtguiglobal.h>
#include <QtCore/qrect.h>
#include <QtGui/qpainterpath.h>

QT_BEGIN_NAMESPACE

class QPen;
class QCustomLineAnchorState;
class QCustomLineAnchor;

class QComplexStrokerPrivate;

class Q_GUI_EXPORT QComplexStroker
{
    Q_DECLARE_PRIVATE(QComplexStroker)
public:
    QComplexStroker();
    QComplexStroker(const QComplexStroker &);
    ~QComplexStroker();

    QComplexStroker &operator=(const QComplexStroker &);

    QVector<qreal> dashPattern() const;
    void setDashPattern(const QVector<qreal> &pattern);

    qreal dashOffset() const;
    void setDashOffset(qreal doffset);

    qreal miterLimit() const;
    void setMiterLimit(qreal limit);

    qreal width() const;
    void setWidth(qreal width);

    Qt::PenJoinStyle joinStyle() const;
    void setJoinStyle(Qt::PenJoinStyle pcs);

    QVector<qreal> compoundArray() const;
    void setCompoundArray(const QVector<qreal> &pattern);

    Qt::PenAnchorStyle startAnchorStyle() const;
    void setStartAnchorStyle(Qt::PenAnchorStyle);
    const QCustomLineAnchor &startAnchor() const;
    void setStartAnchor(const QCustomLineAnchor &);

    Qt::PenAnchorStyle endAnchorStyle() const;
    void setEndAnchorStyle(Qt::PenAnchorStyle);
    const QCustomLineAnchor &endAnchor() const;
    void setEndAnchor(const QCustomLineAnchor &);

    Qt::PenAlignment alignment() const;
    void setAlignment(Qt::PenAlignment alignment);

    Qt::PenCapStyle startCapStyle() const;
    void setStartCapStyle(Qt::PenCapStyle s);

    Qt::PenCapStyle endCapStyle() const;
    void setEndCapStyle(Qt::PenCapStyle s);

    Qt::PenCapStyle dashCapStyle() const;
    void setDashCapStyle(Qt::PenCapStyle s);

    QRectF getClipRect() const;
    void setClipRect(const QRectF& rc);

    QPainterPath createStroke(const QPainterPath &path, const qreal flatness = 0.25) const;

    static QComplexStroker fromPen(const QPen &);

private:
    void detach();

    QComplexStrokerPrivate *d_ptr;
};

QT_END_NAMESPACE

#endif // QCOMPLEXSTROKER_H