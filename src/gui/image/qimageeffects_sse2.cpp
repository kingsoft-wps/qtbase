
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
#include "qimage_p.h"
#include <private/qsimd_p.h>
#include <private/qdrawhelper_p.h>
#include <private/qdrawingprimitive_sse2_p.h>

#if QT_COMPILER_SUPPORTS_HERE(SSE2)

QT_BEGIN_NAMESPACE

void __qt_setbilevel_sse(uint *buffer, int length, const quint16 percent)
{
    __m128i *pSrc = reinterpret_cast<__m128i*>(buffer);
    __m128i *pEnd = pSrc + length / 4;
    const __m128i alphaMask = _mm_set1_epi32(0xff000000);
    const __m128i redMask = _mm_set1_epi32(0x000000ff);
    const __m128i mpercent = _mm_set1_epi32(percent);

    if (percent == 0) {
        for (; pSrc < pEnd; pSrc ++) {
            const __m128i msrc = _mm_loadu_si128(pSrc);
            const __m128i malpha = _mm_and_si128(msrc, alphaMask);
            __m128i mwhite = _mm_or_si128(malpha, _mm_srli_epi32(malpha, 8));
            mwhite = _mm_or_si128(mwhite, _mm_srli_epi32(mwhite, 16));
            _mm_storeu_si128(pSrc, mwhite);
        }
    } else {
        for (; pSrc < pEnd; pSrc ++) {
            const __m128i msrc = _mm_loadu_si128(pSrc);
            const __m128i malpha = _mm_and_si128(msrc, alphaMask);
            __m128i mlhs = _mm_and_si128(msrc, redMask);
            mlhs = _mm_slli_epi32(mlhs, QImageEffectsPrivate::base_shift);
            __m128i mrhs = _mm_srli_epi32(malpha, 24);
            mrhs = _mm_mullo_epi16(mrhs, mpercent);
            __m128i result = _mm_cmpgt_epi32(mlhs, mrhs);
            result = _mm_and_si128(result, _mm_srli_epi32(malpha, 24));
            result = _mm_or_si128(result, _mm_or_si128(_mm_slli_epi32(result, 8), _mm_slli_epi32(result, 16)));
            _mm_storeu_si128(pSrc, _mm_or_si128(malpha, result));
        }
    }
}

void qt_setbilevel_sse(uint *buffer, int length, const quint16 percent)
{
    __qt_setbilevel_sse(buffer, length, percent);

    switch (length % 4) {
    case 3: qt_setbilevel(buffer[--length], percent);
    case 2: qt_setbilevel(buffer[--length], percent);
    case 1: qt_setbilevel(buffer[--length], percent);
    }
}

bool convert_ARGB_to_ARGB_PM_inplace_sse2(QImageData *data, Qt::ImageConversionFlags)
{
    Q_ASSERT(data->format == QImage::Format_ARGB32);

    // extra pixels on each line
    const int spare = data->width & 3;
    // width in pixels of the pad at the end of each line
    const int pad = (data->bytes_per_line >> 2) - data->width;
    const int iter = data->width >> 2;
    int height = data->height;

    const __m128i alphaMask = _mm_set1_epi32(0xff000000);
    const __m128i nullVector = _mm_setzero_si128();
    const __m128i half = _mm_set1_epi16(0x80);
    const __m128i colorMask = _mm_set1_epi32(0x00ff00ff);

    __m128i *d = reinterpret_cast<__m128i*>(data->data);
    while (height--) {
        const __m128i *end = d + iter;

        for (; d != end; ++d) {
            const __m128i srcVector = _mm_loadu_si128(d);
            const __m128i srcVectorAlpha = _mm_and_si128(srcVector, alphaMask);
            if (_mm_movemask_epi8(_mm_cmpeq_epi32(srcVectorAlpha, alphaMask)) == 0xffff) {
                // opaque, data is unchanged
            } else if (_mm_movemask_epi8(_mm_cmpeq_epi32(srcVectorAlpha, nullVector)) == 0xffff) {
                // fully transparent
                _mm_storeu_si128(d, nullVector);
            } else {
                __m128i alphaChannel = _mm_srli_epi32(srcVector, 24);
                alphaChannel = _mm_or_si128(alphaChannel, _mm_slli_epi32(alphaChannel, 16));

                __m128i result;
                BYTE_MUL_SSE2(result, srcVector, alphaChannel, colorMask, half);
                result = _mm_or_si128(_mm_andnot_si128(alphaMask, result), srcVectorAlpha);
                _mm_storeu_si128(d, result);
            }
        }

        QRgb *p = reinterpret_cast<QRgb*>(d);
        QRgb *pe = p+spare;
        for (; p != pe; ++p) {
            if (*p < 0x00ffffff)
                *p = 0;
            else if (*p < 0xff000000)
                *p = qPremultiply(*p);
        }

        d = reinterpret_cast<__m128i*>(p+pad);
    }

    data->format = QImage::Format_ARGB32_Premultiplied;
    return true;
}

