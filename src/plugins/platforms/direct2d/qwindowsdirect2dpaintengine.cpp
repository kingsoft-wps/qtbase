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

#include "qwindowsdirect2dpaintengine.h"
#include "qwindowsdirect2dplatformpixmap.h"
#include "qwindowsdirect2dpaintdevice.h"
#include "qwindowsdirect2dcontext.h"
#include "qwindowsdirect2dhelpers.h"
#include "qwindowsdirect2dbitmap.h"
#include "qwindowsdirect2ddevicecontext.h"

#include <QtFontDatabaseSupport/private/qwindowsfontdatabase_p.h>
#include <QtFontDatabaseSupport/private/qwindowsfontengine_p.h>
#include "qwindowsintegration.h"

#include <QtCore/qmath.h>
#include <QtCore/qstack.h>
#include <QtCore/qsettings.h>
#include <QtCore/qcache.h>
#include <QtGui/private/qpaintengine_p.h>
#include <QtGui/private/qtextengine_p.h>
#include <QtGui/private/qfontengine_p.h>
#include <QtGui/private/qstatictext_p.h>

#include <algorithm>

#include <d2d1_1.h>
#include <dwrite_1.h>
#include <wrl.h>
#include <memory>

using Microsoft::WRL::ComPtr;

QT_BEGIN_NAMESPACE

// The enum values below are set as tags on the device context
// in the various draw methods. When EndDraw is called the device context
// will report the last set tag number in case of errors
// along with an error code

// Microsoft keeps a list of d2d error codes here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd370979(v=vs.85).aspx
enum {
    D2DDebugDrawInitialStateTag = -1,
    D2DDebugFillTag = 1,
    D2DDebugFillRectTag,
    D2DDebugDrawRectsTag,
    D2DDebugDrawRectFsTag,
    D2DDebugDrawEllipseTag,
    D2DDebugDrawEllipseFTag,
    D2DDebugDrawImageTag,
    D2DDebugDrawPixmapTag,
    D2DDebugDrawStaticTextItemTag,
    D2DDebugDrawTextItemTag
};

//Clipping flags
enum : unsigned {
    SimpleSystemClip = 0x1
};

enum ClipType {
    AxisAlignedClip,
    LayerClip
};

// Since d2d is a float-based system we need to be able to snap our drawing to whole pixels.
// Applying the magical aliasing offset to coordinates will do so, just make sure that
// aliased painting is turned on on the d2d device context.
static const qreal MAGICAL_ALIASING_OFFSET = 0.5;

#ifdef QT_DEBUG
#define D2D_TAG(tag) d->dc()->SetTags(tag, tag)
#else
#define D2D_TAG(tag)
#endif

Q_GUI_EXPORT QImage qt_imageForBrush(int brushStyle, bool invert);

static inline ID2D1Factory1 *factory()
{
    return QWindowsDirect2DContext::instance()->d2dFactory();
}

static inline D2D1_MATRIX_3X2_F transformFromLine(const QLineF &line, qreal penWidth, qreal dashOffset)
{
    const qreal halfWidth = penWidth / 2;
    const qreal angle = -qDegreesToRadians(line.angle());
    const qreal sinA = qSin(angle);
    const qreal cosA = qCos(angle);
    QTransform transform = QTransform::fromTranslate(line.p1().x() + dashOffset * cosA + sinA * halfWidth,
                                                     line.p1().y() + dashOffset * sinA - cosA * halfWidth);
    transform.rotateRadians(angle);
    return to_d2d_matrix_3x2_f(transform);
}

static void adjustLine(QPointF *p1, QPointF *p2);
static bool isLinePositivelySloped(const QPointF &p1, const QPointF &p2);

static QVector<D2D1_GRADIENT_STOP> qGradientStopsToD2DStops(const QGradientStops &qstops)
{
    QVector<D2D1_GRADIENT_STOP> stops(qstops.count());
    for (int i = 0, count =  stops.size(); i < count; ++i) {
        stops[i].position = FLOAT(qstops.at(i).first);
        stops[i].color = to_d2d_color_f(qstops.at(i).second);
    }
    return stops;
}

class Direct2DPathGeometryWriter
{
public:
    bool begin()
    {
        HRESULT hr = factory()->CreatePathGeometry(&m_geometry);
        if (FAILED(hr)) {
            qWarning("%s: Could not create path geometry: %#lx", __FUNCTION__, hr);
            return false;
        }

        hr = m_geometry->Open(&m_sink);
        if (FAILED(hr)) {
            qWarning("%s: Could not create geometry sink: %#lx", __FUNCTION__, hr);
            return false;
        }

        return true;
    }

    void setWindingFillEnabled(bool enable)
    {
        if (enable)
            m_sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        else
            m_sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
    }

    void setAliasingEnabled(bool enable)
    {
        m_roundCoordinates = enable;
    }

    void setPositiveSlopeAdjustmentEnabled(bool enable)
    {
        m_adjustPositivelySlopedLines = enable;
    }

    bool isInFigure() const
    {
        return m_inFigure;
    }

    void moveTo(const QPointF &point)
    {
        if (m_inFigure)
            m_sink->EndFigure(D2D1_FIGURE_END_OPEN);

        m_sink->BeginFigure(adjusted(point), D2D1_FIGURE_BEGIN_FILLED);
        m_inFigure = true;
        m_previousPoint = point;
    }

    void lineTo(const QPointF &point)
    {
        QPointF pt = point;
        if (m_adjustPositivelySlopedLines && isLinePositivelySloped(m_previousPoint, point)) {
            moveTo(m_previousPoint - QPointF(0, 1));
            pt -= QPointF(0, 1);
        }
        m_sink->AddLine(adjusted(pt));
        if (pt != point)
            moveTo(point);
        m_previousPoint = point;
    }

    void curveTo(const QPointF &p1, const QPointF &p2, const QPointF &p3)
    {
        D2D1_BEZIER_SEGMENT segment = {
            adjusted(p1),
            adjusted(p2),
            adjusted(p3)
        };

        m_sink->AddBezier(segment);
        m_previousPoint = p3;
    }

    void close()
    {
        if (m_inFigure)
            m_sink->EndFigure(D2D1_FIGURE_END_OPEN);

        m_sink->Close();
    }

    ComPtr<ID2D1PathGeometry1> geometry() const
    {
        return m_geometry;
    }

private:
    D2D1_POINT_2F adjusted(const QPointF &point)
    {
        static const QPointF adjustment(MAGICAL_ALIASING_OFFSET,
                                        MAGICAL_ALIASING_OFFSET);

        if (m_roundCoordinates)
            return to_d2d_point_2f(point + adjustment);
        else
            return to_d2d_point_2f(point);
    }

    ComPtr<ID2D1PathGeometry1> m_geometry;
    ComPtr<ID2D1GeometrySink> m_sink;

    bool m_inFigure = false;
    bool m_roundCoordinates = false;
    bool m_adjustPositivelySlopedLines = false;
    QPointF m_previousPoint;
};

struct D2DVectorPathCache {
    ComPtr<ID2D1PathGeometry1> aliased;
    ComPtr<ID2D1PathGeometry1> antiAliased;

    static void cleanup_func(QPaintEngineEx *engine, void *data) {
        Q_UNUSED(engine);
        D2DVectorPathCache *e = static_cast<D2DVectorPathCache *>(data);
        delete e;
    }
};

struct LinearGradientKey
{
    explicit LinearGradientKey(const QLinearGradient& g)
        : start(g.start()), end(g.finalStop()), stops(g.stops())
    {
    }

    bool operator==(const LinearGradientKey& rhs) const
    {
        return start == rhs.start
            && end == rhs.end
            && stops == rhs.stops;
    }

    QPointF start;
    QPointF end;
    QGradientStops stops;
};

struct RadialGradientKey
{
    explicit RadialGradientKey(const QRadialGradient& g)
        : radius(g.radius()), center(g.center()), focus(g.focalPoint()), stops(g.stops())
    {
    }

    bool operator==(const RadialGradientKey& rhs) const
    {
        return radius == rhs.radius
            && center == rhs.center
            && focus == rhs.focus
            && stops == rhs.stops;
    }

    qreal radius;
    QPointF center;
    QPointF focus;
    QGradientStops stops;
};

struct SolidColorBrushKey
{
    explicit SolidColorBrushKey(const QColor &c)
        : color(c)
    {
    }

    bool operator==(const SolidColorBrushKey& rhs) const
    {
        return color == rhs.color;
    }

    QColor color;
};

uint qHash(const QPointF& p, uint seed)
{
    return qHash(p.y(), qHash(p.x(), seed));
}

uint qHash(const QColor& c, uint seed)
{
    return qHash(c.rgba(), seed);
}

uint qHash(const LinearGradientKey& k, uint seed)
{
    return qHash(k.stops, qHash(k.end, qHash(k.start, seed)));
}

uint qHash(const RadialGradientKey& k, uint seed)
{
    return qHash(k.stops, qHash(k.center, qHash(k.focus, qHash(k.radius, seed))));
}

uint qHash(const SolidColorBrushKey &k, uint seed)
{
    return qHash(k.color, seed);
}

#define Q_BRUSH_CACHE_SIZE_MAX 256
class QDirect2DBrushCache
{
public:
    ComPtr<ID2D1Brush> findLinearGradient(const QLinearGradient *qlinear)
    {
        auto it = m_linearBrushes.find(LinearGradientKey(*qlinear));
        if (it != m_linearBrushes.constEnd())
            return *it;
        return ComPtr<ID2D1Brush>();
    }

    void addLinearGradient(const QLinearGradient *qlinear, const ComPtr<ID2D1Brush> &brush)
    {
        if (m_linearBrushes.size() >= Q_BRUSH_CACHE_SIZE_MAX)
            m_linearBrushes.clear();

        m_linearBrushes.insert(LinearGradientKey(*qlinear), brush);
    }

    ComPtr<ID2D1Brush> findRadialGradient(const QRadialGradient *qradial)
    {
        auto it = m_radialBrushes.find(RadialGradientKey(*qradial));
        if (it != m_radialBrushes.constEnd())
            return *it;
        return ComPtr<ID2D1Brush>();
    }

    void addRadialGradient(const QRadialGradient *qradial, const ComPtr<ID2D1Brush> &brush)
    {
        if (m_radialBrushes.size() >= Q_BRUSH_CACHE_SIZE_MAX)
            m_radialBrushes.clear();

        m_radialBrushes.insert(RadialGradientKey(*qradial), brush);
    }

    ComPtr<ID2D1Brush> findSolidColorBrush(const QColor &color)
    {
        auto it = m_solidColorBrushes.find(SolidColorBrushKey(color));
        if (it != m_solidColorBrushes.constEnd())
            return *it;
        return ComPtr<ID2D1Brush>();
    }

