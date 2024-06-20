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

#ifndef QPATHGRADIENT_P_H
#define QPATHGRADIENT_P_H

#include <algorithm>
#include <private/qsimd_p.h>
#include <qmath.h>

QT_BEGIN_NAMESPACE

extern qreal calc_distance(qreal x1, qreal y1, qreal x2, qreal y2);

inline void matrix_multiply (QMatrix *result, const QMatrix *a, const QMatrix *b)
{
    qreal m11 = a->m11() * b->m11() + a->m12() * b->m21();
    qreal m12 = a->m11() * b->m12() + a->m12() * b->m22();

    qreal m21 = a->m21() * b->m11() + a->m22() * b->m21();
    qreal m22 = a->m21() * b->m12() + a->m22() * b->m22();

    qreal dx = a->dx() * b->m11() + a->dy() * b->m21() + b->dx();
    qreal dy = a->dx() * b->m12() + a->dy() * b->m22() + b->dy();

    result->setMatrix(m11, m12, m21, m22, dx, dy);
}

inline void TransformPoints(const QMatrix &tmtx, QPointF *pts, 
                     int count = 1)
{
    Q_ASSERT(NULL != pts && count > 0);

    for (int i = 0; i < count ; i++)
    {
        QPointF pt(pts[i].x(), pts[i].y());
        pts[i] = tmtx.map(pt);
    }
}

inline void GetPathPoints(const QPainterPath &qpath, QPointF *pts, int nCount)
{
    Q_ASSERT(NULL != pts && nCount > 0);
    nCount = qMin(nCount, qpath.elementCount());
    for (int i = 0; i < nCount ; i++)
    {
        pts[i] = (QPointF)qpath.elementAt(i);
    }
}

inline void Transform(QPainterPath &qpath, const QMatrix &matrix)
{
    if (matrix.isIdentity())
        return;

    int nCount = qpath.elementCount();
    for (int i = 0; i < nCount; i++)
    {
        QPainterPath::Element item = qpath.elementAt(i);
        QPointF pt = matrix.map(QPointF(item));
        qpath.setElementPositionAt(i, pt.x(), pt.y());
    }
}

inline void Flatten(QPainterPath &qpath)
{
    QPolygonF poly = qpath.toFillPolygon();
    QPainterPath path;
    path.addPolygon(poly);
    qpath = path;
}

inline int GetInterpolationColors(const QGradientStops& stops, 
                                  QColor* presetColors,
                                  qreal* blendPositions, 
                                  int count)
{
    Q_ASSERT(NULL != presetColors && NULL !=blendPositions);
    Q_ASSERT(count > 0);

    count = qMin(count, stops.size());
    for (int i = 0; i < count; i++)
    {
        Q_ASSERT(0 <= stops[i].first && 1 >= stops[i].first);

        blendPositions[count - 1 - i] = 1 - stops[i].first;
        presetColors[count - 1 - i] = stops[i].second;
    }

    return count;
}

template<class ColorT>
ColorT &Convert2ARGB8(const QColor& from, ColorT &clr)
{
    return clr = ColorT(from.alpha(), from.red(), from.green(), from.blue());
}

inline void TransformX(const QMatrix& mtx, qreal* x, qreal* y)
{
    *x = *x * mtx.m11() + *y * mtx.m21() + mtx.dx();
}

class Q_GUI_EXPORT QPathGradientBrush
{
public:
    QPainterPath m_path;
    QPointF m_centerPoint;
    //position 0.0 for the center color and 1.0 for the boundary
    QGradientStops m_stops;
    QMatrix m_mtx;
    qreal m_xscale, m_yscale;

public:
    QPathGradientBrush(const QPainterPath& path,
        const QPointF& centerPt,
        const QGradientStops& stops,
        const QMatrix& mtx,
        qreal xscale = 0,
        qreal yscale = 0):
        m_path(path),
        m_centerPoint(centerPt),
        m_stops(stops),
        m_mtx(mtx),
        m_xscale(xscale),
        m_yscale(yscale)
    {
        Q_ASSERT(path.elementCount() > 2);
        Q_ASSERT(m_stops.size() >= 2);
        Q_ASSERT(m_stops.front().first == 0 && m_stops.back().first == 1);
    }
};

class Q_GUI_EXPORT argb8
{
    typedef unsigned char value_type;
    typedef unsigned int calc_type;
    enum base_scale_e
    {
        base_shift = 8,
        base_scale = 1 << base_shift,
    };

public:
    //don't disorder, must be 0xaarrggbb in unsigned int
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    value_type b, g, r, a;
#else
    value_type a, r, g, b;
#endif

public:
    argb8()
     {
        a = 255;
        r = 0;
        g = 0;
        b = 0;
     }
    argb8(unsigned r_, unsigned g_, unsigned b_)
    {
        a = 255;
        r = value_type(r_);
        g = value_type(g_);
        b = value_type(b_);
    }
    argb8(unsigned a_, unsigned r_, unsigned g_, unsigned b_)
    {
        a = value_type(a_);
        r = value_type(r_);
        g = value_type(g_);
        b = value_type(b_);
    }
    argb8(const argb8 &other)
    {
        memcpy(this, &other, sizeof(argb8));
    }

public:
    inline const argb8& premultiply()
    {
        return *this;
    }
    inline const argb8& demultiply()
    {
        return *this;
    }
    inline argb8 gradient(const argb8& c, double k) const
    {
        if (*this == c)
            return c;

        calc_type ik = calc_type(k * base_scale + 0.5);

        if (ik <= 0)
            return *this;
        if (ik >= base_scale)
            return c;

        argb8 ret;
        ret.a = (value_type)(calc_type(a) + (((calc_type(c.a) - a) * ik) >> base_shift));
        ret.r = (value_type)(calc_type(r) + (((calc_type(c.r) - r) * ik) >> base_shift));
        ret.g = (value_type)(calc_type(g) + (((calc_type(c.g) - g) * ik) >> base_shift));
        ret.b = (value_type)(calc_type(b) + (((calc_type(c.b) - b) * ik) >> base_shift));
        
        return ret;
    }
    inline bool operator==(const argb8 &other) const
    {
        return !memcmp(&other, this, sizeof(other));
    }
    inline argb8& operator=(const argb8 &other)
    {
        memcpy(this, &other, sizeof(argb8));

        return *this;
    }
};

// -------------------------------------------------------------------------
template <typename ColorT>
class InterpolationColor
{
public:
    class InterColorNode
    {
    public:
        InterColorNode(qreal offset = 0, const ColorT& pcolor = ColorT())
            : m_offset(offset)
            , m_pcolor(pcolor)
        {
            Q_ASSERT(m_offset >= 0 && m_offset <= 1);
        }
        
        qreal offset() const
        {
            return m_offset;
        }

        ColorT pcolor() const
        {
            return m_pcolor;
        }
        
        bool operator < (const InterColorNode& r) const
        {
            return m_offset < r.m_offset;
        }
        
        bool operator == (const InterColorNode& r) const
        {
            return m_offset == r.m_offset;
        }
        
