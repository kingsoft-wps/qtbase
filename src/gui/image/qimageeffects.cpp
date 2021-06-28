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
#include "qimageeffects.h"
#include "qimageeffects_p.h"
#include "private/qdrawhelper_p.h"

QT_BEGIN_NAMESPACE

const float grayRedFactor = 0.2157f;
const float grayGreenFactor = 0.7137f;
const float grayBlueFactor = 0.0706f;

const float grayRedFactorOld = 0.299f;
const float grayGreenFactorOld = 0.587f;
const float grayBlueFactorOld = 0.114f;

void qt_setbilevel(uint &rgb, const quint16 percent)
{
    Q_ASSERT(qIsGray(rgb));

    quint8 *p = (quint8*)&rgb;
    if (percent == 0 || ((quint32)p[0] << QImageEffectsPrivate::base_shift) > ((quint32)p[3] * percent))
        p[0] = p[1] = p[2] = p[3];
    else
        p[0] = p[1] = p[2] = 0;
}

void qt_setbilevel(uint *buffer, int length, const quint16 percent)
{
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    if (qCpuHasFeature(SSE2))
        return qt_setbilevel_sse(buffer, length, percent);
#endif
    for (int i = 0; i < length; i++)
        qt_setbilevel(buffer[i], percent);
}

bool qt_inTolerance(const QRgb &clr, const quint8 low[4], const quint8 hight[4])
{
    const quint8 *p = (const quint8*)&clr;

    return (low[0] <= p[0] && p[0] <= hight[0])
        && (low[1] <= p[1] && p[1] <= hight[1])
        && (low[2] <= p[2] && p[2] <= hight[2])
        && (low[3] <= p[3] && p[3] <= hight[3]);
}

bool qt_inTolerance_noAlpha(const QRgb& clr, const quint8 low[4], const quint8 hight[4])
{
    const quint8 *p = (const quint8*)&clr;

    return (low[0] <= p[0] && p[0] <= hight[0])
        && (low[1] <= p[1] && p[1] <= hight[1])
        && (low[2] <= p[2] && p[2] <= hight[2]);
}

// red' = (low + ((high - low) / 255.0) * (red * 255.0 / alpha)) * (alpha / 255.0)
void qt_shadowTransform(uint *buffer, int length, quint8 shadowLow, quint8 shadowHight)
{
    Q_ASSERT(shadowHight > shadowLow);
    uint grayRange = shadowHight - shadowLow;

    for (int i = 0; i < length; i++) {
        quint8* pChannel = (quint8*)(&buffer[i]);
        uint alphaLowGray = shadowLow * pChannel[3];
        pChannel[0] = (alphaLowGray + (grayRange * pChannel[0])) >> 8;
        pChannel[1] = (alphaLowGray + (grayRange * pChannel[1])) >> 8;
        pChannel[2] = (alphaLowGray + (grayRange * pChannel[2])) >> 8;
    }
}

QImageEffectsPrivate::QImageEffectsPrivate()
    : hasColorMatirx(0)
    , hasColorKey(0)
    , hasDuotone(0)
    , hasBilevel(0)
    , hasSubstColor(0)
    , isGray(0)
    , hasRecolor(0)
    , hasAlpha(0)
    , hasShadow(0)
    , effectMode(QImageEffects::Mode1)
    , checkBound(1)
    , colorKey(0)
    , tolerance(0)
    , bilevelThreshold(base_scale >> 1)
    , duotoneColor1(0)
    , duotoneColor2(0)
    , substColor(0)
    , recolorValue(0)
    , alphaValue(0)
    , brightness(0)
    , contrast(1)
    , shadowLow(0)
    , shadowHight(255)
    , m_pTransformProc(&QImageEffectsPrivate::transform_cpp)
    , m_pInToleranceFunc(&qt_inTolerance)
    , m_pTransformMatrixAndBilevel(&QImageEffectsPrivate::transformMatrixAndBilevel)
{   
    memset(colorMatrixInt[0], 0, sizeof(colorMatrixInt));
    colorMatrixInt[0][0] = colorMatrixInt[1][1] = colorMatrixInt[2][2] = colorMatrixInt[3][3] = base_scale;
    brightContrastParas[0] = brightContrastParas[1] = base_scale;
    ref = 1;
}

QImageEffectsPrivate::~QImageEffectsPrivate()
{
}

QImageEffectsPrivate::QImageEffectsPrivate(const QImageEffectsPrivate &rhs)
{
    init(rhs);
}

QImageEffectsPrivate &QImageEffectsPrivate::operator=(const QImageEffectsPrivate &rhs)
{
    init(rhs);
    return *this;
}

bool QImageEffectsPrivate::isTransparent() const
{
    if (hasColorKey)
        return true;

    QList<QRgb> newColors = colorMap.values();
    for (auto color: newColors) {
        if (qAlpha(color) != 0xff)
            return true;
    }

    return false;
}