    void addSolidColorBrush(const QColor &color, const ComPtr<ID2D1Brush> &brush)
    {
        if (m_solidColorBrushes.size() >= Q_BRUSH_CACHE_SIZE_MAX)
            m_solidColorBrushes.clear();

        m_solidColorBrushes.insert(SolidColorBrushKey(color), brush);
    }

    void reset()
    {
        m_linearBrushes.clear();
        m_radialBrushes.clear();
        m_solidColorBrushes.clear();
    }

private:
    QHash<LinearGradientKey, ComPtr<ID2D1Brush>> m_linearBrushes;
    QHash<RadialGradientKey, ComPtr<ID2D1Brush>> m_radialBrushes;
    QHash<SolidColorBrushKey, ComPtr<ID2D1Brush>> m_solidColorBrushes;
};

static QHash<ID2D1DeviceContext*, std::shared_ptr<QDirect2DBrushCache>> brushCaches;

void enableDirect2DBrushCache(ID2D1DeviceContext *dc)
{
    if (brushCaches.contains(dc))
        return;
    brushCaches.insert(dc, std::make_shared<QDirect2DBrushCache>());
}

void cleanDirect2DBrushCache(ID2D1DeviceContext *dc)
{
    brushCaches.remove(dc);
}

void clearDirect2DBrushCache(ID2D1DeviceContext* dc)
{
    auto f = brushCaches.find(dc);
    if (f != brushCaches.end())
        f->reset();
}

template<typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

class QWindowsDirect2DPaintEnginePrivate : public QPaintEngineExPrivate
{
    Q_DECLARE_PUBLIC(QWindowsDirect2DPaintEngine)
public:
    QWindowsDirect2DPaintEnginePrivate(QWindowsDirect2DBitmap *bm, QWindowsDirect2DPaintEngine::Flags flags)
        : bitmap(bm)
        , flags(flags)
    {
        pen.reset();
        brush.reset();

        dc()->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        auto iter = brushCaches.constFind(dc());
        if (iter != brushCaches.constEnd())
            m_brushCache = iter.value();
    }

    QWindowsDirect2DBitmap *bitmap;
    QImage fallbackImage;

    unsigned int clipFlags = 0;
    QStack<ClipType> pushedClips;
    QWindowsDirect2DPaintEngine::Flags flags;

    QPointF currentBrushOrigin;

    QHash< QFontDef, ComPtr<IDWriteFontFace> > fontCache;

    struct {
        bool emulate;
        QPen qpen;
        ComPtr<ID2D1Brush> brush;
        ComPtr<ID2D1StrokeStyle1> strokeStyle;
        ComPtr<ID2D1BitmapBrush1> dashBrush;
        int dashLength;

        inline void reset() {
            emulate = false;
            qpen = QPen();
            brush.Reset();
            strokeStyle.Reset();
            dashBrush.Reset();
            dashLength = 0;
        }
    } pen;

    struct {
        bool emulate;
        QBrush qbrush;
        ComPtr<ID2D1Brush> brush;

        inline void reset() {
            emulate = false;
            brush.Reset();
            qbrush = QBrush();
        }
    } brush;

    inline bool own(ID2D1Resource* res)
    {
        ComPtr<ID2D1Factory> resfactory, myfactory;
        res->GetFactory(&resfactory);
        dc()->GetFactory(&myfactory);
        return resfactory == myfactory;
    }

    inline ID2D1DeviceContext *dc() const
    {
        Q_ASSERT(bitmap);
        return bitmap->deviceContext()->get();
    }

    inline D2D1_INTERPOLATION_MODE interpolationMode() const
    {
        Q_Q(const QWindowsDirect2DPaintEngine);
        return (q->state()->renderHints & QPainter::SmoothPixmapTransform)
                ? (q->state()->renderHints & QPainter::HighQualityPixmapTransform
                           ? D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC
                           : D2D1_INTERPOLATION_MODE_LINEAR)
                : D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    }

    inline D2D1_ANTIALIAS_MODE antialiasMode() const
    {
        Q_Q(const QWindowsDirect2DPaintEngine);
        return (q->state()->renderHints & QPainter::Antialiasing) ? D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
                                                                  : D2D1_ANTIALIAS_MODE_ALIASED;
    }

    inline D2D1_LAYER_OPTIONS1 layerOptions() const
    {
        if (flags & QWindowsDirect2DPaintEngine::TranslucentTopLevelWindow)
            return D2D1_LAYER_OPTIONS1_NONE;
        else
            return D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
    }

    void updateTransform(const QTransform &transform)
    {
        dc()->SetTransform(to_d2d_matrix_3x2_f(transform));
    }

    void updateOpacity(qreal opacity)
    {
        if (brush.brush)
            brush.brush->SetOpacity(FLOAT(opacity));
        if (pen.brush)
            pen.brush->SetOpacity(FLOAT(opacity));
    }

