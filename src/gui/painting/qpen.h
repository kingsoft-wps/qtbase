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

#ifndef QPEN_H
#define QPEN_H

#include <QtGui/qtguiglobal.h>
#include <QtGui/qcolor.h>
#include <QtGui/qbrush.h>

QT_BEGIN_NAMESPACE


class QVariant;
class QPenPrivate;
class QBrush;
class QPen;
class QCustomLineAnchor;

#ifndef QT_NO_DATASTREAM
Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QPen &);
Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QPen &);
#endif

class Q_GUI_EXPORT QPen
{
public:
    QPen();
    QPen(Qt::PenStyle);
    QPen(const QColor &color);
    QPen(const QBrush &brush, qreal width, Qt::PenStyle s = Qt::SolidLine,
         Qt::PenCapStyle c = Qt::SquareCap, Qt::PenJoinStyle j = Qt::BevelJoin);
    QPen(const QPen &pen) Q_DECL_NOTHROW;

    ~QPen();

    QPen &operator=(const QPen &pen) Q_DECL_NOTHROW;
#ifdef Q_COMPILER_RVALUE_REFS
    QPen(QPen &&other) Q_DECL_NOTHROW
        : d(other.d) { other.d = nullptr; }
    QPen &operator=(QPen &&other) Q_DECL_NOTHROW
    { qSwap(d, other.d); return *this; }
#endif
    void swap(QPen &other) Q_DECL_NOTHROW { qSwap(d, other.d); }

    Qt::PenStyle style() const;
    void setStyle(Qt::PenStyle);

    QVector<qreal> dashPattern() const;
    void setDashPattern(const QVector<qreal> &pattern);

    qreal dashOffset() const;
    void setDashOffset(qreal doffset);

    qreal miterLimit() const;
    void setMiterLimit(qreal limit);

    qreal widthF() const;
    void setWidthF(qreal width);

    int width() const;
    void setWidth(int width);

    QColor color() const;
    void setColor(const QColor &color);

    QBrush brush() const;
    void setBrush(const QBrush &brush);

    bool isSolid() const;

    Qt::PenCapStyle capStyle() const;
    void setCapStyle(Qt::PenCapStyle pcs);

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

    bool isCosmetic() const;
    void setCosmetic(bool cosmetic);

    bool operator==(const QPen &p) const;
    inline bool operator!=(const QPen &p) const { return !(operator==(p)); }
    operator QVariant() const;

    bool isDetached();

    void setDrawCustomTextBold(bool bCustomBold);
    bool isDrawCustomTextBold() const;

    void setDrawPdfTextBoldContent(bool bDrawPdfTextBoldContent);
    bool isDrawPdfTextBoldContent() const;

    void setCustomBoldFactor(qreal factor);
    qreal customBoldFactor();

    void setSupportComoplex(bool bSupport);
    bool isSupportComoplex() const;
    
private:
    friend Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QPen &);
    friend Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QPen &);

    void detach();
    class QPenPrivate *d;

public:
    typedef QPenPrivate * DataPtr;
    inline DataPtr &data_ptr() { return d; }
};

Q_DECLARE_SHARED(QPen)

#ifndef QT_NO_DEBUG_STREAM
Q_GUI_EXPORT QDebug operator<<(QDebug, const QPen &);
#endif

QT_END_NAMESPACE

#endif // QPEN_H
