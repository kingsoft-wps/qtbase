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
#ifndef QIMAGEEFFECTS_P_H
#define QIMAGEEFFECTS_P_H

#include <QtGui/private/qtguiglobal_p.h>
#include <QMatrix4x4>
#include <private/qsimd_p.h>

QT_BEGIN_NAMESPACE

#if QT_COMPILER_SUPPORTS_HERE(SSE2)
struct Q_GUI_EXPORT QM128Data {
    void *m_mmtxs;

    QM128Data();

    QM128Data(const QM128Data &rhs);

    ~QM128Data();

    QM128Data& operator=(const QM128Data &rhs);

    void* operator new(size_t size);

    void operator delete(void *p);
};
#endif // QT_COMPILER_SUPPORTS_HERE(SSE2)

typedef QMap<QRgb, QRgb> ColorMap;

class Q_GUI_EXPORT QImageEffectsPrivate
{
public:
    enum base_scale_e
    {
        base_shift = 8,
        base_scale = 1 << base_shift,
    };

    QImageEffectsPrivate();
    QImageEffectsPrivate(const QImageEffectsPrivate &);
    ~QImageEffectsPrivate();
    QImageEffectsPrivate &operator=(const QImageEffectsPrivate &);

    bool isTransparent() const;
    void updateColorMatrixInt();
    void updateColorMatrix();
    void updateColorMatrix_mode2();
    void updateBrightContrastParas();
    void prepare();
    void prepare_mode2();
    void transformEffects(uint* buffer, int length, bool ignoreColorKey = false) const;
    void transformMatrixAndBilevel(uint* buffer, int length) const;
    void transformMatrixAndBilevel_old(uint* buffer, int length) const;
    void handleColorKey(uint *buffer, int length) const;
    void handleColorMap(uint *buffer, int length) const;
    void handleBrightContrastTransfrom(uint *buffer, int length) const;
    void handleShadowTransform(uint *buffer, int length) const;
    void transform(QRgb *buffer, int length) const;
    void transform_cpp(QRgb &rgb) const;
    void transform_cpp(QRgb *buffer, int length) const;
    void setTransformFunc();

    QMatrix4x4 createDuotoneMatrix(const QRgb clr1, const QRgb clr2, const qreal gray_r, const qreal gray_g, const qreal gray_b) const;
    void resetState();

    QAtomicInt ref;
    bool hasColorMatirx;
    bool hasColorKey;
    bool hasDuotone;
    bool hasBilevel;
    bool hasSubstColor;
    bool isGray;
    bool hasRecolor;
    bool hasAlpha;
    bool hasShadow;
    bool checkBound;
    quint32 effectMode;

    QMatrix4x4 colorMatrix;
    int colorMatrixInt[4][4];
    QRgb colorKey;
    ColorMap colorMap;
    ColorMap brushColorMap;
    int brightContrastParas[2];
    quint8 tolerance;
    quint8 colorKeyLow[4];
    quint8 colorKeyHight[4];
    quint16 bilevelThreshold;
    QRgb duotoneColor1;
    QRgb duotoneColor2;
    QRgb substColor;
    QRgb recolorValue;
    QRgb alphaValue;
    qreal brightness;
    qreal contrast;
    quint8 shadowLow;
    quint8 shadowHight;

    typedef void (QImageEffectsPrivate::*TransformProc)(QRgb *buffer, int length) const;
    TransformProc m_pTransformProc;
    typedef bool (*InToleranceFunc)(const QRgb &clr, const quint8 low[4], const quint8 hight[4]);
    InToleranceFunc m_pInToleranceFunc;
    typedef void (QImageEffectsPrivate::*TransformMatrixAndBilevelFunc)(uint* buffer, int length) const;
    TransformMatrixAndBilevelFunc m_pTransformMatrixAndBilevel;

#if QT_COMPILER_SUPPORTS_HERE(SSE2)
    QM128Data d;
    void transform_sse2(QRgb &rgb) const;
    void transform_sse2(QRgb *buffer, int length) const;
#endif

#if QT_COMPILER_SUPPORTS_HERE(SSE4_1)
    void transform_sse4(QRgb &rgb) const;
    void transform_sse4(QRgb *buffer, int length) const;
#endif

private:
    void init(const QImageEffectsPrivate &);
    friend class QRasterPaintEngine;
};

void qt_setbilevel(uint &rgb, const quint16 percent);
#if QT_COMPILER_SUPPORTS_HERE(SSE2)
void qt_setbilevel_sse(uint *buffer, int length, const quint16 percent);
#endif // QT_COMPILER_SUPPORTS_HERE(SSE2)

QT_END_NAMESPACE

#endif // QIMAGEEFFECTSP_H