    void pushClip(const QVectorPath &path)
    {
        Q_Q(QWindowsDirect2DPaintEngine);

        if (path.isEmpty()) {
            D2D_RECT_F rect = {0, 0, 0, 0};
            dc()->PushAxisAlignedClip(rect, antialiasMode());
            pushedClips.push(AxisAlignedClip);
        } else if (path.isRect() && (q->state()->matrix.type() <= QTransform::TxScale)) {
            const qreal * const points = path.points();
            D2D_RECT_F rect = {
                FLOAT(points[0]), // left
                FLOAT(points[1]), // top
                FLOAT(points[2]), // right,
                FLOAT(points[5])  // bottom
            };

            dc()->PushAxisAlignedClip(rect, antialiasMode());
            pushedClips.push(AxisAlignedClip);
        } else {
            ComPtr<ID2D1PathGeometry1> geometry = vectorPathToID2D1PathGeometry(path);
            if (!geometry) {
                qWarning("%s: Could not convert vector path to painter path!", __FUNCTION__);
                return;
            }

            dc()->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                                   geometry.Get(),
                                                   antialiasMode(),
                                                   D2D1::IdentityMatrix(),
                                                   1.0,
                                                   nullptr,
                                                   layerOptions()),
                            nullptr);
            pushedClips.push(LayerClip);
        }
    }

    void clearClips()
    {
        while (!pushedClips.isEmpty()) {
            switch (pushedClips.pop()) {
            case AxisAlignedClip:
                dc()->PopAxisAlignedClip();
                break;
            case LayerClip:
                dc()->PopLayer();
                break;
            }
        }
    }

    void updateClipEnabled(bool enabled)
    {
        if (!enabled)
            clearClips();
        else if (pushedClips.isEmpty())
            replayClipOperations();
    }

    void clip(const QVectorPath &path, Qt::ClipOperation operation)
    {
        switch (operation) {
        case Qt::NoClip:
            clearClips();
            break;
        case Qt::ReplaceClip:
            clearClips();
            pushClip(path);
            break;
        case Qt::IntersectClip:
            pushClip(path);
            break;
        }
    }

    void popClip(int count)
    {
        if (count <= 0)
            return;

        Q_ASSERT(pushedClips.size() >= count);
        if (count > pushedClips.size())
            count = pushedClips.size();

        while (count--) {
            switch (pushedClips.pop()) {
            case AxisAlignedClip:
                dc()->PopAxisAlignedClip();
                break;
            case LayerClip:
                dc()->PopLayer();
                break;
            }
        }
    }

    void updateCompositionMode(QPainter::CompositionMode mode)
    {
        bool needComposite = false;
        switch (mode) {
        case QPainter::CompositionMode_Source:
            dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            break;
        case QPainter::CompositionMode_SourceOver:
            dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
            break;
        case QPainter::CompositionMode_Difference:
        {
            // The effect is not the same as CompositionMode_Difference, so for the time being
            enterComposite(D2D1_COMPOSITE_MODE_MASK_INVERT);
            needComposite = true;
            break;
        }
        default:
            // Activating an unsupported mode at any time will cause the QImage
            // fallback to be used for the remainder of the active paint session
            dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            flags |= QWindowsDirect2DPaintEngine::EmulateComposition;
            break;
        }

        if (!needComposite)
            leaveComposite();
    }

    void updateBrush(const QBrush &newBrush)
    {
        Q_Q(const QWindowsDirect2DPaintEngine);

        if (qbrush_fast_equals(brush.qbrush, newBrush) && (brush.brush || brush.emulate))
            return;

        brush.brush = to_d2d_brush(newBrush, &brush.emulate);
        brush.qbrush = newBrush;

        if (brush.brush) {
            brush.brush->SetOpacity(FLOAT(q->state()->opacity));
            applyBrushOrigin(currentBrushOrigin);
        }
    }

    void updateBrushOrigin(const QPointF &brushOrigin)
    {
        negateCurrentBrushOrigin();
        applyBrushOrigin(brushOrigin);
    }

    void negateCurrentBrushOrigin()
    {
        if (brush.brush && !currentBrushOrigin.isNull()) {
            D2D1_MATRIX_3X2_F transform;
            brush.brush->GetTransform(&transform);

            brush.brush->SetTransform(*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
                                      * D2D1::Matrix3x2F::Translation(FLOAT(-currentBrushOrigin.x()),
                                                                      FLOAT(-currentBrushOrigin.y())));
        }
    }

    void applyBrushOrigin(const QPointF &origin)
    {
        if (brush.brush && !origin.isNull()) {
            D2D1_MATRIX_3X2_F transform;
            brush.brush->GetTransform(&transform);

            brush.brush->SetTransform(*(D2D1::Matrix3x2F::ReinterpretBaseType(&transform))
                                      * D2D1::Matrix3x2F::Translation(FLOAT(origin.x()), FLOAT(origin.y())));
        }

        currentBrushOrigin = origin;
    }

    void updatePen(const QPen &newPen)
    {
        Q_Q(const QWindowsDirect2DPaintEngine);
        if (qpen_fast_equals(newPen, pen.qpen) && (pen.brush || pen.emulate))
            return;

        pen.reset();
        pen.qpen = newPen;

        if (newPen.style() == Qt::NoPen)
            return;

        pen.brush = to_d2d_brush(newPen.brush(), &pen.emulate);
        if (!pen.brush)
            return;

        pen.brush->SetOpacity(FLOAT(q->state()->opacity));

        D2D1_STROKE_STYLE_PROPERTIES1 props = {};

        switch (newPen.capStyle()) {
        case Qt::SquareCap:
            props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_SQUARE;
            break;
        case Qt::RoundCap:
            props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_ROUND;
            break;
        case Qt::FlatCap:
        default:
            props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;
            break;
        }

        switch (newPen.joinStyle()) {
        case Qt::BevelJoin:
            props.lineJoin = D2D1_LINE_JOIN_BEVEL;
            break;
        case Qt::RoundJoin:
            props.lineJoin = D2D1_LINE_JOIN_ROUND;
            break;
        case Qt::MiterJoin:
        default:
            props.lineJoin = D2D1_LINE_JOIN_MITER;
            break;
        }

        props.miterLimit = FLOAT(newPen.miterLimit() * qreal(2.0)); // D2D and Qt miter specs differ
        props.dashOffset = FLOAT(newPen.dashOffset());

        if (newPen.widthF() == 0) {
            props.transformType = D2D1_STROKE_TRANSFORM_TYPE_HAIRLINE;
            pen.qpen.setWidth(1);
        }
        else if (qt_pen_is_cosmetic(newPen, q->state()->renderHints))
            props.transformType = D2D1_STROKE_TRANSFORM_TYPE_FIXED;
        else
            props.transformType = D2D1_STROKE_TRANSFORM_TYPE_NORMAL;

        switch (newPen.style()) {
        case Qt::SolidLine:
            props.dashStyle = D2D1_DASH_STYLE_SOLID;
            break;

        case Qt::DotLine:
        case Qt::DashDotLine:
        case Qt::DashDotDotLine:
            // Try and match Qt's raster engine in output as closely as possible
            if (newPen.widthF() <= 1.0)
                props.startCap = props.endCap = props.dashCap = D2D1_CAP_STYLE_FLAT;

            Q_FALLTHROUGH();
        default:
            props.dashStyle = D2D1_DASH_STYLE_CUSTOM;
            break;
        }

        HRESULT hr;

        if (props.dashStyle == D2D1_DASH_STYLE_CUSTOM) {
            QVector<qreal> dashes = newPen.dashPattern();
            QVector<FLOAT> converted(dashes.size());
            qreal penWidth = pen.qpen.widthF();
            qreal brushWidth = 0;
            for (int i = 0; i < dashes.size(); i++) {
                converted[i] = FLOAT(dashes[i]);
                brushWidth += penWidth * dashes[i];
            }

            hr = factory()->CreateStrokeStyle(props, converted.constData(), UINT32(converted.size()), &pen.strokeStyle);

            // Create a combined brush/dash pattern for optimized line drawing
            QWindowsDirect2DBitmap bitmap;
            bitmap.resize(int(ceil(brushWidth)), int(ceil(penWidth)));
            bitmap.deviceContext()->begin();
            bitmap.deviceContext()->get()->SetAntialiasMode(antialiasMode());
            bitmap.deviceContext()->get()->SetTransform(D2D1::IdentityMatrix());
            bitmap.deviceContext()->get()->Clear();
            const qreal offsetX = (qreal(bitmap.size().width()) - brushWidth) / 2;
            const qreal offsetY = qreal(bitmap.size().height()) / 2;
            bitmap.deviceContext()->get()->DrawLine(D2D1::Point2F(FLOAT(offsetX), FLOAT(offsetY)),
                                                    D2D1::Point2F(FLOAT(brushWidth), FLOAT(offsetY)),
                                                    pen.brush.Get(), FLOAT(penWidth), pen.strokeStyle.Get());
            bitmap.deviceContext()->end();
            D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = D2D1::BitmapBrushProperties1(
                        D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_CLAMP, D2D1_INTERPOLATION_MODE_LINEAR);
            hr = dc()->CreateBitmapBrush(bitmap.bitmap(), bitmapBrushProperties, &pen.dashBrush);
            pen.dashLength = bitmap.size().width();
        } else {
            hr = factory()->CreateStrokeStyle(props, nullptr, 0, &pen.strokeStyle);
        }

        if (FAILED(hr))
            qWarning("%s: Could not create stroke style: %#lx", __FUNCTION__, hr);
    }

    ComPtr<ID2D1Brush> toLinearGradinetBrush(const QLinearGradient *qlinear)
    {
        ComPtr<ID2D1Brush> result;

        if (!m_brushCache.expired()) {
            result = m_brushCache.lock()->findLinearGradient(qlinear);
            if (result)
                return result;
        }

        HRESULT hr = E_FAIL;
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linearGradientBrushProperties;
        ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

        linearGradientBrushProperties.startPoint = to_d2d_point_2f(qlinear->start());
        linearGradientBrushProperties.endPoint = to_d2d_point_2f(qlinear->finalStop());

        const QVector<D2D1_GRADIENT_STOP> stops = qGradientStopsToD2DStops(qlinear->stops());

        hr = dc()->CreateGradientStopCollection(stops.constData(),
                                                UINT32(stops.size()),
                                                &gradientStopCollection);
        if (FAILED(hr)) {
            qWarning("%s: Could not create gradient stop collection for linear gradient: %#lx", __FUNCTION__, hr);
            return result;
        }

        ComPtr<ID2D1LinearGradientBrush> linear;
        hr = dc()->CreateLinearGradientBrush(linearGradientBrushProperties, gradientStopCollection.Get(),
                                             &linear);
        if (FAILED(hr)) {
            qWarning("%s: Could not create Direct2D linear gradient brush: %#lx", __FUNCTION__, hr);
            return result;
        }

        result.Attach(linear.Detach());

        if (!m_brushCache.expired())
            m_brushCache.lock()->addLinearGradient(qlinear, result);

        return result;
    }

    ComPtr<ID2D1Brush> toRadialGradinetBrush(const QRadialGradient* qradial)
    {
        ComPtr<ID2D1Brush> result;

        if (!m_brushCache.expired()) {
            result = m_brushCache.lock()->findRadialGradient(qradial);
            if (result)
                return result;
        }

        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radialGradientBrushProperties;
        ComPtr<ID2D1GradientStopCollection> gradientStopCollection;

        radialGradientBrushProperties.center = to_d2d_point_2f(qradial->center());
        radialGradientBrushProperties.gradientOriginOffset = to_d2d_point_2f(qradial->focalPoint() - qradial->center());
        radialGradientBrushProperties.radiusX = FLOAT(qradial->radius());
        radialGradientBrushProperties.radiusY = FLOAT(qradial->radius());

        const QVector<D2D1_GRADIENT_STOP> stops = qGradientStopsToD2DStops(qradial->stops());

        HRESULT hr = dc()->CreateGradientStopCollection(stops.constData(), stops.size(), &gradientStopCollection);
        if (FAILED(hr)) {
            qWarning("%s: Could not create gradient stop collection for radial gradient: %#lx", __FUNCTION__, hr);
            return result;
        }

        ComPtr<ID2D1RadialGradientBrush> radial;
        hr = dc()->CreateRadialGradientBrush(radialGradientBrushProperties, gradientStopCollection.Get(),
                                             &radial);
        if (FAILED(hr)) {
            qWarning("%s: Could not create Direct2D radial gradient brush: %#lx", __FUNCTION__, hr);
            return result;
        }

        result.Attach(radial.Detach());

        if (!m_brushCache.expired())
            m_brushCache.lock()->addRadialGradient(qradial, result);

        return result;
    }

    ComPtr<ID2D1Brush> to_d2d_brush(const QBrush &newBrush, bool *needsEmulation)
    {
        HRESULT hr;
        ComPtr<ID2D1Brush> result;

        Q_ASSERT(needsEmulation);

        *needsEmulation = false;

        switch (newBrush.style()) {
        case Qt::NoBrush:
            break;

        case Qt::SolidPattern:
        {
            const QColor color = newBrush.color();
            if (!m_brushCache.expired()) {
                result = m_brushCache.lock()->findSolidColorBrush(color);
                if (result)
                    break;
            }

            ComPtr<ID2D1SolidColorBrush> solid;

            hr = dc()->CreateSolidColorBrush(to_d2d_color_f(color), &solid);
            if (FAILED(hr)) {
                qWarning("%s: Could not create solid color brush: %#lx", __FUNCTION__, hr);
                break;
            }

            result.Attach(solid.Detach());

            if (!m_brushCache.expired())
                m_brushCache.lock()->addSolidColorBrush(color, result);
        }
            break;

        case Qt::Dense1Pattern:
        case Qt::Dense2Pattern:
        case Qt::Dense3Pattern:
        case Qt::Dense4Pattern:
        case Qt::Dense5Pattern:
        case Qt::Dense6Pattern:
        case Qt::Dense7Pattern:
        case Qt::HorPattern:
        case Qt::VerPattern:
        case Qt::CrossPattern:
        case Qt::BDiagPattern:
        case Qt::FDiagPattern:
        case Qt::DiagCrossPattern:
        {
            ComPtr<ID2D1BitmapBrush1> bitmapBrush;
            D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = {
                D2D1_EXTEND_MODE_WRAP,
                D2D1_EXTEND_MODE_WRAP,
                interpolationMode()
            };

            QImage brushImg = qt_imageForBrush(newBrush.style(), false);
            brushImg.setColor(0, newBrush.color().rgba());
            brushImg.setColor(1, qRgba(0, 0, 0, 0));

            QWindowsDirect2DBitmap bitmap;
            bool success = bitmap.fromImage(brushImg, Qt::AutoColor);
            if (!success) {
                qWarning("%s: Could not create Direct2D bitmap from Qt pattern brush image", __FUNCTION__);
                break;
            }

            hr = dc()->CreateBitmapBrush(bitmap.bitmap(),
                                         bitmapBrushProperties,
                                         &bitmapBrush);
            if (FAILED(hr)) {
                qWarning("%s: Could not create Direct2D bitmap brush for Qt pattern brush: %#lx", __FUNCTION__, hr);
                break;
            }

            result.Attach(bitmapBrush.Detach());
        }
            break;

        case Qt::LinearGradientPattern:
            if (newBrush.gradient()->spread() != QGradient::PadSpread) {
                *needsEmulation = true;
            } else {
                auto qlinear = static_cast<const QLinearGradient *>(newBrush.gradient());
                result = toLinearGradinetBrush(qlinear);
            }
            break;

        case Qt::RadialGradientPattern:
            if (newBrush.gradient()->spread() != QGradient::PadSpread) {
                *needsEmulation = true;
            } else {
                auto qradial = static_cast<const QRadialGradient *>(newBrush.gradient());
                result = toRadialGradinetBrush(qradial);
            }
            break;

        case Qt::ConicalGradientPattern:
            *needsEmulation = true;
            break;

        case Qt::TexturePattern:
        {
            ComPtr<ID2D1BitmapBrush1> bitmapBrush;
            D2D1_BITMAP_BRUSH_PROPERTIES1 bitmapBrushProperties = {
                D2D1_EXTEND_MODE_WRAP,
                D2D1_EXTEND_MODE_WRAP,
                interpolationMode()
            };

            QWindowsDirect2DPlatformPixmap *pp = static_cast<QWindowsDirect2DPlatformPixmap *>(newBrush.texture().handle());
            QWindowsDirect2DBitmap *bitmap = pp->bitmap();
            hr = dc()->CreateBitmapBrush(bitmap->bitmap(),
                                         bitmapBrushProperties,
                                         &bitmapBrush);

            if (FAILED(hr)) {
                qWarning("%s: Could not create texture brush: %#lx", __FUNCTION__, hr);
                break;
            }

            result.Attach(bitmapBrush.Detach());
        }
            break;
        }

        if (result && !newBrush.transform().isIdentity())
            result->SetTransform(to_d2d_matrix_3x2_f(newBrush.transform()));

        return result;
    }

    ComPtr<ID2D1PathGeometry1> vectorPathToID2D1PathGeometry(const QVectorPath &path, bool isStroke = false)
    {
        Q_Q(QWindowsDirect2DPaintEngine);

        const bool alias = !q->antiAliasingEnabled();

        QVectorPath::CacheEntry *cacheEntry = path.isCacheable() ? path.lookupCacheData(q)
                                                                 : nullptr;

        if (cacheEntry) {
            D2DVectorPathCache *e = static_cast<D2DVectorPathCache *>(cacheEntry->data);
            if (alias && e->aliased)
                return e->aliased;
            else if (!alias && e->antiAliased)
                return e->antiAliased;
        }

        Direct2DPathGeometryWriter writer;
        if (!writer.begin())
            return nullptr;

        writer.setWindingFillEnabled(path.hasWindingFill());
        writer.setAliasingEnabled(alias);
        writer.setPositiveSlopeAdjustmentEnabled(isStroke && (path.shape() == QVectorPath::LinesHint
                                                 || path.shape() == QVectorPath::PolygonHint));

        const QPainterPath::ElementType *types = path.elements();
        const int count = path.elementCount();
        const qreal *points = path.points();

        Q_ASSERT(points);

        if (types) {
            qreal x, y;

            for (int i = 0; i < count; i++) {
                x = points[i * 2];
                y = points[i * 2 + 1];

                switch (types[i]) {
                case QPainterPath::MoveToElement:
                    writer.moveTo(QPointF(x, y));
                    break;

                case QPainterPath::LineToElement:
                    writer.lineTo(QPointF(x, y));
                    break;

                case QPainterPath::CurveToElement:
                {
                    Q_ASSERT((i + 2) < count);
                    Q_ASSERT(types[i+1] == QPainterPath::CurveToDataElement);
                    Q_ASSERT(types[i+2] == QPainterPath::CurveToDataElement);

                    i++;
                    const qreal x2 = points[i * 2];
                    const qreal y2 = points[i * 2 + 1];

                    i++;
                    const qreal x3 = points[i * 2];
                    const qreal y3 = points[i * 2 + 1];

                    writer.curveTo(QPointF(x, y), QPointF(x2, y2), QPointF(x3, y3));
                }
                    break;

                case QPainterPath::CurveToDataElement:
                    qWarning("%s: Unhandled Curve Data Element", __FUNCTION__);
                    break;
                }
            }
        } else {
            writer.moveTo(QPointF(points[0], points[1]));
            for (int i = 1; i < count; i++)
                writer.lineTo(QPointF(points[i * 2], points[i * 2 + 1]));
        }

        if (writer.isInFigure())
            if (path.hasImplicitClose())
                writer.lineTo(QPointF(points[0], points[1]));

        writer.close();
        ComPtr<ID2D1PathGeometry1> geometry = writer.geometry();

        if (path.isCacheable()) {
            if (!cacheEntry)
                cacheEntry = path.addCacheData(q, new D2DVectorPathCache, D2DVectorPathCache::cleanup_func);

            D2DVectorPathCache *e = static_cast<D2DVectorPathCache *>(cacheEntry->data);
            if (alias)
                e->aliased = geometry;
            else
                e->antiAliased = geometry;
        } else {
            path.makeCacheable();
        }

        return geometry;
    }

    void updateHints()
    {
        dc()->SetAntialiasMode(antialiasMode());
    }

    void drawGlyphRun(const D2D1_POINT_2F &pos,
                      IDWriteFontFace *fontFace,
                      const QFontDef &fontDef,
                      int numGlyphs,
                      const UINT16 *glyphIndices,
                      const FLOAT *glyphAdvances,
                      const DWRITE_GLYPH_OFFSET *glyphOffsets,
                      bool rtl)
    {
        Q_Q(QWindowsDirect2DPaintEngine);

        DWRITE_GLYPH_RUN glyphRun = {
            fontFace,          //    IDWriteFontFace           *fontFace;
            FLOAT(fontDef.pixelSize), // FLOAT                 fontEmSize;
            UINT32(numGlyphs), //    UINT32                    glyphCount;
            glyphIndices,      //    const UINT16              *glyphIndices;
            glyphAdvances,     //    const FLOAT               *glyphAdvances;
            glyphOffsets,      //    const DWRITE_GLYPH_OFFSET *glyphOffsets;
            FALSE,             //    BOOL                      isSideways;
            rtl ? 1u : 0u      //    UINT32                    bidiLevel;
        };

        const bool antiAlias = bool((q->state()->renderHints & QPainter::TextAntialiasing)
                                    && !(fontDef.styleStrategy & QFont::NoAntialias));
        const D2D1_TEXT_ANTIALIAS_MODE antialiasMode = (flags & QWindowsDirect2DPaintEngine::TranslucentTopLevelWindow)
                ? D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE : D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
        dc()->SetTextAntialiasMode(antiAlias ? antialiasMode : D2D1_TEXT_ANTIALIAS_MODE_ALIASED);

        dc()->DrawGlyphRun(pos,
                           &glyphRun,
                           nullptr,
                           pen.brush.Get(),
                           DWRITE_MEASURING_MODE_GDI_CLASSIC);
    }

    void stroke(const QVectorPath &path)
    {
        Q_Q(QWindowsDirect2DPaintEngine);

        // Default path (no optimization)
        if (!(path.shape() == QVectorPath::LinesHint || path.shape() == QVectorPath::PolygonHint)
                || !pen.dashBrush || q->state()->renderHints.testFlag(QPainter::HighQualityAntialiasing)) {
            ComPtr<ID2D1Geometry> geometry = vectorPathToID2D1PathGeometry(path, true);
            if (!geometry) {
                qWarning("%s: Could not convert path to d2d geometry", __FUNCTION__);
                return;
            }
            dc()->DrawGeometry(geometry.Get(), pen.brush.Get(),
                               FLOAT(pen.qpen.widthF()), pen.strokeStyle.Get());
            return;
        }

        // Optimized dash line drawing
        const bool isPolygon = path.shape() == QVectorPath::PolygonHint && path.elementCount() >= 3;
        const bool implicitClose = isPolygon && (path.hints() & QVectorPath::ImplicitClose);
        const bool skipJoin = !isPolygon // Non-polygons don't require joins
                || (pen.qpen.joinStyle() == Qt::MiterJoin && qFuzzyIsNull(pen.qpen.miterLimit()));
        const qreal *points = path.points();
        const int lastElement = path.elementCount() - (implicitClose ? 1 : 2);
        qreal dashOffset = 0;
        QPointF jointStart;
        ID2D1Brush *brush = pen.dashBrush ? pen.dashBrush.Get() : pen.brush.Get();
        for (int i = 0; i <= lastElement; ++i) {
            QPointF p1(points[i * 2], points[i * 2 + 1]);
            QPointF p2 = implicitClose && i == lastElement ? QPointF(points[0], points[1])
                                                           : QPointF(points[i * 2 + 2], points[i * 2 + 3]);
            if (!isPolygon) // Advance the count for lines
                ++i;

            // Match raster engine output
            if (p1 == p2 && pen.qpen.widthF() <= 1.0) {
                q->fillRect(QRectF(p1, QSizeF(pen.qpen.widthF(), pen.qpen.widthF())), pen.qpen.brush());
                continue;
            }

            if (!q->antiAliasingEnabled())
                adjustLine(&p1, &p2);

            q->adjustForAliasing(&p1);
            q->adjustForAliasing(&p2);

            const QLineF line(p1, p2);
            const qreal lineLength = line.length();
            if (pen.dashBrush) {
                pen.dashBrush->SetTransform(transformFromLine(line, pen.qpen.widthF(), dashOffset));
                dashOffset = pen.dashLength - fmod(lineLength - dashOffset, pen.dashLength);
            }
            dc()->DrawLine(to_d2d_point_2f(p1), to_d2d_point_2f(p2),
                           brush, FLOAT(pen.qpen.widthF()), pen.strokeStyle.Get());

            if (skipJoin)
                continue;

            // Patch the join with the original brush
            const qreal patchSegment = pen.dashBrush ? qBound(0.0, (pen.dashLength - dashOffset) / lineLength, 1.0)
                                                     : pen.qpen.widthF();
            if (i > 0) {
                Direct2DPathGeometryWriter writer;
                writer.begin();
                writer.moveTo(jointStart);
                writer.lineTo(p1);
                writer.lineTo(line.pointAt(patchSegment));
                writer.close();
                dc()->DrawGeometry(writer.geometry().Get(), pen.brush.Get(),
                                   FLOAT(pen.qpen.widthF()), pen.strokeStyle.Get());
            }
            // Record the start position of the next joint
            jointStart = line.pointAt(1 - patchSegment);

            if (implicitClose && i == lastElement) { // Close the polygon
                Direct2DPathGeometryWriter writer;
                writer.begin();
                writer.moveTo(jointStart);
                writer.lineTo(p2);
                writer.lineTo(QLineF(p2, QPointF(points[2], points[3])).pointAt(patchSegment));
                writer.close();
                dc()->DrawGeometry(writer.geometry().Get(), pen.brush.Get(),
                                   FLOAT(pen.qpen.widthF()), pen.strokeStyle.Get());
            }
        }
    }

    static QString getDWriteFontPath(IDWriteFontFace *pFace)
    {
        UINT32 count = 0;
        HRESULT hr = pFace->GetFiles(&count, nullptr);
        if (count != 1)
            return QString();

        ComPtr<IDWriteFontFile> spFontFile;
        pFace->GetFiles(&count, &spFontFile);
        UINT32 refSize = 0;
        wchar_t *pKey = 0;
        hr = spFontFile->GetReferenceKey((const void **)&pKey, &refSize);
        ComPtr<IDWriteFontFileLoader> spLoader;
        hr = spFontFile->GetLoader(&spLoader);
        if (!spLoader)
            return QString();

        ComPtr<IDWriteLocalFontFileLoader> spLocalLoader;
        spLoader.CopyTo(spLocalLoader.GetAddressOf());
        if (!spLocalLoader)
            return QString();

        UINT32 pathLen = 0;
        hr = spLocalLoader->GetFilePathLengthFromKey(pKey, refSize, &pathLen);
        std::wstring path(pathLen + 1, 0);
        hr = spLocalLoader->GetFilePathFromKey(pKey, refSize, &path[0], pathLen + 1);
        return QString::fromStdWString(path);
    }

    static bool compareFontStyle(DWRITE_FONT_STYLE dest, uint src)
    {
        return src == QFont::StyleNormal && dest == DWRITE_FONT_STYLE_NORMAL
                || src > QFont::StyleNormal && dest > DWRITE_FONT_STYLE_NORMAL;
    }

    static bool compareFontWeight(DWRITE_FONT_WEIGHT dest, uint src)
    {
        return src == QFont::Light && dest == DWRITE_FONT_WEIGHT_LIGHT
                || src == QFont::Normal && dest == DWRITE_FONT_WEIGHT_NORMAL
                || src == QFont::DemiBold && dest == DWRITE_FONT_WEIGHT_DEMI_BOLD
                || src == QFont::Bold && dest == DWRITE_FONT_WEIGHT_BOLD
                || src == QFont::Black && dest == DWRITE_FONT_WEIGHT_BLACK;
    }

    static bool compareFontStretch(DWRITE_FONT_STRETCH dest, uint src)
    {
        return src == QFont::UltraCondensed && dest == DWRITE_FONT_STRETCH_ULTRA_CONDENSED
                || src == QFont::ExtraCondensed && dest == DWRITE_FONT_STRETCH_EXTRA_CONDENSED
                || src == QFont::Condensed && dest == DWRITE_FONT_STRETCH_CONDENSED
                || src == QFont::SemiCondensed && dest == DWRITE_FONT_STRETCH_SEMI_CONDENSED
                || src == QFont::Unstretched && dest == DWRITE_FONT_STRETCH_NORMAL
                || src == QFont::SemiExpanded && dest == DWRITE_FONT_STRETCH_SEMI_EXPANDED
                || src == QFont::Expanded && dest == DWRITE_FONT_STRETCH_EXPANDED
                || src == QFont::ExtraExpanded && dest == DWRITE_FONT_STRETCH_EXTRA_EXPANDED
                || src == QFont::UltraExpanded && dest == DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
    }

    static bool findDirectWriteFontForLOGFONT(const QFontEngine *fontEngine, LOGFONT *pLogicFont, IDWriteFontFace **ppFontFace)
    {
        ComPtr<IDWriteGdiInterop> spGdiInterop;
        HRESULT hr = QWindowsDirect2DContext::instance()->dwriteFactory()->GetGdiInterop(&spGdiInterop);
        if (FAILED(hr) || !spGdiInterop)
            return false;

        ComPtr<IDWriteFontCollection> spFontCol;
        hr = QWindowsDirect2DContext::instance()->dwriteFactory()->GetSystemFontCollection(&spFontCol);
        if (FAILED(hr) && !spFontCol)
            return false;

        HDC hDC = GetDC(NULL);
        HFONT hFont = CreateFontIndirect(pLogicFont);
        HGDIOBJ hOldGdiObj = SelectObject(hDC, hFont);

        ComPtr<IDWriteFontFace> spGdiFontFace;
        hr = spGdiInterop->CreateFontFaceFromHdc(hDC, &spGdiFontFace);

        ComPtr<IDWriteFontFace> spCollectionFontFace;

        auto createFontFaceFromCollection = [&] {
            if (FAILED(hr) || !spGdiFontFace)
                return E_FAIL;

            ComPtr<IDWriteFont> spCollectionFont;
            hr = spFontCol->GetFontFromFontFace(spGdiFontFace.Get(), &spCollectionFont);
            if (FAILED(hr) || !spCollectionFont)
                return E_FAIL;

            hr = spCollectionFont->CreateFontFace(&spCollectionFontFace);
            return SUCCEEDED(hr) && spCollectionFontFace ? S_OK : E_FAIL;
        };
        hr = createFontFaceFromCollection();

        SelectObject(hDC, hOldGdiObj);

        DeleteObject(hFont);
        ReleaseDC(NULL, hDC);

        if (FAILED(hr)
            || !spCollectionFontFace
            || spCollectionFontFace->GetGlyphCount() != fontEngine->glyphCount()) {
            return false;
        }

#ifdef DEBUG_FONT_INFO
        qDebug() << "font path : " << getDWriteFontPath(spGdiFontFace.Get());
#endif
        *ppFontFace = spCollectionFontFace.Detach();
        return true;
    }

    static bool findFontInSysCollection(const QFontEngine *fontEngine, QFontDef &fontDef, IDWriteFontFace **ppFontFace)
    {
        QString &fam = fontDef.family;
        if (fam.startsWith(QLatin1Char('@')))
            fam.remove(0, 1);

        ComPtr<IDWriteFontCollection> spFontCol;
        HRESULT hr = QWindowsDirect2DContext::instance()->dwriteFactory()->GetSystemFontCollection(
                &spFontCol);
        if (!spFontCol)
            return false;

#ifdef DEBUG_FONT_INFO
        travelSystemFonts(spFontCol.Get());
#endif

        UINT32 i = 0;
        BOOL exsists = FALSE;
        hr = spFontCol->FindFamilyName((const WCHAR *)fam.utf16(), &i, &exsists);
        if (!exsists)
            return false;

        ComPtr<IDWriteFontFamily> spFontFamily;
        hr = spFontCol->GetFontFamily(i, &spFontFamily);
        if (FAILED(hr) || !spFontFamily)
            return false;

        // Find the font with the same name and compare the font attributes
        UINT32 fontCnt = spFontFamily->GetFontCount();
        for (UINT32 j = 0; j < fontCnt; ++j) {
            ComPtr<IDWriteFont> spFont;
            hr = spFontFamily->GetFont(j, &spFont);
            if (FAILED(hr) || !spFont)
                continue;

            // Italic
            if (!compareFontStyle(spFont->GetStyle(), fontDef.style))
                continue;

            // Bold
            if (!compareFontWeight(spFont->GetWeight(), fontDef.weight))
                continue;

            if (!compareFontStretch(spFont->GetStretch(), fontDef.stretch))
                continue;

            // TODO: Others
            // Compare the number of font primitives to ensure that they are two identical fonts
            ComPtr<IDWriteFontFace> spFace;
            hr = spFont->CreateFontFace(&spFace);
#ifdef DEBUG_FONT_INFO
            qDebug() << "font path : " << getDWriteFontPath(spFace.Get());
#endif
            if (SUCCEEDED(hr) && spFace->GetGlyphCount() == fontEngine->glyphCount()) {
                *ppFontFace = spFace.Detach();
                return true;
            }
        }
        return false;
    }

    static bool matchDWriteFont(const QFontEngine *fontEngine, IDWriteFontFace **ppFontFace)
    {
        QFontDef fontDef = fontEngine->fontDef;
        // First, get a matching font from the system font collection exposed by
        // DirectWrite.
        if (findFontInSysCollection(fontEngine, fontDef, ppFontFace))
            return true;

        LOGFONT logicFont = QWindowsFontDatabase::fontDefToLOGFONT(fontDef, QString());
        // Second try to find the logic font by using GDI interface.
        // If that fails then try and find a match from the DirectWrite font collection.
        if (findDirectWriteFontForLOGFONT(fontEngine, &logicFont, ppFontFace))
            return true;

        // If we fail to find a match then try fallback to the default font on the system.
        NONCLIENTMETRICS metrics = { 0 };
        metrics.cbSize = sizeof(metrics);
        if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
            return false;
        }

        if (wcsncmp(logicFont.lfFaceName, metrics.lfMessageFont.lfFaceName,
                    arraysize(logicFont.lfFaceName))) {
            wcscpy_s(logicFont.lfFaceName, arraysize(logicFont.lfFaceName),
                     metrics.lfMessageFont.lfFaceName);
            return findDirectWriteFontForLOGFONT(fontEngine, &logicFont, ppFontFace);
        }
        return false;
    }