    private:
        qreal m_offset;
        ColorT m_pcolor;
    };

public:
    InterpolationColor(const QVector<qreal>& inter_offset,
                       const QVector<ColorT>& inter_colors)
    {
        Q_ASSERT(inter_offset.size() == inter_colors.size());
        Q_ASSERT(inter_offset.size() >= 2);
        Q_ASSERT(inter_offset.front() == 0 && inter_offset.back() == 1);

        const int s = inter_offset.size();
        for (int i = 0; i < s; ++i)
        {
            ColorT pc = inter_colors[i];
            pc.premultiply();
            m_interColors.push_back(InterColorNode(inter_offset[i], pc));
        }

        std::unique(m_interColors.begin(), m_interColors.end());
        std::sort(m_interColors.begin(), m_interColors.end());
    }

    ColorT GetColor(qreal offset) const
    {
        Q_ASSERT(offset >= 0 && offset <= 1);
        const int s = m_interColors.size();
        for (int i = 1; i < s; ++i)
        {
            if (offset <= m_interColors[i].offset())
            {
                offset = (offset - m_interColors.at(i-1).offset()) / 
                    (m_interColors.at(i).offset() - m_interColors.at(i-1).offset());
                const ColorT& pc1 = m_interColors.at(i-1).pcolor();
                const ColorT& pc2 = m_interColors.at(i).pcolor();
                return pc1.gradient(pc2, offset).demultiply();
            }
        }
        return m_interColors.back().pcolor().demultiply();
    }

private:
    QVector<InterColorNode> m_interColors;
};

// -----------------------------------------------------------gradient_color
struct gradient_color
{
public:
    typedef argb8 color_type;
    
    enum color_scale
    {
        color_base_scale     = 8,
        color_base_num       = 1 << color_base_scale,
        color_base_mask      = color_base_num - 1,
        color_base_num_flipx = color_base_num << 1
    };
    
    // ---------------------------------------------------------------------
    gradient_color()
    {}
    
    // ---------------------------------------------------------------------
    gradient_color(const color_type& cl1, 
                   const color_type& cl2,
                   bool bflipx)
                   : m_bFlipX(bflipx)
    {
        generate_gradient(cl1, cl2, bflipx);
    }

    // ---------------------------------------------------------------------
    gradient_color(const color_type& cl1,
                   const color_type& cl2,
                   const QVector<qreal>&      inter_offset,
                   const QVector<color_type>&inter_colors, 
                   bool  bflipx)
                   : m_bFlipX(bflipx)
    {
        generate_inter(cl1, cl2, inter_offset, inter_colors, bflipx);
    }

    // ---------------------------------------------------------------------
    const color_type &operator [] (unsigned v) const
    {
        return m_clr[v];
    }

    // ---------------------------------------------------------------------
    bool is_flipx() const
    {
        return m_bFlipX;
    }

private:
    void generate_same(const color_type& cl, bool bFlipX)
    {
        Q_ASSERT(sizeof(color_type) == 4);
        const int cv = *((int*)(&cl));
        const int vl = bFlipX ? color_base_num_flipx : color_base_num;
        memset((int*)m_clr, cv, vl);
    }

    void generate_diff(const color_type& cl1,
                       const color_type& cl2,
                       bool  bFlipX)
    {
        color_type c1(cl1), c2(cl2);
        c1.premultiply();
        c2.premultiply();
        
        static const qreal spd = 1.0f / 256.0f;
        if (bFlipX)
        {	
            qreal scale = 0;
            for (unsigned i = 0; i < color_base_num; ++i)
            {
                m_clr[i] = 
                m_clr[color_base_num_flipx - i - 1] = 
                c1.gradient(c2, scale).demultiply(); 
                scale = scale + spd;
            }
        }
        else
        {
            qreal scale = 0;
            for (unsigned i = 0; i < color_base_num; ++i)
            {
                m_clr[i] = 
                c1.gradient(c2, scale).demultiply(); 
                scale = scale + spd;
            }
        }
    }

    void generate_gradient(const color_type& cl1,
                           const color_type& cl2,
                           bool  bFlipX)
    {
        if (is_same_color(cl1, cl2))
        {
            generate_same(cl1, bFlipX);
        }
        else
        {
            generate_diff(cl1, cl2, bFlipX);
        }
    }

    void generate_inter(const color_type& cl1,
                        const color_type& cl2,
                        const QVector<qreal>& inter_offset,
                        const QVector<color_type>& inter_colors,
                        bool  bFlipX)
    {
        if (!inter_offset.empty())
        {
            InterpolationColor<color_type> ic(inter_offset, inter_colors);
            static const qreal spd = 1.0f / 256.0f;
            qreal offset = 0;
            for (int i = 0; i < color_base_num; ++i)
            {
                if (bFlipX)
                {
                    m_clr[i] = 
                    m_clr[color_base_num_flipx-i-1] = ic.GetColor(offset);
                }
                else
                {
                    m_clr[i] = ic.GetColor(offset);
                }
                offset += spd;
            }
        }
        else
        {
            generate_gradient(cl1, cl2, bFlipX);
        }
    }

    bool is_same_color(const color_type& cl1,
                       const color_type& cl2)
    {
        Q_ASSERT(sizeof(color_type) == 4);
        return *((int*)(&cl1)) == *((int*)(&cl2));
    }
    
private:
    bool m_bFlipX;
    color_type m_clr[color_base_num_flipx];
};

class SpanGenerator
{
public:
    typedef argb8 ColorType;
    SpanGenerator()
    {
    }

    virtual ~SpanGenerator()
    {
    }

    // ---------------------------------------------------------------------
    virtual void prepare() = 0;

    // ---------------------------------------------------------------------
    virtual void generate(ColorType *span, int x, int y, unsigned len) = 0;

private:
    SpanGenerator(const SpanGenerator&);
    const SpanGenerator& operator=(const SpanGenerator&);
};

// -------------------------------------------------------------------------
struct ColorNode 
{
    typedef argb8 color_type;
    ColorNode()
    {}

    // ---------------------------------------------------------------------
    ColorNode(const color_type &cl, const QPointF &pt)
        : c(cl), p(pt)
    {}

    ColorNode(const color_type &cl, qreal x, qreal y)
        : c(cl), p(x, y)
    {}

    color_type gradientx(const ColorNode& node, qreal offset) const
    {
        return c.gradient(node.c, offset);
    }

    // ---------------------------------------------------------------------
    bool operator < (const ColorNode& r) const
    {
        return p.x() < r.p.x();
    }

    bool operator > (const ColorNode& r) const
    {
        return p.x() >= r.p.x();
    }

    bool operator == (const ColorNode& r) const 
    {
        return qFuzzyCompare(p.x(), r.p.x());
    }
    color_type c;
    QPointF p;
};


// -------------------------------------------------------------------------
struct XNode 
{
    XNode()
    {}
    
    // ---------------------------------------------------------------------
    XNode(const qreal& x, const bool& b)
        : ix(x), bi(b)
    {}
        
    // ---------------------------------------------------------------------
    bool operator < (const XNode& r) const
    {
        return ix <= r.ix;
    }
    
    bool operator > (const XNode& r) const
    {
        return ix >= r.ix;
    }
    
    bool operator == (const XNode& r) const 
    {
        return qFuzzyCompare(ix, r.ix);
    }

    qreal ix;
    bool bi;
};