void QImageEffectsPrivate::init(const QImageEffectsPrivate &rhs)
{
    hasColorMatirx = rhs.hasColorMatirx;
    hasColorKey = rhs.hasColorKey;
    hasDuotone = rhs.hasDuotone;
    hasBilevel = rhs.hasBilevel;
    hasSubstColor = rhs.hasSubstColor;
    isGray = rhs.isGray;
    hasRecolor = rhs.hasRecolor;
    hasAlpha = rhs.hasAlpha;
    hasShadow = rhs.hasShadow;
    effectMode = rhs.effectMode;
    checkBound = rhs.checkBound;
    colorMatrix = rhs.colorMatrix;
    colorKey = rhs.colorKey;
    colorMap = rhs.colorMap;
    brushColorMap = rhs.brushColorMap;
    tolerance = rhs.tolerance;
    bilevelThreshold = rhs.bilevelThreshold;
    duotoneColor1 = rhs.duotoneColor1;
    duotoneColor2 = rhs.duotoneColor2;
    substColor = rhs.substColor;
    recolorValue = rhs.recolorValue;
    alphaValue = rhs.alphaValue;
    brightness = rhs.brightness;
    contrast = rhs.contrast;
    shadowLow = rhs.shadowLow;
    shadowHight = rhs.shadowHight;
    m_pTransformProc = rhs.m_pTransformProc;
    m_pInToleranceFunc = rhs.m_pInToleranceFunc;
    m_pTransformMatrixAndBilevel = rhs.m_pTransformMatrixAndBilevel;
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    d = rhs.d;
#endif

    //temporary data;
    memcpy(colorMatrixInt, rhs.colorMatrixInt, sizeof(colorMatrixInt));
    memcpy(brightContrastParas, rhs.brightContrastParas, sizeof(brightContrastParas));
    memcpy(colorKeyLow, rhs.colorKeyLow, sizeof(colorKeyLow));
    memcpy(colorKeyHight, rhs.colorKeyHight, sizeof(colorKeyHight));
}

void QImageEffectsPrivate::resetState()
{
    hasColorMatirx = false;
    hasColorKey = false;
    hasDuotone = false;
    hasBilevel = false;
    hasSubstColor = false;
    isGray = false;
    hasRecolor = false;
    hasAlpha = false;
    hasShadow = false;
    effectMode = QImageEffects::Mode1;
    checkBound = true;

    brightness = 0;
    contrast = 1;
    m_pTransformProc = &QImageEffectsPrivate::transform_cpp;
    m_pInToleranceFunc = &qt_inTolerance;
    m_pTransformMatrixAndBilevel = &QImageEffectsPrivate::transformMatrixAndBilevel;

    memset(colorMatrixInt[0], 0, sizeof(colorMatrixInt));
    colorMatrixInt[0][0] = colorMatrixInt[1][1] = colorMatrixInt[2][2] = colorMatrixInt[3][3] = base_scale;

    colorMap.clear();
    brushColorMap.clear();

    brightContrastParas[0] = brightContrastParas[1] = base_scale;
}

QMatrix4x4 QImageEffectsPrivate::createDuotoneMatrix(const QRgb clr1, const QRgb clr2, const qreal gray_r, const qreal gray_g, const qreal gray_b) const
{
    QMatrix4x4 colorMatrix1(gray_r, gray_g, gray_b, 0.0,
                            gray_r, gray_g, gray_b, 0.0,
                            gray_r, gray_g, gray_b, 0.0,
                            0.0,0.0,0.0,1.0);

    qreal R1 = qRed(clr1) / 255.0f;
    qreal G1 = qGreen(clr1) / 255.0f;
    qreal B1 = qBlue(clr1) / 255.0f;

    qreal R2 = qRed(clr2) / 255.0f;
    qreal G2 = qGreen(clr2) / 255.0f;
    qreal B2 = qBlue(clr2) / 255.0f;

    qreal DR = R2 - R1;
    qreal DG = G2 - G1;
    qreal DB = B2 - B1;

    QMatrix4x4 colorMatrix2(
        DR, 0, 0, R1,
        0, DG, 0, G1,
        0, 0, DB, B1,
        0,  0,  0, 1.0f);

    return colorMatrix2 * colorMatrix1;
}

/*!
    \internal
*/
void QImageEffectsPrivate::updateColorMatrixInt()
{
    const QMatrix4x4 mtx = colorMatrix * base_scale;
    auto pReals = mtx.constData();
    int *pInts = colorMatrixInt[0]; 
    for (int i = 0; i < 16; i++)
    {
        pInts[i] = qRound(pReals[i]);
    }
}