#ifdef DEBUG_FONT_INFO
    static void travelSystemFonts(IDWriteFontCollection *pFontCol)
    {
        static bool travelOnce = false;
        if (travelOnce)
            return;

        qDebug() << "begin travel system font by dwrite";
        HRESULT hr = S_OK;
        auto allFontCnt = pFontCol->GetFontFamilyCount();
        for (UINT32 i = 0; i < allFontCnt; ++i) {
            ComPtr<IDWriteFontFamily> spCurrFontFamily;
            hr = pFontCol->GetFontFamily(i, &spCurrFontFamily);
            if (FAILED(hr) || !spCurrFontFamily)
                return;

            ComPtr<IDWriteLocalizedStrings> spStrings;
            spCurrFontFamily->GetFamilyNames(&spStrings);
            auto nameCnt = spStrings->GetCount();
            for (UINT32 j = 0; j < nameCnt; ++j) {
                UINT32 strLen = 0;
                hr = spStrings->GetLocaleNameLength(j, &strLen);
                std::vector<WCHAR> localName(strLen + 1, 0);
                hr = spStrings->GetLocaleName(j, localName.data(), strLen + 1);
                qDebug() << "font locale: " << QString::fromUtf16((const ushort *)localName.data());
                hr = spStrings->GetStringLength(j, &strLen);
                std::vector<WCHAR> fontName(strLen + 1, 0);
                hr = spStrings->GetString(j, fontName.data(), strLen + 1);
                qDebug() << "font name: " << QString::fromUtf16((const ushort *)fontName.data());
            }

            UINT32 fontCnt = spCurrFontFamily->GetFontCount();
            for (UINT32 j = 0; j < fontCnt; ++j) {
                ComPtr<IDWriteFont> spFont;
                hr = spCurrFontFamily->GetFont(j, &spFont);
                if (FAILED(hr) || !spFont)
                    continue;

                ComPtr<IDWriteFontFace> spFace;
                hr = spFont->CreateFontFace(&spFace);
                qDebug() << "font path: " << getDWriteFontPath(spFace.Get());
            }
        }
        qDebug() << "end travel system font by dwrite";
        travelOnce = true;
    }
