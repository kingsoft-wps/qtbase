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

#ifndef QCOMPLEXSTROKER_P_H
#define QCOMPLEXSTROKER_P_H

#include <QtGui/private/qtguiglobal_p.h>
#include <QVector>

QT_BEGIN_NAMESPACE

class QVertices;
struct vertex_dist;
template<typename T>
class QUnshareVector;

class QMathStroker
{
public:
    explicit QMathStroker();

    qreal GetWidth() const;
    void SetWidth(qreal width);
    Qt::PenJoinStyle GetLineJoin() const;
    void SetLineJoin(Qt::PenJoinStyle linejoin);
    qreal GetMiterLimit() const;
    void SetMiterLimit(qreal miterlimit);
    Qt::PenCapStyle GetStartCap() const;
    Qt::PenCapStyle GetEndCap() const;
    void SetLineCap(Qt::PenCapStyle startCap,  Qt::PenCapStyle endCap);
    void SetStartCap(Qt::PenCapStyle startcap);
    void SetEndCap(Qt::PenCapStyle endcap);


    int  GetCompoundArrayCount() const;
    bool GetCompoundArray(qreal *compoundArray,  int count) const;
    bool SetCompoundArray(const qreal *compoundArray,  int count);

    Qt::PenAlignment GetAlignment() const;
    void SetAlignment(Qt::PenAlignment penAlignment);

    void GetScale(qreal *sx, qreal *sy) const;
    void SetScale(qreal sx, qreal sy);

    void StrokePath(const QList<QPolygonF>& path2stroke, QPainterPath& outPath) const;
    void StrokePath(const QPainterPath& path2stroke, QPainterPath& outPath, const qreal flatness = 0.25) const;

private:	
    void StrokeSubPath(const QPolygonF &subPath, QPainterPath& outPath) const;
    void StrokeCloseSubPath(const QPolygonF &subPath, QPainterPath &outPath) const;
    void StrokeOpenSubPath(const QPolygonF &subPath, QPainterPath &outPath) const;

    void CalcPoints(const QVertices& vertices, int first, int second, int third, 
        QUnshareVector<QPointF>& firstPts, QUnshareVector<QPointF>& secondPts,
        QVector<QVector<QPointF> > & compoundPts) const; 

    void CalcStartPoints(const QVertices& vertices, 
        QUnshareVector<QPointF>& firstPts, QUnshareVector<QPointF>& secondPts,
        QVector<QVector<QPointF> > & compoundPts, QUnshareVector<QPointF>& startCapPts) const;
    void CalcEndPoints(const QVertices& vertices,
        QUnshareVector<QPointF>& firstPts, QUnshareVector<QPointF>& secondPts,
        QVector<QVector<QPointF> > & compoundPts, QUnshareVector<QPointF>& endCapPts) const;

    void CalcStartPoints(const QPointF& p0, const QPointF& p1, qreal len01, QPointF& firstPt, QPointF& secondPt) const;
    void CalcStartPoints(QPointF& firstPt, QPointF& secondPt, const QPointF& p0, const QPointF& p1, const QPointF& p2, 
                        qreal len01, qreal len12) const;
    void CalcEndPoints(const QPointF& p0, const QPointF& p1, qreal len01, QPointF& firstPt, QPointF& secondPt) const;
    void CalcEndPoints(QPointF& firstPt, QPointF& secondPt, const QPointF& p0, const QPointF& p1, const QPointF& p2, 
                        qreal len01, qreal len12) const;

    void CalcCompoundPoints(const QVector<QPointF>& firstpts, const QVector<QPointF>& secondpts, QVector<QVector<QPointF> >& compoundpts) const;

    void ScalePoints(QPointF *pts, int nCount, const QPointF& centerPt) const;

    void CalcCap(const QPointF& p0, const QPointF& p1, qreal len01, Qt::PenCapStyle lineCap, QUnshareVector<QPointF>& capPts) const;
    void CalcJoin(QUnshareVector<QPointF>& firstpts, QUnshareVector<QPointF>& secondpts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal len01, qreal len12) const;

    void CalcCapFlat(const QPointF& p0, const QPointF& p1, qreal dx, qreal dy, QUnshareVector<QPointF>& capPts) const;
    void CalcCapSquare(const QPointF& p0, const QPointF& p1, qreal dx, qreal dy, QUnshareVector<QPointF>& capPts) const;
    void CalcCapTriangle(const QPointF& p0, const QPointF& p1, qreal dx, qreal dy, QUnshareVector<QPointF>& capPts) const;
    void CalcCapRound(const QPointF& p0, const QPointF& p1, qreal dx, qreal dy, QUnshareVector<QPointF>& capPts) const;