/*!
    \internal
    Update the color matrix according to the effect data.
    Note that the color matrix mustn't have been set by setColorMatrix().
    \sa setColorMatrix(), unsetColorMatrix()
*/
void QImageEffectsPrivate::updateColorMatrix()
{
    Q_ASSERT(!hasColorMatirx);

    colorMatrix.setToIdentity();

    if (hasBilevel) {
        auto p = colorMatrix.data();
        p[0] = p[1] = p[2] = grayRedFactor;
        p[4] = p[5] = p[6] = grayGreenFactor;
        p[8] = p[9] = p[10] = grayBlueFactor;
    } else if (hasDuotone) {
        colorMatrix = createDuotoneMatrix(duotoneColor1, duotoneColor2, grayRedFactor, grayGreenFactor, grayBlueFactor);
    }
    updateColorMatrixInt();
}

void QImageEffectsPrivate::updateColorMatrix_mode2()
{
    Q_ASSERT(effectMode == QImageEffects::Mode2);
    Q_ASSERT(!hasColorMatirx);

    colorMatrix.setToIdentity();
    auto p = colorMatrix.data();

    QMatrix4x4 duotoneMatrix;
    if (hasDuotone)
        duotoneMatrix = createDuotoneMatrix(duotoneColor1, duotoneColor2, grayRedFactorOld, grayGreenFactorOld, grayBlueFactorOld);

    if (brightness != 0 || contrast != 1){
        qreal propContrast = contrast;
        qreal propBright = brightness / 2;
        qreal fOffset = 0.0f;

        if (propContrast < 1.0f && propBright > 0.0f)
            fOffset = 0.15f * (propContrast - 1.0f);
        else
            fOffset = 0.5f * (propContrast - 1.0f) - propBright * (propContrast - 1.0f);

        qreal fOffsetVal = brightness - fOffset;
        if (hasBilevel) {
            p[0] = p[1] = p[2] = grayRedFactorOld * propContrast;
            p[4] = p[5] = p[6] = grayGreenFactorOld * propContrast;
            p[8] = p[9] = p[10] = grayBlueFactorOld * propContrast;
            p[12] = p[13] = p[14] = fOffsetVal;
        } else {
            p[0] = p[5] = p[10] = propContrast;
            p[12] = p[13] = p[14] = fOffsetVal;
            if (hasDuotone)
                colorMatrix *= duotoneMatrix;
        }
    } else if (hasBilevel) {
        p[0] = p[1] = p[2] = grayRedFactorOld;
        p[4] = p[5] = p[6] = grayGreenFactorOld;
        p[8] = p[9] = p[10] = grayBlueFactorOld;
    } else if (hasDuotone) {
        colorMatrix = duotoneMatrix;
    }
    updateColorMatrixInt();
}

/*!
    \fn inline void QImageEffectsData::transform(QRgb &rgb) const
    Transform the rgb value use the color matrix.
    Note: The alpha channel is unchanged.
    sa\ transform_cpp(), transform_sse2(), transform_sse4()
*/
void QImageEffectsPrivate::transform(QRgb *buffer, int length) const
{
    Q_ASSERT(m_pTransformProc);

    (this->*m_pTransformProc)(buffer, length);
}

void QImageEffectsPrivate::transformEffects(uint* buffer, int length, bool ignoreColorKey /* = false */) const
{
    if (!ignoreColorKey)
        handleColorKey(buffer, length);
    handleColorMap(buffer, length);

    Q_ASSERT(m_pTransformMatrixAndBilevel);
    (this->*m_pTransformMatrixAndBilevel)(buffer, length);

    handleShadowTransform(buffer, length);
}

void QImageEffectsPrivate::transformMatrixAndBilevel(uint* buffer, int length) const
{
    if (!colorMatrix.isIdentity())
        transform(buffer, length);

    if (hasBilevel) {
        qt_setbilevel(buffer, length, bilevelThreshold);
    }

    handleBrightContrastTransfrom(buffer, length);
}

void QImageEffectsPrivate::transformMatrixAndBilevel_old(uint* buffer, int length) const
{
    if (!colorMatrix.isIdentity())
        transform(buffer, length);

    if (hasBilevel) {
        qt_setbilevel(buffer, length, bilevelThreshold);
    }
}

void QImageEffectsPrivate::handleShadowTransform(uint *buffer, int length) const
{
    if (hasShadow)
        qt_shadowTransform(buffer, length, shadowLow, shadowHight);
}

void QImageEffectsPrivate::handleColorKey(uint *buffer, int length) const
{
    Q_ASSERT(buffer);

    if (!hasColorKey)
        return;

    if (tolerance == 0) {
        if (effectMode == 1) {
            for (int i = 0; i < length; i++) {
                if (colorKey == buffer[i])
                    buffer[i] = 0;
            }
        } else {
            for (int i = 0; i < length; i++) {
                if ((colorKey & 0xffffff) == (buffer[i] & 0xffffff))
                    buffer[i] = 0;
            }
        }
    } else {
        Q_ASSERT(m_pInToleranceFunc);
        for (int i = 0; i < length; i++) {
            if ((*m_pInToleranceFunc)(buffer[i], colorKeyLow, colorKeyHight))
                buffer[i] = 0;
        }
    }
}

