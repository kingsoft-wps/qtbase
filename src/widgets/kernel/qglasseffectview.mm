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

#include "qglasseffectview.h"
#include "qglasseffectview_p.h"
#include <QWidget>
#include <QEvent>
#include "qdebug.h"
#include <private/qstylehelper_p.h>
#include <QtGui/private/qcoregraphics_p.h>

QT_BEGIN_NAMESPACE
QGlassEffectView::QGlassEffectView(QWidget* hostWnd)
    : QObject(hostWnd)
    , d_ptr(new QGlassEffectViewPrivate(hostWnd))
{
    hostWnd->setGlassView(this);
    connect(qApp, &QApplication::glassEffectStateChanged, this, &QGlassEffectView::onGlassEffectStateChanged);
    onGlassEffectStateChanged(qApp->usingGlassEffect());
}

QGlassEffectView::QGlassEffectView(QGlassEffectView* hostView, QGlassEffectView::ViewOrdering order)
    : QObject(hostView)
    , d_ptr(new QGlassEffectViewPrivate(hostView->getViewData(), order))
{
    d_ptr->setState(QGlassEffectView::State::Active);
    connect(qApp, &QApplication::glassEffectStateChanged, this, &QGlassEffectView::onGlassEffectStateChanged);
    onGlassEffectStateChanged(qApp->usingGlassEffect());
}

QGlassEffectView::QGlassEffectView(QWidget* hostWnd, int radius, QGlassEffectView::Material mtl)
    : QObject(hostWnd)
    , d_ptr(new QGlassEffectViewPrivate(hostWnd, radius, mtl))
{
    hostWnd->setGlassView(this);
    d_ptr->setState(QGlassEffectView::State::Active);
    connect(qApp, &QApplication::glassEffectStateChanged, this, &QGlassEffectView::onGlassEffectStateChanged);
    onGlassEffectStateChanged(qApp->usingGlassEffect());
}

QGlassEffectView::QGlassEffectView(QGlassEffectView* hostView, int radius, QGlassEffectView::Material mtl, QGlassEffectView::ViewOrdering order)
    : QObject(hostView)
    , d_ptr(new QGlassEffectViewPrivate(hostView->getViewData(), radius, mtl, order))
{
    d_ptr->setState(QGlassEffectView::State::Active);
    connect(qApp, &QApplication::glassEffectStateChanged, this, &QGlassEffectView::onGlassEffectStateChanged);
    onGlassEffectStateChanged(qApp->usingGlassEffect());
}

QGlassEffectView::~QGlassEffectView()
{
}

QWidget* QGlassEffectView::getHostWnd()
{
    Q_D(QGlassEffectView);
    return d->getHostWnd();
}
 
QGlassEffectViewPrivate* QGlassEffectView::getViewData()
{
    Q_D(QGlassEffectView);
    return d;
}

void QGlassEffectView::setFrame(QRect rect)
{
    Q_D(QGlassEffectView);
    d->setFrame(rect);
}

void QGlassEffectView::setMaterial(QGlassEffectView::Material material)
{
    Q_D(QGlassEffectView);
    d->setMaterial(material);
}


void QGlassEffectView::setBlendingMode(QGlassEffectView::BlendingMode mode)
{
    Q_D(QGlassEffectView);
    d->setBlendingMode(mode);
}

void QGlassEffectView::setState(QGlassEffectView::State state)
{
    Q_D(QGlassEffectView);
    d->setState(state);
}

void QGlassEffectView::setColor(QColor clr)
{
    Q_D(QGlassEffectView);
    d->setColor(clr);
}

void QGlassEffectView::setBackgroundColor(QColor clr)
{
    Q_D(QGlassEffectView);
    d->setBackgroundColor(clr);
}

void QGlassEffectView::setAlpha(float value)
{
    Q_D(QGlassEffectView);
    d->setAlpha(value);
}

void QGlassEffectView::setRoundCorner(int roundRadius, QGlassEffectView::CornerMasks mask)
{
    Q_D(QGlassEffectView);
    d->setRoundCorner(roundRadius, mask);
}