//
// QM128Data
//
QM128Data::QM128Data()
{
#if defined(Q_OS_WIN)
    m_mmtxs = (__m128i*)_aligned_malloc(sizeof(__m128i) * 2, 16);
#else
    m_mmtxs = _mm_malloc(sizeof(__m128i) * 2, 16);
#endif

    __m128i* temp128i = (__m128i*)m_mmtxs;
    temp128i[0] = _mm_set1_epi16(0);
    temp128i[1] = _mm_set1_epi16(0);
}

QM128Data::QM128Data(const QM128Data &rhs)
{
#if defined(Q_OS_WIN)
    m_mmtxs = (__m128i*)_aligned_malloc(sizeof(__m128i) * 2, 16);
#else
    m_mmtxs = _mm_malloc(sizeof(__m128i) * 2, 16);
#endif

    __m128i* dest = (__m128i*)m_mmtxs;
    const __m128i* source = (const __m128i*)rhs.m_mmtxs;
    dest[0] = source[0];
    dest[1] = source[1];
}

QM128Data::~QM128Data()
{
#if defined(Q_OS_WIN)
    _aligned_free(m_mmtxs);
#else
    _mm_free(m_mmtxs);
#endif
}

QM128Data& QM128Data::operator=(const QM128Data &rhs)
{
    __m128i* dest = (__m128i*)m_mmtxs;
    const __m128i* source = (const __m128i*)rhs.m_mmtxs;
    dest[0] = source[0];
    dest[1] = source[1];

    return *this;
}

void* QM128Data::operator new(size_t size)
{
#if defined(Q_OS_WIN)
    return _aligned_malloc(size, 16);
#else
    return _mm_malloc(size, 16);
#endif
}

void QM128Data::operator delete(void *p)
{
    QM128Data *pc = static_cast<QM128Data*>(p);
#if defined(Q_OS_WIN)
    _aligned_free(pc);
#else
    _mm_free(pc);
#endif
}


/*!
    \fn void QImageEffectsData::transform_sse2(QRgb &rgb) const
    Transform the rgb value use the color matrix.
    Note: The alpha channel is unchanged.
    sa\ transform_cpp(), transform_sse4()
*/
void QImageEffectsPrivate::transform_sse2(QRgb &rgb) const
{
    Q_ASSERT(qCpuHasFeature(SSE2));

    quint8 *pb = (quint8*)&rgb;
    __m128i rg = _mm_set1_epi32((pb[2] << 16) | pb[1]);
    __m128i ba = _mm_set1_epi32((pb[0] << 16) | pb[3]);

    __m128i* m128temp = (__m128i*)d.m_mmtxs;
    __m128i mrg = _mm_madd_epi16(rg, m128temp[0]);
    __m128i mba = _mm_madd_epi16(ba, m128temp[1]);
    __m128i mrgba = _mm_add_epi32(mrg, mba);
    mrgba = _mm_srai_epi32(mrgba, 8);

    qint32* cast_rgba = (qint32*)&mrgba;

    if (checkBound) {
        pb[2] = qBound<int>(0, cast_rgba[3], pb[3]);
        pb[1] = qBound<int>(0, cast_rgba[2], pb[3]);
        pb[0] = qBound<int>(0, cast_rgba[1], pb[3]);
    } else {
        pb[2] = cast_rgba[3];
        pb[1] = cast_rgba[2];
        pb[0] = cast_rgba[1];
    }
}

