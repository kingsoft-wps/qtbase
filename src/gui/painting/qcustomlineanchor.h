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

#ifndef QCUSTOMLINEANCHOR_H
#define QCUSTOMLINEANCHOR_H

#include <QtGui/qtguiglobal.h>
#include <QtGui/qpainterpath.h>

QT_BEGIN_NAMESPACE

class QCustomLineAnchorState;
class QCustomLineAnchor;

#ifndef QT_NO_DATASTREAM
Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QCustomLineAnchor &);
Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QCustomLineAnchor &);
#endif

class Q_GUI_EXPORT QCustomLineAnchor 
{
public:
    enum AnchorPathUsage {
        PathFill,
        PathStroke,
    };

    QCustomLineAnchor();
    QCustomLineAnchor(Qt::PenAnchorStyle anchor);
    QCustomLineAnchor(const QPainterPath &anchorPath, AnchorPathUsage usage, Qt::PenCapStyle baseCap = Qt::FlatCap, qreal baseInset = 0);
    QCustomLineAnchor(const QCustomLineAnchor &);
    ~QCustomLineAnchor();
    QCustomLineAnchor &operator=(const QCustomLineAnchor &);

    Qt::PenCapStyle strokeStartCap() const;
    void setStrokeStartCap(Qt::PenCapStyle startCap);
    Qt::PenCapStyle strokeEndCap() const;
    void setStrokeEndCap(Qt::PenCapStyle endCap);
    Qt::PenJoinStyle strokeJoin() const;
    void setStrokeJoin(Qt::PenJoinStyle lineJoin);

    Qt::PenCapStyle baseCap() const;
    void setBaseCap(Qt::PenCapStyle baseCap);
    qreal baseInset() const;
    void setBaseInset(qreal inset);

    qreal widthScale() const;
    void setWidthScale(qreal widthScale);

    void setFlatness(qreal flatness);

    QPainterPath capPath() const;

    bool isValid() const;

    bool operator==(const QCustomLineAnchor &p) const;
    inline bool operator!=(const QCustomLineAnchor &p) const {return !(operator==(p));};

private:
    QCustomLineAnchor(QCustomLineAnchorState *capState);
    void reset(QCustomLineAnchorState *capState);
    static QCustomLineAnchorState *createLineCapState(
        const QPainterPath &anchorPath, AnchorPathUsage usage);

private:
    QCustomLineAnchorState *m_cap;

    friend Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QCustomLineAnchor &);
    friend Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QCustomLineAnchor &);

public:
    typedef const QCustomLineAnchorState *DataPtr;
    inline DataPtr data_ptr() const { return m_cap; }
};

QT_END_NAMESPACE

#endif // QCUSTOMLINEANCHOR_H