#endif

    ComPtr<IDWriteFontFace> fontFaceFromFontEngine(QFontEngine *fe)
    {
        const QFontDef fontDef = fe->fontDef;
        ComPtr<IDWriteFontFace> fontFace = fontCache.value(fontDef);
        if (fontFace)
            return fontFace;

        matchDWriteFont(fe, &fontFace);
        if (!fontFace) {
            LOGFONT lf = QWindowsFontDatabase::fontDefToLOGFONT(fontDef, QString());

            // Get substitute name
            static const char keyC[] = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows "
                                       "NT\\CurrentVersion\\FontSubstitutes";
            const QString familyName = QString::fromWCharArray(lf.lfFaceName);
            const QString nameSubstitute = QSettings(QLatin1String(keyC), QSettings::NativeFormat)
                                                   .value(familyName, familyName)
                                                   .toString();
            if (nameSubstitute != familyName) {
                const int nameSubstituteLength = qMin(nameSubstitute.length(), LF_FACESIZE - 1);
                memcpy(lf.lfFaceName, nameSubstitute.utf16(),
                       size_t(nameSubstituteLength) * sizeof(wchar_t));
                lf.lfFaceName[nameSubstituteLength] = 0;
            }

            ComPtr<IDWriteFont> dwriteFont;
            HRESULT hr =
                    QWindowsDirect2DContext::instance()->dwriteGdiInterop()->CreateFontFromLOGFONT(
                            &lf, &dwriteFont);

            if (FAILED(hr)) {
                qDebug("%s: CreateFontFromLOGFONT failed: %#lx", __FUNCTION__, hr);
                return fontFace;
            }

            hr = dwriteFont->CreateFontFace(&fontFace);
            if (FAILED(hr)) {
                qDebug("%s: CreateFontFace failed: %#lx", __FUNCTION__, hr);
                return fontFace;
            }
        }

        if (fontFace)
            fontCache.insert(fontDef, fontFace);

        return fontFace;
    }

