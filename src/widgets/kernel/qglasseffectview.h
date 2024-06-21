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

#ifndef QGLASSEFFECTVIEW_H
#define QGLASSEFFECTVIEW_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtGui/qwindowdefs.h>
#include <QtCore/qobject.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qglobal.h>

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtGui/qwindowdefs.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QGlassEffectViewPrivate;

class Q_WIDGETS_EXPORT QGlassEffectView : public QObject
{
public:
    enum Material {
        Titlebar,
        Selection,
        Menu,
        Popover,
        Sidebar,
        HeaderView,
        Sheet,
        WindowBackground,
        HUDWindow,
        FullScreenUI,
        ToolTip,
        ContentBackground,
        UnderWindowBackground,
        UnderPageBackground,
        Alert,
        AlertForApplicationModal,
        AlertForWindowModal,
    };

    enum BlendingMode { BehindWindow, WithinWindow };

    enum State { FollowsWindowActiveState, Active, Inactive };

    enum CornerMask {
        All = 0x1 << 0,
        TopLeft = 0x1 << 1,
        TopRight = 0x1 << 2,
        BottomLeft = 0x1 << 3,
        BottomRight = 0x1 << 4
    };
    Q_DECLARE_FLAGS(CornerMasks, CornerMask)

    enum ViewOrdering { Above, Below };

    Q_DECLARE_PRIVATE(QGlassEffectView)
public:
    //based on current window ,create glass effectview;
    explicit QGlassEffectView(QWidget *hostwnd);
    QGlassEffectView(QWidget *hostwnd, int radius, QGlassEffectView::Material mtl);
    //based on existed glassview，create a new glassview on it
    explicit QGlassEffectView(QGlassEffectView *hostView, QGlassEffectView::ViewOrdering order = QGlassEffectView::Above);
    QGlassEffectView(QGlassEffectView *hostView, int radius, QGlassEffectView::Material mtl, QGlassEffectView::ViewOrdering order = QGlassEffectView::Above);
    ~QGlassEffectView();

    QWidget *getHostWnd();
    QGlassEffectViewPrivate *getViewData();

    void setFrame(QRect rect);
    void setMaterial(QGlassEffectView::Material material);
    void setBlendingMode(QGlassEffectView::BlendingMode node);
    void setState(QGlassEffectView::State state);
    void setColor(QColor clr);
    void setBackgroundColor(QColor clr);
    void setAlpha(float value);
    void setRoundCorner(int roundRadius,
                        QGlassEffectView::CornerMasks mask = QGlassEffectView::CornerMask::All);

    void setVisible(bool isVisble);
    static QColor getSystemAccentColor();
    static QColor getMenuSelectedTextColor();

    void loadGlassEffectView();
public Q_SLOTS:
    void onGlassEffectStateChanged(bool bEnabled);

private:
    QScopedPointer<QGlassEffectViewPrivate> d_ptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QGlassEffectView::CornerMasks)
QT_END_NAMESPACE
#endif