    void CalcInnerJoin(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2, qreal len01, qreal len12) const;
    void CalcOuterJoin(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;	
    void CalcStraightJoin(QUnshareVector<QPointF>& firstpts, QUnshareVector<QPointF>& secondpts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;

    void CalcLineJoinMiter(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;
    void CalcLineJoinMiterClipped(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;
    void CalcLineJoinBevel(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;
    void CalcLineJoinRound(QUnshareVector<QPointF>& pts, const QPointF& p0, const QPointF& p1, const QPointF& p2, qreal dx1, qreal dy1, qreal dx2, qreal dy2) const;

private:
    Q_DISABLE_COPY(QMathStroker)
    qreal	 m_width;
    Qt::PenJoinStyle m_lineJoin;
    qreal	 m_miterLimit;
    Qt::PenCapStyle	 m_startCap;
    Qt::PenCapStyle  m_endCap;
    QVector<qreal> m_compoundArray;
    Qt::PenAlignment m_penAlignment; 

    qreal m_scaleX, m_scaleY;
};

class QSimplePolygon
{
public:
    QSimplePolygon(const QPolygonF &polygon);

    bool IsValid() const;
    bool IsClosed() const;
    bool IsClockWise() const;
    unsigned GetPointsCount() const;
    const QVector<QPointF> &GetPoints() const;

    bool GetZoomPoints(QVector<QPointF>& pts, qreal offset) const;

private:
    static bool EraseStraightPointsOnPolygon(QVector<QPointF>& pts);

    static void GetEdgeOffsetPts(const QPointF& pt0, const QPointF& pt1, qreal offset, QLineF& newLine);

private:
    QVector<QPointF> m_pts;
    bool m_bClosed;
    bool m_bClockWise;
};

class QPathZoomer
{
public:
    explicit QPathZoomer(qreal offset = 0);
    void SetOffset(qreal offset);
    qreal GetOffSet() const;
    void ZoomPath(const QPainterPath& path2zoom, QPainterPath& pathAfterZoom, const qreal flatness) const;

private:
	static void GetEdgeOffsetPts(const QPointF& pt0, const QPointF& pt1, qreal offset, QLineF& newLine);

private:
    qreal m_offset;
};


class QPathDasher
{
public:
    QPathDasher();

    qreal GetWidth() const;
    int GetDashPatternCount() const;
    bool GetDashPattern(qreal *dashArray, int count) const;
    qreal GetDashOffset() const;

    bool SetDashPattern(const qreal* dasharray, int count);
    void SetDashOffset(qreal dashoffset);
    void SetWidth(qreal width);

    QRectF GetClipRect() const;
    void SetClipRect(const QRectF& rc);
    
    void GetDashedPath(const QPainterPath& path2dash,  QPainterPath& dashedPath, const qreal flatness) const;
    void GetDashedPath(const QPainterPath& path2dash,  QPainterPath& openStartPath,
        QPainterPath& openMiddleAndClosePath, QPainterPath& openEndPath, const qreal flatness) const;
    
private:
    void GetDashedPath(const QPolygonF& subPath, QPainterPath& outPath) const;

    void GenerateDash(const QVertices& vertices, QPainterPath& outPath) const;
    
    void CalcDashStart(qreal& currentDashLen, int& currentDashIndex, double totalLen) const;
    void IncCurrentDash(int& currentDashIndex, qreal& currentDashLen, const QVector<qreal>& pattern, qreal width) const;
    void AddStart(const vertex_dist& v0, const vertex_dist& v1, qreal dist, QPainterPath& outPath) const;
    void AddPoint(const vertex_dist& v0, const vertex_dist& v1, qreal dist, QPainterPath& outPath) const;

private:
    qreal m_dashOffset;
    QVector<qreal> m_dashPattern;

    qreal m_width;
    QRectF m_clipRect;
};

class QComplexStrokerPrivate
{
public:
    QComplexStrokerPrivate();
    bool isSolid() const;
    qreal alignmentOffset();

    QMathStroker *prepareMathStroker();
    QPathDasher *prepareDasher();

    QAtomicInt ref;
    qreal width;
    QVector<qreal> dashPattern;
    Qt::PenJoinStyle joinStyle;
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
    QRectF clipRect;
};

QT_END_NAMESPACE

#endif // QCOMPLEXSTROKER_P_H