private:
    void enterComposite(D2D1_COMPOSITE_MODE cm)
    {
        Q_ASSERT(cm != D2D1_COMPOSITE_MODE_SOURCE_OVER && cm != D2D1_COMPOSITE_MODE_SOURCE_COPY);
        if (m_compositeCommands != nullptr) {
            Q_ASSERT(false);
            return;
        }

        ID2D1DeviceContext* ctx = dc();
        if (SUCCEEDED(ctx->CreateCommandList(&m_compositeCommands))) {
            ctx->GetTarget(&m_savedTarget);
            ctx->SetTarget(m_compositeCommands.Get());
            m_compsiteMode = cm;
        } else {
            m_compositeCommands.Reset();
        }
    }

    void leaveComposite()
    {
        if (m_compositeCommands) {
            Q_ASSERT(m_savedTarget);

            ID2D1DeviceContext *ctx = dc();
            ctx->SetTarget(m_savedTarget.Get());
            m_savedTarget.Reset();
            
            if (SUCCEEDED(m_compositeCommands->Close())) {
                D2D1_MATRIX_3X2_F bakTrans;
                ctx->GetTransform(&bakTrans);

                ctx->SetTransform(D2D1::IdentityMatrix());
                ctx->DrawImage(m_compositeCommands.Get(), D2D1_INTERPOLATION_MODE_LINEAR,
                                m_compsiteMode);

                ctx->SetTransform(bakTrans);
            }
            m_compsiteMode = D2D1_COMPOSITE_MODE_SOURCE_OVER;
            m_compositeCommands.Reset();
        }
    }

private:
    ComPtr<ID2D1CommandList> m_compositeCommands;
    ComPtr<ID2D1Image> m_savedTarget;
    std::weak_ptr<QDirect2DBrushCache> m_brushCache;
    D2D1_COMPOSITE_MODE m_compsiteMode = D2D1_COMPOSITE_MODE_SOURCE_OVER;
};

QWindowsDirect2DPaintEngine::QWindowsDirect2DPaintEngine(QWindowsDirect2DBitmap *bitmap, Flags flags)
    : QPaintEngineEx(*(new QWindowsDirect2DPaintEnginePrivate(bitmap, flags)))
{
    QPaintEngine::PaintEngineFeatures unsupported =
            // As of 1.1 Direct2D does not natively support complex composition modes
            // However, using Direct2D effects that implement them should be possible
            QPaintEngine::PorterDuff
            | QPaintEngine::BlendModes
            | QPaintEngine::RasterOpModes

            // As of 1.1 Direct2D does not natively support perspective transforms
            // However, writing a custom effect that implements them should be possible
            // The built-in 3D transform effect unfortunately changes output image size, making
            // it unusable for us.
            | QPaintEngine::PerspectiveTransform;

    gccaps &= ~unsupported;
}

bool QWindowsDirect2DPaintEngine::begin(QPaintDevice * pdev)
{
    Q_D(QWindowsDirect2DPaintEngine);

    d->bitmap->deviceContext()->begin();
    d->dc()->SetTransform(D2D1::Matrix3x2F::Identity());

    if (systemClip().rectCount() > 1) {
        QPainterPath p;
        p.addRegion(systemClip());

        ComPtr<ID2D1PathGeometry1> geometry = d->vectorPathToID2D1PathGeometry(qtVectorPathForPath(p));
        if (!geometry)
            return false;

        d->dc()->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                               geometry.Get(),
                                               d->antialiasMode(),
                                               D2D1::IdentityMatrix(),
                                               1.0,
                                               nullptr,
                                               d->layerOptions()),
                        nullptr);
    } else {
        QRect clip(0, 0, pdev->width(), pdev->height());
        if (!systemClip().isEmpty())
            clip &= systemClip().boundingRect();
        d->dc()->PushAxisAlignedClip(to_d2d_rect_f(clip), D2D1_ANTIALIAS_MODE_ALIASED);
        d->clipFlags |= SimpleSystemClip;
    }

    D2D_TAG(D2DDebugDrawInitialStateTag);

    setActive(true);
    return true;
}

bool QWindowsDirect2DPaintEngine::end()
{
    Q_D(QWindowsDirect2DPaintEngine);

    // Always clear all emulation-related things so we are in a clean state for our next painting run
    const bool emulatingComposition = d->flags.testFlag(EmulateComposition);
    d->flags &= ~QWindowsDirect2DPaintEngine::EmulateComposition;
    if (!d->fallbackImage.isNull()) {
        if (emulatingComposition) {
            D2D1_PRIMITIVE_BLEND prevBlend = d->dc()->GetPrimitiveBlend();
            d->dc()->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
            drawImage(d->fallbackImage.rect(), d->fallbackImage, d->fallbackImage.rect());
            d->dc()->SetPrimitiveBlend(prevBlend);
        }
        d->fallbackImage = QImage();
    }

    // Pop any user-applied clipping
    d->clearClips();
    // Now the system clip from begin() above
    if (d->clipFlags & SimpleSystemClip) {
        d->dc()->PopAxisAlignedClip();
        d->clipFlags &= ~SimpleSystemClip;
    } else {
        d->dc()->PopLayer();
    }

    return d->bitmap->deviceContext()->end();
}

QPaintEngine::Type QWindowsDirect2DPaintEngine::type() const
{
    return QPaintEngine::Direct2D;
}

static inline bool operator==(const QPainterClipInfo& lhs, const QPainterClipInfo& rhs)
{
    if (lhs.clipType == rhs.clipType
        && lhs.operation == rhs.operation
        && lhs.matrix == rhs.matrix) {
        switch (lhs.clipType) {
        case QPainterClipInfo::RegionClip:
            return lhs.region == rhs.region;
            break;
        case QPainterClipInfo::PathClip:
            return lhs.path == rhs.path;
            break;
        case QPainterClipInfo::RectClip:
            return lhs.rect == rhs.rect;
            break;
        case QPainterClipInfo::RectFClip:
            return lhs.rectf == rhs.rectf;
            break;
        }
    }
    return false;
}

class QDirect2DPaintEngineState : public QPainterState
{
public:
    enum ClipState
    {
        ClipNoChange,
        ClipCanPop,
        ClipRedo,
    };
    QDirect2DPaintEngineState() = default;
    QDirect2DPaintEngineState(const QDirect2DPaintEngineState* orig)
        : QPainterState(orig)
        , originalState(orig)
    {

    }

    bool isCopyOf(const QDirect2DPaintEngineState *orig) const
    {
        return orig != nullptr && orig == this->originalState;
    }

    ClipState checkClipState(const QDirect2DPaintEngineState *newState)
    {
        bool oldEnabled = this->clipEnabled;
        bool newEnabled = newState->clipEnabled;
        auto& oldInfo = this->clipInfo;
        auto& newInfo = newState->clipInfo;
        if (oldEnabled == newEnabled && oldInfo.size() == newInfo.size()) {
            if (oldInfo == newInfo)
                return ClipNoChange;
        }

        bool canPop = false;
        if (oldEnabled && newEnabled && oldInfo.size() > newInfo.size()) {
            canPop = std::equal(newInfo.begin(), newInfo.end(), oldInfo.begin())
                || std::all_of(oldInfo.begin() + newInfo.size(), oldInfo.end(),
                                   [](auto &info) { return info.operation == Qt::IntersectClip; });
        }
        return canPop ? ClipCanPop : ClipRedo;
    }

private:
    const QDirect2DPaintEngineState *originalState = nullptr;
};

QPainterState* QWindowsDirect2DPaintEngine::createState(QPainterState* orig) const
{
    QDirect2DPaintEngineState *s;
    if (orig)
        s = new QDirect2DPaintEngineState(static_cast<QDirect2DPaintEngineState *>(orig));
    else
        s = new QDirect2DPaintEngineState;
    return s;
}

void QWindowsDirect2DPaintEngine::setState(QPainterState *s)
{
    Q_D(QWindowsDirect2DPaintEngine);

    auto oldState = static_cast<QDirect2DPaintEngineState *>(QPaintEngineEx::state());
    auto newState = static_cast<QDirect2DPaintEngineState *>(s);

    QPaintEngineEx::setState(s);

    if (newState == oldState || newState->isCopyOf(oldState))
        return;

    if (oldState == nullptr) {
        penChanged();
        brushChanged();
        brushOriginChanged();
        opacityChanged();
        compositionModeChanged();
        renderHintsChanged();
        transformChanged();
        d->clearClips();
        clipEnabledChanged();
    } else {
        auto clipState = oldState->checkClipState(newState);

        if (oldState->pen != newState->pen)
            penChanged();
        if (oldState->brush != newState->brush)
            brushChanged();
        if (oldState->brushOrigin != newState->brushOrigin)
            brushOriginChanged();
        if (oldState->opacity != newState->opacity)
            opacityChanged();
        if (oldState->composition_mode != newState->composition_mode)
            compositionModeChanged();
        if (oldState->renderHints != newState->renderHints)
            renderHintsChanged();
        if (oldState->matrix != newState->matrix)
            transformChanged();

        if (clipState == QDirect2DPaintEngineState::ClipRedo) {
            d->clearClips();
            clipEnabledChanged();
        } else if (clipState == QDirect2DPaintEngineState::ClipCanPop) {
            d->popClip(oldState->clipInfo.size() - newState->clipInfo.size());
        }

    }
}

void QWindowsDirect2DPaintEngine::draw(const QVectorPath &path)
{
    const QBrush &brush = state()->brush;
    if (qbrush_style(brush) != Qt::NoBrush) {
        if (emulationRequired(BrushEmulation))
            rasterFill(path, brush);
        else
            fill(path, brush);
    }

    const QPen &pen = state()->pen;
    if (qpen_style(pen) != Qt::NoPen && qbrush_style(qpen_brush(pen)) != Qt::NoBrush) {
        if (emulationRequired(PenEmulation))
            QPaintEngineEx::stroke(path, pen);
        else
            stroke(path, pen);
    }
}