// ----------------------------------------------------------SuperColorBevel
class SuperColorBevel
{
public:
    typedef argb8 color_type;

    enum ColorType
    {
        SolidColor,
        GradientColor,
        BlendColor,
        InterpolationColor
    };

    SuperColorBevel()
    {
        memset(this, 0, sizeof(SuperColorBevel));
    }
    
    SuperColorBevel(ColorType ct,
        const QPointF& pt1, const QPointF& pt2,
        const color_type& cl1, const color_type& cl2)
        : m_ctype(ct)
        , m_pt1(pt1)
        , m_pt2(pt2)
        , m_cl1(cl1)
        , m_cl2(cl2)
        , m_pInterOffset(NULL)
        , m_pInterColors(NULL) 
        , m_dist((ct == SolidColor) ? 
        0.0f : calc_distance(m_pt1.x(), m_pt1.y(), m_pt2.x(), m_pt2.y()))
    {
        Q_ASSERT(ct == SolidColor || ct == GradientColor);
    }
        
    SuperColorBevel(ColorType ct,
        const QPointF& pt1, const QPointF& pt2,
        const color_type& cl1, const color_type& cl2,
        const QVector<qreal>&  inter_offset,
        const QVector<color_type>&inter_colors)
        : m_ctype(ct)
        , m_pt1(pt1)
        , m_pt2(pt2)
        , m_cl1(cl1)
        , m_cl2(cl2)
        , m_pInterOffset(&inter_offset)
        , m_pInterColors(&inter_colors) 
        , m_dist((ct == SolidColor) ? 
        0.0f : calc_distance(m_pt1.x(), m_pt1.y(), m_pt2.x(), m_pt2.y()))
    {
        Q_ASSERT(ct == InterpolationColor);
    }
    
    bool cross_horizontal(int y, ColorNode& cn, qreal minY, qreal maxY) const
    {
        const qreal hy = (qreal)y;
        const qreal max = qMax(m_pt1.y(), m_pt2.y());
        const qreal min = qMin(m_pt1.y(), m_pt2.y());
        const qreal ceil = qCeil(max);
        const qreal floor = qFloor(min);

        // deal with scanlines near the top and bottom edge
        if (hy < maxY - 1.1 && hy > minY + 1.1)
        {
            if ((m_pt1.y() < hy && m_pt2.y() < hy) ||
                (m_pt1.y() > hy && m_pt2.y() > hy) ||
                (m_pt1.y() == m_pt2.y() && m_pt1.y() == hy))
            {
                return false;
            }
        }
        else if (hy < floor || hy > ceil)
        {
            return false;
        }

        const qreal dx = m_pt1.x() - m_pt2.x();
        const qreal dy = m_pt1.y() - m_pt2.y();
        if (qFuzzyIsNull(dy))
        {
            return false;
        }
        //y = kx + dd; k = dy /dx
        cn.p.rx() = m_pt1.x() - (m_pt1.y() - hy) * dx / dy;
        cn.p.ry() = hy;
        
        switch (m_ctype)
        {
        case SolidColor:
            cn.c = m_cl1;
            break;
            
        case GradientColor:
            cn.c = m_cl1.gradient(m_cl2,
                calc_distance(cn.p.x(), cn.p.y(), m_pt1.x(), m_pt1.y()) / m_dist);
            break;
                        
        case InterpolationColor:
            {
                Q_ASSERT(m_pInterOffset && m_pInterColors);
                const qreal dist = calc_distance(cn.p.x(), cn.p.y(), m_pt1.x(), m_pt1.y());
                qreal scale = dist / m_dist;
                if (scale > 1)
                {
                    scale = 1;
                }
                for (int i = 0; i < m_pInterOffset->size() - 1; ++i)
                {
                    const qreal prev_offset = (*m_pInterOffset)[i];
                    const qreal next_offset = (*m_pInterOffset)[i+1];
                    if (scale >= prev_offset &&
                        scale <= next_offset)
                    {
                        scale = (scale - prev_offset) /
                            (next_offset - prev_offset);
                        cn.c = (*m_pInterColors)[i].gradient(
                            (*m_pInterColors)[i+1], scale);
                        break;
                    }
                }
                break;
            }
            
        default:
            Q_ASSERT(!"Error type!");
            break;
            
        }
        return true;
    }
    
    const QPointF& startPoint() const
    {
        return m_pt1;
    }

    const QPointF& endPoint() const
    {
        return m_pt2;
    }

private:
    ColorType m_ctype;
    QPointF m_pt1, m_pt2;
    color_type m_cl1, m_cl2;
    const QVector<qreal>  *m_pInterOffset;
    const QVector<color_type>*m_pInterColors;
    qreal   m_dist;
};


class SolidSpanWrapper
{
public:
    typedef argb8 color_type;

    SolidSpanWrapper(color_type *span, int x1, int x2, const color_type& clr)
        : m_span(span)
        , m_x1(x1)
        , m_x2(x2)
        , m_clr(clr)
    {
    }

    void insert(qreal ix)
    {
        m_ixs.push_back(ix);
    }

    void generate()
    {
        if (m_ixs.size() > 1)
        {
            std::sort(m_ixs.begin(), m_ixs.end());
            const int ix1 = (int)(m_ixs.front());
            const int ix2 = (int)(m_ixs.back()  + 0.5);
            const int strt= qMax(m_x1, ix1);
            const int end = qMin(m_x2, ix2);
            if (strt > end ||
                strt < m_x1 || 
                end  > m_x2)
            {
                return;
            }
            const int off = strt - m_x1;
            color_type *span = m_span + off;
            int i = end - strt + 1;
            while (i--)
            {
                *span++ = m_clr;
            }
        }
    }

private:
    color_type * const m_span;
    const int m_x1;
    const int m_x2;
    QVector<qreal> m_ixs;
    const color_type& m_clr;
};
// ------------------------------------------------------GradientSpanWrapper
class GradientSpanWrapper
{
public:
    typedef argb8 color_type;

    GradientSpanWrapper(color_type *span, int x1, int x2)
        : m_span(span)
        , m_x1(x1)
        , m_x2(x2)
    {
        Q_ASSERT(span);
    }
    
    inline void insert(const ColorNode& cn)
    {
        m_nodes.push_back(cn);
    }

    void prepare()
    {
        if (m_nodes.size() > 1)
        {
            qSort(m_nodes.begin(), m_nodes.end());
        }		
    }
    
    void generate() const
    {
        if (m_nodes.size() < 2)
        {
            return;
        }
        Q_ASSERT(m_nodes.size() > 1);
        
        int strt, end;
        if (false == get_range(strt, end))
        { 
            return;
        }
        Q_ASSERT(strt >= m_x1 && end <= m_x2 && strt <= end);

        int strt_prev_index, end_next_index;
        get_range_index(strt_prev_index, end_next_index, strt, end);

        int curx;
        generate_prev(strt_prev_index, strt, end, curx);
        generate_middle(strt, end, strt_prev_index, end_next_index, curx);
        generate_next(end_next_index, strt, end, curx);
    }

private:
    bool get_range(int& x1, int& x2) const
    {
        const int ix1 = (int)(m_nodes.front().p.x());
        const int ix2 = (int)(m_nodes.back().p.x()  + 0.5);
        const int strt= qMax(m_x1, ix1);
        const int end = qMin(m_x2, ix2);
        if (strt > end ||
            strt < m_x1 || 
            end  > m_x2)
        {
            return false;
        }

        x1 = strt;
        x2 = end;

        return true;
    }

