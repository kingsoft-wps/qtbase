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

// QtCore
#include <qdebug.h>
#include <qmath.h>
#include <qmutex.h>

// QtGui
#include "qbitmap.h"
#include "qimage.h"
#include "qpaintdevice.h"
#include "qpaintengine.h"
#include "qpainter.h"
#include "qpaintercg.h"
#include "qpainter_p.h"
#include "qpainterpath.h"
#include "qpicture.h"
#include "qpixmapcache.h"
#include "qpolygon.h"
#include "qtextlayout.h"
#include "qthread.h"
#include "qvarlengtharray.h"
#include "qstatictext.h"
#include "qglyphrun.h"

#include <qpa/qplatformtheme.h>
#include <qpa/qplatformintegration.h>

#include <private/qfontengine_p.h>
#include <private/qpaintengine_p.h>
#include <private/qemulationpaintengine_p.h>
#include <private/qpainterpath_p.h>
#include <private/qtextengine_p.h>
#include <private/qpaintengine_raster_p.h>
#include <private/qmath_p.h>
#include <private/qstatictext_p.h>
#include <private/qglyphrun_p.h>
#include <private/qhexstring_p.h>
#include <private/qguiapplication_p.h>
#include <private/qrawfont_p.h>

#import <AppKit/AppKit.h>

QT_BEGIN_NAMESPACE

#define QGradient_StretchToDevice 0x10000000
#define QPaintEngine_OpaqueBackground 0x40000000

// #define QT_DEBUG_DRAW
#ifdef QT_DEBUG_DRAW
bool qt_show_painter_debug_output = true;
#endif

extern QPixmap qt_pixmapForBrush(int style, bool invert);

static void drawTextItemDecoration(QPainter *painter, const QPointF &pos, const QFontEngine *fe, QTextEngine *textEngine,
                                   QTextCharFormat::UnderlineStyle underlineStyle,
                                   QTextItem::RenderFlags flags, qreal width,
                                   const QTextCharFormat &charFormat);
// Helper function to calculate left most position, width and flags for decoration drawing
Q_GUI_EXPORT void qt_draw_decoration_for_glyphs(QPainter *painter, const glyph_t *glyphArray,
                                                const QFixedPoint *positions, int glyphCount,
                                                QFontEngine *fontEngine, const QFont &font,
                                                const QTextCharFormat &charFormat);

static inline QGradient::CoordinateMode coordinateMode(const QBrush &brush)
{
    switch (brush.style()) {
    case Qt::LinearGradientPattern:
    case Qt::RadialGradientPattern:
    case Qt::ConicalGradientPattern:
        return brush.gradient()->coordinateMode();
    default:
        ;
    }
    return QGradient::LogicalMode;
}

extern bool qHasPixmapTexture(const QBrush &);

static inline bool is_brush_transparent(const QBrush &brush) {
    Qt::BrushStyle s = brush.style();
    if (s != Qt::TexturePattern)
        return s >= Qt::Dense1Pattern && s <= Qt::DiagCrossPattern;
    if (qHasPixmapTexture(brush))
        return brush.texture().isQBitmap() || brush.texture().hasAlphaChannel();
    else {
        const QImage texture = brush.textureImage();
        return texture.hasAlphaChannel() || (texture.depth() == 1 && texture.colorCount() == 0);
    }
}

static inline bool is_pen_transparent(const QPen &pen) {
    return pen.style() > Qt::SolidLine || is_brush_transparent(pen.brush());
}

/* Discards the emulation flags that are not relevant for line drawing
   and returns the result
*/
static inline uint line_emulation(uint emulation)
{
    return emulation & (QPaintEngine::PrimitiveTransform
                        | QPaintEngine::AlphaBlend
                        | QPaintEngine::Antialiasing
                        | QPaintEngine::BrushStroke
                        | QPaintEngine::ConstantOpacity
                        | QGradient_StretchToDevice
                        | QPaintEngine::ObjectBoundingModeGradients
                        | QPaintEngine_OpaqueBackground);
}

#ifndef QT_NO_DEBUG
static bool qt_painter_thread_test(int devType, int engineType, const char *what)
{
    const QPlatformIntegration *platformIntegration = QGuiApplicationPrivate::platformIntegration();
    switch (devType) {
    case QInternal::Image:
    case QInternal::Printer:
    case QInternal::Picture:
        // can be drawn onto these devices safely from any thread
        break;
    default:
        if (QThread::currentThread() != qApp->thread()
                // pixmaps cannot be targets unless threaded pixmaps are supported
                && (devType != QInternal::Pixmap || !platformIntegration->hasCapability(QPlatformIntegration::ThreadedPixmaps))
                // framebuffer objects and such cannot be targets unless threaded GL is supported
                && (devType != QInternal::OpenGL || !platformIntegration->hasCapability(QPlatformIntegration::ThreadedOpenGL))
                // widgets cannot be targets except for QGLWidget
                && (devType != QInternal::Widget || !platformIntegration->hasCapability(QPlatformIntegration::ThreadedOpenGL)
                    || (engineType != QPaintEngine::OpenGL && engineType != QPaintEngine::OpenGL2))) {
            qWarning("QPainter: It is not safe to use %s outside the GUI thread", what);
            return false;
        }
        break;
    }
    return true;
}
#endif

static inline QBrush stretchGradientToUserSpace(const QBrush &brush, const QRectF &boundingRect)
{
    Q_ASSERT(brush.style() >= Qt::LinearGradientPattern
             && brush.style() <= Qt::ConicalGradientPattern);

    QTransform gradientToUser(boundingRect.width(), 0, 0, boundingRect.height(),
                              boundingRect.x(), boundingRect.y());

    QGradient g = *brush.gradient();
    g.setCoordinateMode(QGradient::LogicalMode);

    QBrush b(g);
    if (brush.gradient()->coordinateMode() == QGradient::ObjectMode)
        b.setTransform(b.transform() * gradientToUser);
    else
        b.setTransform(gradientToUser * b.transform());
    return b;
}

static bool convertQPainterPathToCGPath(const QPainterPath &painterPath, CGMutablePathRef cgPath, qreal height)
{
    QPainterPath::Element moveElement;

    if (painterPath.isEmpty())
    {
        return false;
    }

    bool needClose = false;

    for (int i = 0; i < painterPath.elementCount(); ++i)
    {
        QPainterPath::Element element = painterPath.elementAt(i);
        switch (element.type)
        {
            case QPainterPath::MoveToElement:
            {
                if (needClose)
                {
                    CGPathCloseSubpath(cgPath);
                }
                needClose = true;
                moveElement = element;
                CGPathMoveToPoint(cgPath, NULL, element.x, height - element.y);
                break;
            }
            case QPainterPath::LineToElement:
            {
                if (element.x == moveElement.x && moveElement.y == element.y)
                {
                    CGPathCloseSubpath(cgPath);
                    needClose = false;
                }else
                {
                    CGPathAddLineToPoint(cgPath, NULL, element.x, height - element.y);
                }
                break;
            }
            case QPainterPath::CurveToElement:
            {
                ++i;
                QPainterPath::Element element1 = painterPath.elementAt(i);
                ++i;
                QPainterPath::Element element2 = painterPath.elementAt(i);
                CGPathAddCurveToPoint(cgPath, NULL, element.x, height - element.y, element1.x, height - element1.y, element2.x, height - element2.y);
                break;
            }
            case QPainterPath::CurveToDataElement:
                Q_ASSERT(!"toSubpaths(), bad element type");
                break;
            default:
                break;
        }
    }
    if (needClose)
        CGPathCloseSubpath(cgPath);
    return true;
}

QCGPainter::QCGPainter()
    : cgContextRef(nullptr)
{

}

QCGPainter::QCGPainter(QPaintDevice *pd)
    : cgContextRef(nullptr)
{

}

/*!
    Destroys the painter.
*/
QCGPainter::~QCGPainter()
{

}