void QWindowsDirect2DPaintEngine::fill(const QVectorPath &path, const QBrush &brush)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugFillTag);

    if (path.isEmpty())
        return;

    ensureBrush(brush);
    if (emulationRequired(BrushEmulation)) {
        rasterFill(path, brush);
        return;
    }

    if (!d->brush.brush)
        return;

    ComPtr<ID2D1Geometry> geometry = d->vectorPathToID2D1PathGeometry(path);
    if (!geometry) {
        qWarning("%s: Could not convert path to d2d geometry", __FUNCTION__);
        return;
    }

    d->dc()->FillGeometry(geometry.Get(), d->brush.brush.Get());
}

void QWindowsDirect2DPaintEngine::stroke(const QVectorPath &path, const QPen &pen)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugFillTag);

    if (path.isEmpty())
        return;

    ensurePen(pen);
    if (emulationRequired(PenEmulation)) {
        QPaintEngineEx::stroke(path, pen);
        return;
    }

    if (!d->pen.brush)
        return;

    d->stroke(path);
}

void QWindowsDirect2DPaintEngine::clip(const QVectorPath &path, Qt::ClipOperation op)
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->clip(path, op);
}

void QWindowsDirect2DPaintEngine::clipEnabledChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateClipEnabled(state()->clipEnabled);
}

void QWindowsDirect2DPaintEngine::penChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updatePen(state()->pen);
}

void QWindowsDirect2DPaintEngine::brushChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateBrush(state()->brush);
}

void QWindowsDirect2DPaintEngine::brushOriginChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateBrushOrigin(state()->brushOrigin);
}

void QWindowsDirect2DPaintEngine::opacityChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateOpacity(state()->opacity);
}

void QWindowsDirect2DPaintEngine::compositionModeChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateCompositionMode(state()->compositionMode());
}

void QWindowsDirect2DPaintEngine::renderHintsChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateHints();
}

void QWindowsDirect2DPaintEngine::transformChanged()
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateTransform(state()->transform());
}

void QWindowsDirect2DPaintEngine::fillRect(const QRectF &rect, const QBrush &brush)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugFillRectTag);

    ensureBrush(brush);

    if (emulationRequired(BrushEmulation)) {
        QPaintEngineEx::fillRect(rect, brush);
    } else {
        QRectF r = rect.normalized();
        adjustForAliasing(&r);

        if (d->brush.brush)
            d->dc()->FillRectangle(to_d2d_rect_f(rect), d->brush.brush.Get());
    }
}

void QWindowsDirect2DPaintEngine::drawRects(const QRect *rects, int rectCount)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawRectsTag);

    ensureBrush();
    ensurePen();

    if (emulationRequired(BrushEmulation) || emulationRequired(PenEmulation)) {
        QPaintEngineEx::drawRects(rects, rectCount);
    } else {
        QRectF rect;
        for (int i = 0; i < rectCount; i++) {
            rect = rects[i].normalized();
            adjustForAliasing(&rect);

            D2D1_RECT_F d2d_rect = to_d2d_rect_f(rect);

            if (d->brush.brush)
                d->dc()->FillRectangle(d2d_rect, d->brush.brush.Get());

            if (d->pen.brush)
                d->dc()->DrawRectangle(d2d_rect, d->pen.brush.Get(),
                                       FLOAT(d->pen.qpen.widthF()), d->pen.strokeStyle.Get());
        }
    }
}

void QWindowsDirect2DPaintEngine::drawRects(const QRectF *rects, int rectCount)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawRectFsTag);

    ensureBrush();
    ensurePen();

    if (emulationRequired(BrushEmulation) || emulationRequired(PenEmulation)) {
        QPaintEngineEx::drawRects(rects, rectCount);
    } else {
        QRectF rect;
        for (int i = 0; i < rectCount; i++) {
            rect = rects[i].normalized();
            adjustForAliasing(&rect);

            D2D1_RECT_F d2d_rect = to_d2d_rect_f(rect);

            if (d->brush.brush)
                d->dc()->FillRectangle(d2d_rect, d->brush.brush.Get());

            if (d->pen.brush)
                d->dc()->DrawRectangle(d2d_rect, d->pen.brush.Get(),
                                       FLOAT(d->pen.qpen.widthF()), d->pen.strokeStyle.Get());
        }
    }
}

static bool isLinePositivelySloped(const QPointF &p1, const QPointF &p2)
{
    if (p2.x() > p1.x())
        return p2.y() < p1.y();

    if (p1.x() > p2.x())
        return p1.y() < p2.y();

    return false;
}

static void adjustLine(QPointF *p1, QPointF *p2)
{
    if (isLinePositivelySloped(*p1, *p2)) {
        p1->ry() -= qreal(1.0);
        p2->ry() -= qreal(1.0);
    }
}

void QWindowsDirect2DPaintEngine::drawEllipse(const QRectF &r)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawEllipseFTag);

    ensureBrush();
    ensurePen();

    if (emulationRequired(BrushEmulation) || emulationRequired(PenEmulation)) {
        QPaintEngineEx::drawEllipse(r);
    } else {
        QPointF p = r.center();
        adjustForAliasing(&p);

        D2D1_ELLIPSE ellipse = {
            to_d2d_point_2f(p),
            FLOAT(r.width() / 2.0),
            FLOAT(r.height() / 2.0)
        };

        if (d->brush.brush)
            d->dc()->FillEllipse(ellipse, d->brush.brush.Get());

        if (d->pen.brush)
            d->dc()->DrawEllipse(ellipse, d->pen.brush.Get(),
                                 FLOAT(d->pen.qpen.widthF()),
                                 d->pen.strokeStyle.Get());
    }
}

void QWindowsDirect2DPaintEngine::drawEllipse(const QRect &r)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawEllipseTag);

    ensureBrush();
    ensurePen();

    if (emulationRequired(BrushEmulation) || emulationRequired(PenEmulation)) {
        QPaintEngineEx::drawEllipse(r);
    } else {
        QPointF p = r.center();
        adjustForAliasing(&p);

        D2D1_ELLIPSE ellipse = {
            to_d2d_point_2f(p),
            FLOAT(r.width() / 2.0),
            FLOAT(r.height() / 2.0)
        };

        if (d->brush.brush)
            d->dc()->FillEllipse(ellipse, d->brush.brush.Get());

        if (d->pen.brush)
            d->dc()->DrawEllipse(ellipse, d->pen.brush.Get(),
                                 FLOAT(d->pen.qpen.widthF()),
                                 d->pen.strokeStyle.Get());
    }
}

void QWindowsDirect2DPaintEngine::drawImage(const QRectF &rectangle, const QImage &image,
                                            const QRectF &sr, Qt::ImageConversionFlags flags)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawImageTag);

    QPixmap pixmap = QPixmap::fromImage(image, flags);
    drawPixmap(rectangle, pixmap, sr);
}

void QWindowsDirect2DPaintEngine::drawPixmap(const QRectF &r,
                                             const QPixmap &pm,
                                             const QRectF &sr)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawPixmapTag);

    if (pm.isNull())
        return;

    if (pm.handle()->pixelType() == QPlatformPixmap::BitmapType) {
        QImage i = pm.toImage();
        i.setColor(0, qRgba(0, 0, 0, 0));
        i.setColor(1, d->pen.qpen.color().rgba());
        drawImage(r, i, sr);
        return;
    }

    if (d->flags.testFlag(EmulateComposition)) {
        const qreal points[] = {
            r.x(), r.y(),
            r.x() + r.width(), r.y(),
            r.x() + r.width(), r.y() + r.height(),
            r.x(), r.y() + r.height()
        };
        const QVectorPath vp(points, 4, nullptr, QVectorPath::RectangleHint);
        QBrush brush(sr.isValid() ? pm.copy(sr.toRect()) : pm);
        brush.setTransform(QTransform::fromTranslate(r.x(), r.y()));
        rasterFill(vp, brush);
        return;
    }

    QWindowsDirect2DPlatformPixmap *pp = static_cast<QWindowsDirect2DPlatformPixmap *>(pm.handle());
    QWindowsDirect2DBitmap *bitmap = pp->bitmap();

    if (!d->own(bitmap->bitmap()))
        return;

    ensurePen();

    if (bitmap->bitmap() != d->bitmap->bitmap()) {
        // Good, src bitmap != dst bitmap
        if (sr.isValid())
            d->dc()->DrawBitmap(bitmap->bitmap(),
                                to_d2d_rect_f(r), FLOAT(state()->opacity),
                                d->interpolationMode(),
                                to_d2d_rect_f(sr));
        else
            d->dc()->DrawBitmap(bitmap->bitmap(),
                                to_d2d_rect_f(r), FLOAT(state()->opacity),
                                d->interpolationMode());
    } else {
        // Ok, so the source pixmap and destination pixmap is the same.
        // D2D is not fond of this scenario, deal with it through
        // an intermediate bitmap
        QWindowsDirect2DBitmap intermediate;

        if (sr.isValid()) {
            bool r = intermediate.resize(int(sr.width()), int(sr.height()));
            if (!r) {
                qWarning("%s: Could not resize intermediate bitmap to source rect size", __FUNCTION__);
                return;
            }

            D2D1_RECT_U d2d_sr =  to_d2d_rect_u(sr.toRect());
            HRESULT hr = intermediate.bitmap()->CopyFromBitmap(nullptr,
                                                               bitmap->bitmap(),
                                                               &d2d_sr);
            if (FAILED(hr)) {
                qWarning("%s: Could not copy source rect area from source bitmap to intermediate bitmap: %#lx", __FUNCTION__, hr);
                return;
            }
        } else {
            bool r = intermediate.resize(bitmap->size().width(),
                                         bitmap->size().height());
            if (!r) {
                qWarning("%s: Could not resize intermediate bitmap to source bitmap size", __FUNCTION__);
                return;
            }

            HRESULT hr = intermediate.bitmap()->CopyFromBitmap(nullptr,
                                                               bitmap->bitmap(),
                                                               nullptr);
            if (FAILED(hr)) {
                qWarning("%s: Could not copy source bitmap to intermediate bitmap: %#lx", __FUNCTION__, hr);
                return;
            }
        }

        d->dc()->DrawBitmap(intermediate.bitmap(),
                            to_d2d_rect_f(r), FLOAT(state()->opacity),
                            d->interpolationMode());
    }
}

