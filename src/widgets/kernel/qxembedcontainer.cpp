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

#include "qxembedcontainer.h"
#include <private/qwidget_p.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformwindow.h>
#include <private/qwidgetwindow_p.h>

QT_BEGIN_NAMESPACE
class QXEmbedContainerPrivate : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(QXEmbedContainer)
public:
    QXEmbedContainerPrivate()
        : client(0)
    {
    }

    void notifyClient(QEvent *event);
    static QXEmbedContainerPrivate *get(QWidget *w);
private:
    WId client;
};

QXEmbedContainer::QXEmbedContainer(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(*new QXEmbedContainerPrivate, parent, flags)
{
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
    createWinId();
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    qApp->installEventFilter(this);
    setAcceptDrops(true);
}

QXEmbedContainer::~QXEmbedContainer()
{
}

void QXEmbedContainer::embedClient(WId id)
{
    Q_D(QXEmbedContainer);
    if (id == 0)
        return;
    if (windowHandle()->handle()->embedClient(id)) {
        d->client = id;
        if (window()->isActiveWindow()) {
            QEvent event(QEvent::WindowActivate);
            d->notifyClient(&event);
        }
        if (focusWidget() == this && hasFocus()) {
            QFocusEvent event(QEvent::FocusIn, Qt::TabFocusReason);
            d->notifyClient(&event);
        }
        else {
            QFocusEvent event(QEvent::FocusOut);
            d->notifyClient(&event);
        }
    }
}

WId QXEmbedContainer::clientWinId() const
{
    Q_D(const QXEmbedContainer);
    return d->client;
}

void QXEmbedContainer::discardClient()
{
    Q_D(QXEmbedContainer);
    if (!d->client)
        return;
    auto nativaHandle = windowHandle() ? windowHandle()->handle() : nullptr;
    if (nativaHandle)
        nativaHandle->discardClient();
    d->client = 0;
}

bool QXEmbedContainer::eventFilter(QObject *obj, QEvent *event)
{
    Q_D(QXEmbedContainer);
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        if (obj == this && d->client) {
            d->notifyClient(event);
            return true;
        }
        break;
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        if (obj == window() && d->client)
            d->notifyClient(event);
        break;
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        if (obj == this && d->client)
            d->notifyClient(event);
        break;
    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

void QXEmbedContainer::paintEvent(QPaintEvent*)
{
}

void QXEmbedContainerPrivate::notifyClient(QEvent *event)
{
    auto nativeHandle = windowHandle() ? windowHandle()->handle() : nullptr;
    if (nativeHandle)
        nativeHandle->windowEvent(event);
}

QXEmbedContainerPrivate* QXEmbedContainerPrivate::get(QWidget *w)
{
    QXEmbedContainer *xec = qobject_cast<QXEmbedContainer *>(w);
    if (xec)
        return xec->d_func();
    return nullptr;
}
QT_END_NAMESPACE