void QGlassEffectView::setVisible(bool isVisible)
{
    Q_D(QGlassEffectView);
    d->setVisible(isVisible);
}

QColor QGlassEffectView::getSystemAccentColor()
{
    NSColor *selectedMenuItemColor = nil;
    if (__builtin_available(macOS 10.14, *)) {
        selectedMenuItemColor = [[NSColor selectedContentBackgroundColor] highlightWithLevel:0.13];
    } else {
        selectedMenuItemColor = [NSColor keyboardFocusIndicatorColor];
    }
    return qt_mac_toQColor(selectedMenuItemColor);
}

QColor QGlassEffectView::getMenuSelectedTextColor()
{
    return qt_mac_toQColor([NSColor selectedMenuItemTextColor]);
}

void QGlassEffectView::loadGlassEffectView()
{
    Q_D(QGlassEffectView);
    d->loadGlassEffectView();
}

void QGlassEffectView::onGlassEffectStateChanged(bool bEnabled)
{
    Q_D(QGlassEffectView);
    if (bEnabled)
        d->setState(QGlassEffectView::State::Active);
    else
        d->setState(QGlassEffectView::State::Inactive);
}


QGlassEffectViewPrivate::QGlassEffectViewPrivate(QWidget* hostWnd)
    : m_hostWnd(hostWnd)
    , m_glassView(nullptr)
    , m_superView(nullptr)
{
    init();
    if (qApp)
        qApp->installEventFilter(this);
}

QGlassEffectViewPrivate::QGlassEffectViewPrivate(QGlassEffectViewPrivate* hostView, QGlassEffectView::ViewOrdering order)
    : m_hostWnd(nullptr)
    , m_glassView(nullptr)
    , m_superView(nullptr)
{
    m_superView = hostView->getView();
    initWithView(order);
    if(m_hostWnd)
        m_hostWnd->installEventFilter(this);
    if (qApp)
        qApp->installEventFilter(this);
}

QGlassEffectViewPrivate::QGlassEffectViewPrivate(QWidget* hostwnd, int radius, QGlassEffectView::Material mtl)
    : m_hostWnd(hostwnd)
    , m_glassView(nullptr)
    , m_superView(nullptr)
{
    init();
    setRoundCorner(radius);
    setMaterial(mtl);
    if(m_hostWnd)
        m_hostWnd->installEventFilter(this);
    if (qApp)
        qApp->installEventFilter(this);
}

QGlassEffectViewPrivate::QGlassEffectViewPrivate(QGlassEffectViewPrivate* hostView, int radius, QGlassEffectView::Material mtl, QGlassEffectView::ViewOrdering order)
    : m_hostWnd(nullptr)
    , m_glassView(nullptr)
    , m_superView(nullptr)
{
    m_superView = hostView->getView();
    initWithView(order);
    setMaterial(mtl);
    setRoundCorner(radius);
    if(m_hostWnd)
        m_hostWnd->installEventFilter(this);
    if (qApp)
        qApp->installEventFilter(this);
}

QGlassEffectViewPrivate::~QGlassEffectViewPrivate()
{
    if(m_glassView)
    {
        [m_glassView release];
        m_glassView = nullptr;
    }
    if (m_hostWnd)
        m_hostWnd->removeEventFilter(this);
    if (qApp)
        qApp->removeEventFilter(this);
}

bool QGlassEffectViewPrivate::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QAppThemeChangeEvent::AppThemeChange && watched == qApp)
    {
        onBorderColorChanged();
        return false;
    }
    if (m_hostWnd == watched && event->type() == QEvent::Resize)
    {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (!w)
            return false;
        
        QRect rect = w->rect();
        setFrame(rect);
    }
    if (m_hostWnd == watched && (event->type() == QEvent::Polish || event->type() == QEvent::WindowActivate))
        loadGlassEffectView();

    return false;
}

void QGlassEffectViewPrivate::init()
{
    loadGlassEffectView();
    setState(QGlassEffectView::State::Active);
    setBlendingMode(QGlassEffectView::BlendingMode::BehindWindow);
}