    void get_range_index(int& strt_prev_index, int& end_next_index,
                         const int& strt,      const int& end) const
    {
        strt_prev_index = 0;
        end_next_index  = m_nodes.size() - 1;
        for (int i = 0; i < m_nodes.size() - 1; ++i)
        {
            const qreal x1 = m_nodes[i].p.x();
            const qreal x2 = m_nodes[i+1].p.x();
            if (x1 <= strt)
            {
                strt_prev_index = i;	
            }
            if (x2 >= end)
            {
                end_next_index = i + 1;
                break;
            }
        }
    }

    void generate_prev(const int& strt_prev_index,
                       int& strt, const int& end, int& curx) const
    {
        const int istrt = static_cast<int>(m_nodes[strt_prev_index].p.x()) + 1;
        const color_type& cl = m_nodes[strt_prev_index].c;
        curx = strt;
        color_type *span = m_span + curx - m_x1;
        while (curx <= istrt && curx <= end && curx >= strt)
        {
            *span = cl;
            span->demultiply();
            ++span;
            ++curx;
        }
    }

#if QT_COMPILER_SUPPORTS_HERE(SSE4_1)
    QT_FUNCTION_TARGET(SSE4_1)
    __m128i gradient_sse4(const __m128i &from, const __m128i &to, const __m128i &pos, const int &shift) const
    {
        __m128i result = _mm_sub_epi32(to, from);
        result = _mm_mullo_epi32(result, pos);
        result = _mm_srai_epi32(result, shift);
        result = _mm_add_epi32(from, result);

        qint32* cast_m128 = (qint32*)&result;
        Q_ASSERT(0 <= cast_m128[0] && cast_m128[0] <= 255);
        Q_ASSERT(0 <= cast_m128[1] && cast_m128[1] <= 255);
        Q_ASSERT(0 <= cast_m128[2] && cast_m128[2] <= 255);
        Q_ASSERT(0 <= cast_m128[3] && cast_m128[3] <= 255);
/*      
        Q_ASSERT(0 <= result.m128i_i32[0] && result.m128i_i32[0] <= 255);
        Q_ASSERT(0 <= result.m128i_i32[1] && result.m128i_i32[1] <= 255);
        Q_ASSERT(0 <= result.m128i_i32[2] && result.m128i_i32[2] <= 255);
        Q_ASSERT(0 <= result.m128i_i32[3] && result.m128i_i32[3] <= 255);
*/
        return result;
    }

    QT_FUNCTION_TARGET(SSE4_1)
    void generate_gradient_sse4(argb8 *buffer, int length, 
                                const argb8 &clrFrom, const argb8 &clrTo,
                                const qreal &start, const qreal &dx) const
    {
        Q_ASSERT(sizeof(argb8) == sizeof(uint));

        enum {
            base_shift = 16,
            base_scale = 1 << base_shift
        };
        const __m128i from[4] = {
            _mm_set1_epi32(clrFrom.b),
            _mm_set1_epi32(clrFrom.g),
            _mm_set1_epi32(clrFrom.r),
            _mm_set1_epi32(clrFrom.a)
        };
        const __m128i to[4] = {
            _mm_set1_epi32(clrTo.b),
            _mm_set1_epi32(clrTo.g),
            _mm_set1_epi32(clrTo.r),
            _mm_set1_epi32(clrTo.a)
        };
        __m128i *pSrc = (__m128i *)buffer;
        __m128i *pEnd = pSrc + length / 4;
        __m128i result[4];
        for (int i = 0; pSrc < pEnd; pSrc++, i += 4) {
            const int pos[4] = {
                int((start + i * dx) * base_scale + 0.5),
                int((start + (i + 1) * dx) * base_scale + 0.5),
                int((start + (i + 2) * dx) * base_scale + 0.5),
                int((start + (i + 3) * dx) * base_scale + 0.5)
            };
            const __m128i mpos = _mm_set_epi32(pos[3], pos[2], pos[1], pos[0]);
            result[3] = gradient_sse4(from[3], to[3], mpos, base_shift); // alpha
            result[2] = gradient_sse4(from[2], to[2], mpos, base_shift); // red
            result[1] = gradient_sse4(from[1], to[1], mpos, base_shift); // green
            result[0] = gradient_sse4(from[0], to[0], mpos, base_shift); // blue

            result[3] = _mm_or_si128(_mm_slli_epi32(result[3], 24),  _mm_slli_epi32(result[2], 16));
            result[1] = _mm_or_si128(_mm_slli_epi32(result[1], 8),  result[0]);
            _mm_storeu_si128(pSrc,  _mm_or_si128(result[3], result[1]));
        }
        switch (length % 4) {
                case 3: --length; buffer[length] = clrFrom.gradient(clrTo, start + length * dx);
                case 2: --length; buffer[length] = clrFrom.gradient(clrTo, start + length * dx);
                case 1: --length; buffer[length] = clrFrom.gradient(clrTo, start + length * dx);
        }
    }
#endif // QT_COMPILER_SUPPORTS_HERE(SSE4_1)

    void generate_gradient(argb8 *buffer, int length, 
                            const argb8 &clrFrom, const argb8 &clrTo,
                            const qreal &start, const qreal &dx) const 
    {
        if (clrFrom == clrTo) {
            qt_memfill32((quint32 *)buffer, qRgba(clrFrom.r, clrFrom.g, clrFrom.b, clrFrom.a), length);
            return;
        }

#if QT_COMPILER_SUPPORTS_HERE(SSE4_1)
        if (qCpuHasFeature(SSE4_1))
            return generate_gradient_sse4(buffer, length, clrFrom, clrTo, start, dx);        
#endif // QT_COMPILER_SUPPORTS_HERE(SSE4_1)

        qreal pos = start;
        for (int i = 0; i < length; i++) {
            buffer[i] = clrFrom.gradient(clrTo, pos);
            pos += dx;
        }
    }
    void generate_middle(const int& /*strt*/,
                         const int& end, 
                         const int& strt_prev_index,
                         const int& end_next_index,
                         int&  curx) const
    {
        int cur_prev_index = strt_prev_index;
        int cur_next_index = strt_prev_index + 1;
        while (cur_next_index <= end_next_index)
        {
            const color_type& cl1 = m_nodes[cur_prev_index].c;
            const color_type& cl2 = m_nodes[cur_next_index].c;
            const qreal rcurx = static_cast<qreal>(curx);
            const qreal istr = m_nodes[cur_prev_index].p.x();
            const qreal iend = m_nodes[cur_next_index].p.x();
            if (!qFuzzyCompare(istr, iend))
            {
                qreal scale = (rcurx - istr) / (iend - istr);
                Q_ASSERT(scale >= 0 && "current index is less than the start index");
                const qreal incr = 1.0f / (iend - istr);
                const int offset= curx - m_x1;
                color_type *span = m_span + offset;
                const int lastx = qMin(end, static_cast<int>(iend));
                generate_gradient(span, lastx - curx + 1, cl1, cl2, scale, incr);
                curx = lastx + 1;
            }
            ++cur_prev_index;
            ++cur_next_index;
        }
    }

