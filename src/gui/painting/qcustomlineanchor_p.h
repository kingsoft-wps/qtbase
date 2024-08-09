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

#ifndef QCUSTOMLINEANCHOR_P_H
#define QCUSTOMLINEANCHOR_P_H

QT_BEGIN_NAMESPACE

class QVertices;
struct vertex_dist;

class QAnchorGenerator
{
public:
    QAnchorGenerator(const QCustomLineAnchor& startCap,
                     const QCustomLineAnchor& endCap,
                     qreal width);

    void Generate(const QPainterPath& path2generate,
        QPainterPath& pathAfterGenerate,
        QPainterPath& generatedCapPath,
        const qreal flatness) const;

private:
    static QCustomLineAnchor *GetLineCap(Qt::PenAnchorStyle as, const QCustomLineAnchor& customLineCap);

    // for open sub path
    void GenerateCap(const QPointF *pts, const unsigned count,
        QPainterPath& pathAfterGenerate,
        QPainterPath& generatedCapPath) const;

private:
    Q_DISABLE_COPY(QAnchorGenerator)
    qreal m_width;
    qreal m_flatness;
    
    const QCustomLineAnchorState* m_pCustomStartCap;
    const QCustomLineAnchorState* m_pCustomEndCap;
};

class QCustomLineAnchorState 
{
private:
    Q_DISABLE_COPY(QCustomLineAnchorState)

public:
    enum AnchorType {AnchorTypeFill, AnchorTypeStroke};

    QCustomLineAnchorState();
    virtual ~QCustomLineAnchorState();

    virtual QCustomLineAnchorState *Clone() const = 0;
    virtual qreal GetDevideDistance(qreal width) const = 0;
    virtual void GenerateCap(qreal width, const QPointF& fromPt, const QPointF& toPt,
    const QPointF& centerPt, QPainterPath& capPath) const = 0;
    virtual qreal GetMaxDistance(qreal width) const = 0;
    virtual QPainterPath GetCapPath() const = 0;
    virtual AnchorType GetAnchorType() const = 0;

    qreal baseInset() const;
    qreal widthScale() const;
    Qt::PenCapStyle strokeStartCap() const;
    void setStrokeStartCap(Qt::PenCapStyle startCap);
    Qt::PenCapStyle strokeEndCap() const;
    void setStrokeEndCap(Qt::PenCapStyle endCap);
    Qt::PenJoinStyle strokeJoin() const;
    Qt::PenCapStyle baseCap() const;
    void setBaseInset(qreal inset);
    void setWidthScale(qreal widthScale);
    void setStrokeJoin(Qt::PenJoinStyle lineJoin);
    void setBaseCap(Qt::PenCapStyle baseCap);
    qreal calcInsetScale(qreal penWidth) const;
    bool GetDevidePoint(qreal width, const QPointF *pts, unsigned count, QPointF &devidePt, int &prevPtOffset) const;

    void setFlatness(qreal flatness);

protected:
    void CalcTransform(qreal width, const QPointF& fromPt, const QPointF& toPt, const QPointF& centerPt, QMatrix& mtx) const;
    bool CalcCrossYPts(const QPainterPath& path, QVector<qreal>& dists, qreal width = 1) const;
    void CopyTo(QCustomLineAnchorState& capState) const;
    static void CalcCrossYPt(const QPointF& pt1, const QPointF& pt2, int flag1, int flag2, QVector<qreal>& dists);

protected:
    qreal m_baseInset;
    qreal m_widthScale;
    Qt::PenCapStyle m_strokeStartCap, m_strokeEndCap;
    Qt::PenJoinStyle m_strokeLineJoin;
    Qt::PenCapStyle	m_baseCap;
    qreal m_flatness;
};


class QCustomFillAnchor : public QCustomLineAnchorState
{
public:
    QCustomFillAnchor(const QPainterPath& path);
    virtual ~QCustomFillAnchor();
    virtual QCustomLineAnchorState *Clone() const;
    virtual qreal GetDevideDistance(qreal width) const;
    virtual void GenerateCap(qreal width, const QPointF& fromPt, const QPointF& toPt,
    const QPointF& centerPt, QPainterPath& capPath) const;
    virtual qreal GetMaxDistance(qreal width) const;
    virtual QPainterPath GetCapPath() const {return m_capPath;}
    virtual AnchorType GetAnchorType() const {return AnchorTypeFill;}

private:
    QPainterPath m_capPath;
};


class QCustomStrokeAnchor : public QCustomLineAnchorState
{
public:
    QCustomStrokeAnchor(const QPainterPath& path);
    virtual ~QCustomStrokeAnchor();
    virtual QCustomLineAnchorState *Clone() const;
    virtual qreal GetDevideDistance(qreal width) const;
    virtual void GenerateCap(qreal width, const QPointF& fromPt, const QPointF& toPt,
    const QPointF& centerPt, QPainterPath& capPath) const;
    virtual qreal GetMaxDistance(qreal width) const;
    virtual QPainterPath GetCapPath() const {return m_capPath;}
    virtual AnchorType GetAnchorType() const {return AnchorTypeStroke;}
    QPointF FindNestestPoint(const QPointF& pt, const QPainterPath& path) const;
private:
    QPainterPath m_capPath;
};

QT_END_NAMESPACE

#endif // QCUSTOMLINEANCHOR_P_H