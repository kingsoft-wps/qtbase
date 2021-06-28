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

#ifndef QIMAGEEFFECTS_H
#define QIMAGEEFFECTS_H

#include <QtGui/qtguiglobal.h>
#include <QtGui/qrgb.h>

QT_BEGIN_NAMESPACE
class QMatrix4x4;
class QImageEffectsPrivate;

class Q_GUI_EXPORT QImageEffects
{
public:
    enum EffectMode { Mode1, Mode2 };

public:
    QImageEffects();
    QImageEffects(const QImageEffects &);
    ~QImageEffects();
    QImageEffects &operator=(const QImageEffects &);

    void setBilevel(qreal threshold = 0.5);
    void unsetBilevel();
    bool hasBilevel() const;
    qreal bilevel() const;
    void setGray(bool b);
    bool isGray() const;
    void setBrightness(qreal brightness);
    qreal brightness() const;

    void setColorKey(QRgb key, quint8 tolerance = 0);
    void unsetColorKey();
    bool hasColorKey() const;
    QRgb colorKey() const;
    quint8 colorKeyTolerance() const;

    void setRemapTable(const QMap<QRgb, QRgb>& colorMap);
    QMap<QRgb, QRgb> remapTable() const;
	void setBrushRemapTable(const QMap<QRgb, QRgb>& colorMap);
	QMap<QRgb, QRgb> brushRemapTable() const;

    void setColorMatrix(const QMatrix4x4 &mtx);
    QMatrix4x4 colorMatrix() const;
    void unsetColorMatrix();
    void setContrast(qreal contrast);
    qreal contrast() const;

    void setDuotone(QRgb color1, QRgb color2);
    void unsetDuotone();
    bool hasDuotone() const;
    void getDuotone(QRgb &color1, QRgb &color2) const;

    void setSubstituteColor(QRgb clr);
    void unsetSubstituteColor();
    bool hasSubstituteColor() const;
    QRgb substituteColor() const;

    void setRecolor(QRgb clr);
    void unsetRecolor();
    bool hasRecolor() const;
    QRgb recolor() const;

    void setAlpha(QRgb clr);
    void unsetAlpha();
    bool hasAlpha() const;
    QRgb alpha() const;

    void setShadow(quint8 low, quint8 hight);
    void unsetShadow();
    bool hasShadow() const;
    void getShadow(quint8 &low, quint8 &hight) const;

    void setEffectMode(EffectMode mode);
    QImageEffects::EffectMode getEffectMode() const;

    bool hasEffects() const;
    void resetState();

    void makeEffects(uint *buffer, int length);

    void detach();

    bool operator==(const QImageEffects &e) const;
    inline bool operator!=(const QImageEffects &e) const { return !(operator==(e)); }

private:
    mutable QImageEffectsPrivate *d;
    static QImageEffectsPrivate shared_null;
    void clean();
public:
    typedef QImageEffectsPrivate * DataPtr;
    DataPtr data_ptr() const;
    DataPtr const_data_ptr() const { return d; }
};

#if !defined(QT_NO_DATASTREAM)
Q_GUI_EXPORT QDataStream &operator<<(QDataStream &, const QImageEffects &);
Q_GUI_EXPORT QDataStream &operator>>(QDataStream &, QImageEffects &);
#endif


QT_END_NAMESPACE

#endif // QIMAGEEFFECTS_H