    void generate_next(const int& end_next_index, 
                       const int& strt, const int& end, int& curx) const
    {
        const int iend = static_cast<int>(m_nodes[end_next_index].p.x());
        const color_type& cl = m_nodes[end_next_index].c;
        color_type *span = m_span + curx - m_x1;
        while (curx >= iend && curx <= end && curx >= strt)
        {
            *span = cl;
            span->demultiply();
            ++span;
            ++curx;
        }
    }
    
private:
    color_type * const m_span;
    const int m_x1;
    const int m_x2;
    QVector<ColorNode> m_nodes;//QVector is much faster than std::vector
};


// --------------------------------------------------------gradient_triangle
class gradient_triangle
{
public:
    typedef argb8 color_type;

    gradient_triangle()
    {
    }

    gradient_triangle(const QPointF& pt0, 
                      const QPointF& pt1, 
                      const QPointF& pt2,
                      const color_type& cl0,
                      const color_type& cl1,
                      const color_type& cl2)
    {
        GenerateBevels(pt0, pt1, pt2, cl0, cl1, cl2, m_bevels);
    }

    gradient_triangle(const QPointF& pt0, 
                      const QPointF& pt1, 
                      const QPointF& pt2,
                      const color_type& cl0, 
                      const color_type& cl1,
                      const color_type& cl2,
                      const QVector<qreal>&   inter_offset,
                      const QVector<color_type>& inter_colors)
    {
        GenerateBevels(
            pt0, pt1, pt2, cl0, cl1, cl2, inter_offset, inter_colors, m_bevels);
    }

    void reset(const QPointF& pt0, 
               const QPointF& pt1, 
               const QPointF& pt2,
               const color_type& cl0, 
               const color_type& cl1,
               const color_type& cl2)
    {
        GenerateBevels(pt0, pt1, pt2, cl0, cl1, cl2, m_bevels);
    }
    
    void reset(const QPointF& pt0, 
               const QPointF& pt1, 
               const QPointF& pt2,
               const color_type& cl0, 
               const color_type& cl1,
               const color_type& cl2,
               const QVector<qreal>&   inter_offset,
               const QVector<color_type>& inter_colors)
    {
        GenerateBevels(
            pt0, pt1, pt2, cl0, cl1, cl2, inter_offset, inter_colors, m_bevels);
    }

    void prepare(int hy, bool& prevbi01, ColorNode& prev01cn, qreal minY, qreal maxY) const
    {
        if (!m_bevels.empty())
        {
            Q_ASSERT(m_bevels.size() > 2);
            const SuperColorBevel& bevel01 = m_bevels.back();
            prevbi01 = bevel01.cross_horizontal(hy, prev01cn, minY, maxY);
        }
    }

    void generate(int x0, int x1, int y, color_type *span,
                  bool& prevbi01, ColorNode& prev01cn, qreal minY, qreal maxY) const
    {
        GradientSpanWrapper spanWrapper(span, x0, x1);
        if (prevbi01)
        {
            spanWrapper.insert(prev01cn);
        }
        
        const SuperColorBevel& bevel02 = m_bevels.front();
        prevbi01 = bevel02.cross_horizontal(y, prev01cn, minY, maxY);
        // skip if the points of the triangle are on the same straight line
        if (!isValidTriangle())
            return;
        if (prevbi01)
        {
            spanWrapper.insert(prev01cn);
        }
        
        for (int i = 1; i < size(); ++i)
        {
            const SuperColorBevel& bevel = m_bevels[i];
            ColorNode cn;
            if (bevel.cross_horizontal(y, cn, minY, maxY))
            {
                spanWrapper.insert(cn);
            }
        }
        
        spanWrapper.prepare();
        spanWrapper.generate();
    }

    bool isValidTriangle() const
    {
        QPointF firstLineP1 = m_bevels.first().startPoint();
        QPointF firstLineP2 = m_bevels.first().endPoint();
        QPointF lastLineP1 = m_bevels.last().startPoint();
        QPointF lastLineP2 = m_bevels.last().endPoint();
        if (firstLineP1 == firstLineP2 && (firstLineP1 == lastLineP1 || firstLineP1 == lastLineP2))
            return false;
        if (lastLineP1 == lastLineP2 && (firstLineP1 == lastLineP1 || firstLineP2 == lastLineP1))
            return false;
        return true;
    }

private:
    int size() const
    {
        return m_bevels.size() - 1;
    }

    static void GenerateBevels(const QPointF& pt0,
                               const QPointF& pt1,
                               const QPointF& pt2,
                               const color_type& cl0,
                               const color_type& cl1, 
                               const color_type& cl2,
                               QVector<SuperColorBevel>& bevels)
    {			
        // 02, 12, 01
        bevels.push_back(SuperColorBevel(
                         SuperColorBevel::GradientColor, 
                         pt0, pt2, cl0, cl2));
        bevels.push_back(SuperColorBevel(
                         SuperColorBevel::GradientColor,
                         pt1, pt2, cl1, cl2));
        bevels.push_back(SuperColorBevel(
                         SuperColorBevel::GradientColor, 
                         pt0, pt1, cl0, cl1));
    }

    static void GenerateBevels(const QPointF& pt0, 
                               const QPointF& pt1,
                               const QPointF& pt2, 
                               const color_type& cl0,
                               const color_type& cl1, 
                               const color_type& cl2,
                               const QVector<qreal>&  inter_offset,
                               const QVector<color_type>&inter_colors,
                               QVector<SuperColorBevel>& bevels)
    {				
        if (!inter_colors.empty())
        {
            // 20, 12... 10
            const qreal dx10 = pt0.x() - pt1.x();
            const qreal dy10 = pt0.y() - pt1.y();
            const qreal dx20 = pt0.x() - pt2.x();
            const qreal dy20 = pt0.y() - pt2.y();
            
            bevels.push_back(SuperColorBevel(
                             SuperColorBevel::InterpolationColor, 
                             pt2, pt0, cl2, cl0, inter_offset, inter_colors));

            const int s = inter_offset.size();
            for (int i = s - 2; i > 0; --i)
            {
                const qreal offset = inter_offset[i];
                const QPointF ptA(pt1.x() + offset * dx10,
                                 pt1.y() + offset * dy10);
                const QPointF ptB(pt2.x() + offset * dx20,
                                 pt2.y() + offset * dy20);
                bevels.push_back(SuperColorBevel(
                                 SuperColorBevel::SolidColor, 
                                 ptA, ptB, inter_colors[i], inter_colors[i]));
            }
            bevels.push_back(SuperColorBevel(
                             SuperColorBevel::SolidColor, 
                             pt1, pt2, inter_colors.front(), inter_colors.front()));
            bevels.push_back(SuperColorBevel(
                             SuperColorBevel::InterpolationColor,
                             pt1, pt0, cl1, cl0, inter_offset, inter_colors));
        }
        else
        {
            GenerateBevels(pt0, pt1, pt2, cl0, cl1, cl2, bevels);
        }
    }
    
private:
    QVector<SuperColorBevel> m_bevels;
};