void QImageEffectsPrivate::handleColorMap(uint *buffer, int length) const
{
    Q_ASSERT(buffer);

    for (int i = 0; i < length; i++) {
        if (colorMap.contains(buffer[i]))
            buffer[i] = colorMap.value(buffer[i]);
    }
}

void QImageEffectsPrivate::handleBrightContrastTransfrom(uint *buffer, int length) const
{
    Q_ASSERT(buffer);

    if (brightness != 0.0f || contrast != 1.0f) {
        if (brightness < -0.99f) {
            for (int i = 0; i < length; ++i) {
                quint8* p = (quint8*)(buffer + i);
                p[2] = p[1] = p[0] = 0;
            }
        } else if (brightness > 0.99f) {
            for (int i = 0; i < length; ++i) {
                quint8* p = (quint8*)(buffer + i);
                p[2] = p[1] = p[0] = p[3];
            }
        } else {
            for (int i = 0; i < length; ++i) {
                quint8* p = (quint8*)(buffer + i);
                int r = (p[2] * brightContrastParas[0] + p[3] * brightContrastParas[1]) >> QImageEffectsPrivate::base_shift;
                int g = (p[1] * brightContrastParas[0] + p[3] * brightContrastParas[1]) >> QImageEffectsPrivate::base_shift;
                int b = (p[0] * brightContrastParas[0] + p[3] * brightContrastParas[1]) >> QImageEffectsPrivate::base_shift;
                p[2] = qBound<int>(0, r, p[3]);
                p[1] = qBound<int>(0, g, p[3]);
                p[0] = qBound<int>(0, b, p[3]);
            }
        }
    }
}

/*!
    \fn void QImageEffectsData::transform_cpp(QRgb &rgb) const
    Transform the rgb value use the color matrix.
    Note: The alpha channel is unchanged
    sa\ transform_sse2(), transform_sse4()
*/
void QImageEffectsPrivate::transform_cpp(QRgb &rgb) const
{
    quint8 *pb = (quint8*)&rgb;
    int r, g, b;
    if (hasColorMatirx || hasDuotone) {
        r =  (pb[2] * colorMatrixInt[0][0] 
            + pb[1] * colorMatrixInt[1][0]
            + pb[0] * colorMatrixInt[2][0] 
            + pb[3] * colorMatrixInt[3][0]) >> base_shift;
        g =  (pb[2] * colorMatrixInt[0][1] 
            + pb[1] * colorMatrixInt[1][1]
            + pb[0] * colorMatrixInt[2][1] 
            + pb[3] * colorMatrixInt[3][1]) >> base_shift;
        b =  (pb[2] * colorMatrixInt[0][2] 
            + pb[1] * colorMatrixInt[1][2]
            + pb[0] * colorMatrixInt[2][2] 
            + pb[3] * colorMatrixInt[3][2]) >> base_shift;
    } else {
        if (hasBilevel) {
            r =  (pb[2] * colorMatrixInt[0][0] 
                + pb[1] * colorMatrixInt[1][0]
                + pb[0] * colorMatrixInt[2][0] 
                + pb[3] * colorMatrixInt[3][0]) >> base_shift;
            pb[2] = pb[1] = pb[0] = qBound<int>(0, r, pb[3]);
            return;
        } else {
            r = (pb[2] * colorMatrixInt[0][0] + pb[3] * colorMatrixInt[3][0]) >> base_shift;
            g = (pb[1] * colorMatrixInt[1][1] + pb[3] * colorMatrixInt[3][1]) >> base_shift;
            b = (pb[0] * colorMatrixInt[2][2] + pb[3] * colorMatrixInt[3][2]) >> base_shift;
        }
    }

    pb[2] = qBound<int>(0, r, pb[3]);
    pb[1] = qBound<int>(0, g, pb[3]);
    pb[0] = qBound<int>(0, b, pb[3]);
}

/*!
    \overload
*/
void QImageEffectsPrivate::transform_cpp(QRgb *buffer, int length) const
{
    for (int i = 0; i < length; i++)
        transform_cpp(buffer[i]);
}

/*!
    \internal
*/

void QImageEffectsPrivate::updateBrightContrastParas()
{
    qreal propContrast = contrast;
    qreal propBright = brightness / 2;
    qreal fOffset = 0.0f;

    if (propContrast < 1.0f && propBright > 0.0f)
        fOffset = 0.15f * (propContrast - 1.0f);
    else
        fOffset = 0.5f * (propContrast - 1.0f) - propBright * (propContrast - 1.0f);

    qreal fOffsetVal = brightness - fOffset;

    brightContrastParas[0] = qRound(propContrast * base_scale);
    brightContrastParas[1] = qRound(fOffsetVal * base_scale);
}

