/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <Cocoa/Cocoa.h>
#import "qprintdialogpreview_mac.h"
#include "qprintdialog.h"
QT_USE_NAMESPACE
@implementation QPrintDialogPreview
//In the preview view class of the print interface, set the dialog box to which the current view belongs
- (void)setFatherDiag:(QPrintDialog *)printDialog
{
    fatherPrintDialog = printDialog;
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }

    return self;
}

//When the preview window interface is processed, every time the preview window is changed, the paper information needs to be refreshed
- (void)dealNewFrame
{
    NSPrintOperation *po = [NSPrintOperation currentOperation];
    NSPrintInfo *printInfo = [po printInfo];

    //Get the current rect that can be drawn
    //Refresh where it is drawn
    NSRect newFrame;
    newFrame.origin = NSZeroPoint;
    newFrame.size = [printInfo paperSize];
    [self setFrame:newFrame];
    pageRect = newFrame;

}
//Tell the print control, how many pages are currently printed in the app, and it will be called every time the print interface is modified
- (BOOL)knowsPageRange:(NSRangePointer)range
{
    NSPrintOperation *po = [NSPrintOperation currentOperation];
    NSPrintInfo *printInfo = [po printInfo];

    bool paperOrientationLand = ([printInfo orientation] == NSPaperOrientationLandscape) ? true : false;

    bool selOnly = [printInfo isSelectionOnly];

    //Get the size of the current paper, and draw adaptation on the ui layer
    NSSize paperSize = [printInfo paperSize];
    //Pass to the kernel the current drawing page number and the size of the paper to be drawn, the kernel will draw, we get the corresponding QImage
    CGFloat rectWidth = paperSize.width*qGuiApp->devicePixelRatio();
    CGFloat rectHeight = paperSize.height*qGuiApp->devicePixelRatio();
    if (!fatherPrintDialog)
        return NO;
    int pageNum = fatherPrintDialog->GetPageCount(paperOrientationLand, rectWidth, rectHeight, selOnly);

    range->location = 1;
    range->length = pageNum;

    return YES;
}

//When printing or previewing, ask us about the area that can be drawn
- (NSRect)rectForPage:(NSInteger)page
{
    [self dealNewFrame];
    curPage = page;
    return pageRect;
}
//The print control will be called automatically, given the drawing area, we draw the content from the top
- (void)drawRect:(NSRect)dirtyRect
{
    // Drawing code here.
    // Get the size of the current paper, and draw adaptation on the ui layer
    NSPrintOperation *po = [NSPrintOperation currentOperation];
    NSPrintInfo *printInfo = [po printInfo];
    NSSize paperSize = [printInfo paperSize];

    //Pass to the kernel the current drawing page number and the size of the paper to be drawn, the kernel will draw, we get the corresponding QImage
    CGFloat rectWidth = paperSize.width;
    CGFloat rectHeight = paperSize.height;

    bool selOnly = [printInfo isSelectionOnly];

    bool paperOrientationLand = ([printInfo orientation] == NSPaperOrientationLandscape) ? true: false;
    CGContextRef myContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    bool isPrintPreview = false;
    // 设置打印的信息
    QPrintDialog::PrintPageInfo info;
    info.destRect = QRect(dirtyRect.origin.x, dirtyRect.origin.y, dirtyRect.size.width, dirtyRect.size.height);
    info.pageSize = QSize(rectWidth, rectHeight);
    info.isOrientationLand = paperOrientationLand;
    info.isSelectionOnly = selOnly;
    info.isPrintPreviewer = isPrintPreview;
    if (!fatherPrintDialog)
        return;
    fatherPrintDialog->PrintPage((void*)myContext, curPage, info);
}
@end