class gradient_quadrangle
{
public:
    typedef argb8 color_type;

    gradient_quadrangle()
    {
    }

    gradient_quadrangle(const QPointF& pt00, 
        const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1)
        : m_centerTriangle(pt00, pt11, pt12, 
        SolidTriangle::GenerateCenterColor(clr0))
    {
        GenerateBevels(pt11, pt12, pt21, pt22, clr0, clr1, m_bevels);
    }

    gradient_quadrangle(const QPointF& pt00, 
        const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1,
        const QVector<qreal>&  inter_offset,
        const QVector<color_type>&inter_colors)
        : m_centerTriangle(pt00, pt11, pt12,
        SolidTriangle::GenerateCenterColor(
        clr0, clr1, inter_offset, inter_colors))
    {
        GenerateBevels(pt11, pt12, pt21, pt22, clr0, clr1, 
            inter_offset, inter_colors, m_bevels);
    }

    void reset(const QPointF& pt00, 
        const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1)
    {
        m_centerTriangle.reset(pt00, pt11, pt12, 
            SolidTriangle::GenerateCenterColor(clr0));
        GenerateBevels(pt11, pt12, pt21, pt22, clr0, clr1, m_bevels);
    }

    void reset(const QPointF& pt00, 
        const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1,
        const QVector<qreal>&  inter_offset,
        const QVector<color_type>&inter_colors)
    {
        m_centerTriangle.reset(pt00, pt11, pt12,
            SolidTriangle::GenerateCenterColor(clr0, clr1,
            inter_offset, inter_colors));
        GenerateBevels(pt11, pt12, pt21, pt22, clr0, clr1, 
            inter_offset, inter_colors, m_bevels);
    }

    void prepare(int hy,
        bool& prevbi0011, qreal &prev0011ix, 
        bool& prevbi1121, ColorNode& prev1121cn, qreal minY, qreal maxY) const
    {
        if (!m_bevels.empty())
        {
            Q_ASSERT(m_bevels.size() > 3);
            const SuperColorBevel& bevel1121 = m_bevels.back();
            prevbi1121 = bevel1121.cross_horizontal(hy, prev1121cn, minY, maxY);
            m_centerTriangle.prepare(hy, prevbi0011, prev0011ix, minY, maxY);
        }
    }

    void generate_gradient_quadrangle(int x0, int x1, int y, color_type *span,
        bool& prevbi1121, ColorNode& prev1121cn,
        bool& bi1112, qreal& ix1112, qreal minY, qreal maxY) const
    {
        Q_ASSERT(size() > 2);
        GradientSpanWrapper spanWrapper(span, x0, x1);
        if (prevbi1121)
        {
            spanWrapper.insert(prev1121cn);
        }

        const SuperColorBevel& bevel1112 = m_bevels.front();
        ColorNode cn;
        bi1112 = bevel1112.cross_horizontal(y, cn, minY, maxY);
        ix1112 = cn.p.rx();
        if (bi1112)
        {
            spanWrapper.insert(cn);
        }

        for (int i = 1; i < size() - 1; ++i)
        {
            const SuperColorBevel& bevel = m_bevels[i];
            if (bevel.cross_horizontal(y, cn, minY, maxY))
            {
                spanWrapper.insert(cn);
            }
        }

        const SuperColorBevel& bevel1222 = m_bevels[size() - 1];
        prevbi1121 = bevel1222.cross_horizontal(y, prev1121cn, minY, maxY);
        if (prevbi1121)
        {
            spanWrapper.insert(prev1121cn);
        }

        spanWrapper.prepare();
        spanWrapper.generate();
    }

    void generate_solid_triangle(int x0, int x1, int y, color_type *span,
        bool& prev0011, qreal& prev0011ix,
        const bool& bi1112, const qreal& ix1112, qreal minY, qreal maxY) const
    {
        m_centerTriangle.generate(
            x0, x1, y, span, prev0011, prev0011ix, bi1112, ix1112, minY, maxY);
    }

private:
    int size() const
    {
        return m_bevels.size() - 1;
    }

    class SolidTriangle
    {
    public:
        SolidTriangle()
        {
        }

        SolidTriangle(const QPointF &pt0, const QPointF &pt1, 
            const QPointF &pt2, const color_type &clr)
            : m_pt0(pt0)
            , m_pt1(pt1)
            , m_pt2(pt2)
            , m_clr(clr)
        {
        }

        void reset(const QPointF &pt0, const QPointF &pt1, 
            const QPointF &pt2, const color_type &clr)
        {
            m_pt0 = pt0;
            m_pt1 = pt1;
            m_pt2 = pt2;
            m_clr = clr;
        }

        void prepare(int hy, bool& prev1121, qreal &prev1121ix, qreal minY , qreal maxY) const
        {
            calc_segment_intersect_horizontal(
                m_pt0.x(), m_pt0.y(), m_pt1.x(), m_pt1.y(), hy, prev1121, prev1121ix, minY , maxY);
        }

        void generate(int x1, int x2, int hy, color_type *span,
            bool& bi01, qreal& i01,
            const bool& bi12, const qreal& i12, qreal minY, qreal maxY) const
        {
            Q_ASSERT(span);

            qreal i20 = 0.0f;
            bool bi20 = false;
            calc_segment_intersect_horizontal(
                m_pt2.x(), m_pt2.y(), m_pt0.x(), m_pt0.y(), hy, bi20, i20, minY, maxY);

            SolidSpanWrapper spanWrapper(span, x1, x2, m_clr);
            if (bi01) spanWrapper.insert(i01);
            if (bi12) spanWrapper.insert(i12);
            if (bi20) spanWrapper.insert(i20);

            spanWrapper.generate();
            i01 = i20;
            bi01 = bi20;
        }

        static color_type GenerateCenterColor(const color_type& clr0)
        {
            color_type c(clr0);
            c.demultiply();
            return c;
        }

        static color_type GenerateCenterColor(const color_type& clr0, const color_type& /*clr1*/,
            const QVector<qreal>&  inter_offset,
            const QVector<color_type>& inter_colors)
        {
            color_type c(clr0);
            if (!inter_offset.empty())
            {
                c = inter_colors.back();
            }
            c.demultiply();
            return c;
        }

    private:
        void calc_segment_intersect_horizontal(const qreal& x1, const qreal& y1,
            const qreal& x2, const qreal& y2,
            int hy, bool& bi, qreal& ix, qreal minY, qreal maxY) const
        {
            const qreal rhy = static_cast<qreal>(hy);
            if (rhy > minY + 1.1 && rhy < maxY - 1.1)
            {
                if ((y1 < rhy && y2 < rhy) ||
                    (y1 > rhy && y2 > rhy) ||
                    (y1 == y2 && y1 == rhy))
                {
                    bi = false;
                    return;
                }
            }
            else
            {
                qreal max = qMax(y1, y2);
                qreal min = qMin(y1, y2);
                qreal ceil = qCeil(max);
                qreal floor = qFloor(min);
                if (rhy < floor || rhy > ceil || (y1 == y2 && y1 == rhy))
                {
                    bi = false;
                    return;
                }
            }

            const qreal dx = x1 - x2;
            const qreal dy = y1 - y2;
            ix = x1 - (y1 - rhy) * dx / dy;
            bi = true;
            return;
        }