/*!
    \overload
*/
void QImageEffectsPrivate::transform_sse2(QRgb *buffer, int length) const
{
    Q_ASSERT(qCpuHasFeature(SSE2));

    const int *p = colorMatrixInt[0];
    __m128i *pSrc = reinterpret_cast<__m128i*>(buffer);
    __m128i *pEnd = pSrc + length / 4;
    const __m128i alphaMask = _mm_set1_epi32(0xff000000);
    const __m128i colorMask = _mm_set1_epi32(0x00ff00ff);
    __m128i mmtxr1 = _mm_set1_epi32((p[0] << 16) | p[8]);
    __m128i mmtxr2 = _mm_set1_epi32((p[12] << 16) | p[4]);
    __m128i mmtxg1 = _mm_set1_epi32((p[1] << 16) | p[9]);
    __m128i mmtxg2 = _mm_set1_epi32((p[13] << 16) | p[5]);
    __m128i mmtxb1 = _mm_set1_epi32((p[2] << 16) | p[10]);
    __m128i mmtxb2 = _mm_set1_epi32((p[14] << 16) | p[6]);

    for (; pSrc < pEnd; pSrc++) {
        const __m128i msrc = _mm_loadu_si128(pSrc);
        const __m128i mrb = _mm_and_si128(msrc, colorMask);
        const __m128i mag = _mm_srli_epi16(msrc, 8);
        const __m128i mas = _mm_and_si128(msrc, alphaMask);
        const __m128i upperbound = _mm_srli_epi32(mas, 24);

        //red channel
        const __m128i mr1 = _mm_madd_epi16(mrb, mmtxr1);
        const __m128i mr2 = _mm_madd_epi16(mag, mmtxr2);
        __m128i mrs = _mm_add_epi32(mr1, mr2);
        mrs = _mm_srai_epi32(mrs, 8);

        //green channel
        const __m128i mg1 = _mm_madd_epi16(mrb, mmtxg1);
        const __m128i mg2 = _mm_madd_epi16(mag, mmtxg2);
        __m128i mgs = _mm_add_epi32(mg1, mg2);
        mgs = _mm_srai_epi32(mgs, 8);

        //blue channel
        const __m128i mb1 = _mm_madd_epi16(mrb, mmtxb1);
        const __m128i mb2 = _mm_madd_epi16(mag, mmtxb2);
        __m128i mbs = _mm_add_epi32(mb1, mb2);
        mbs = _mm_srai_epi32(mbs, 8);

        qint32* cast_mrs = (qint32*)&mrs;
        qint32* cast_mgs = (qint32*)&mgs;
        qint32* cast_mbs = (qint32*)&mbs;
        qint32* cast_upperbound = (qint32*)&upperbound;

        if (checkBound) {
            cast_mrs[3] = qBound(0, cast_mrs[3], cast_upperbound[3]);
            cast_mrs[2] = qBound(0, cast_mrs[2], cast_upperbound[2]);
            cast_mrs[1] = qBound(0, cast_mrs[1], cast_upperbound[1]);
            cast_mrs[0] = qBound(0, cast_mrs[0], cast_upperbound[0]);

            cast_mgs[3] = qBound(0, cast_mgs[3], cast_upperbound[3]);
            cast_mgs[2] = qBound(0, cast_mgs[2], cast_upperbound[2]);
            cast_mgs[1] = qBound(0, cast_mgs[1], cast_upperbound[1]);
            cast_mgs[0] = qBound(0, cast_mgs[0], cast_upperbound[0]);

            cast_mbs[3] = qBound(0, cast_mbs[3], cast_upperbound[3]);
            cast_mbs[2] = qBound(0, cast_mbs[2], cast_upperbound[2]);
            cast_mbs[1] = qBound(0, cast_mbs[1], cast_upperbound[1]);
            cast_mbs[0] = qBound(0, cast_mbs[0], cast_upperbound[0]);
        }

        mrs = _mm_slli_epi32(mrs, 16);
        mgs = _mm_slli_epi32(mgs, 8);

        _mm_storeu_si128(pSrc, _mm_or_si128(_mm_or_si128(mas, mrs), _mm_or_si128(mgs, mbs)));
    }

    switch (length % 4) {
    case 3: transform_sse2(buffer[--length]);
    case 2: transform_sse2(buffer[--length]);
    case 1: transform_sse2(buffer[--length]);
    }
}

QT_END_NAMESPACE

#endif // QT_HAVE_SSE2