void QImageEffectsPrivate::prepare()
{
    if (effectMode == QImageEffects::Mode2)
        return prepare_mode2();

    if (brightness < -1 || brightness > 1) {
        qWarning("the brightness's value is out of range!");
        brightness = qBound<qreal>(-1, brightness, 1);
    }
    if (contrast < 0) {
        qWarning("The contrast should not be less than 0!");
        contrast = 0;
    }

    if (brightness != 0 || contrast != 1)
        updateBrightContrastParas();
    else {
        brightContrastParas[0] = brightContrastParas[1] = base_scale;
    }

    if (!hasColorMatirx) {
        updateColorMatrix();
    } else {
        auto p = colorMatrix.constData();
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                colorMatrixInt[row][col] = qRound(p[row * 4 + col] * base_scale);
    }
    if (hasColorKey) {
        colorKey = qPremultiply(colorKey);
        if (tolerance != 0){
            const int tol = BYTE_MUL(tolerance, qAlpha(colorKey));
            colorKeyLow[0] = qMax(0, qBlue(colorKey) - tol);
            colorKeyLow[1] = qMax(0, qGreen(colorKey) - tol);
            colorKeyLow[2] = qMax(0, qRed(colorKey) - tol);
            colorKeyLow[3] = qMax(0, qAlpha(colorKey) - tol);
            colorKeyHight[0] = qMin(255, qBlue(colorKey) + tol);
            colorKeyHight[1] = qMin(255, qGreen(colorKey) + tol);
            colorKeyHight[2] = qMin(255, qRed(colorKey) + tol);
            colorKeyHight[3] = qMin(255, qAlpha(colorKey) + tol);
        }
    }

    ColorMap premuledMap;
    ColorMap::const_iterator it = colorMap.constBegin();
    for (; it != colorMap.constEnd(); it++) {
        QRgb preOldColor = qPremultiply(it.key());
        QRgb preNewColor = qPremultiply(it.value());
        premuledMap[preOldColor] = preNewColor;
    }
    colorMap = premuledMap;

    if (hasDuotone || hasColorMatirx)
        checkBound = true;
    else
        checkBound = false;

#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    setTransformFunc();
#endif
}

void QImageEffectsPrivate::prepare_mode2()
{
    Q_ASSERT(effectMode == QImageEffects::Mode2);

    if (brightness < -1 || brightness > 1) {
        qWarning("the brightness's value is out of range!");
        brightness = qBound<qreal>(-1, brightness, 1);
    }
    if (contrast < 0) {
        qWarning("The contrast should not be less than 0!");
        contrast = 0;
    }

    if (!hasColorMatirx) {
        updateColorMatrix_mode2();
    } else {
        auto p = colorMatrix.constData();
        for (int row = 0; row < 4; row++)
            for (int col = 0; col < 4; col++)
                colorMatrixInt[row][col] = qRound(p[row * 4 + col] * base_scale);
    }
    if (hasColorKey) {
        colorKey = qPremultiply(colorKey);
        if (tolerance != 0){
            const int tol = BYTE_MUL(tolerance, qAlpha(colorKey));
            colorKeyLow[0] = qMax(0, qBlue(colorKey) - tol);
            colorKeyLow[1] = qMax(0, qGreen(colorKey) - tol);
            colorKeyLow[2] = qMax(0, qRed(colorKey) - tol);
            colorKeyHight[0] = qMin(255, qBlue(colorKey) + tol);
            colorKeyHight[1] = qMin(255, qGreen(colorKey) + tol);
            colorKeyHight[2] = qMin(255, qRed(colorKey) + tol);
        }
    }

    ColorMap premuledMap;
    ColorMap::const_iterator it = colorMap.constBegin();
    for (; it != colorMap.constEnd(); it++) {
        QRgb preOldColor = qPremultiply(it.key());
        QRgb preNewColor = qPremultiply(it.value());
        premuledMap[preOldColor] = preNewColor;
    }
    colorMap = premuledMap;

    if (hasDuotone || hasColorMatirx || brightness != 0 || contrast != 1)
        checkBound = true;
    else
        checkBound = false;

    m_pInToleranceFunc = &qt_inTolerance_noAlpha;
    m_pTransformMatrixAndBilevel = &QImageEffectsPrivate::transformMatrixAndBilevel_old;
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    setTransformFunc();
#endif
}


QImageEffectsPrivate QImageEffects::shared_null = QImageEffectsPrivate();

QImageEffects::QImageEffects()
    : d(&shared_null)
{
    d->ref.ref();
}

QImageEffects::QImageEffects(const QImageEffects &rhs)
{
    d = rhs.d;
    if (d)
        d->ref.ref();
}

QImageEffects::~QImageEffects()
{
    clean();
}

void QImageEffects::clean()
{
    if (d && !d->ref.deref() && d != &shared_null) {
        delete d;
        d = nullptr;
    }
}