        QPointF m_pt0;
        QPointF m_pt1;
        QPointF m_pt2;
        color_type m_clr;
    };


    static void GenerateBevels(const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1,
        QVector<SuperColorBevel>& bevels)
    {	
        // 1112, 2122, 1222, 1121
        bevels.push_back(SuperColorBevel(
            SuperColorBevel::SolidColor, 
            pt11, pt12, clr0, clr0));
        bevels.push_back(SuperColorBevel(
            SuperColorBevel::SolidColor, 
            pt21, pt22, clr1, clr1));
        bevels.push_back(SuperColorBevel(
            SuperColorBevel::GradientColor,
            pt12, pt22, clr0, clr1));
        bevels.push_back(SuperColorBevel(
            SuperColorBevel::GradientColor,
            pt11, pt21, clr0, clr1));
    }

    

    static void GenerateBevels(const QPointF& pt11, const QPointF& pt12,
        const QPointF& pt21, const QPointF& pt22,
        const color_type& clr0, const color_type& clr1,
        const QVector<qreal>&  inter_offset,
        const QVector<color_type>& inter_colors,
        QVector<SuperColorBevel>& bevels)
    {
        if (!inter_colors.empty())
        {
            // 1112... 2122, 2212, 2111
            const qreal dx2111 = pt11.x() - pt21.x();
            const qreal dy2111 = pt11.y() - pt21.y();
            const qreal dx2212 = pt12.x() - pt22.x();
            const qreal dy2212 = pt12.y() - pt22.y();

            bevels.push_back(SuperColorBevel(
                SuperColorBevel::SolidColor, 
                pt11, pt12, inter_colors.back(), inter_colors.back()));
            const int s = inter_offset.size();
            for (int i = 1; i < s - 1; ++i)
            {
                const qreal offset = inter_offset[i];
                const QPointF pt1(pt21.x() + offset * dx2111,
                    pt21.y() + offset * dy2111);
                const QPointF pt2(pt22.x() + offset * dx2212,
                    pt22.y() + offset * dy2212);
                bevels.push_back(SuperColorBevel(
                    SuperColorBevel::SolidColor, 
                    pt1, pt2, inter_colors[i], inter_colors[i]));
            }
            bevels.push_back(SuperColorBevel(
                SuperColorBevel::SolidColor, 
                pt21, pt22, inter_colors.front(), inter_colors.front()));
            bevels.push_back(SuperColorBevel(
                SuperColorBevel::InterpolationColor,
                pt22, pt12, clr0, clr1, inter_offset, inter_colors));
            bevels.push_back(SuperColorBevel(
                SuperColorBevel::InterpolationColor,
                pt21, pt11, clr0, clr1, inter_offset, inter_colors));
        }
        else
        {
            GenerateBevels(pt11, pt12, pt21, pt22, clr0, clr1, bevels);
        }
    }

    SolidTriangle m_centerTriangle;
    QVector<SuperColorBevel> m_bevels;
};


//--------------------------------------------------------------------------
class path_gradient_clamp_span_gen
{
public:

    typedef argb8 color_type;

    // ---------------------------------------------------------------------
    path_gradient_clamp_span_gen(const QPathGradientBrush& pgb,
                                 const QMatrix& mtx)
    {
        QMatrix transmtx = pgb.m_mtx;
        matrix_multiply(&transmtx, &mtx, &transmtx);
        const QMatrix& tmtx = transmtx;

        QPainterPath surround_path = pgb.m_path;
        Transform(surround_path, tmtx);
        Flatten(surround_path);

        const int surround_point_count = surround_path.elementCount();
        if (1 > surround_point_count)
        {
            qWarning("path is empty!");
            return;
        }

        qreal maxY = surround_path.elementAt(0).y;
        qreal minY = maxY;
        for (int j = 1; j < surround_path.elementCount(); j++)
        {
            qreal curY = surround_path.elementAt(j).y;
            if (curY < minY)
                minY = curY;
            else if (curY > maxY)
                maxY = curY;
        }
        m_maxY = qCeil(maxY);
        m_minY = qFloor(minY);

        m_surround_points.resize(surround_point_count);
        m_surround_colors.resize(surround_point_count);
        GetPathPoints(surround_path, &m_surround_points[0], surround_point_count);
        QVector<QColor> surround_colors(surround_point_count);
        surround_colors[0] = pgb.m_stops.back().second;
        
        for (int scn = 1; scn < surround_point_count; scn++)
        {
            surround_colors[scn] = surround_colors.at(scn - 1);
        }

        for (int i = 0; i < surround_point_count; ++i)
        {
            Convert2ARGB8(surround_colors[i], m_surround_colors[i]);
            m_surround_colors[i].premultiply();
        }
        
        m_center_point = pgb.m_centerPoint;
        TransformPoints(tmtx, &m_center_point);
        
        QColor center_color = pgb.m_stops.front().second;
        Convert2ARGB8(center_color, m_center_color);
        m_center_color.premultiply();

        int inter_count = pgb.m_stops.size();
        Q_ASSERT(inter_count > 0);
        {
            m_inter_offset.resize(inter_count);
            m_inter_colors.resize(inter_count);
            QVector<QColor> inter_colors(inter_count);
            GetInterpolationColors(pgb.m_stops, &inter_colors[0], &m_inter_offset[0], inter_count);
            for (int i = 0; i < inter_count; ++i)
            {
                Convert2ARGB8(inter_colors[i], m_inter_colors[i]);
                m_inter_colors[i].premultiply();
            }

            QVector<InterpolationColor<color_type>::InterColorNode> interNodes;
            for (int i = 0; i < inter_count; ++i)
            {
                interNodes.push_back(InterpolationColor<color_type>::InterColorNode(m_inter_offset[i], m_inter_colors[i]));
            }
            std::unique(interNodes.begin(), interNodes.end());
            std::sort(interNodes.begin(), interNodes.end());
            
            m_inter_offset.clear();
            m_inter_colors.clear();
            inter_count = interNodes.size();
            
            for (int j = 0; j < inter_count; ++j)
            {
                m_inter_offset.push_back(interNodes[j].offset());
                m_inter_colors.push_back(interNodes[j].pcolor());
            }
        }
        m_xscale = pgb.m_xscale;
        m_yscale = pgb.m_yscale;
        if (qFuzzyCompare(m_xscale, 0) && qFuzzyCompare(m_yscale, 0))
        {
            m_generater.reset_inter_triangles(m_center_point,
                m_surround_points,
                m_center_color, m_surround_colors,
                m_inter_offset, m_inter_colors);
        }
        else
        {
            QPainterPath focus_path(surround_path);
            QMatrix scalemtx;
            scalemtx.translate(m_center_point.x(), m_center_point.y());
            scalemtx.scale(m_xscale, m_yscale);
            scalemtx.translate(-m_center_point.x(), -m_center_point.y());
            Transform(focus_path, scalemtx);

            const int focus_points_count = focus_path.elementCount();
            m_focus_points.resize(focus_points_count);
            GetPathPoints(focus_path, &m_focus_points[0], focus_points_count);

            m_generater.reset_inter_quadrangles(m_center_point, 
                m_focus_points, m_surround_points, m_center_color, 
                m_surround_colors, m_inter_offset, m_inter_colors);
        }
    }
    
