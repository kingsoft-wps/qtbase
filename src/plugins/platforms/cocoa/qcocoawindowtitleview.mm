/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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


#import "qcocoawindowtitleview.h"

@interface QCocoaWindowTitleView()
{
    NSColor *_backgroundColor;
    NSColor *_titleColor; 
    NSString *_title;
}

@end

@implementation QCocoaWindowTitleView

- (id) initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        _backgroundColor = nil;
        _title = nil;
        _titleColor = nil;
    }
    
    return self;
}

- (void) dealloc
{
    [_backgroundColor release];
    _backgroundColor = nil;
    
    [_titleColor release];
    _titleColor = nil;
    
    [_title release];
    _title = nil;
    
    [super dealloc];
}

- (void) setBackgroundColor: (NSColor*) clr {
    if (_backgroundColor)
        [_backgroundColor release];
    _backgroundColor = [clr copy];
}

- (void) setTitleColor: (NSColor*) clr {
    if (_titleColor)
        [_titleColor release];
    _titleColor = [clr copy];
}

- (void) setTitle: (NSString*) title {
    if (_title)
        [_title release];
    _title = [title copy];
}

- (void)drawString:(NSString *)string inRect:(NSRect)rect {
    NSMutableParagraphStyle *style = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
    [style setLineBreakMode:NSLineBreakByTruncatingTail];
    [style setAlignment:NSTextAlignmentCenter];
    NSDictionary *att = [[NSDictionary alloc] initWithObjectsAndKeys:style, NSParagraphStyleAttributeName,
                                                                     _titleColor, NSForegroundColorAttributeName,
                                                                     [NSFont fontWithName:@"PingFangSC-Regular" size:12], NSFontAttributeName, nil];
    
    [string drawInRect:rect withAttributes:att];
    
    [att release];
    [style release];
}


- (void)drawRect:(NSRect)dirtyRect {
    NSRect windowFrame = [NSWindow  frameRectForContentRect:[[[self window] contentView] bounds] styleMask:[[self window] styleMask]];
    NSRect contentBounds = [[[self window] contentView] bounds];
    
    NSRect titlebarRect = NSMakeRect(0, 0, self.bounds.size.width, windowFrame.size.height - contentBounds.size.height);
    titlebarRect.origin.y = self.bounds.size.height - titlebarRect.size.height;
    
    [_backgroundColor set];
    NSRectFill(titlebarRect);
    
    [self drawString:_title inRect:titlebarRect];
}


@end