QImageEffects &QImageEffects::operator=(const QImageEffects &rhs)
{
    if (rhs.d != d) {
        if (rhs.d)
            rhs.d->ref.ref();
        clean();
        d = rhs.d;
    }

    return *this;
}

QImageEffects::DataPtr QImageEffects::data_ptr() const
{
    if (d == &shared_null) {
        d->ref.deref();
        d = new QImageEffectsPrivate();
        d->ref = 1;
    }

    return d;
}

/*!
    \fn void QImageEffects::setBilevel()
    Set the bilevel flag and the parameters.
    The alpha channel is unchanged.
    The function will clear the recolor flag.
    \sa unsetBilevel()
*/
void QImageEffects::setBilevel(qreal threshold)
{
    if (threshold < 0 || 1.0 < threshold) {
        qWarning("threshold must be between 0 and 1.0!");
        return;
    }

    detach();

    unsetDuotone();
    d->hasBilevel = true;
    d->bilevelThreshold = qRound(threshold * QImageEffectsPrivate::base_scale);
    Q_ASSERT(d->bilevelThreshold <= QImageEffectsPrivate::base_scale);
}

/*!
    \fn void QImageEffects::unsetBilevel()
    \sa setBilevel()
*/
void QImageEffects::unsetBilevel()
{
    if (!d->hasBilevel)
        return;

    detach();

    d->hasBilevel = false;
}

/*!
    \fn void QImageEffects::setColorMatrix(const QMatrix4x4 &mtx)
    Set the color matrix to \a mtx.
    The red, green and blue channel of each pixel in the image will be transformed 
    by \a mtx before drawing. The alpha channel is unchanged.
    Note that once set the color matrix, the other effects such as gray and bilevel
    are disable. Call unsetColorMatrix() to enable them.
    \sa unsetColorMatrix()
*/
void QImageEffects::setColorMatrix(const QMatrix4x4 &mtx)
{
    detach();

    d->hasColorMatirx = true;
    d->colorMatrix = mtx;
}

QMatrix4x4 QImageEffects::colorMatrix() const
{
    Q_ASSERT(d->hasColorMatirx);

    return d->colorMatrix;
}

/*!
    \fn void QImageEffects::unsetColorMatrix()
    Discard the color matrix set by setColorMatrix and enable the other effects.
    \sa setColorMatrix()
*/
void QImageEffects::unsetColorMatrix()
{
    if (!d->hasColorMatirx)
        return;

    detach();

    d->hasColorMatirx = false;
}

/*!
*/

void QImageEffects::setDuotone( QRgb color1, QRgb color2 )
{
    detach();

    unsetBilevel();
    d->hasDuotone = true;
    d->duotoneColor1 = color1;
    d->duotoneColor2 = color2;
}

bool QImageEffects::hasDuotone() const
{
    return d->hasDuotone;
}

void QImageEffects::getDuotone(QRgb &color1, QRgb &color2) const
{
    Q_ASSERT(d->hasDuotone);

    color1 = d->duotoneColor1;
    color2 = d->duotoneColor2;
}

void QImageEffects::unsetDuotone()
{
    if (!d->hasDuotone)
        return;

    detach();

    d->hasDuotone = false;
}


void QImageEffects::setBrightness(qreal brightness)
{
    if (d->brightness == brightness)
        return;
    detach();

    d->brightness = brightness;
}

void QImageEffects::setColorKey(QRgb key, quint8 tolerance/* = 0*/)
{
    detach();

    d->hasColorKey = true;
    d->colorKey = key;
    d->tolerance = tolerance;
}

void QImageEffects::unsetColorKey()
{
    if (!d->hasColorKey)
        return;

    detach();

    d->hasColorKey = false;
}

void QImageEffects::setRemapTable(const QMap<QRgb, QRgb>& colorMap)
{
    if (colorMap == d->colorMap)
        return;

    detach();

    d->colorMap = colorMap;
}

QMap<QRgb, QRgb> QImageEffects::remapTable() const
{
    return d->colorMap;
}

void QImageEffects::setBrushRemapTable(const QMap<QRgb, QRgb>& colorMap)
{
    if (colorMap == d->brushColorMap)
        return;

    detach();

    d->brushColorMap = colorMap;
}

QMap<QRgb, QRgb> QImageEffects::brushRemapTable() const
{
    return d->brushColorMap;
}

void QImageEffects::setContrast(qreal contrast)
{
    if (d->contrast == contrast)
        return;

    detach();

    d->contrast = contrast;
}

/*!
    \fn bool QImageEffects::hasEffects() const
    Returns whether the data has any effects.
*/
bool QImageEffects::hasEffects() const
{
    if (d->hasColorMatirx && !d->colorMatrix.isIdentity())
        return true;
    else
        return (d->hasDuotone
                || d->hasBilevel
                || d->hasColorKey
                || d->hasSubstColor
                || d->isGray
                || d->hasRecolor
                || d->hasAlpha
                || d->hasShadow
                || d->brightness != 0
                || d->contrast != 1
                || !d->colorMap.isEmpty()
                || !d->brushColorMap.isEmpty());
}