void QGlassEffectViewPrivate::initWithView(QGlassEffectView::ViewOrdering order)
{
    if (!m_superView)
        return;

    if (!m_glassView)
        m_glassView  = [[NSVisualEffectView alloc] initWithFrame:m_superView.frame];
    if (QGlassEffectView::ViewOrdering::Above == order)
        [m_superView.superview addSubview:m_glassView positioned:NSWindowAbove relativeTo:m_superView];
    else if (QGlassEffectView::ViewOrdering::Below == order)
    {
        [m_superView.superview addSubview:m_glassView positioned:NSWindowBelow relativeTo:m_superView];
    }
    else
    {}
    setState(QGlassEffectView::State::Active);
    setBlendingMode(QGlassEffectView::BlendingMode::BehindWindow);
}

void QGlassEffectViewPrivate::setMaterial(QGlassEffectView::Material material)
{
    if (!m_glassView)
        return ;
    if (__builtin_available(macOS 10.14, *))
    {
        NSVisualEffectMaterial mtl = NSVisualEffectMaterialTitlebar;
        switch(material)
        {
            case QGlassEffectView::Material::Selection :
                mtl = NSVisualEffectMaterialSelection;
                break;
            case QGlassEffectView::Material::Menu :
                mtl = NSVisualEffectMaterialMenu;
                break;
            case QGlassEffectView::Material::Popover :
                mtl = NSVisualEffectMaterialPopover;
                break;
            case QGlassEffectView::Material::Sidebar :
                mtl = NSVisualEffectMaterialSidebar;
                break;
            case QGlassEffectView::Material::HeaderView :
                mtl = NSVisualEffectMaterialHeaderView;
                break;
            case QGlassEffectView::Material::Sheet :
                mtl = NSVisualEffectMaterialSheet;
                break;
            case QGlassEffectView::Material::WindowBackground :
                mtl = NSVisualEffectMaterialWindowBackground;
                break;
            case QGlassEffectView::Material::HUDWindow :
                mtl = NSVisualEffectMaterialHUDWindow;
                break;
            case QGlassEffectView::Material::FullScreenUI :
                mtl = NSVisualEffectMaterialFullScreenUI;
                break;
            case QGlassEffectView::Material::ToolTip :
                mtl = NSVisualEffectMaterialToolTip;
                break;
            case QGlassEffectView::Material::ContentBackground :
                mtl = NSVisualEffectMaterialContentBackground;
                break;
            case QGlassEffectView::Material::UnderWindowBackground :
                mtl = NSVisualEffectMaterialUnderWindowBackground;
                break;
            case QGlassEffectView::Material::UnderPageBackground :
                mtl = NSVisualEffectMaterialUnderPageBackground;
                break;
            case QGlassEffectView::Material::Alert :
            case QGlassEffectView::Material::AlertForApplicationModal :
                if (__builtin_available(macOS 11.0, *))
                    mtl = (NSVisualEffectMaterial)31;
                else
                    mtl = NSVisualEffectMaterialSheet;
                break;
            case QGlassEffectView::Material::AlertForWindowModal :
                if (__builtin_available(macOS 11.0, *))
                    mtl = (NSVisualEffectMaterial)33;
                else
                    mtl = NSVisualEffectMaterialSheet;
                break;
            default :
                mtl = NSVisualEffectMaterialTitlebar;
        }
        m_glassView.material = mtl;
    }
}

void QGlassEffectViewPrivate::setFrame(QRect rec)
{
    if (!m_glassView)
        return ;
    NSRect rect = NSMakeRect(rec.x(),rec.y(),rec.width(),rec.height());
    [m_glassView setFrame: rect];
}

void QGlassEffectViewPrivate::setBlendingMode(QGlassEffectView::BlendingMode mode)
{
    NSVisualEffectBlendingMode md;

    if (mode == QGlassEffectView::BlendingMode::BehindWindow)
        md = NSVisualEffectBlendingModeBehindWindow;
    else
        md = NSVisualEffectBlendingModeWithinWindow;

    m_glassView.blendingMode = md;
}

void QGlassEffectViewPrivate::setState(QGlassEffectView::State state)
{
    NSVisualEffectState st;

    if (state == QGlassEffectView::State::Active)
        st = NSVisualEffectStateActive;
    else if (state == QGlassEffectView::State::Inactive)
        st = NSVisualEffectStateInactive;
    else
        st = NSVisualEffectStateFollowsWindowActiveState;

    m_glassView.state = st;
}