    //----------------------------------------------------------------------
    ~path_gradient_clamp_span_gen()
    {
    }
    
    //----------------------------------------------------------------------
    void generate(color_type* span, int x, int y, unsigned len) const
    {
        m_generater.generate(span, x, y, len, m_minY, m_maxY);
    }

private:
    class triangle_quadrangle_generater
    {
    public:
        triangle_quadrangle_generater()
        {
        }
        
        ~triangle_quadrangle_generater()
        {
        }

        void reset_triangles(const QPointF& center_point,
            const QVector<QPointF>& surround_points,
            const color_type& center_color,
            const QVector<color_type>& surround_colors)
        {
            clear();
            const int s = surround_points.size();
            m_triangles.resize(s);
            for (int j = 0; j < s; ++j)
            {
                int i = j + 1;
                
                if (i == s)
                {
                    i = 0;
                }
                
                m_triangles[j].reset(center_point,
                    surround_points[j],
                    surround_points[i],
                    center_color,
                    surround_colors[j], 
                    surround_colors[i]);
            }
        }

        void reset_inter_triangles(const QPointF& center_point,
            const QVector<QPointF>& surround_points,
            const color_type& center_color,
            const QVector<color_type>& surround_colors,
            const QVector<qreal>& inter_offset,
            const QVector<color_type>& inter_colors)
        {
            clear();
            int s = surround_points.size();
            if (surround_points.front() == surround_points.back())
                s--;
            m_triangles.resize(s);
            for (int j = 0; j < s; ++j)
            {
                int i = j + 1;
                
                if (i == s)
                {
                    i = 0;
                }

                m_triangles[j].reset(center_point,
                    surround_points[j],
                    surround_points[i],
                    center_color,
                    surround_colors[j], 
                    surround_colors[i],
                    inter_offset,
                    inter_colors);
            }
        }

        void reset_inter_quadrangles(const QPointF& center_point,
            const QVector<QPointF>& focus_points,
            const QVector<QPointF>& surround_points,
            const color_type& center_color,
            const QVector<color_type>& surround_colors,
            const QVector<qreal>& inter_offset,
            const QVector<color_type>& inter_colors)
        {
            clear();
            const int s = surround_points.size();
            m_quadrangles.resize(s);
            for (int j = 0; j < s; ++j)
            {
                int i = j + 1;

                if (i == s)
                {
                    i = 0;
                }

                m_quadrangles[j].reset(center_point,
                    focus_points[j],
                    focus_points[i],
                    surround_points[j],
                    surround_points[i],
                    center_color,
                    surround_colors[0],
                    inter_offset,
                    inter_colors);
            }
        }

        void generate(color_type* span, int x, int y, unsigned len, qreal minY, qreal maxY) const
        {
            const int x0 = x;
            const int x1 = x + len - 1;
            if (!m_triangles.empty())
            {
                bool prevbi01 = false;
                ColorNode prev01cn;	
                m_triangles[0].prepare(y, prevbi01, prev01cn, minY, maxY);
                const int s = m_triangles.size();
                for (int i = 0; i < s; ++i)
                {
                    m_triangles[i].generate(x0, x1, y, span, prevbi01, prev01cn, minY, maxY);
                }
            }
            else
            {
                Q_ASSERT(m_quadrangles.size() > 0);
                bool prevbi0011 = false;
                qreal prev0011ix = 0.0f; 
                bool prevbi1121 = false;
                ColorNode prev1121cn;
                m_quadrangles[0].prepare(y, prevbi0011, prev0011ix, prevbi1121, prev1121cn, minY, maxY);
                const int s = m_quadrangles.size();
                QVector<XNode> xNodes(s);
                for (int i = 0; i < s; ++i)
                {
                    m_quadrangles[i].generate_gradient_quadrangle(
                        x0, x1, y, span, prevbi1121, prev1121cn, xNodes[i].bi, xNodes[i].ix, minY, maxY);
                }
                for (int i = 0; i < s; ++i)
                {
                    m_quadrangles[i].generate_solid_triangle(
                        x0, x1, y, span, prevbi0011, prev0011ix, xNodes[i].bi, xNodes[i].ix, minY, maxY);
                }
            }
            
        }

    private:
        void clear()
        {
            m_triangles.clear();
            m_quadrangles.clear();
        }

    private:
        QVector<gradient_triangle> m_triangles;
        QVector<gradient_quadrangle> m_quadrangles;
    };


private:
    triangle_quadrangle_generater m_generater;
    QVector<QPointF> m_surround_points;
    QVector<color_type> m_surround_colors;
    QPointF m_center_point;
    color_type m_center_color;
    QVector<qreal> m_inter_offset;
    QVector<color_type> m_inter_colors;
    qreal m_xscale, m_yscale;
    QVector<QPointF> m_focus_points;
    qreal m_maxY;
    qreal m_minY;
   };

//--------------------------------------------------------------------------
class path_gradient_span_gen_cache
{
public:
    typedef argb8 color_type;

    path_gradient_span_gen_cache(const QPathGradientBrush& pgb,
                                 const QMatrix& mtx)
                                 : m_clamp_generater(NULL)
                                 , m_bound_rect(pgb.m_path.boundingRect())
    {
        CalcWorldToDeviceTransform(m_matrix, pgb, mtx);
    }

    ~path_gradient_span_gen_cache()
    {
        m_clamp_generater = NULL;
    }

    void reset_clamp_span_generater(
        const path_gradient_clamp_span_gen& clamp_span_generater)
    {
        m_clamp_generater = &clamp_span_generater;
    }

    void generate(color_type* span, int x, int y, unsigned len) const
    {
        memset(span, 0, sizeof(color_type) * len);
        m_clamp_generater->generate(span, x, y, len);
    }

private:
    static void CalcWorldToDeviceTransform(QMatrix &matrix, const QPathGradientBrush& pgb,
                                             const QMatrix& mtx)
    {
        matrix = pgb.m_mtx;
        matrix_multiply(&matrix, &mtx, &matrix);
    }


private:
    const path_gradient_clamp_span_gen *m_clamp_generater;
    QRectF   m_bound_rect;
    QMatrix  m_matrix;
};


// -------------------------------------------------------------------------
class Q_GUI_EXPORT path_gradient_span_gen : public SpanGenerator
{
public:
    typedef argb8 color_type;

    path_gradient_span_gen(const QPathGradientBrush& pgb,
                           const QMatrix& mtx = QMatrix())
                           : m_clamp_span_gen(pgb, mtx)
                           , m_span_cache(pgb, mtx)
    {
        m_span_cache.reset_clamp_span_generater(m_clamp_span_gen);
    }

    virtual ~path_gradient_span_gen()
    {
    }

    virtual void prepare()
    {
    }

    virtual void generate(color_type* span, int x, int y, unsigned len)
    {
        m_span_cache.generate(span, x, y, len);
    }

private:
    path_gradient_clamp_span_gen m_clamp_span_gen;
    path_gradient_span_gen_cache m_span_cache;
};

QT_END_NAMESPACE

// -------------------------------------------------------------------------
#endif //QPATHGRADIENT_P_H 