void QImageEffects::resetState() 
{
    detach();

    d->resetState();
}

void QImageEffects::makeEffects(uint *buffer, int length)
{
    if (buffer)
    {
        d->prepare();
        return d->transformEffects(buffer, length);
    }
}

void QImageEffects::detach()
{
    if (d->ref == 1 && d != &shared_null)
        return;

    QImageEffectsPrivate *x = new QImageEffectsPrivate(*d);
    clean();

    x->ref = 1;
    d = x;
}

bool QImageEffects::operator==(const QImageEffects &e) const
{
    if (d == e.d)
        return true;

    const QImageEffectsPrivate* lhs = d;
    const QImageEffectsPrivate* rhs = e.d;

    return (lhs->hasBilevel && rhs->hasBilevel && lhs->bilevelThreshold == rhs->bilevelThreshold || (!lhs->hasBilevel && !rhs->hasBilevel))
        && (lhs->isGray == rhs->isGray)
        && lhs->brightness == rhs->brightness
        && (lhs->hasColorKey && rhs->hasColorKey && lhs->colorKey == rhs->colorKey && lhs->tolerance == rhs->tolerance || (!lhs->hasColorKey && !rhs->hasColorKey))
        && (lhs->hasColorMatirx && rhs->hasColorMatirx && lhs->colorMatrix == rhs->colorMatrix) || (!lhs->hasColorMatirx && !rhs->hasColorMatirx)
        && lhs->colorMap == rhs->colorMap
        && lhs->brushColorMap == rhs->brushColorMap
        && lhs->contrast == rhs->contrast
        && (lhs->hasDuotone && rhs->hasDuotone && lhs->duotoneColor1 == rhs->duotoneColor1 && lhs->duotoneColor2 == rhs->duotoneColor2 || (!lhs->hasDuotone && !lhs->hasDuotone))
        && (lhs->hasSubstColor && rhs->hasSubstColor && lhs->substColor == rhs->substColor || (!lhs->hasSubstColor && !rhs->hasSubstColor))
        && (lhs->hasAlpha && rhs->hasAlpha && lhs->alphaValue == rhs->alphaValue || (!lhs->hasAlpha && !rhs->hasAlpha))
        && (lhs->hasShadow && rhs->hasShadow && lhs->shadowLow == rhs->shadowLow && lhs->shadowHight == rhs->shadowHight || (!lhs->hasShadow && !rhs->hasShadow))
        && lhs->effectMode == rhs->effectMode;
}

bool QImageEffects::hasColorKey() const
{
    return d->hasColorKey;
}

bool QImageEffects::hasBilevel() const
{
    return d->hasBilevel;
}

qreal QImageEffects::bilevel() const
{
    Q_ASSERT(d->hasBilevel);

    return (qreal)d->bilevelThreshold / (qreal)QImageEffectsPrivate::base_scale;
}

qreal QImageEffects::brightness() const
{
    return d->brightness;
}

QRgb QImageEffects::colorKey() const
{
    Q_ASSERT(d->hasColorKey);

    return d->colorKey;
}

quint8 QImageEffects::colorKeyTolerance() const
{
    Q_ASSERT(d->hasColorKey);

    return d->tolerance;
}

qreal QImageEffects::contrast() const
{
    return d->contrast;
}

void QImageEffects::setGray(bool b)
{
    detach();

    d->isGray = b;
}

bool QImageEffects::isGray() const
{
    return d->isGray;
}

void QImageEffects::setSubstituteColor(QRgb clr)
{
    detach();

    d->hasSubstColor = true;
    d->substColor = clr;
}

void QImageEffects::unsetSubstituteColor()
{
    detach();

    d->hasSubstColor = false;
}

bool QImageEffects::hasSubstituteColor() const
{
    return d->hasSubstColor;
}

QRgb QImageEffects::substituteColor() const
{
    Q_ASSERT(d->hasSubstColor);

    return d->substColor;
}

void QImageEffects::setRecolor(QRgb clr)
{
    detach();

    d->hasRecolor = true;
    d->recolorValue = clr;
}

void QImageEffects::unsetRecolor()
{
    detach();

    d->hasRecolor = false;
}

bool QImageEffects::hasRecolor() const
{
    return d->hasRecolor;
}

QRgb QImageEffects::recolor() const
{
    Q_ASSERT(d->hasRecolor);

    return d->recolorValue;
}

void QImageEffects::setAlpha(QRgb clr)
{
    detach();

    d->hasAlpha = true;
    d->alphaValue = clr;
}

void QImageEffects::unsetAlpha()
{
    detach();

    d->hasAlpha = false;
}