void QGlassEffectViewPrivate::setColor(QColor clr)
{
    //only when blendingmode is equaled to WithinWindow, color can be set successfully
    setBlendingMode(QGlassEffectView::BlendingMode::WithinWindow);
    
    m_glassView.wantsLayer = YES;
    m_glassView.layer.backgroundColor = [NSColor colorWithRed:clr.redF() green:clr.greenF()  blue:clr.blueF()  alpha:clr.alphaF()].CGColor;
}

void QGlassEffectViewPrivate::setBackgroundColor(QColor clr)
{
    if (!m_superView)
        return;

    m_superView.wantsLayer = YES;;
    m_superView.layer.backgroundColor = [NSColor colorWithRed:clr.redF() green:clr.greenF()  blue:clr.blueF()  alpha:clr.alphaF()].CGColor;
}

void QGlassEffectViewPrivate::setAlpha(float value)
{
    m_glassView.alphaValue = value;
}

void QGlassEffectViewPrivate::setRoundCorner(int roundRadius, QGlassEffectView::CornerMasks mask)
{
    m_glassView.wantsLayer = YES;
    m_glassView.layer.cornerRadius = roundRadius;

    if (m_superView)
    {
        m_superView.wantsLayer = YES;
        m_superView.layer.cornerRadius = roundRadius;
        m_superView.layer.borderWidth = 1.0;
        onBorderColorChanged();
    }

    if (mask == QGlassEffectView::CornerMask::All)
        return ;
    
    CACornerMask maskRes = 0x0;
    if (mask.testFlag(QGlassEffectView::CornerMask::TopLeft))
        maskRes |= kCALayerMinXMaxYCorner;
    if (mask.testFlag(QGlassEffectView::CornerMask::TopRight))
        maskRes |= kCALayerMaxXMaxYCorner;
    if (mask.testFlag(QGlassEffectView::CornerMask::BottomLeft))
        maskRes |= kCALayerMinXMinYCorner;
    if (mask.testFlag(QGlassEffectView::CornerMask::BottomRight))
        maskRes |= kCALayerMaxXMinYCorner;
    if (__builtin_available(macOS 10.14, *))
        m_glassView.layer.maskedCorners = maskRes;
}

void QGlassEffectViewPrivate::setVisible(bool isVisible)
{
    if (isVisible)
    {
        m_glassView.hidden = NO;
    }
    else
    {
        m_glassView.hidden = YES;
    }
}

void QGlassEffectViewPrivate::onBorderColorChanged()
{
    if (!m_superView)
        return;

    m_superView.wantsLayer = YES;
    if (qt_mac_applicationIsInDarkMode() && m_hostWnd->windowModality() != Qt::WindowModal)
        m_superView.layer.borderColor = [[NSColor tertiaryLabelColor] CGColor];
    else
        m_superView.layer.borderColor = [[NSColor clearColor] CGColor];
}

void QGlassEffectViewPrivate::loadGlassEffectView()
{
    if (!m_superView)
        m_superView = (__bridge NSView *)reinterpret_cast<void *>(m_hostWnd->winId());
    
    if (!m_glassView)
        m_glassView  = [[NSVisualEffectView alloc] initWithFrame:m_superView.frame];

    bool loadSubView = true;
    for (NSView* subView in [m_superView.superview subviews])
    {
        if (subView == m_glassView)
        {
            loadSubView = false;
            break;
        }
        else if ([subView isKindOfClass:[NSVisualEffectView class]])
        {
            if (NSVisualEffectView *visualEffectView = (NSVisualEffectView *)subView)
                visualEffectView.material = m_glassView.material;
        }
    }

    if (loadSubView)
        [m_superView.superview addSubview:m_glassView positioned:NSWindowBelow relativeTo:m_superView];

    NSWindow *window = [m_glassView window];
    if (window != nil)
        window.animationBehavior = NSWindowAnimationBehaviorNone;
}
QT_END_NAMESPACE
