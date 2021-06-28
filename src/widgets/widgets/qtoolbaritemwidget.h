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


#ifndef QTOOLBARITEMWIDGET_H
#define QTOOLBARITEMWIDGET_H

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtWidgets/qwidget.h>

QT_REQUIRE_CONFIG(toolbar);

QT_BEGIN_NAMESPACE

class Q_WIDGETS_EXPORT QToolBarItemWidget
{
public:
    virtual ~QToolBarItemWidget() = 0;
    virtual Qt::Orientation orientation() const = 0;
    virtual void setOrientation(Qt::Orientation o) = 0;
    virtual QSize sizeHint(Qt::Orientation o) const = 0;
    virtual QSize minimumSize(Qt::Orientation o) const = 0;
    virtual QSize maximumSize(Qt::Orientation o) const = 0;
    virtual QSize minimumSizeHint(Qt::Orientation o) const = 0;
};

inline QToolBarItemWidget::~QToolBarItemWidget() {}

QT_END_NAMESPACE
#endif // QTOOLBARITEMWIDGET_H