bool QImageEffects::hasAlpha() const
{
    return d->hasAlpha;
}

QRgb QImageEffects::alpha() const
{
    return d->alphaValue;
}

void QImageEffects::setShadow(quint8 low,quint8 hight)
{
    Q_ASSERT(hight > low);
    detach();

    d->hasShadow = true;
    d->shadowLow = low;
    d->shadowHight = hight;
}

void QImageEffects::unsetShadow()
{
    detach();

    d->hasShadow = false;
}

bool QImageEffects::hasShadow() const
{
    return d->hasShadow;
}

void QImageEffects::getShadow(quint8 &low,quint8 &hight) const
{
    low = d->shadowLow;
    hight = d->shadowHight;
}

void QImageEffects::setEffectMode(EffectMode mode)
{
    detach();

    d->effectMode = mode;
}

QImageEffects::EffectMode  QImageEffects::getEffectMode() const
{
    return (QImageEffects::EffectMode)d->effectMode;
}

/*!
    \internal
    Choose the fastest transform function depending on the CPU's features and
    the other data member.
    transform_sse4() is faster than transform_sse2(), and transform_sse2() is
    faster than transform_cpp(). However, transform_cpp() is the common version.
*/
void QImageEffectsPrivate::setTransformFunc()
{
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    if (!hasColorMatirx) {
        const int *p = colorMatrixInt[0];
        bool sseOverflow = (p[0] < 0 || p[1] < 0 || p[2] < 0 || p[4] < 0 || p[5] < 0 || p[6] < 0
                         || p[8] < 0 || p[9] < 0 || p[10] < 0 || p[12] < 0 || p[13] < 0 || p[14] < 0);
        if (sseOverflow) {
            return;
#if QT_COMPILER_SUPPORTS_HERE(SSE4_1)
        }
        else if (qCpuHasFeature(SSE4_1)) {
            m_pTransformProc = &QImageEffectsPrivate::transform_sse4;
#endif //QT_COMPILER_SUPPORTS_HERE(SSE4_1)
        } else if (qCpuHasFeature(SSE2))
            m_pTransformProc = &QImageEffectsPrivate::transform_sse2;

        __m128i * cast_mtxs = (__m128i*)d.m_mmtxs;
        cast_mtxs[0] = _mm_set_epi16(p[0], p[4], p[1], p[5], p[2], p[6], p[3], p[7]);
        cast_mtxs[1] = _mm_set_epi16(p[8], p[12], p[9], p[13], p[10], p[14], p[11], p[15]);
    }
#endif // QT_COMPILER_SUPPORTS_HERE(SSE2)
}

#if !defined(QT_NO_DATASTREAM)
QDataStream &operator<<(QDataStream &s, const QImageEffects &effects)
{
    const QImageEffects::DataPtr d = effects.const_data_ptr();
    s    << d->hasColorMatirx
        << d->hasColorKey
        << d->hasDuotone
        << d->hasBilevel
        << d->hasSubstColor
        << d->isGray
        << d->hasRecolor
        << d->hasAlpha
        << d->hasShadow
        << d->effectMode
        << d->checkBound
        << d->colorKey
        << d->tolerance
        << d->bilevelThreshold
        << d->duotoneColor1
        << d->duotoneColor2
        << d->substColor
        << d->recolorValue
        << d->alphaValue
        << d->brightness
        << d->contrast
        << d->shadowLow
        << d->shadowHight;

    int colorCount = d->colorMap.size();
    s << colorCount;
    if (colorCount > 0)
        s << d->colorMap;

    colorCount = d->brushColorMap.size();
    s << colorCount;
    if (colorCount > 0)
        s << d->brushColorMap;

    return s;
}

QDataStream &operator>>(QDataStream &s, QImageEffects &effects)
{
    QImageEffects::DataPtr d = effects.data_ptr();
    s    >> d->hasColorMatirx
        >> d->hasColorKey
        >> d->hasDuotone
        >> d->hasBilevel
        >> d->hasSubstColor
        >> d->isGray
        >> d->hasRecolor
        >> d->hasAlpha
        >> d->hasShadow
        >> d->effectMode
        >> d->checkBound
        >> d->colorKey
        >> d->tolerance
        >> d->bilevelThreshold
        >> d->duotoneColor1
        >> d->duotoneColor2
        >> d->substColor
        >> d->recolorValue
        >> d->alphaValue
        >> d->brightness
        >> d->contrast
        >> d->shadowLow
        >> d->shadowHight;

    int colorCount = d->colorMap.size();
    s >> colorCount;
    if (colorCount > 0)
        s >> d->colorMap;

    colorCount = d->brushColorMap.size();
    s >> colorCount;
    if (colorCount > 0)
        s >> d->brushColorMap;

    return s;
}
#endif // !defined(QT_NO_DATASTREAM)

QT_END_NAMESPACE