void QWindowsDirect2DPaintEngine::drawStaticTextItem(QStaticTextItem *staticTextItem)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawStaticTextItemTag);

    if (staticTextItem->numGlyphs == 0)
        return;

    ensurePen();

    // If we can't support the current configuration with Direct2D, fall back to slow path
    if (emulationRequired(PenEmulation)) {
        QPaintEngineEx::drawStaticTextItem(staticTextItem);
        return;
    }

    ComPtr<IDWriteFontFace> fontFace = d->fontFaceFromFontEngine(staticTextItem->fontEngine());
    if (!fontFace) {
        qWarning("%s: Could not find font - falling back to slow text rendering path.", __FUNCTION__);
        QPaintEngineEx::drawStaticTextItem(staticTextItem);
        return;
    }

    QVarLengthArray<UINT16> glyphIndices(staticTextItem->numGlyphs);
    QVarLengthArray<FLOAT> glyphAdvances(staticTextItem->numGlyphs);
    QVarLengthArray<DWRITE_GLYPH_OFFSET> glyphOffsets(staticTextItem->numGlyphs);

    for (int i = 0; i < staticTextItem->numGlyphs; i++) {
        glyphIndices[i] = UINT16(staticTextItem->glyphs[i]); // Imperfect conversion here

        // This looks  a little funky because the positions are precalculated
        glyphAdvances[i] = 0;
        glyphOffsets[i].advanceOffset = FLOAT(staticTextItem->glyphPositions[i].x.toReal());
        // Qt and Direct2D seem to disagree on the direction of the ascender offset...
        glyphOffsets[i].ascenderOffset = FLOAT(staticTextItem->glyphPositions[i].y.toReal() * -1);
    }

    d->drawGlyphRun(D2D1::Point2F(0, 0),
                    fontFace.Get(),
                    staticTextItem->fontEngine()->fontDef,
                    staticTextItem->numGlyphs,
                    glyphIndices.constData(),
                    glyphAdvances.constData(),
                    glyphOffsets.constData(),
                    false);
}

void QWindowsDirect2DPaintEngine::drawTextItem(const QPointF &p, const QTextItem &textItem)
{
    Q_D(QWindowsDirect2DPaintEngine);
    D2D_TAG(D2DDebugDrawTextItemTag);

    const QTextItemInt &ti = static_cast<const QTextItemInt &>(textItem);
    if (ti.glyphs.numGlyphs == 0)
        return;

    ensurePen();

    // If we can't support the current configuration with Direct2D, fall back to slow path
    if (emulationRequired(PenEmulation)) {
        QPaintEngine::drawTextItem(p, textItem);
        return;
    }

    ComPtr<IDWriteFontFace> fontFace = d->fontFaceFromFontEngine(ti.fontEngine);
    if (!fontFace) {
        qWarning("%s: Could not find font - falling back to slow text rendering path.", __FUNCTION__);
        QPaintEngine::drawTextItem(p, textItem);
        return;
    }

    QVarLengthArray<UINT16> glyphIndices(ti.glyphs.numGlyphs);
    QVarLengthArray<FLOAT> glyphAdvances(ti.glyphs.numGlyphs);
    QVarLengthArray<DWRITE_GLYPH_OFFSET> glyphOffsets(ti.glyphs.numGlyphs);

    for (int i = 0; i < ti.glyphs.numGlyphs; i++) {
        glyphIndices[i] = UINT16(ti.glyphs.glyphs[i]); // Imperfect conversion here
        glyphAdvances[i] = FLOAT(ti.glyphs.effectiveAdvance(i).toReal());
        glyphOffsets[i].advanceOffset = FLOAT(ti.glyphs.offsets[i].x.toReal());

        // XXX Should we negate the y value like for static text items?
        glyphOffsets[i].ascenderOffset = FLOAT(ti.glyphs.offsets[i].y.toReal());
    }

    const bool rtl = (ti.flags & QTextItem::RightToLeft);
    const QPointF offset(rtl ? ti.width.toReal() : 0, 0);

    d->drawGlyphRun(to_d2d_point_2f(p + offset),
                    fontFace.Get(),
                    ti.fontEngine->fontDef,
                    ti.glyphs.numGlyphs,
                    glyphIndices.constData(),
                    glyphAdvances.constData(),
                    glyphOffsets.constData(),
                    rtl);
}

bool QWindowsDirect2DPaintEngine::matchDWriteFont(const QFontEngine *fontEngine, IDWriteFontFace **ppFontFace)
{
    return QWindowsDirect2DPaintEnginePrivate::matchDWriteFont(fontEngine, ppFontFace);
}

QString QWindowsDirect2DPaintEngine::getDWriteFontPath(IDWriteFontFace *pFontFace)
{
    return QWindowsDirect2DPaintEnginePrivate::getDWriteFontPath(pFontFace);
}

void QWindowsDirect2DPaintEngine::ensureBrush()
{
    ensureBrush(state()->brush);
}

void QWindowsDirect2DPaintEngine::ensureBrush(const QBrush &brush)
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updateBrush(brush);
}

void QWindowsDirect2DPaintEngine::ensurePen()
{
    ensurePen(state()->pen);
}

void QWindowsDirect2DPaintEngine::ensurePen(const QPen &pen)
{
    Q_D(QWindowsDirect2DPaintEngine);
    d->updatePen(pen);
}

void QWindowsDirect2DPaintEngine::rasterFill(const QVectorPath &path, const QBrush &brush)
{
    Q_D(QWindowsDirect2DPaintEngine);

    if (d->fallbackImage.isNull()) {
        if (d->flags.testFlag(EmulateComposition)) {
            QWindowsDirect2DPaintEngineSuspender suspender(this);
            d->fallbackImage = d->bitmap->toImage();
        } else {
            d->fallbackImage = QImage(d->bitmap->size(), QImage::Format_ARGB32_Premultiplied);
            d->fallbackImage.fill(Qt::transparent);
        }
    }

    QImage &img = d->fallbackImage;
    QPainter p;
    QPaintEngine *engine = img.paintEngine();

    if (img.isNull() || !engine)    // tdr tolerance
        return;

    if (engine->isExtended() && p.begin(&img)) {
        p.setRenderHints(state()->renderHints);
        p.setCompositionMode(state()->compositionMode());
        p.setOpacity(state()->opacity);
        p.setBrushOrigin(state()->brushOrigin);
        p.setBrush(state()->brush);
        p.setPen(state()->pen);

        QPaintEngineEx *extended = static_cast<QPaintEngineEx *>(engine);
        for (const QPainterClipInfo &info : qAsConst(state()->clipInfo)) {
            extended->state()->matrix = info.matrix;
            extended->transformChanged();

            switch (info.clipType) {
            case QPainterClipInfo::RegionClip:
                extended->clip(info.region, info.operation);
                break;
            case QPainterClipInfo::PathClip:
                extended->clip(info.path, info.operation);
                break;
            case QPainterClipInfo::RectClip:
                extended->clip(info.rect, info.operation);
                break;
            case QPainterClipInfo::RectFClip:
                qreal right = info.rectf.x() + info.rectf.width();
                qreal bottom = info.rectf.y() + info.rectf.height();
                qreal pts[] = { info.rectf.x(), info.rectf.y(),
                                right, info.rectf.y(),
                                right, bottom,
                                info.rectf.x(), bottom };
                QVectorPath vp(pts, 4, nullptr, QVectorPath::RectangleHint);
                extended->clip(vp, info.operation);
                break;
            }
        }

        extended->state()->matrix = state()->matrix;
        extended->transformChanged();

        extended->fill(path, brush);
        if (!p.end())
            qWarning("%s: Paint Engine end returned false", __FUNCTION__);

        if (!d->flags.testFlag(EmulateComposition)) { // Emulated fallback will be flattened in end()
            d->updateClipEnabled(false);
            d->updateTransform(QTransform());
            drawImage(img.rect(), img, img.rect());
            d->fallbackImage = QImage();
            transformChanged();
            clipEnabledChanged();
        }
    } else {
        qWarning("%s: Could not fall back to QImage", __FUNCTION__);
    }
}

bool QWindowsDirect2DPaintEngine::emulationRequired(EmulationType type) const
{
    Q_D(const QWindowsDirect2DPaintEngine);

    if (d->flags.testFlag(EmulateComposition))
        return true;

    if (!state()->matrix.isAffine())
        return true;

    switch (type) {
    case PenEmulation:
        return d->pen.emulate;
        break;
    case BrushEmulation:
        return d->brush.emulate;
        break;
    }

    return false;
}

bool QWindowsDirect2DPaintEngine::antiAliasingEnabled() const
{
    return state()->renderHints & QPainter::Antialiasing;
}

void QWindowsDirect2DPaintEngine::adjustForAliasing(QRectF *rect)
{
   if (!antiAliasingEnabled()) {
       rect->adjust(MAGICAL_ALIASING_OFFSET,
                    MAGICAL_ALIASING_OFFSET,
                    MAGICAL_ALIASING_OFFSET,
                    MAGICAL_ALIASING_OFFSET);
   }
}

void QWindowsDirect2DPaintEngine::adjustForAliasing(QPointF *point)
{
    static const QPointF adjustment(MAGICAL_ALIASING_OFFSET,
                                    MAGICAL_ALIASING_OFFSET);

    if (!antiAliasingEnabled())
        (*point) += adjustment;
}

void QWindowsDirect2DPaintEngine::suspend()
{
    end();
}

void QWindowsDirect2DPaintEngine::resume()
{
    begin(paintDevice());
    clipEnabledChanged();
    penChanged();
    brushChanged();
    brushOriginChanged();
    opacityChanged();
    compositionModeChanged();
    renderHintsChanged();
    transformChanged();
}

class QWindowsDirect2DPaintEngineSuspenderImpl
{
    Q_DISABLE_COPY(QWindowsDirect2DPaintEngineSuspenderImpl)
    QWindowsDirect2DPaintEngine *m_engine;
    bool m_active;
public:
    QWindowsDirect2DPaintEngineSuspenderImpl(QWindowsDirect2DPaintEngine *engine)
        : m_engine(engine)
        , m_active(engine->isActive())
    {
        if (m_active)
            m_engine->suspend();
    }

    ~QWindowsDirect2DPaintEngineSuspenderImpl()
    {
        if (m_active)
            m_engine->resume();
    }
};

class QWindowsDirect2DPaintEngineSuspenderPrivate
{
public:
    QWindowsDirect2DPaintEngineSuspenderPrivate(QWindowsDirect2DPaintEngine *engine)
        : engineSuspender(engine)
        , dcSuspender(static_cast<QWindowsDirect2DPaintEnginePrivate *>(engine->d_ptr.data())->bitmap->deviceContext())
    {
    }

    QWindowsDirect2DPaintEngineSuspenderImpl engineSuspender;
    QWindowsDirect2DDeviceContextSuspender dcSuspender;
};

QWindowsDirect2DPaintEngineSuspender::QWindowsDirect2DPaintEngineSuspender(QWindowsDirect2DPaintEngine *engine)
    : d_ptr(new QWindowsDirect2DPaintEngineSuspenderPrivate(engine))
{

}

QWindowsDirect2DPaintEngineSuspender::~QWindowsDirect2DPaintEngineSuspender()
{
}

void QWindowsDirect2DPaintEngineSuspender::resume()
{
    d_ptr.reset();
}

QT_END_NAMESPACE