/*!
    Returns the paint device on which this painter is currently
    painting, or 0 if the painter is not active.

    \sa isActive()
*/

QPaintDevice *QCGPainter::device() const
{
    return {};
}

/*!
    Returns \c true if begin() has been called and end() has not yet been
    called; otherwise returns \c false.

    \sa begin(), QPaintDevice::paintingActive()
*/

bool QCGPainter::isActive() const
{
    return {};
}

/*!
    Initializes the painters pen, background and font to the same as
    the given \a device.

    \obsolete

    \sa begin(), {QPainter#Settings}{Settings}
*/
void QCGPainter::initFrom(const QPaintDevice *device)
{

}


/*!
    Saves the current painter state (pushes the state onto a stack). A
    save() must be followed by a corresponding restore(); the end()
    function unwinds the stack.

    \sa restore()
*/

void QCGPainter::save()
{
    CGContextSaveGState((CGContextRef)cgContextRef);
    m_stkClipPaths.push(m_clipPath);
}

/*!
    Restores the current painter state (pops a saved state off the
    stack).

    \sa save()
*/

void QCGPainter::restore()
{
    CGContextRestoreGState((CGContextRef)cgContextRef);
    if (!m_stkClipPaths.isEmpty())
    {
        m_clipPath = m_stkClipPaths.pop();
    }
}

