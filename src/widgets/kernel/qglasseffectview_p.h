/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QGLASSEFFECTVIEW_P_H
#define QGLASSEFFECTVIEW_P_H

#include <QWidget>
#include "qglasseffectview.h"
#include <Cocoa/Cocoa.h>

QT_BEGIN_NAMESPACE

class QGlassEffectViewPrivate : public QObject
{
public:
    QGlassEffectViewPrivate(QWidget* hostWnd);
    QGlassEffectViewPrivate(QGlassEffectViewPrivate* hostView, QGlassEffectView::ViewOrdering order);
    QGlassEffectViewPrivate(QWidget* hostwnd, int radius, QGlassEffectView::Material mtl);
    QGlassEffectViewPrivate(QGlassEffectViewPrivate* hostView, int radius, QGlassEffectView::Material mtl, QGlassEffectView::ViewOrdering order);
    ~QGlassEffectViewPrivate();

    NSVisualEffectView* getView(){return m_glassView;}
    QWidget* getHostWnd(){return m_hostWnd;}
    
    void setFrame(QRect rec);
    void setMaterial(QGlassEffectView::Material material);
    void setBlendingMode(QGlassEffectView::BlendingMode mode);
    void setState(QGlassEffectView::State state);
    void setColor(QColor clr);
    void setBackgroundColor(QColor clr);
    void setAlpha(float value);
    void setRoundCorner(int roundRadius, QGlassEffectView::CornerMasks mask = QGlassEffectView::CornerMask::All);

    void setVisible(bool isVisible);
    void onBorderColorChanged();
    void loadGlassEffectView();
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void init();
    void initWithView(QGlassEffectView::ViewOrdering order);

private:
    NSVisualEffectView* m_glassView; //glass view
    NSView* m_superView; //host view
    QWidget* m_hostWnd; //host wind
};

QT_END_NAMESPACE
#endif