bool QCGPainter::begin(QPaintDevice *pd)
{
    if (pd->devType() == QInternal::Image)
    {
        QImage *pImg = dynamic_cast<QImage*>(pd);
        if (pImg)
        {
            CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
            cgContextRef = CGBitmapContextCreate(pImg->bits(), pImg->width(), pImg->height(),
                                                     8, pImg->bytesPerLine(), colorspace,
                                                     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
            m_contextSize = pImg->size();
            return true;
        }
    }
    return false;
}

/*!
    Ends painting. Any resources used while painting are released. You
    don't normally need to call this since it is called by the
    destructor.

    Returns \c true if the painter is no longer active; otherwise returns \c false.

    \sa begin(), isActive()
*/

bool QCGPainter::end()
{
    if (cgContextRef)
    {
        CGContextRelease((CGContextRef)cgContextRef);
        cgContextRef = nullptr;
    }
    return true;
}


/*!
    Returns the paint engine that the painter is currently operating
    on if the painter is active; otherwise 0.

    \sa isActive()
*/
QPaintEngine *QCGPainter::paintEngine() const
{
    return nullptr;
}


void QCGPainter::beginNativePainting()
{

}

void QCGPainter::endNativePainting()
{

}

void QCGPainter::setContext(void* context)
{
    cgContextRef = context;
}

void* QCGPainter::getContext()
{
    return cgContextRef;
}

void QCGPainter::setPainterPrivate(QPainterPrivate *d)
{
    d_ptr = d;
}

/*!
    Returns the font metrics for the painter if the painter is
    active. Otherwise, the return value is undefined.

    \sa font(), isActive(), {QPainter#Settings}{Settings}
*/

QFontMetrics QCGPainter::fontMetrics() const
{
    QFont font;
    return QFontMetrics(font);
}


/*!
    Returns the font info for the painter if the painter is
    active. Otherwise, the return value is undefined.

    \sa font(), isActive(), {QPainter#Settings}{Settings}
*/

QFontInfo QCGPainter::fontInfo() const
{
    QFont font;
    return QFontInfo(font);
}

/*!
    \since 4.2

    Returns the opacity of the painter. The default value is
    1.
*/

qreal QCGPainter::opacity() const
{
    return 0.0;
}

void QCGPainter::setOpacity(qreal opacity)
{

}


/*!
    Returns the currently set brush origin.

    \sa setBrushOrigin(), {QPainter#Settings}{Settings}
*/

QPoint QCGPainter::brushOrigin() const
{
    return {};
}

/*!
    \fn void QCGPainter::setBrushOrigin(const QPointF &position)

    Sets the brush origin to \a position.

    The brush origin specifies the (0, 0) coordinate of the painter's
    brush.

    Note that while the brushOrigin() was necessary to adopt the
    parent's background for a widget in Qt 3, this is no longer the
    case since the Qt 4 painter doesn't paint the background unless
    you explicitly tell it to do so by setting the widget's \l
    {QWidget::autoFillBackground}{autoFillBackground} property to
    true.

    \sa brushOrigin(), {QPainter#Settings}{Settings}
*/

void QCGPainter::setBrushOrigin(const QPointF &p)
{

}

void QCGPainter::setCompositionMode(QPainter::CompositionMode mode)
{

}

/*!
  Returns the current composition mode.

  \sa CompositionMode, setCompositionMode()
*/
QPainter::CompositionMode QCGPainter::compositionMode() const
{
    return {};
}

/*!
    Returns the current background brush.

    \sa setBackground(), {QPainter#Settings}{Settings}
*/

const QBrush &QCGPainter::background() const
{
    return {};
}


/*!
    Returns \c true if clipping has been set; otherwise returns \c false.

    \sa setClipping(), {QPainter#Clipping}{Clipping}
*/

bool QCGPainter::hasClipping() const
{
    return {};
}


/*!
    Enables clipping if  \a enable is true, or disables clipping if  \a
    enable is false.

    \sa hasClipping(), {QPainter#Clipping}{Clipping}
*/

void QCGPainter::setClipping(bool enable)
{

}


/*!
    Returns the currently set clip region. Note that the clip region
    is given in logical coordinates.

    \warning QPainter does not store the combined clip explicitly as
    this is handled by the underlying QPaintEngine, so the path is
    recreated on demand and transformed to the current logical
    coordinate system. This is potentially an expensive operation.

    \sa setClipRegion(), clipPath(), setClipping()
*/

QRegion QCGPainter::clipRegion() const
{
    return {};
}

/*!
    Returns the current clip path in logical coordinates.

    \warning QPainter does not store the combined clip explicitly as
    this is handled by the underlying QPaintEngine, so the path is
    recreated on demand and transformed to the current logical
    coordinate system. This is potentially an expensive operation.

    \sa setClipPath(), clipRegion(), setClipping()
*/
QPainterPath QCGPainter::clipPath() const
{
    return {};
}

/*!
    Returns the bounding rectangle of the current clip if there is a clip;
    otherwise returns an empty rectangle. Note that the clip region is
    given in logical coordinates.

    The bounding rectangle is not guaranteed to be tight.

    \sa setClipRect(), setClipPath(), setClipRegion()

    \since 4.8
 */

QRectF QCGPainter::clipBoundingRect() const
{
    return {};
}

/*!
    \fn void QCGPainter::setClipRect(const QRectF &rectangle, Qt::ClipOperation operation)

    Enables clipping, and sets the clip region to the given \a
    rectangle using the given clip \a operation. The default operation
    is to replace the current clip rectangle.

    Note that the clip rectangle is specified in logical (painter)
    coordinates.

    \sa clipRegion(), setClipping(), {QPainter#Clipping}{Clipping}
*/
void QCGPainter::setClipRect(const QRectF &rect, Qt::ClipOperation op)
{
    QPainterPath path;
    path.addRect(rect);
    setClipPath(path, op);
}

void QCGPainter::setClipRect(const QRect &rect, Qt::ClipOperation op)
{
    QPainterPath path;
    path.addRect(rect);
    setClipPath(path, op);
}

void QCGPainter::setClipRegion(const QRegion &r, Qt::ClipOperation op)
{
    QPainterPath path;
    path.addRegion(r);
    setClipPath(path, op);
}

void QCGPainter::setWorldMatrix(const QMatrix &matrix, bool combine)
{
    setWorldTransform(QTransform(matrix), combine);
}

/*!
    \since 4.2
    \obsolete

    Returns the world transformation matrix.

    It is advisable to use worldTransform() because worldMatrix() does not
    preserve the properties of perspective transformations.

    \sa {QPainter#Coordinate Transformations}{Coordinate Transformations},
    {Coordinate System}
*/

const QMatrix &QCGPainter::worldMatrix() const
{
    return {};
}

/*!
    \obsolete

    Use setWorldTransform() instead.

    \sa setWorldTransform()
*/

void QCGPainter::setMatrix(const QMatrix &matrix, bool combine)
{
    setWorldTransform(QTransform(matrix), combine);
}

/*!
    \obsolete

    Use worldTransform() instead.

    \sa worldTransform()
*/

const QMatrix &QCGPainter::matrix() const
{
    return worldMatrix();
}


/*!
    \since 4.2
    \obsolete

    Returns the transformation matrix combining the current
    window/viewport and world transformation.

    It is advisable to use combinedTransform() instead of this
    function to preserve the properties of perspective transformations.

    \sa setWorldTransform(), setWindow(), setViewport()
*/
QMatrix QCGPainter::combinedMatrix() const
{
    return combinedTransform().toAffine();
}


/*!
    \obsolete

    Returns the matrix that transforms from logical coordinates to
    device coordinates of the platform dependent paint device.

    \note It is advisable to use deviceTransform() instead of this
    function to preserve the properties of perspective transformations.

    This function is \e only needed when using platform painting
    commands on the platform dependent handle (Qt::HANDLE), and the
    platform does not do transformations nativly.

    The QPaintEngine::PaintEngineFeature enum can be queried to
    determine whether the platform performs the transformations or
    not.

    \sa worldMatrix(), QPaintEngine::hasFeature(),
*/
const QMatrix &QCGPainter::deviceMatrix() const
{
    return {};
}

/*!
    \obsolete

    Resets any transformations that were made using translate(), scale(),
    shear(), rotate(), setWorldMatrix(), setViewport() and
    setWindow().

    It is advisable to use resetTransform() instead of this function
    to preserve the properties of perspective transformations.

    \sa {QPainter#Coordinate Transformations}{Coordinate
    Transformations}
*/

void QCGPainter::resetMatrix()
{
    resetTransform();
}


/*!
    \since 4.2

    Enables transformations if \a enable is true, or disables
    transformations if \a enable is false. The world transformation
    matrix is not changed.

    \sa worldMatrixEnabled(), worldTransform(), {QPainter#Coordinate
    Transformations}{Coordinate Transformations}
*/

void QCGPainter::setWorldMatrixEnabled(bool enable)
{

}

/*!
    \since 4.2

    Returns \c true if world transformation is enabled; otherwise returns
    false.

    \sa setWorldMatrixEnabled(), worldTransform(), {Coordinate System}
*/

bool QCGPainter::worldMatrixEnabled() const
{
    return false;
}

/*!
    \obsolete

    Use setWorldMatrixEnabled() instead.

    \sa setWorldMatrixEnabled()
*/

void QCGPainter::setMatrixEnabled(bool enable)
{
    setWorldMatrixEnabled(enable);
}

/*!
    \obsolete

    Use worldMatrixEnabled() instead

    \sa worldMatrixEnabled()
*/

bool QCGPainter::matrixEnabled() const
{
    return worldMatrixEnabled();
}

/*!
    Scales the coordinate system by (\a{sx}, \a{sy}).

    \sa setWorldTransform(), {QPainter#Coordinate Transformations}{Coordinate Transformations}
*/

void QCGPainter::scale(qreal sx, qreal sy)
{

}

/*!
    Shears the coordinate system by (\a{sh}, \a{sv}).

    \sa setWorldTransform(), {QPainter#Coordinate Transformations}{Coordinate Transformations}
*/

void QCGPainter::shear(qreal sh, qreal sv)
{

}

/*!
    \fn void QCGPainter::rotate(qreal angle)

    Rotates the coordinate system clockwise. The given \a angle parameter is in degrees.

    \sa setWorldTransform(), {QPainter#Coordinate Transformations}{Coordinate Transformations}
*/

void QCGPainter::rotate(qreal a)
{

}

/*!
    Translates the coordinate system by the given \a offset; i.e. the
    given \a offset is added to points.

    \sa setWorldTransform(), {QPainter#Coordinate Transformations}{Coordinate Transformations}
*/
void QCGPainter::translate(const QPointF &offset)
{

}

/*!
    \fn void QCGPainter::translate(const QPoint &offset)
    \overload

    Translates the coordinate system by the given \a offset.
*/

/*!
    \fn void QCGPainter::translate(qreal dx, qreal dy)
    \overload

    Translates the coordinate system by the vector (\a dx, \a dy).
*/

/*!
    \fn void QCGPainter::setClipPath(const QPainterPath &path, Qt::ClipOperation operation)

    Enables clipping, and sets the clip path for the painter to the
    given \a path, with the clip \a operation.

    Note that the clip path is specified in logical (painter)
    coordinates.

    \sa clipPath(), clipRegion(), {QPainter#Clipping}{Clipping}

*/
void QCGPainter::setClipPath(const QPainterPath &path, Qt::ClipOperation op)
{
    if (path.isEmpty())
    {
        return;
    }
    
    if (Qt::ReplaceClip == op || Qt::NoClip == op || Qt::NoClip == d_ptr->state->clipOperation)
    {
        CGContextResetClip((CGContextRef)cgContextRef);
        QPainterPath path;
        m_clipPath.swap(path);
    }

    QPainterPath clipPath;
    if (d_ptr /*&& d_ptr->state->WxF*/)
    {
        clipPath = d_ptr->state->matrix.map(path);
    }
    else
    {
        clipPath = path;
    }
    if (Qt::NoClip == op)
    {
        return;
    }
    else if (Qt::ReplaceClip == op)
    {
        m_clipPath = clipPath;
    }
    else if (Qt::IntersectClip == op)
    {
        m_clipPath &= clipPath;
    }
    // else if (Qt::UniteClip == op)
    // {
    //     m_clipPath += clipPath;
    //     CGContextResetClip((CGContextRef)cgContextRef);
    //     clipPath = m_clipPath;
    // }

    CGMutablePathRef cgClipPath = CGPathCreateMutable();
    if (!convertQPainterPathToCGPath(clipPath, cgClipPath, m_contextSize.height()))
        return;

    CGContextAddPath((CGContextRef)cgContextRef, cgClipPath);
    CGContextClip((CGContextRef)cgContextRef);
//    CGContextEOClip((CGContextRef)cgContextRef);
    CGPathRelease(cgClipPath);
}
/*!
    Draws the outline (strokes) the path \a path with the pen specified
    by \a pen

    \sa fillPath(), {QPainter#Drawing}{Drawing}
*/
void QCGPainter::strokePath(const QPainterPath &path, const QPen &pen)
{

}

/*!
    Fills the given \a path using the given \a brush. The outline is
    not drawn.

    Alternatively, you can specify a QColor instead of a QBrush; the
    QBrush constructor (taking a QColor argument) will automatically
    create a solid pattern brush.

    \sa drawPath()
*/
void QCGPainter::fillPath(const QPainterPath &path, const QBrush &brush)
{

}

/*!
    Draws the given painter \a path using the current pen for outline
    and the current brush for filling.

    \table 100%
    \row
    \li \inlineimage qpainter-path.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 5
    \endtable

    \sa {painting/painterpaths}{the Painter Paths
    example},{painting/deform}{the Vector Deformation example}
*/
void QCGPainter::drawPath(const QPainterPath &path)
{

}

/*!
    \fn void QCGPainter::drawLine(const QLineF &line)

    Draws a line defined by \a line.

    \table 100%
    \row
    \li \inlineimage qpainter-line.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 6
    \endtable

    \sa drawLines(), drawPolyline(), {Coordinate System}
*/

/*!
    \fn void QCGPainter::drawLine(const QLine &line)
    \overload

    Draws a line defined by \a line.
*/

/*!
    \fn void QCGPainter::drawLine(const QPoint &p1, const QPoint &p2)
    \overload

    Draws a line from \a p1 to \a p2.
*/

/*!
    \fn void QCGPainter::drawLine(const QPointF &p1, const QPointF &p2)
    \overload

    Draws a line from \a p1 to \a p2.
*/

/*!
    \fn void QCGPainter::drawLine(int x1, int y1, int x2, int y2)
    \overload

    Draws a line from (\a x1, \a y1) to (\a x2, \a y2).
*/

/*!
    \fn void QCGPainter::drawRect(const QRectF &rectangle)

    Draws the current \a rectangle with the current pen and brush.

    A filled rectangle has a size of \a{rectangle}.size(). A stroked
    rectangle has a size of \a{rectangle}.size() plus the pen width.

    \table 100%
    \row
    \li \inlineimage qpainter-rectangle.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 7
    \endtable

    \sa drawRects(), drawPolygon(), {Coordinate System}
*/

/*!
    \fn void QCGPainter::drawRect(const QRect &rectangle)

    \overload

    Draws the current \a rectangle with the current pen and brush.
*/

/*!
    \fn void QCGPainter::drawRect(int x, int y, int width, int height)

    \overload

    Draws a rectangle with upper left corner at (\a{x}, \a{y}) and
    with the given \a width and \a height.
*/

/*!
    \fn void QCGPainter::drawRects(const QRectF *rectangles, int rectCount)

    Draws the first \a rectCount of the given \a rectangles using the
    current pen and brush.

    \sa drawRect()
*/
void QCGPainter::drawRects(const QRectF *rects, int rectCount)
{

}

/*!
    \fn void QCGPainter::drawRects(const QRect *rectangles, int rectCount)
    \overload

    Draws the first \a rectCount of the given \a rectangles using the
    current pen and brush.
*/
void QCGPainter::drawRects(const QRect *rects, int rectCount)
{

}

/*!
    \fn void QCGPainter::drawRects(const QVector<QRectF> &rectangles)
    \overload

    Draws the given \a rectangles using the current pen and brush.
*/

/*!
    \fn void QCGPainter::drawRects(const QVector<QRect> &rectangles)

    \overload

    Draws the given \a rectangles using the current pen and brush.
*/

/*!
  \fn void QCGPainter::drawPoint(const QPointF &position)

    Draws a single point at the given \a position using the current
    pen's color.

    \sa {Coordinate System}
*/

/*!
    \fn void QCGPainter::drawPoint(const QPoint &position)
    \overload

    Draws a single point at the given \a position using the current
    pen's color.
*/

/*! \fn void QCGPainter::drawPoint(int x, int y)

    \overload

    Draws a single point at position (\a x, \a y).
*/

/*!
    Draws the first \a pointCount points in the array \a points using
    the current pen's color.

    \sa {Coordinate System}
*/
void QCGPainter::drawPoints(const QPointF *points, int pointCount)
{

}

/*!
    \overload

    Draws the first \a pointCount points in the array \a points using
    the current pen's color.
*/

void QCGPainter::drawPoints(const QPoint *points, int pointCount)
{

}

void QCGPainter::setBackgroundMode(Qt::BGMode mode)
{

}

/*!
    Returns the current background mode.

    \sa setBackgroundMode(), {QPainter#Settings}{Settings}
*/
Qt::BGMode QCGPainter::backgroundMode() const
{
    return {};
}


void QCGPainter::setPen(const QColor &color)
{

}

/*!
    Sets the painter's pen to be the given \a pen.

    The \a pen defines how to draw lines and outlines, and it also
    defines the text color.

    \sa pen(), {QPainter#Settings}{Settings}
*/

void QCGPainter::setPen(const QPen &pen)
{

}

/*!
    \overload

    Sets the painter's pen to have the given \a style, width 1 and
    black color.
*/

void QCGPainter::setPen(Qt::PenStyle style)
{

}

/*!
    Returns the painter's current pen.

    \sa setPen(), {QPainter#Settings}{Settings}
*/

const QPen &QCGPainter::pen() const
{
    return {};
}


/*!
    Sets the painter's brush to the given \a brush.

    The painter's brush defines how shapes are filled.

    \sa brush(), {QPainter#Settings}{Settings}
*/

void QCGPainter::setBrush(const QBrush &brush)
{

}


/*!
    \overload

    Sets the painter's brush to black color and the specified \a
    style.
*/

void QCGPainter::setBrush(Qt::BrushStyle style)
{

}

/*!
    Returns the painter's current brush.

    \sa QCGPainter::setBrush(), {QPainter#Settings}{Settings}
*/

const QBrush &QCGPainter::brush() const
{
    return {};
}

/*!
    \fn void QCGPainter::setBackground(const QBrush &brush)

    Sets the background brush of the painter to the given \a brush.

    The background brush is the brush that is filled in when drawing
    opaque text, stippled lines and bitmaps. The background brush has
    no effect in transparent background mode (which is the default).

    \sa background(), setBackgroundMode(),
    {QPainter#Settings}{Settings}
*/

void QCGPainter::setBackground(const QBrush &bg)
{

}

/*!
    Sets the painter's font to the given \a font.

    This font is used by subsequent drawText() functions. The text
    color is the same as the pen color.

    If you set a font that isn't available, Qt finds a close match.
    font() will return what you set using setFont() and fontInfo() returns the
    font actually being used (which may be the same).

    \sa font(), drawText(), {QPainter#Settings}{Settings}
*/

void QCGPainter::setFont(const QFont &font)
{

}

/*!
    Returns the currently set font used for drawing text.

    \sa setFont(), drawText(), {QPainter#Settings}{Settings}
*/
const QFont &QCGPainter::font() const
{
    return {};
}

/*!
    \since 4.4

    Draws the given rectangle \a rect with rounded corners.

    The \a xRadius and \a yRadius arguments specify the radii
    of the ellipses defining the corners of the rounded rectangle.
    When \a mode is Qt::RelativeSize, \a xRadius and
    \a yRadius are specified in percentage of half the rectangle's
    width and height respectively, and should be in the range
    0.0 to 100.0.

    A filled rectangle has a size of rect.size(). A stroked rectangle
    has a size of rect.size() plus the pen width.

    \table 100%
    \row
    \li \inlineimage qpainter-roundrect.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 8
    \endtable

    \sa drawRect(), QPen
*/
void QCGPainter::drawRoundedRect(const QRectF &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode)
{

}

/*!
    \fn void QCGPainter::drawRoundedRect(const QRect &rect, qreal xRadius, qreal yRadius,
                                       Qt::SizeMode mode = Qt::AbsoluteSize);
    \since 4.4
    \overload

    Draws the given rectangle \a rect with rounded corners.
*/

/*!
    \fn void QCGPainter::drawRoundedRect(int x, int y, int w, int h, qreal xRadius, qreal yRadius,
                                       Qt::SizeMode mode = Qt::AbsoluteSize);
    \since 4.4
    \overload

    Draws the given rectangle \a x, \a y, \a w, \a h with rounded corners.
*/

/*!
    \obsolete

    Draws a rectangle \a r with rounded corners.

    The \a xRnd and \a yRnd arguments specify how rounded the corners
    should be. 0 is angled corners, 99 is maximum roundedness.

    A filled rectangle has a size of r.size(). A stroked rectangle
    has a size of r.size() plus the pen width.

    \sa drawRoundedRect()
*/
void QCGPainter::drawRoundRect(const QRectF &r, int xRnd, int yRnd)
{
    drawRoundedRect(r, xRnd, yRnd, Qt::RelativeSize);
}


/*!
    \fn void QCGPainter::drawRoundRect(const QRect &r, int xRnd = 25, int yRnd = 25)

    \overload
    \obsolete

    Draws the rectangle \a r with rounded corners.
*/

/*!
    \obsolete

    \fn QCGPainter::drawRoundRect(int x, int y, int w, int h, int xRnd, int yRnd)

    \overload

    Draws the rectangle \a x, \a y, \a w, \a h with rounded corners.
*/

/*!
    \fn void QCGPainter::drawEllipse(const QRectF &rectangle)

    Draws the ellipse defined by the given \a rectangle.

    A filled ellipse has a size of \a{rectangle}.\l
    {QRect::size()}{size()}. A stroked ellipse has a size of
    \a{rectangle}.\l {QRect::size()}{size()} plus the pen width.

    \table 100%
    \row
    \li \inlineimage qpainter-ellipse.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 9
    \endtable

    \sa drawPie(), {Coordinate System}
*/
void QCGPainter::drawEllipse(const QRectF &r)
{

}

/*!
    \fn void QCGPainter::drawEllipse(const QRect &rectangle)

    \overload

    Draws the ellipse defined by the given \a rectangle.
*/
void QCGPainter::drawEllipse(const QRect &r)
{

}



void QCGPainter::drawArc(const QRectF &r, int a, int alen)
{

}

/*! \fn void QCGPainter::drawArc(const QRect &rectangle, int startAngle,
                               int spanAngle)

    \overload

    Draws the arc defined by the given \a rectangle, \a startAngle and
    \a spanAngle.
*/

/*!
    \fn void QCGPainter::drawArc(int x, int y, int width, int height,
                               int startAngle, int spanAngle)

    \overload

    Draws the arc defined by the rectangle beginning at (\a x, \a y)
    with the specified \a width and \a height, and the given \a
    startAngle and \a spanAngle.
*/

/*!
    \fn void QCGPainter::drawPie(const QRectF &rectangle, int startAngle, int spanAngle)

    Draws a pie defined by the given \a rectangle, \a startAngle and \a spanAngle.

    The pie is filled with the current brush().

    The startAngle and spanAngle must be specified in 1/16th of a
    degree, i.e. a full circle equals 5760 (16 * 360). Positive values
    for the angles mean counter-clockwise while negative values mean
    the clockwise direction. Zero degrees is at the 3 o'clock
    position.

    \table 100%
    \row
    \li \inlineimage qpainter-pie.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 11
    \endtable

    \sa drawEllipse(), drawChord(), {Coordinate System}
*/
void QCGPainter::drawPie(const QRectF &r, int a, int alen)
{

}

void QCGPainter::drawChord(const QRectF &r, int a, int alen)
{

}
/*!
    \fn void QCGPainter::drawChord(const QRect &rectangle, int startAngle, int spanAngle)

    \overload

    Draws the chord defined by the given \a rectangle, \a startAngle and
    \a spanAngle.
*/

/*!
    \fn void QCGPainter::drawChord(int x, int y, int width, int height, int
    startAngle, int spanAngle)

    \overload

   Draws the chord defined by the rectangle beginning at (\a x, \a y)
   with the specified \a width and \a height, and the given \a
   startAngle and \a spanAngle.
*/


/*!
    Draws the first \a lineCount lines in the array \a lines
    using the current pen.

    \sa drawLine(), drawPolyline()
*/
void QCGPainter::drawLines(const QLineF *lines, int lineCount)
{

}

/*!
    \fn void QCGPainter::drawLines(const QLine *lines, int lineCount)
    \overload

    Draws the first \a lineCount lines in the array \a lines
    using the current pen.
*/
void QCGPainter::drawLines(const QLine *lines, int lineCount)
{
    QTransform matrix;
    if (d_ptr && d_ptr->state->WxF)
    {
        matrix = d_ptr->state->matrix;
    }
    CGFloat lineWidth = 1.0;
    QColor qColor;
    if (d_ptr)
    {
        qreal m11 = matrix.m11();
        qreal m22 = matrix.m22();
        lineWidth = d_ptr->state->pen.widthF() * m11;
        qColor = d_ptr->state->pen.color();
    }
    CGContextRef ctx = (CGContextRef)cgContextRef;
    CGContextSetLineWidth(ctx, lineWidth);
    CGContextSetShouldAntialias(ctx, NO);
    CGContextBeginPath(ctx);
    for (int i = 0; i < lineCount; i++)
    {
        QPoint p1 = lines[i].p1();
        QPoint p2 = lines[i].p2();
        /*if (!s->flags.antialiased) */{
            uint txop = matrix.type();
            if (txop == QTransform::TxNone) {
            } else if (txop == QTransform::TxTranslate) {
                p1 = p1 + QPoint(matrix.dx(), matrix.dy());
                p2 = p2 + QPoint(matrix.dx(), matrix.dy());
            } else if (txop == QTransform::TxScale) {
                p1 = matrix.map(p1);
                p2 = matrix.map(p2);
            }
        }
        CGContextMoveToPoint(ctx, p1.x(), m_contextSize.height() - p1.y());
        CGContextAddLineToPoint(ctx, p2.x(), m_contextSize.height() - p2.y());
    }
    CGContextClosePath(ctx);

    CGContextSetRGBStrokeColor(ctx, qColor.redF(), qColor.greenF(), qColor.blueF(), qColor.alphaF());
    CGContextStrokePath(ctx);
}

/*!
    \overload

    Draws the first \a lineCount lines in the array \a pointPairs
    using the current pen.  The lines are specified as pairs of points
    so the number of entries in \a pointPairs must be at least \a
    lineCount * 2.
*/
void QCGPainter::drawLines(const QPointF *pointPairs, int lineCount)
{

}

/*!
    \overload

    Draws the first \a lineCount lines in the array \a pointPairs
    using the current pen.
*/
void QCGPainter::drawLines(const QPoint *pointPairs, int lineCount)
{

}


/*!
    \fn void QCGPainter::drawLines(const QVector<QPointF> &pointPairs)
    \overload

    Draws a line for each pair of points in the vector \a pointPairs
    using the current pen. If there is an odd number of points in the
    array, the last point will be ignored.
*/

/*!
    \fn void QCGPainter::drawLines(const QVector<QPoint> &pointPairs)
    \overload

    Draws a line for each pair of points in the vector \a pointPairs
    using the current pen.
*/

/*!
    \fn void QCGPainter::drawLines(const QVector<QLineF> &lines)
    \overload

    Draws the set of lines defined by the list \a lines using the
    current pen and brush.
*/

/*!
    \fn void QCGPainter::drawLines(const QVector<QLine> &lines)
    \overload

    Draws the set of lines defined by the list \a lines using the
    current pen and brush.
*/

/*!
    Draws the polyline defined by the first \a pointCount points in \a
    points using the current pen.

    Note that unlike the drawPolygon() function the last point is \e
    not connected to the first, neither is the polyline filled.

    \table 100%
    \row
    \li
    \snippet code/src_gui_painting_qpainter.cpp 13
    \endtable

    \sa drawLines(), drawPolygon(), {Coordinate System}
*/
void QCGPainter::drawPolyline(const QPointF *points, int pointCount)
{

}

/*!
    \overload

    Draws the polyline defined by the first \a pointCount points in \a
    points using the current pen.
 */
void QCGPainter::drawPolyline(const QPoint *points, int pointCount)
{

}

void QCGPainter::drawPolygon(const QPointF *points, int pointCount, Qt::FillRule fillRule)
{

}

/*! \overload

    Draws the polygon defined by the first \a pointCount points in the
    array \a points.
*/
void QCGPainter::drawPolygon(const QPoint *points, int pointCount, Qt::FillRule fillRule)
{

}

/*! \fn void QCGPainter::drawPolygon(const QPolygonF &points, Qt::FillRule fillRule)

    \overload

    Draws the polygon defined by the given \a points using the fill
    rule \a fillRule.
*/

/*! \fn void QCGPainter::drawPolygon(const QPolygon &points, Qt::FillRule fillRule)

    \overload

    Draws the polygon defined by the given \a points using the fill
    rule \a fillRule.
*/

/*!
    \fn void QCGPainter::drawConvexPolygon(const QPointF *points, int pointCount)

    Draws the convex polygon defined by the first \a pointCount points
    in the array \a points using the current pen.

    \table 100%
    \row
    \li \inlineimage qpainter-polygon.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 15
    \endtable

    The first point is implicitly connected to the last point, and the
    polygon is filled with the current brush().  If the supplied
    polygon is not convex, i.e. it contains at least one angle larger
    than 180 degrees, the results are undefined.

    On some platforms (e.g. X11), the drawConvexPolygon() function can
    be faster than the drawPolygon() function.

    \sa drawPolygon(), drawPolyline(), {Coordinate System}
*/

/*!
    \fn void QCGPainter::drawConvexPolygon(const QPoint *points, int pointCount)
    \overload

    Draws the convex polygon defined by the first \a pointCount points
    in the array \a points using the current pen.
*/

/*!
    \fn void QCGPainter::drawConvexPolygon(const QPolygonF &polygon)

    \overload

    Draws the convex polygon defined by \a polygon using the current
    pen and brush.
*/

/*!
    \fn void QCGPainter::drawConvexPolygon(const QPolygon &polygon)
    \overload

    Draws the convex polygon defined by \a polygon using the current
    pen and brush.
*/

void QCGPainter::drawConvexPolygon(const QPoint *points, int pointCount)
{

}

void QCGPainter::drawConvexPolygon(const QPointF *points, int pointCount)
{

}

static inline QPointF roundInDeviceCoordinates(const QPointF &p, const QTransform &m)
{
    return m.inverted().map(QPointF(m.map(p).toPoint()));
}

/*!
    \fn void QCGPainter::drawPixmap(const QRectF &target, const QPixmap &pixmap, const QRectF &source)

    Draws the rectangular portion \a source of the given \a pixmap
    into the given \a target in the paint device.

    \note The pixmap is scaled to fit the rectangle, if both the pixmap and rectangle size disagree.
    \note See \l{Drawing High Resolution Versions of Pixmaps and Images} on how this is affected
    by QPixmap::devicePixelRatio().

    \table 100%
    \row
    \li
    \snippet code/src_gui_painting_qpainter.cpp 16
    \endtable

    If \a pixmap is a QBitmap it is drawn with the bits that are "set"
    using the pens color. If backgroundMode is Qt::OpaqueMode, the
    "unset" bits are drawn using the color of the background brush; if
    backgroundMode is Qt::TransparentMode, the "unset" bits are
    transparent. Drawing bitmaps with gradient or texture colors is
    not supported.

    \sa drawImage(), QPixmap::devicePixelRatio()
*/
void QCGPainter::drawPixmap(const QPointF &p, const QPixmap &pm)
{

}

void QCGPainter::drawPixmap(const QRectF &r, const QPixmap &pm, const QRectF &sr)
{

}

void QCGPainter::drawImage(const QPointF &p, const QImage &image)
{

}

void QCGPainter::drawImage(const QRectF &targetRect, const QImage &image, const QRectF &sourceRect,
                         Qt::ImageConversionFlags flags)
{

}

#if !defined(QT_NO_RAWFONT)
void QCGPainter::drawGlyphRun(const QPointF &position, const QGlyphRun &glyphRun)
{

}

#endif // QT_NO_RAWFONT

void QCGPainter::drawStaticText(const QPointF &topLeftPosition, const QStaticText &staticText)
{

}

/*!
   \internal
*/
void QCGPainter::drawText(const QPointF &p, const QString &str, int tf, int justificationPadding)
{

}

void QCGPainter::drawText(const QRect &r, int flags, const QString &str, QRect *br)
{

}

/*!
    \fn void QCGPainter::drawText(const QPoint &position, const QString &text)

    \overload

    Draws the given \a text with the currently defined text direction,
    beginning at the given \a position.

    By default, QPainter draws text anti-aliased.

    \note The y-position is used as the baseline of the font.

    \sa setFont(), setPen()
*/

/*!
    \fn void QCGPainter::drawText(const QRectF &rectangle, int flags, const QString &text, QRectF *boundingRect)
    \overload

    Draws the given \a text within the provided \a rectangle.
    The \a rectangle along with alignment \a flags defines the anchors for the \a text.

    \table 100%
    \row
    \li \inlineimage qpainter-text.png
    \li
    \snippet code/src_gui_painting_qpainter.cpp 17
    \endtable

    The \a boundingRect (if not null) is set to what the bounding rectangle
    should be in order to enclose the whole text. For example, in the following
    image, the dotted line represents \a boundingRect as calculated by the
    function, and the dashed line represents \a rectangle:

    \table 100%
    \row
    \li \inlineimage qpainter-text-bounds.png
    \li \snippet code/src_gui_painting_qpainter.cpp drawText
    \endtable

    The \a flags argument is a bitwise OR of the following flags:

    \list
    \li Qt::AlignLeft
    \li Qt::AlignRight
    \li Qt::AlignHCenter
    \li Qt::AlignJustify
    \li Qt::AlignTop
    \li Qt::AlignBottom
    \li Qt::AlignVCenter
    \li Qt::AlignCenter
    \li Qt::TextDontClip
    \li Qt::TextSingleLine
    \li Qt::TextExpandTabs
    \li Qt::TextShowMnemonic
    \li Qt::TextWordWrap
    \li Qt::TextIncludeTrailingSpaces
    \endlist

    \sa Qt::AlignmentFlag, Qt::TextFlag, boundingRect(), layoutDirection()

    By default, QPainter draws text anti-aliased.

    \note The y-coordinate of \a rectangle is used as the top of the font.
*/
void QCGPainter::drawText(const QRectF &r, int flags, const QString &str, QRectF *br)
{

}

/*!
    \fn void QCGPainter::drawText(const QRect &rectangle, int flags, const QString &text, QRect *boundingRect)
    \overload

    Draws the given \a text within the provided \a rectangle according
    to the specified \a flags.

    The \a boundingRect (if not null) is set to the what the bounding rectangle
    should be in order to enclose the whole text. For example, in the following
    image, the dotted line represents \a boundingRect as calculated by the
    function, and the dashed line represents \a rectangle:

    \table 100%
    \row
    \li \inlineimage qpainter-text-bounds.png
    \li \snippet code/src_gui_painting_qpainter.cpp drawText
    \endtable

    By default, QPainter draws text anti-aliased.

    \note The y-coordinate of \a rectangle is used as the top of the font.

    \sa setFont(), setPen()
*/

/*!
    \fn void QCGPainter::drawText(int x, int y, const QString &text)

    \overload

    Draws the given \a text at position (\a{x}, \a{y}), using the painter's
    currently defined text direction.

    By default, QPainter draws text anti-aliased.

    \note The y-position is used as the baseline of the font.

    \sa setFont(), setPen()
*/

/*!
    \fn void QCGPainter::drawText(int x, int y, int width, int height, int flags,
                                const QString &text, QRect *boundingRect)

    \overload

    Draws the given \a text within the rectangle with origin (\a{x},
    \a{y}), \a width and \a height.

    The \a boundingRect (if not null) is set to the what the bounding rectangle
    should be in order to enclose the whole text. For example, in the following
    image, the dotted line represents \a boundingRect as calculated by the
    function, and the dashed line represents the rectangle defined by
    \a x, \a y, \a width and \a height:

    \table 100%
    \row
    \li \inlineimage qpainter-text-bounds.png
    \li \snippet code/src_gui_painting_qpainter.cpp drawText
    \endtable

    The \a flags argument is a bitwise OR of the following flags:

    \list
    \li Qt::AlignLeft
    \li Qt::AlignRight
    \li Qt::AlignHCenter
    \li Qt::AlignJustify
    \li Qt::AlignTop
    \li Qt::AlignBottom
    \li Qt::AlignVCenter
    \li Qt::AlignCenter
    \li Qt::TextSingleLine
    \li Qt::TextExpandTabs
    \li Qt::TextShowMnemonic
    \li Qt::TextWordWrap
    \endlist

    By default, QPainter draws text anti-aliased.

    \note The y-position is used as the top of the font.

    \sa Qt::AlignmentFlag, Qt::TextFlag, setFont(), setPen()
*/

/*!
    \fn void QCGPainter::drawText(const QRectF &rectangle, const QString &text,
        const QTextOption &option)
    \overload

    Draws the given \a text in the \a rectangle specified using the \a option
    to control its positioning and orientation.

    By default, QPainter draws text anti-aliased.

    \note The y-coordinate of \a rectangle is used as the top of the font.

    \sa setFont(), setPen()
*/
void QCGPainter::drawText(const QRectF &r, const QString &text, const QTextOption &o)
{

}


/*!
    \fn void QCGPainter::drawTextItem(int x, int y, const QTextItem &ti)

    \internal
    \overload
*/

/*!
    \fn void QCGPainter::drawTextItem(const QPoint &p, const QTextItem &ti)

    \internal
    \overload

    Draws the text item \a ti at position \a p.
*/

/*!
    \fn void QCGPainter::drawTextItem(const QPointF &p, const QTextItem &ti)

    \internal
    \since 4.1

    Draws the text item \a ti at position \a p.

    This method ignores the painters background mode and
    color. drawText and qt_format_text have to do it themselves, as
    only they know the extents of the complete string.

    It ignores the font set on the painter as the text item has one of its own.

    The underline and strikeout parameters of the text items font are
    ignored aswell. You'll need to pass in the correct flags to get
    underlining and strikeout.
*/

static QPixmap generateWavyPixmap(qreal maxRadius, const QPen &pen)
{
    return {};
}

void QCGPainter::drawTextItem(const QPointF &p, const QTextItem &ti)
{

}


QRect QCGPainter::boundingRect(const QRect &rect, int flags, const QString &str)
{
    return {};
}



QRectF QCGPainter::boundingRect(const QRectF &rect, int flags, const QString &str)
{
    return {};
}

/*!
    \fn QRectF QCGPainter::boundingRect(const QRectF &rectangle,
        const QString &text, const QTextOption &option)

    \overload

    Instead of specifying flags as a bitwise OR of the
    Qt::AlignmentFlag and Qt::TextFlag, this overloaded function takes
    an \a option argument. The QTextOption class provides a
    description of general rich text properties.

    \sa QTextOption
*/
QRectF QCGPainter::boundingRect(const QRectF &r, const QString &text, const QTextOption &o)
{
    return {};
}

/*!
    \fn void QCGPainter::drawTiledPixmap(const QRectF &rectangle, const QPixmap &pixmap, const QPointF &position)

    Draws a tiled \a pixmap, inside the given \a rectangle with its
    origin at the given \a position.

    Calling drawTiledPixmap() is similar to calling drawPixmap()
    several times to fill (tile) an area with a pixmap, but is
    potentially much more efficient depending on the underlying window
    system.

    drawTiledPixmap() will produce the same visual tiling pattern on
    high-dpi displays (with devicePixelRatio > 1), compared to normal-
    dpi displays. Set the devicePixelRatio on the \a pixmap to control
    the tile size. For example, setting it to 2 halves the tile width
    and height (on both 1x and 2x displays), and produces high-resolution
    output on 2x displays.

    The \a position offset is always in the painter coordinate system,
    indepentent of display devicePixelRatio.

    \sa drawPixmap()
*/
void QCGPainter::drawTiledPixmap(const QRectF &r, const QPixmap &pixmap, const QPointF &sp)
{

}

/*!
    \fn void QCGPainter::drawTiledPixmap(const QRect &rectangle, const QPixmap &pixmap,
                                  const QPoint &position = QPoint())
    \overload

    Draws a tiled \a pixmap, inside the given \a rectangle with its
    origin at the given \a position.
*/

/*!
    \fn void QCGPainter::drawTiledPixmap(int x, int y, int width, int height, const
         QPixmap &pixmap, int sx, int sy);
    \overload

    Draws a tiled \a pixmap in the specified rectangle.

    (\a{x}, \a{y}) specifies the top-left point in the paint device
    that is to be drawn onto; with the given \a width and \a
    height. (\a{sx}, \a{sy}) specifies the top-left point in the \a
    pixmap that is to be drawn; this defaults to (0, 0).
*/

#ifndef QT_NO_PICTURE

/*!
    \fn void QCGPainter::drawPicture(const QPointF &point, const QPicture &picture)

    Replays the given \a picture at the given \a point.

    The QPicture class is a paint device that records and replays
    QPainter commands. A picture serializes the painter commands to an
    IO device in a platform-independent format. Everything that can be
    painted on a widget or pixmap can also be stored in a picture.

    This function does exactly the same as QPicture::play() when
    called with \a point = QPoint(0, 0).

    \table 100%
    \row
    \li
    \snippet code/src_gui_painting_qpainter.cpp 18
    \endtable

    \sa QPicture::play()
*/

void QCGPainter::drawPicture(const QPointF &p, const QPicture &picture)
{

}

/*!
    \fn void QCGPainter::drawPicture(const QPoint &point, const QPicture &picture)
    \overload

    Replays the given \a picture at the given \a point.
*/

/*!
    \fn void QCGPainter::drawPicture(int x, int y, const QPicture &picture)
    \overload

    Draws the given \a picture at point (\a x, \a y).
*/

#endif // QT_NO_PICTURE

/*!
    \fn void QCGPainter::eraseRect(const QRectF &rectangle)

    Erases the area inside the given \a rectangle. Equivalent to
    calling
    \snippet code/src_gui_painting_qpainter.cpp 19

    \sa fillRect()
*/
void QCGPainter::eraseRect(const QRectF &r)
{

}


void QCGPainter::fillRect(const QRectF &r, const QBrush &brush)
{
    if (Qt::SolidPattern == brush.style())
    {
        fillRect(r, brush.color());
    }
}

void QCGPainter::fillRect(const QRect &r, const QBrush &brush)
{
    fillRect(QRectF(r), brush);
}


void QCGPainter::fillRect(const QRect &r, const QColor &qColor)
{
    fillRect(QRectF(r), qColor);
}

void QCGPainter::fillRect(const QRectF &r, const QColor &qColor)
{
    QTransform matrix;
    if (d_ptr && d_ptr->state->WxF)
    {
        matrix = d_ptr->state->matrix;
    }
    QRectF rr = r;
    /*if (!s->flags.antialiased) */{
        uint txop = matrix.type();
        if (txop == QTransform::TxNone) {
        } else if (txop == QTransform::TxTranslate) {
            rr = r.translated(matrix.dx(), matrix.dy());
        } else if (txop == QTransform::TxScale) {
            rr = matrix.mapRect(r);
        }
    }
    CGColorSpaceRef  colorSpace=CGColorSpaceCreateDeviceRGB();
    CGFloat component[]={static_cast<CGFloat>(qColor.red()/255.0),static_cast<CGFloat>(qColor.green()/255.0),
        static_cast<CGFloat>(qColor.blue()/255.0),static_cast<CGFloat>(qColor.alpha()/255.0)};

    CGColorRef color = CGColorCreate(colorSpace, component);
    CGColorSpaceRelease(colorSpace);

    qreal bottom = m_contextSize.height() - rr.bottom();
    CGRect flippedRect = CGRectMake(rr.left(), bottom, rr.width(), rr.height());

    CGContextSetFillColorWithColor((CGContextRef)cgContextRef, color);
    CGContextFillRect((CGContextRef)cgContextRef, flippedRect);
}


void QCGPainter::setRenderHint(QPainter::RenderHint hint, bool on)
{

}

/*!
    \since 4.2

    Sets the given render \a hints on the painter if \a on is true;
    otherwise clears the render hints.

    \sa setRenderHint(), renderHints(), {QPainter#Rendering
    Quality}{Rendering Quality}
*/

void QCGPainter::setRenderHints(QPainter::RenderHints hints, bool on)
{

}

/*!
    Returns a flag that specifies the rendering hints that are set for
    this painter.

    \sa testRenderHint(), {QPainter#Rendering Quality}{Rendering Quality}
*/
QPainter::RenderHints QCGPainter::renderHints() const
{
    return {};
}

/*!
    \fn bool QCGPainter::testRenderHint(RenderHint hint) const
    \since 4.3

    Returns \c true if \a hint is set; otherwise returns \c false.

    \sa renderHints(), setRenderHint()
*/

/*!
    Returns \c true if view transformation is enabled; otherwise returns
    false.

    \sa setViewTransformEnabled(), worldTransform()
*/

bool QCGPainter::viewTransformEnabled() const
{
    return false;
}


/*!
    \fn void QCGPainter::setWindow(const QRect &rectangle)

    Sets the painter's window to the given \a rectangle, and enables
    view transformations.

    The window rectangle is part of the view transformation. The
    window specifies the logical coordinate system. Its sister, the
    viewport(), specifies the device coordinate system.

    The default window rectangle is the same as the device's
    rectangle.

    \sa window(), viewTransformEnabled(), {Coordinate
    System#Window-Viewport Conversion}{Window-Viewport Conversion}
*/

/*!
    \fn void QCGPainter::setWindow(int x, int y, int width, int height)
    \overload

    Sets the painter's window to the rectangle beginning at (\a x, \a
    y) and the given \a width and \a height.
*/

void QCGPainter::setWindow(const QRect &r)
{

}

/*!
    Returns the window rectangle.

    \sa setWindow(), setViewTransformEnabled()
*/

QRect QCGPainter::window() const
{
    return {};
}

/*!
    \fn void QCGPainter::setViewport(const QRect &rectangle)

    Sets the painter's viewport rectangle to the given \a rectangle,
    and enables view transformations.

    The viewport rectangle is part of the view transformation. The
    viewport specifies the device coordinate system. Its sister, the
    window(), specifies the logical coordinate system.

    The default viewport rectangle is the same as the device's
    rectangle.

    \sa viewport(), viewTransformEnabled(), {Coordinate
    System#Window-Viewport Conversion}{Window-Viewport Conversion}
*/

/*!
    \fn void QCGPainter::setViewport(int x, int y, int width, int height)
    \overload

    Sets the painter's viewport rectangle to be the rectangle
    beginning at (\a x, \a y) with the given \a width and \a height.
*/

void QCGPainter::setViewport(const QRect &r)
{

}

/*!
    Returns the viewport rectangle.

    \sa setViewport(), setViewTransformEnabled()
*/

QRect QCGPainter::viewport() const
{
    return {};
}

/*!
    Enables view transformations if \a enable is true, or disables
    view transformations if \a enable is false.

    \sa viewTransformEnabled(), {Coordinate System#Window-Viewport
    Conversion}{Window-Viewport Conversion}
*/

void QCGPainter::setViewTransformEnabled(bool enable)
{

}

/*!
    \threadsafe

    \obsolete

    Please use QWidget::render() instead.

    Redirects all paint commands for the given paint \a device, to the
    \a replacement device. The optional point \a offset defines an
    offset within the source device.

    The redirection will not be effective until the begin() function
    has been called; make sure to call end() for the given \a
    device's painter (if any) before redirecting. Call
    restoreRedirected() to restore the previous redirection.

    \warning Making use of redirections in the QPainter API implies
    that QCGPainter::begin() and QPaintDevice destructors need to hold
    a mutex for a short period. This can impact performance. Use of
    QWidget::render is strongly encouraged.

    \sa redirected(), restoreRedirected()
*/
void QCGPainter::setRedirected(const QPaintDevice *device,
                             QPaintDevice *replacement,
                             const QPoint &offset)
{

}

/*!
    \threadsafe

    \obsolete

    Using QWidget::render() obsoletes the use of this function.

    Restores the previous redirection for the given \a device after a
    call to setRedirected().

    \warning Making use of redirections in the QPainter API implies
    that QCGPainter::begin() and QPaintDevice destructors need to hold
    a mutex for a short period. This can impact performance. Use of
    QWidget::render is strongly encouraged.

    \sa redirected()
 */
void QCGPainter::restoreRedirected(const QPaintDevice *device)
{

}

/*!
    \threadsafe

    \obsolete

    Using QWidget::render() obsoletes the use of this function.

    Returns the replacement for given \a device. The optional out
    parameter \a offset returns the offset within the replaced device.

    \warning Making use of redirections in the QPainter API implies
    that QCGPainter::begin() and QPaintDevice destructors need to hold
    a mutex for a short period. This can impact performance. Use of
    QWidget::render is strongly encouraged.

    \sa setRedirected(), restoreRedirected()
*/
QPaintDevice *QCGPainter::redirected(const QPaintDevice *device, QPoint *offset)
{
    Q_UNUSED(device)
    Q_UNUSED(offset)
    return 0;
}
/*!
    Sets the layout direction used by the painter when drawing text,
    to the specified \a direction.

    The default is Qt::LayoutDirectionAuto, which will implicitly determine the
    direction from the text drawn.

    \sa QTextOption::setTextDirection(), layoutDirection(), drawText(), {QPainter#Settings}{Settings}
*/
void QCGPainter::setLayoutDirection(Qt::LayoutDirection direction)
{

}

/*!
    Returns the layout direction used by the painter when drawing text.

    \sa QTextOption::textDirection(), setLayoutDirection(), drawText(), {QPainter#Settings}{Settings}
*/
Qt::LayoutDirection QCGPainter::layoutDirection() const
{
    return {};
}


void QCGPainter::setTransform(const QTransform &transform, bool combine )
{
//    CGAffineTransform matrix = CGAffineTransformMake(transform.m11(),
//                                                     transform.m12(),
//                                                     transform.m21(),
//                                                     transform.m22(),
//                                                     transform.dx(),
//                                                     transform.dy());
//    if (combine)
//        CGContextConcatCTM(cgContextRef, matrix);
//    else
}

/*!
    Alias for worldTransform().
    Returns the world transformation matrix.

    \sa worldTransform()
*/

const QTransform & QCGPainter::transform() const
{
    return {};
}


/*!
    Returns the matrix that transforms from logical coordinates to
    device coordinates of the platform dependent paint device.

    This function is \e only needed when using platform painting
    commands on the platform dependent handle (Qt::HANDLE), and the
    platform does not do transformations nativly.

    The QPaintEngine::PaintEngineFeature enum can be queried to
    determine whether the platform performs the transformations or
    not.

    \sa worldTransform(), QPaintEngine::hasFeature(),
*/

const QTransform & QCGPainter::deviceTransform() const
{
    return {};
}


/*!
    Resets any transformations that were made using translate(),
    scale(), shear(), rotate(), setWorldTransform(), setViewport()
    and setWindow().

    \sa {Coordinate Transformations}
*/

void QCGPainter::resetTransform()
{

}

/*!
    Sets the world transformation matrix.
    If \a combine is true, the specified \a matrix is combined with the current matrix;
    otherwise it replaces the current matrix.

    \sa transform(), setTransform()
*/

void QCGPainter::setWorldTransform(const QTransform &matrix, bool combine )
{

}

/*!
    Returns the world transformation matrix.
*/

const QTransform & QCGPainter::worldTransform() const
{
    return {};
}

/*!
    Returns the transformation matrix combining the current
    window/viewport and world transformation.

    \sa setWorldTransform(), setWindow(), setViewport()
*/

QTransform QCGPainter::combinedTransform() const
{
    QTransform transform;
    return transform;
}


/*!
    \since 4.7

    This function is used to draw \a pixmap, or a sub-rectangle of \a pixmap,
    at multiple positions with different scale, rotation and opacity. \a
    fragments is an array of \a fragmentCount elements specifying the
    parameters used to draw each pixmap fragment. The \a hints
    parameter can be used to pass in drawing hints.

    This function is potentially faster than multiple calls to drawPixmap(),
    since the backend can optimize state changes.

    \sa QCGPainter::PixmapFragment, QCGPainter::PixmapFragmentHint
*/

void QCGPainter::drawPixmapFragments(const QPainter::PixmapFragment *fragments, int fragmentCount,
                                   const QPixmap &pixmap, QPainter::PixmapFragmentHints hints)
{

}

QT_END_NAMESPACE
