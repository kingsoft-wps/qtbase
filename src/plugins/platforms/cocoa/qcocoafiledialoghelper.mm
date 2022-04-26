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

#include <qpa/qplatformtheme.h>

#include "qcocoafiledialoghelper.h"

/*****************************************************************************
  QFileDialog debug facilities
 *****************************************************************************/
//#define DEBUG_FILEDIALOG_FILTERS

#include <qguiapplication.h>
#include <private/qguiapplication_p.h>
#include "qt_mac_p.h"
#include "qcocoahelpers.h"
#include "qcocoaeventdispatcher.h"
#include <qregexp.h>
#include <qbuffer.h>
#include <qdebug.h>
#include <qstringlist.h>
#include <qvarlengtharray.h>
#include <stdlib.h>
#include <qabstracteventdispatcher.h>
#include <qsysinfo.h>
#include <qoperatingsystemversion.h>
#include <qglobal.h>
#include <qdir.h>
#include <qregularexpression.h>

#include <qpa/qplatformnativeinterface.h>

#include <QApplication>
#include <QWidget>
#include <QImageWriter>
#include "nscustomsavepanel.h"


#import <AppKit/NSSavePanel.h>
#import <CoreFoundation/CFNumber.h>

QT_FORWARD_DECLARE_CLASS(QString)
QT_FORWARD_DECLARE_CLASS(QStringList)
QT_FORWARD_DECLARE_CLASS(QFileInfo)
QT_FORWARD_DECLARE_CLASS(QWindow)
QT_USE_NAMESPACE

typedef QSharedPointer<QFileDialogOptions> SharedPointerFileDialogOptions;
static bool g_fileDialogLive = false;
@interface CustomButton : NSButton

{
}

@property (nonatomic, assign) QVector<QString> icons;
@property (nonatomic, assign) BOOL isHovered;
@property (nonatomic, assign) BOOL isClicked;
@property (nonatomic, retain) NSColor *normalBackgroundColor;
@property (nonatomic, retain) NSColor *hoverBackgroundColor;
@property (nonatomic, retain) NSColor *clickedBackgroundColor;
@property (nonatomic, retain) NSColor *customTitleColor;
@property (nonatomic, retain) NSWindow *toolTipTextWindow;
@property (nonatomic, retain) NSString *toolTipText;
@property (nonatomic, retain) NSImageView *imageView;
@property (nonatomic, retain) NSImage *icon;

@end

@implementation CustomButton

- (id)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.bordered = NO;
        self.wantsLayer = YES;
        self.bezelStyle = NSBezelStyleRoundRect;
        self.focusRingType = NSFocusRingTypeNone;
        self.layer.cornerRadius = 6;

        NSImage *icon = [[NSImage alloc] initWithSize:NSMakeSize(20, 16)];
        [self setImage:icon]; 
        self.imagePosition = NSImageLeft;
        self.imageScaling = NSImageScaleNone;
        [icon release];
        
        self.imageView = [[[NSImageView alloc] initWithFrame:NSMakeRect(4, 5, 16, 16)] autorelease];
        [self addSubview:self.imageView];
        
        [self createTrackingArea];
    }
    return self;
}

- (void)dealloc
{
    [self hideQuickToolTip];
    self.normalBackgroundColor = nil;
    self.hoverBackgroundColor = nil;
    self.clickedBackgroundColor = nil;
    self.toolTipTextWindow = nil;
    self.toolTipText = nil;
    self.imageView = nil;
    self.icon = nil;

    [super dealloc];
}

- (void)createTrackingArea
{
    NSTrackingAreaOptions focusTrackingAreaOptions = NSTrackingActiveInActiveApp;
    focusTrackingAreaOptions |= NSTrackingMouseEnteredAndExited;
    focusTrackingAreaOptions |= NSTrackingAssumeInside;
    focusTrackingAreaOptions |= NSTrackingInVisibleRect;

    NSTrackingArea *focusTrackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
            options:focusTrackingAreaOptions owner:self userInfo:nil];
    [self addTrackingArea:focusTrackingArea];
    [focusTrackingArea release];
}

- (CGFloat)getButtonWidthWithTitle:(NSString *)title
{
    if (title.length == 0)
        return 24;
    NSDictionary *attributes = @{NSFontAttributeName : self.font};
    NSSize maxSize = NSMakeSize(MAXFLOAT, 25);    
    NSSize textSize = [title boundingRectWithSize:maxSize options:NSStringDrawingUsesLineFragmentOrigin attributes:attributes context:nil].size;
    CGFloat width = 24 + textSize.width + 4;
    return width;
}

- (NSSize)getToolTipSizeWithText:(NSString *)text
{
    NSDictionary *attributes = @{NSFontAttributeName : self.font};
    NSSize maxSize = NSMakeSize(240, MAXFLOAT);     
    NSSize textSize = [text boundingRectWithSize:maxSize options:NSStringDrawingUsesLineFragmentOrigin attributes:attributes context:nil].size;
    return textSize;
}

- (NSRect)setCustomTitle:(NSString *)title
{
    CGFloat width = [self getButtonWidthWithTitle:title];
    NSRect rect = self.frame;
    rect.size.width = width;
    [self setFrame:rect];
    
    if (self.customTitleColor) {
        NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
#ifdef MAC_OS_X_VERSION_10_12
        [style setAlignment:NSTextAlignmentCenter];
#else
        [style setAlignment:NSCenterTextAlignment];
#endif
        NSDictionary *attrsDictionary = [NSDictionary dictionaryWithObjectsAndKeys:self.customTitleColor, NSForegroundColorAttributeName, style, NSParagraphStyleAttributeName, nil];
        NSAttributedString *attrString = [[NSAttributedString alloc]initWithString:title attributes:attrsDictionary];
        [self setAttributedTitle:attrString];
        [style release];
        [attrString release];
    } else {
        [self setTitle:title];
    }
    
    return rect;
}
- (void)updateIcons:(QVector<QString>) icons
{
    self.icons = icons;
    [self updateIcon];
}
- (BOOL)setButtonIcon:(QString) iconPath
{
    if (iconPath.isEmpty())
        return NO;

    NSImage *image = [[NSImage alloc] initWithContentsOfFile:iconPath.toNSString()];
    if (image != nil)
    {
        self.imageView.image = image;
        self.imageView.image.size = NSMakeSize(16, 16);
        self.imageView.imageScaling = NSImageScaleNone;
        if (iconPath.endsWith("gif"))
            self.imageView.animates = YES;
        else
            self.imageView.animates = NO;
        [image release];
        return YES;        
    }
    return NO;
}
- (void) updateIcon {
    QString iconPath;
    if (self.isClicked) {
        iconPath = _icons.at(2);
    } else if (self.isHovered) {
        iconPath = _icons.at(1);
    } else {
        iconPath = _icons.at(0);
    }
    if (!iconPath.isEmpty()) {
        [self setButtonIcon:iconPath];
        [self setNeedsDisplay:YES];
    }
}

- (void)showQuickToolTip
{
    if (!self.toolTipText || self.toolTipText.length == 0)
        return;
    
    if (!self.toolTipTextWindow)
    {
        CGFloat offset = 0;
        if (self.superview)
            offset = self.superview.frame.origin.x;
        NSRect buttonRect = [self.window convertRectToScreen:self.frame];
        NSSize toolTipSize = [self getToolTipSizeWithText:self.toolTipText];
        NSRect frame = NSMakeRect(buttonRect.origin.x + buttonRect.size.width - toolTipSize.width - 4 + offset, buttonRect.origin.y + buttonRect.size.height + 4, 
            toolTipSize.width + 8, toolTipSize.height + 8);
        self.toolTipTextWindow = [[[NSWindow alloc] initWithContentRect:frame
                                                        styleMask:NSWindowStyleMaskResizable
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO] autorelease];
        
        NSColor *backgroundColor = nil;
        if (qt_mac_applicationIsInDarkMode())
        {
            backgroundColor = [NSColor colorWithRed:39/255.0 green:39/255.0 blue:43/255.0 alpha:1.0];
        }
        else
        {
            backgroundColor = [NSColor whiteColor];
        }
        [self.toolTipTextWindow setBackgroundColor:backgroundColor];
        [self.toolTipTextWindow setOpaque:NO];
        [self.toolTipTextWindow setLevel:kCGHelpWindowLevel];
        [self.toolTipTextWindow makeKeyAndOrderFront:self.superview];
        // [self.toolTipTextWindow setHasShadow:NO];
        
        NSRect textRect = { { 5, 4 }, toolTipSize };
        NSTextField *textLabel = [[NSTextField alloc] initWithFrame:textRect];
        [textLabel setStringValue:self.toolTipText];
        [textLabel setFont:self.font];
        [textLabel setEditable:false];
        [textLabel setSelectable:false];
        [textLabel setBordered:false];
        [textLabel setDrawsBackground:false];
        [self.toolTipTextWindow.contentView addSubview:textLabel];
    }
    
    [self.toolTipTextWindow makeKeyAndOrderFront:self.superview];
    // self.toolTipTextWindow.alphaValue = 1;
    CGFloat offset = 0;
    if (self.superview)
        offset = self.superview.frame.origin.x;
    NSRect buttonRect = [self.window convertRectToScreen:self.frame];
    NSSize toolTipSize = [self getToolTipSizeWithText:self.toolTipText];
    NSRect frame = NSMakeRect(buttonRect.origin.x + buttonRect.size.width - toolTipSize.width - 4 + offset, buttonRect.origin.y + buttonRect.size.height + 4, 
        toolTipSize.width + 8, toolTipSize.height + 8);
    [self.toolTipTextWindow setFrame:frame display:YES];
}

- (void)hideQuickToolTip
{
    if (!self.toolTipTextWindow)
        return;
    
    [self.toolTipTextWindow orderOut:self.superview];
    // self.toolTipTextWindow.alphaValue = 0;
}

- (void)updateQuickToolTip
{
    if (self.isHovered)
    {
        [self showQuickToolTip];
    }
    else
    {
        [self hideQuickToolTip];
    }
        
}

- (void)delayShowQuickToolTip
{
    __block CustomButton *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            if (g_fileDialogLive && weakSelf)
            {
                [weakSelf updateQuickToolTip];
            }
        });
}

- (void)delayHideQuickToolTip
{
    __block CustomButton *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            if (weakSelf && g_fileDialogLive)
            {
                [weakSelf updateQuickToolTip];
            }
        });
}

- (void)drawRect:(NSRect)dirtyRect {
    NSColor *bgColor = nil;
    if (self.isClicked) {
        bgColor = self.clickedBackgroundColor;
    } else if (self.isHovered) {
        bgColor = self.hoverBackgroundColor;
    } else {
        bgColor = self.normalBackgroundColor;
    }
    if (bgColor) {
        [bgColor setFill];
        NSRectFill(dirtyRect);
    }
    [super drawRect:dirtyRect];
}

- (void)mouseEntered:(NSEvent *)event
{
    //NSLog(@"mouseEntered");
    self.isHovered = YES;
    [self delayShowQuickToolTip];
    [self updateIcon];
    [super mouseEntered:event];
}

- (void)mouseExited:(NSEvent *)event
{
    //NSLog(@"mouseExited");
    self.isHovered = NO;
    [self delayHideQuickToolTip];
    [self updateIcon];
    [super mouseExited:event];
}

- (void)mouseDown:(NSEvent *)event
{
    //NSLog(@"mouseDown");
    self.isClicked = YES;
    [self updateIcon];
    [super mouseDown:event];
    self.isClicked = NO;
    [self updateIcon];
}

- (void)mouseUp:(NSEvent *)event
{
    //NSLog(@"mouseUp");
    self.isClicked = NO;
    [self updateIcon];
    [super mouseUp:event];
}

@end

@interface QT_MANGLE_NAMESPACE(QNSOpenSavePanelDelegate)
    : NSObject<NSOpenSavePanelDelegate>

- (NSString *)strip:(const QString &)label;
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url;
- (void)filterChanged:(id)sender;
- (void)showModelessPanel;
- (BOOL)runApplicationModalPanel;
- (void)showWindowModalSheet:(QWindow *)docWidget;
- (void)updateProperties;
- (QStringList)acceptableExtensionsForSave;
- (QString)removeExtensions:(const QString &)filter;
- (void)createTextField;
- (void)createPopUpButton:(const QString &)selectedFilter hideDetails:(BOOL)hideDetails;
- (QStringList)findStrippedFilterWithVisualFilterName:(QString)name;
- (void)createAccessory;

@end

QT_NAMESPACE_ALIAS_OBJC_CLASS(QNSOpenSavePanelDelegate);

@implementation QNSOpenSavePanelDelegate {
    @public
    NSOpenPanel *mOpenPanel;
    NSSavePanel *mSavePanel;
    NSView *mAccessoryView;
    NSPopUpButton *mPopUpButton;
    NSTextField *mTextField;
    NSButton *mEncryptButton;
    NSButton *mSaveToCloudButton;
    CustomButton *mSaveToCloudSwitchButton;
    CustomButton *mSaveToCloudTipsButton;
    QCocoaFileDialogHelper *mHelper;
    NSString *mCurrentDir;

    int mReturnCode;

    SharedPointerFileDialogOptions mOptions;
    QString *mCurrentSelection;
    QStringList *mNameFilterDropDownList;
    QStringList *mSelectedNameFilter;
}

- (instancetype)initWithAcceptMode:(const QString &)selectFile
                           options:(SharedPointerFileDialogOptions)options
                            helper:(QCocoaFileDialogHelper *)helper
{
    self = [super init];
    mOptions = options;
    mEncryptButton = nil;
    mSaveToCloudButton = nil;
    mSaveToCloudSwitchButton = nil;
    mSaveToCloudTipsButton = nil;
    g_fileDialogLive = true;
    if (mOptions->acceptMode() == QFileDialogOptions::AcceptOpen){
        mOpenPanel = [NSOpenPanel openPanel];
        mSavePanel = mOpenPanel;
    } else {
        mSavePanel = [NSSavePanel savePanel];
        [mSavePanel setCanSelectHiddenExtension:YES];
        mOpenPanel = nil;
    }

    if ([mSavePanel respondsToSelector:@selector(setLevel:)])
        [mSavePanel setLevel:NSModalPanelWindowLevel];

    mReturnCode = -1;
    mHelper = helper;
    mNameFilterDropDownList = new QStringList(mOptions->nameFilters());
    QString selectedVisualNameFilter = mOptions->initiallySelectedNameFilter();
    mSelectedNameFilter = new QStringList([self findStrippedFilterWithVisualFilterName:selectedVisualNameFilter]);

    QFileInfo sel(selectFile);
    if (sel.isDir() && !sel.isBundle()){
        mCurrentDir = [sel.absoluteFilePath().toNSString() retain];
        mCurrentSelection = new QString;
    } else {
        mCurrentDir = [sel.absolutePath().toNSString() retain];
        mCurrentSelection = new QString(sel.absoluteFilePath());
    }

    [mSavePanel setTitle:options->windowTitle().toNSString()];
    [self createSaveToCloudSwitchButton];
    [self createPopUpButton:selectedVisualNameFilter hideDetails:options->testOption(QFileDialogOptions::HideNameFilterDetails)];
    [self createSaveToCloudButton];
    [self createEncryptButton];
    [self createAccessory];
    [self createPopUpButton:selectedVisualNameFilter hideDetails:options->testOption(QFileDialogOptions::HideNameFilterDetails)];
    [self createTextField];
    [self createAccessory];
    if (mSaveToCloudButton)
    {
        [mSavePanel setAccessoryView:mAccessoryView];
    }
    else
    {
        [mSavePanel setAccessoryView:mNameFilterDropDownList->size() > 1 ? mAccessoryView : nil];
    }

    // -setAccessoryView: can result in -panel:directoryDidChange:
    // resetting our mCurrentDir, set the delegate
    // here to make sure it gets the correct value.
    [mSavePanel setDelegate:self];
    mOpenPanel.accessoryViewDisclosed = YES;

    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::Accept))
        [mSavePanel setPrompt:[self strip:options->labelText(QFileDialogOptions::Accept)]];
    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::FileName))
        [mSavePanel setNameFieldLabel:[self strip:options->labelText(QFileDialogOptions::FileName)]];

    [self updateProperties];
    [mSavePanel retain];
    return self;
}

- (void)dealloc
{
    delete mNameFilterDropDownList;
    delete mSelectedNameFilter;
    delete mCurrentSelection;

    if ([mSavePanel respondsToSelector:@selector(orderOut:)])
        [mSavePanel orderOut:mSavePanel];
    if (mEncryptButton)
        [mEncryptButton release];
    if (mSaveToCloudButton)
        [mSaveToCloudButton release];
    if (mSaveToCloudSwitchButton)
        [mSaveToCloudSwitchButton release];
    if (mSaveToCloudTipsButton)
        [mSaveToCloudTipsButton release];
    g_fileDialogLive = false;
    [mSavePanel setAccessoryView:nil];
    [mPopUpButton release];
    [mTextField release];
    [mAccessoryView release];
    [mSavePanel setDelegate:nil];
    [mSavePanel release];
    [mCurrentDir release];
    [super dealloc];
}

static QString strippedText(QString s)
{
    s.remove(QLatin1String("..."));
    return QPlatformTheme::removeMnemonics(s).trimmed();
}

- (NSString *)strip:(const QString &)label
{
    return strippedText(label).toNSString();
}

- (void)closePanel
{
    *mCurrentSelection = QString::fromNSString([[mSavePanel URL] path]).normalized(QString::NormalizationForm_C);
    if ([mSavePanel respondsToSelector:@selector(close)])
        [mSavePanel close];
    if ([mSavePanel isSheet])
        [NSApp endSheet: mSavePanel];
}

- (void)showModelessPanel
{
    if (mOpenPanel){
        QFileInfo info(*mCurrentSelection);
        NSString *filepath = info.filePath().toNSString();
        NSURL *url = [NSURL fileURLWithPath:filepath isDirectory:info.isDir()];
        bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
            || [self panel:nil shouldEnableURL:url];

        [self updateProperties];
        [mSavePanel setNameFieldStringValue:selectable ? info.fileName().toNSString() : @""];

        [mOpenPanel beginWithCompletionHandler:^(NSInteger result){
            mReturnCode = result;
            if (mHelper)
                mHelper->QNSOpenSavePanelDelegate_panelClosed(result == NSModalResponseOK);
        }];
    }
}

- (BOOL)runApplicationModalPanel
{
    QFileInfo info(*mCurrentSelection);
    NSString *filepath = info.filePath().toNSString();
    NSURL *url = [NSURL fileURLWithPath:filepath isDirectory:info.isDir()];
    bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
        || [self panel:nil shouldEnableURL:url];

    [mSavePanel setDirectoryURL: [NSURL fileURLWithPath:mCurrentDir]];
    [mSavePanel setNameFieldStringValue:selectable ? info.fileName().toNSString() : @""];

    // Call processEvents in case the event dispatcher has been interrupted, and needs to do
    // cleanup of modal sessions. Do this before showing the native dialog, otherwise it will
    // close down during the cleanup.
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);

    // Make sure we don't interrupt the runModal call below.
    QCocoaEventDispatcher::clearCurrentThreadCocoaEventDispatcherInterruptFlag();

    mReturnCode = [mSavePanel runModal];

    QAbstractEventDispatcher::instance()->interrupt();
    return (mReturnCode == NSModalResponseOK);
}

- (QPlatformDialogHelper::DialogCode)dialogResultCode
{
    return (mReturnCode == NSModalResponseOK) ? QPlatformDialogHelper::Accepted : QPlatformDialogHelper::Rejected;
}

- (void)showWindowModalSheet:(QWindow *)parent
{
    QFileInfo info(*mCurrentSelection);
    NSString *filepath = info.filePath().toNSString();
    NSURL *url = [NSURL fileURLWithPath:filepath isDirectory:info.isDir()];
    bool selectable = (mOptions->acceptMode() == QFileDialogOptions::AcceptSave)
        || [self panel:nil shouldEnableURL:url];

    [self updateProperties];
    [mSavePanel setDirectoryURL: [NSURL fileURLWithPath:mCurrentDir]];

    [mSavePanel setNameFieldStringValue:selectable ? info.fileName().toNSString() : @""];
    NSWindow *nsparent = static_cast<NSWindow *>(qGuiApp->platformNativeInterface()->nativeResourceForWindow("nswindow", parent));

    [mSavePanel beginSheetModalForWindow:nsparent completionHandler:^(NSInteger result){
        mReturnCode = result;
        if (mHelper)
            mHelper->QNSOpenSavePanelDelegate_panelClosed(result == NSModalResponseOK);
    }];
}

- (BOOL)isHiddenFileAtURL:(NSURL *)url
{
    BOOL hidden = NO;
    if (url) {
        CFBooleanRef isHiddenProperty;
        if (CFURLCopyResourcePropertyForKey((__bridge CFURLRef)url, kCFURLIsHiddenKey, &isHiddenProperty, nullptr)) {
            hidden = CFBooleanGetValue(isHiddenProperty);
            CFRelease(isHiddenProperty);
        }
    }
    return hidden;
}

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url
{
    Q_UNUSED(sender);

    NSString *filename = [url path];
    if ([filename length] == 0)
        return NO;

    // Always accept directories regardless of their names (unless it is a bundle):
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *fileAttrs = [fm attributesOfItemAtPath:filename error:nil];
    if (!fileAttrs)
        return NO; // Error accessing the file means 'no'.
    NSString *fileType = [fileAttrs fileType];
    bool isDir = [fileType isEqualToString:NSFileTypeDirectory];
    if (isDir) {
        if ([mSavePanel treatsFilePackagesAsDirectories] == NO) {
            if ([[NSWorkspace sharedWorkspace] isFilePackageAtPath:filename] == NO)
                return YES;
        }
    }

    QString qtFileName = QFileInfo(QString::fromNSString(filename)).fileName();
    // No filter means accept everything
    bool nameMatches = mSelectedNameFilter->isEmpty();
    // Check if the current file name filter accepts the file:
    for (int i = 0; !nameMatches && i < mSelectedNameFilter->size(); ++i) {
        if (QDir::match(mSelectedNameFilter->at(i), qtFileName))
            nameMatches = true;
    }
    if (!nameMatches)
        return NO;

    QDir::Filters filter = mOptions->filter();
    if ((!(filter & (QDir::Dirs | QDir::AllDirs)) && isDir)
        || (!(filter & QDir::Files) && [fileType isEqualToString:NSFileTypeRegular])
        || ((filter & QDir::NoSymLinks) && [fileType isEqualToString:NSFileTypeSymbolicLink]))
        return NO;

    bool filterPermissions = ((filter & QDir::PermissionMask)
                              && (filter & QDir::PermissionMask) != QDir::PermissionMask);
    if (filterPermissions) {
        if ((!(filter & QDir::Readable) && [fm isReadableFileAtPath:filename])
            || (!(filter & QDir::Writable) && [fm isWritableFileAtPath:filename])
            || (!(filter & QDir::Executable) && [fm isExecutableFileAtPath:filename]))
            return NO;
    }
    if (!(filter & QDir::Hidden)
        && (qtFileName.startsWith(QLatin1Char('.')) || [self isHiddenFileAtURL:url]))
            return NO;

    return YES;
}

- (NSString *)panel:(id)sender userEnteredFilename:(NSString *)filename confirmed:(BOOL)okFlag
{
    Q_UNUSED(sender);
    if (!okFlag)
        return filename;
    if (!mOptions->testOption(QFileDialogOptions::DontConfirmOverwrite))
        return filename;

    // User has clicked save, and no overwrite confirmation should occur.
    // To get the latter, we need to change the name we return (hence the prefix):
    return [@"___qt_very_unlikely_prefix_" stringByAppendingString:filename];
}

- (void)setNameFilters:(const QStringList &)filters hideDetails:(BOOL)hideDetails
{
    [mPopUpButton removeAllItems];
    *mNameFilterDropDownList = filters;
    if (filters.size() > 0){
        for (int i=0; i<filters.size(); ++i) {
            QString filter = hideDetails ? [self removeExtensions:filters.at(i)] : filters.at(i);
            [mPopUpButton addItemWithTitle:filter.toNSString()];
        }
        [mPopUpButton selectItemAtIndex:0];
        [mSavePanel setAccessoryView:mAccessoryView];
    } else
        [mSavePanel setAccessoryView:nil];

    [self filterChanged:self];
}

- (void)filterChanged:(id)sender
{
    // This mDelegate function is called when the _name_ filter changes.
    Q_UNUSED(sender);
    QString selection = mNameFilterDropDownList->value([mPopUpButton indexOfSelectedItem]);
    *mSelectedNameFilter = [self findStrippedFilterWithVisualFilterName:selection];
    if ([mSavePanel respondsToSelector:@selector(validateVisibleColumns:)])
        [mSavePanel validateVisibleColumns];
    [self updateProperties];
    if (mHelper)
        mHelper->QNSOpenSavePanelDelegate_filterSelected([mPopUpButton indexOfSelectedItem]);
}

- (QString)currentNameFilter
{
    return mNameFilterDropDownList->value([mPopUpButton indexOfSelectedItem]);
}

- (QString) getCurrentSaveFileName
{
    NSString *nameField = [mSavePanel nameFieldStringValue];
    if ([nameField pathExtension].length>0) {
        nameField = [nameField stringByDeletingPathExtension];
    }
    QString filename = QString::fromNSString(nameField);
    return filename;
}
- (QList<QUrl>)selectedFiles
{
    if (mOpenPanel) {
        QList<QUrl> result;
        NSArray<NSURL *> *array = [mOpenPanel URLs];
        for (NSURL *url in array) {
            QString path = QString::fromNSString(url.path).normalized(QString::NormalizationForm_C);
            result << QUrl::fromLocalFile(path);
        }
        return result;
    } else {
        QList<QUrl> result;
        QString filename = QString::fromNSString([[mSavePanel URL] path]).normalized(QString::NormalizationForm_C);
        const QString defaultSuffix = mOptions->defaultSuffix();
        const QFileInfo fileInfo(filename);
        // If neither the user or the NSSavePanel have provided a suffix, use
        // the default suffix (if it exists).
        if (fileInfo.suffix().isEmpty() && !defaultSuffix.isEmpty()) {
                filename.append('.').append(defaultSuffix);
        }
        result << QUrl::fromLocalFile(filename.remove(QLatin1String("___qt_very_unlikely_prefix_")));
        return result;
    }
}

- (void)updateProperties
{
    // Call this functions if mFileMode, mFileOptions,
    // mNameFilterDropDownList or mQDirFilter changes.
    // The savepanel does not contain the necessary functions for this.
    const QFileDialogOptions::FileMode fileMode = mOptions->fileMode();
    bool chooseFilesOnly = fileMode == QFileDialogOptions::ExistingFile
        || fileMode == QFileDialogOptions::ExistingFiles;
    bool chooseDirsOnly = fileMode == QFileDialogOptions::Directory
        || fileMode == QFileDialogOptions::DirectoryOnly
        || mOptions->testOption(QFileDialogOptions::ShowDirsOnly);

    [mOpenPanel setCanChooseFiles:!chooseDirsOnly];
    [mOpenPanel setCanChooseDirectories:!chooseFilesOnly];
    [mSavePanel setCanCreateDirectories:!(mOptions->testOption(QFileDialogOptions::ReadOnly))];
    [mOpenPanel setAllowsMultipleSelection:(fileMode == QFileDialogOptions::ExistingFiles)];
    [mOpenPanel setResolvesAliases:!(mOptions->testOption(QFileDialogOptions::DontResolveSymlinks))];
    [mOpenPanel setTitle:mOptions->windowTitle().toNSString()];
    [mSavePanel setTitle:mOptions->windowTitle().toNSString()];
    [mPopUpButton setHidden:chooseDirsOnly];    // TODO hide the whole sunken pane instead?

    if (mOptions->acceptMode() == QFileDialogOptions::AcceptSave) {
        const QStringList ext = [self acceptableExtensionsForSave];
        [mSavePanel setAllowedFileTypes:ext.isEmpty() ? nil : qt_mac_QStringListToNSMutableArray(ext)];
    } else {
        const QStringList ext = [self acceptableExtensionsForOpen];
        [mOpenPanel setAllowedFileTypes:ext.isEmpty() ? nil : qt_mac_QStringListToNSMutableArray(ext)];
    }

    if ([mSavePanel respondsToSelector:@selector(isVisible)] && [mSavePanel isVisible]) {
        if ([mSavePanel respondsToSelector:@selector(validateVisibleColumns)])
            [mSavePanel validateVisibleColumns];
    }
}

- (void)panelSelectionDidChange:(id)sender
{
    Q_UNUSED(sender);
    if (mHelper && [mSavePanel isVisible]) {
        QString selection = QString::fromNSString([[mSavePanel URL] path]);
        if (selection != mCurrentSelection) {
            *mCurrentSelection = selection;
            mHelper->QNSOpenSavePanelDelegate_selectionChanged(selection);
        }
    }
}

- (void)panel:(id)sender directoryDidChange:(NSString *)path
{
    Q_UNUSED(sender);
    if (!mHelper)
        return;
    if (!(path && path.length) || [path isEqualToString:mCurrentDir])
        return;

    [mCurrentDir release];
    mCurrentDir = [path retain];
    mHelper->QNSOpenSavePanelDelegate_directoryEntered(QString::fromNSString(mCurrentDir));
}

/*
    Returns a list of extensions (e.g. "png", "jpg", "gif")
    for the current name filter. If a filter do not conform
    to the format *.xyz or * or *.*, an empty list
    is returned meaning accept everything.
*/
- (QStringList)acceptableExtensionsForSave
{
    QStringList result;
    for (int i=0; i<mSelectedNameFilter->count(); ++i) {
        const QString &filter = mSelectedNameFilter->at(i);
        if (filter.startsWith(QLatin1String("*."))
                && !filter.contains(QLatin1Char('?'))
                && filter.count(QLatin1Char('*')) == 1) {
            result += filter.mid(2);
        } else {
            return QStringList(); // Accept everything
        }
    }
    return result;
}

- (QStringList)acceptableExtensionsForOpen
{
    QStringList result;
    for (int i=0; i<mSelectedNameFilter->count(); ++i) {
        const QString &filter = mSelectedNameFilter->at(i);
        if (filter.startsWith(QLatin1String("*."))
                && !filter.contains(QLatin1Char('?'))
                && filter.count(QLatin1Char('*')) == 1) {
            result += filter.mid(2);
        }
    }
    return result;
}

- (QString)removeExtensions:(const QString &)filter
{
    QRegularExpression regExp(QString::fromLatin1(QPlatformFileDialogHelper::filterRegExp));
    QRegularExpressionMatch match = regExp.match(filter);
    if (match.hasMatch())
        return match.captured(1).trimmed();
    return filter;
}

- (void)createTextField
{
    NSRect textRect = { { 110.0, 3.0 }, { 80.0, 25.0 } };
    mTextField = [[NSTextField alloc] initWithFrame:textRect];
    [[mTextField cell] setFont:[NSFont systemFontOfSize:
            [NSFont systemFontSizeForControlSize:NSControlSizeRegular]]];
    [mTextField setAlignment:NSTextAlignmentRight];
    [mTextField setEditable:false];
    [mTextField setSelectable:false];
    [mTextField setBordered:false];
    [mTextField setDrawsBackground:false];
    if (mOptions->isLabelExplicitlySet(QFileDialogOptions::FileType))
        [mTextField setStringValue:[self strip:mOptions->labelText(QFileDialogOptions::FileType)]];
}

- (void)createEncryptButton
{
    bool isGreaterOne = mNameFilterDropDownList->size() > 1;
    if (isGreaterOne && mOptions->isAccessoryButtonExplicitlySet(QFileDialogOptions::Encrypt))
    {
        NSRect btnRect = { { mPopUpButton.frame.origin.x - 42.0, 4.0 }, { 42.0, 25.0 } };
        mEncryptButton = [[NSButton alloc]initWithFrame:btnRect];
        mEncryptButton.bezelStyle = NSRoundedBezelStyle;
        mEncryptButton.bordered = YES;
#ifdef MAC_OS_X_VERSION_10_12
        [mEncryptButton setButtonType:NSButtonTypeMomentaryPushIn];
#else
        [mEncryptButton setButtonType:NSMomentaryPushButton];
#endif
        //[mEncryptButton setTitle:[self strip:mOptions->accessoryButtonText(QFileDialogOptions::Encrypt)]];
        mEncryptButton.toolTip = [self strip:mOptions->accessoryButtonText(QFileDialogOptions::Encrypt)];
        [mEncryptButton setTitle: @"🔐"];
        mEncryptButton.hidden = NO;
#ifdef MAC_OS_X_VERSION_10_12
        mEncryptButton.alignment = NSTextAlignmentCenter;
#else
        mEncryptButton.alignment = NSCenterTextAlignment;
#endif
        mEncryptButton.transparent = NO;
        mEncryptButton.state = NSOffState;
        mEncryptButton.highlighted = NO;

        [mEncryptButton  setTarget:self];
        [mEncryptButton setAction:@selector(encryptButtonClick:)];
    }
}

- (void) encryptButtonClick:(id) sender
{
    assert(sender);
    
    // Encrypted document
    mHelper->QNSEncryptFile();
    // End eunloop loop
    [[NSApplication sharedApplication] stopModal];
}
- (void)createSaveToCloudButton
{
    if (mOptions->isAccessoryButtonExplicitlySet(QFileDialogOptions::SaveToCloud))
    {
        NSRect btnRect = { { 14.0, 1 }, { 94.0, 25.0 } };
        mSaveToCloudButton = [[NSButton alloc]initWithFrame:btnRect];
        mSaveToCloudButton.bezelStyle = NSRoundedBezelStyle;
        mSaveToCloudButton.bordered = YES;
#ifdef MAC_OS_X_VERSION_10_12
        [mSaveToCloudButton setButtonType:NSButtonTypeMomentaryPushIn];
#else
        [mSaveToCloudButton setButtonType:NSMomentaryPushButton];
#endif
        [mSaveToCloudButton setTitle:[self strip:mOptions->accessoryButtonText(QFileDialogOptions::SaveToCloud)]];
        [mSaveToCloudButton sizeToFit];

        if (!mSaveToCloudSwitchButton)
        {
            btnRect = mSaveToCloudButton.frame;
            CGFloat accessViewWidth = 2 * (mPopUpButton.frame.origin.x + 123);
            CGFloat btnWidth = btnRect.size.width;
            btnRect.origin.x = accessViewWidth - 14.0 - btnWidth;
            [mSaveToCloudButton setFrame:btnRect];
        }

        mSaveToCloudButton.hidden = NO;
#ifdef MAC_OS_X_VERSION_10_12
        mSaveToCloudButton.alignment = NSTextAlignmentCenter;
#else
        mSaveToCloudButton.alignment = NSCenterTextAlignment;
#endif
        mSaveToCloudButton.transparent = NO;
        mSaveToCloudButton.state = NSOffState;
        mSaveToCloudButton.highlighted = NO;

        [mSaveToCloudButton  setTarget:self];
        [mSaveToCloudButton setAction:@selector(saveToCloudButtonClick:)];
    }
}

- (void) onSaveToCloud
{
    mHelper->QNSSaveToCloud();
    [[NSApplication sharedApplication] stopModal];
}
- (void) saveToCloudButtonClick:(id) sender
{
    assert(sender);
    [self onSaveToCloud];
}

- (void)createSaveToCloudSwitchButton
{
    bool isGreaterOne = mNameFilterDropDownList->size() > 1;
    if (isGreaterOne 
        && mOptions->isAccessoryButtonExplicitlySet(QFileDialogOptions::SaveToCloud)
        && mOptions->isAccessoryButtonExplicitlySet(QFileDialogOptions::SaveToCloudSwitch))
    {
        QMap<QString, QVector<QString>> backupToCloudIcons = mOptions->backupToCloudIcons();
        NSRect btnRect = { { 446.0, 6.0 }, { 16.0, 25.0 } };
        mSaveToCloudSwitchButton = [[CustomButton alloc]initWithFrame:btnRect];
#ifdef MAC_OS_X_VERSION_10_12
        [mSaveToCloudSwitchButton setButtonType:NSButtonTypeMomentaryPushIn];
#else
        [mSaveToCloudSwitchButton setButtonType:NSMomentaryPushButton];
#endif
        if (!qt_mac_applicationIsInDarkMode())
        {
            NSColor *titleColor = [NSColor colorWithRed:54/255.0 green:66/255.0 blue:90/255.0 alpha:1.0];
            NSColor *normalColor = [NSColor colorWithRed:198/255.0 green:206/255.0 blue:214/255.0 alpha:0.0];
            NSColor *hoverColor = [NSColor colorWithRed:198/255.0 green:206/255.0 blue:214/255.0 alpha:0.2];
            NSColor *clickColor = [NSColor colorWithRed:198/255.0 green:206/255.0 blue:214/255.0 alpha:0.4];
            mSaveToCloudSwitchButton.customTitleColor = titleColor;
            mSaveToCloudSwitchButton.normalBackgroundColor = normalColor;
            mSaveToCloudSwitchButton.hoverBackgroundColor = hoverColor;
            mSaveToCloudSwitchButton.clickedBackgroundColor = clickColor;
        }
        else
        {
            NSColor *titleColor = [NSColor colorWithRed:255/255.0 green:255/255.0 blue:255/255.0 alpha:0.85];
            NSColor *normalColor = [NSColor colorWithRed:255/255.0 green:255/255.0 blue:255/255.0 alpha:0.0];
            NSColor *hoverColor = [NSColor colorWithRed:255/255.0 green:255/255.0 blue:255/255.0 alpha:0.06];
            NSColor *clickColor = [NSColor colorWithRed:255/255.0 green:255/255.0 blue:255/255.0 alpha:0.1];
            mSaveToCloudSwitchButton.customTitleColor = titleColor;
            mSaveToCloudSwitchButton.normalBackgroundColor = normalColor;
            mSaveToCloudSwitchButton.hoverBackgroundColor = hoverColor;
            mSaveToCloudSwitchButton.clickedBackgroundColor = clickColor;
        }
        if (mOptions->isBackupToCloudEnable())
        {
            [mSaveToCloudSwitchButton updateIcons:backupToCloudIcons.value("enable")];
            btnRect = [mSaveToCloudSwitchButton setCustomTitle:[self strip:mOptions->accessoryButtonText(QFileDialogOptions::SaveToCloudEnabled)]];
            mSaveToCloudSwitchButton.toolTipText = [self strip:mOptions->backupToCloudTip()];
        }
        else
        {
            [mSaveToCloudSwitchButton updateIcons:backupToCloudIcons.value("disable")];
            NSString *title = [self strip:mOptions->accessoryButtonText(QFileDialogOptions::SaveToCloudSwitch)];
            CGFloat width = [mSaveToCloudSwitchButton getButtonWidthWithTitle:title];
            btnRect.origin.x = 298.0 + width;
            [mSaveToCloudSwitchButton setFrame:btnRect];
            btnRect = [mSaveToCloudSwitchButton setCustomTitle:title];
        }

        NSRect tipsBtnRect = { { btnRect.origin.x + btnRect.size.width, 6.0 }, { 16.0, 25.0 } };
        mSaveToCloudTipsButton = [[CustomButton alloc]initWithFrame:tipsBtnRect];

        mSaveToCloudTipsButton.toolTipText = [self strip:mOptions->backupToCloudTip()];
#ifdef MAC_OS_X_VERSION_10_12
        [mSaveToCloudTipsButton setButtonType:NSButtonTypeMomentaryPushIn];
#else
        [mSaveToCloudTipsButton setButtonType:NSMomentaryPushButton];
#endif

        if (mOptions->isBackupToCloudEnable())
        {
            mSaveToCloudTipsButton.hidden = YES;
        }
        else
        {
            mSaveToCloudTipsButton.hidden = NO;
        }
        [mSaveToCloudTipsButton updateIcons:backupToCloudIcons.value("tip")];
        [mSaveToCloudTipsButton setCustomTitle:@""];

#ifdef MAC_OS_X_VERSION_10_12
        mSaveToCloudSwitchButton.alignment = NSTextAlignmentCenter;
#else
        mSaveToCloudSwitchButton.alignment = NSCenterTextAlignment;
#endif

        [mSaveToCloudSwitchButton setTarget:self];
        [mSaveToCloudSwitchButton setAction:@selector(saveToCloudSwitchButtonClick:)];
    }
}

- (void) saveToCloudSwitchButtonClick:(id) sender
{
    assert(sender);
    if (mOptions->isBackupToCloudEnable())
        return ;

    if (!qApp->property("hostLogined").toBool())
    {
        [self onSaveToCloud];
        return;
    }

    mOptions->setBackupToCloudEnable(true);
    [mSaveToCloudSwitchButton updateIcons:mOptions->backupToCloudIcons().value("loading")];
    // 云文档
    mHelper->QNSSaveToCloudSwitch();
    __block QNSOpenSavePanelDelegate* weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            if (g_fileDialogLive && weakSelf)
            {
                [weakSelf onEnableCloudBackupCallback:true];
            }
        });
}

- (void) onEnableCloudBackupCallback:(bool) success
{
    if (success)
    {
        mOptions->setBackupToCloudEnable(true);
        [mSaveToCloudSwitchButton updateIcons:mOptions->backupToCloudIcons().value("enable")];
        [mSaveToCloudSwitchButton setCustomTitle:[self strip:mOptions->accessoryButtonText(QFileDialogOptions::SaveToCloudEnabled)]];
        mSaveToCloudSwitchButton.toolTipText = [self strip:mOptions->backupToCloudTip()];
        mSaveToCloudTipsButton.hidden = YES;
    }
    else
    {
        mOptions->setBackupToCloudEnable(false);
        [mSaveToCloudSwitchButton updateIcons:mOptions->backupToCloudIcons().value("error")];
    }
}
- (void)createPopUpButton:(const QString &)selectedFilter hideDetails:(BOOL)hideDetails
{
    NSRect popUpRect = { { 200.0, 4.0 }, { 250.0, 25.0 } };
    if (mSaveToCloudSwitchButton)
    {
        popUpRect.origin.x = mSaveToCloudSwitchButton.frame.origin.x - popUpRect.size.width;
    }
    mPopUpButton = [[NSPopUpButton alloc] initWithFrame:popUpRect pullsDown:NO];
    [mPopUpButton setTarget:self];
    [mPopUpButton setAction:@selector(filterChanged:)];

    if (mNameFilterDropDownList->size() > 0) {
        int filterToUse = -1;
        for (int i=0; i<mNameFilterDropDownList->size(); ++i) {
            QString currentFilter = mNameFilterDropDownList->at(i);
            if (selectedFilter == currentFilter ||
                (filterToUse == -1 && currentFilter.startsWith(selectedFilter)))
                filterToUse = i;
            QString filter = hideDetails ? [self removeExtensions:currentFilter] : currentFilter;
            [mPopUpButton addItemWithTitle:filter.toNSString()];
        }
        if (filterToUse != -1)
            [mPopUpButton selectItemAtIndex:filterToUse];
    }
}

- (QStringList) findStrippedFilterWithVisualFilterName:(QString)name
{
    for (int i=0; i<mNameFilterDropDownList->size(); ++i) {
        if (mNameFilterDropDownList->at(i).startsWith(name))
            return QPlatformFileDialogHelper::cleanFilterList(mNameFilterDropDownList->at(i));
    }
    return QStringList();
}

- (void)createAccessory
{
    bool isGreaterOne = mNameFilterDropDownList->size() > 1;
    CGFloat width = 2 * (mPopUpButton.frame.origin.x + 123);
    NSRect accessoryRect = { { 0.0, 0.0 }, { width, 33.0 } };
    mAccessoryView = [[NSView alloc] initWithFrame:accessoryRect];
    if (isGreaterOne)
    {
        if (mTextField)
            [mAccessoryView addSubview:mTextField];
        [mAccessoryView addSubview:mPopUpButton];
    }
    if (mEncryptButton)
        [mAccessoryView addSubview:mEncryptButton];
    if (mSaveToCloudButton)
        [mAccessoryView addSubview:mSaveToCloudButton];
    if (mSaveToCloudSwitchButton)
        [mAccessoryView addSubview:mSaveToCloudSwitchButton];
    if (mSaveToCloudTipsButton)
        [mAccessoryView addSubview:mSaveToCloudTipsButton];
}

@end

QT_BEGIN_NAMESPACE

QCocoaFileDialogHelper::QCocoaFileDialogHelper()
    : mDelegate(nil)
{
}

QCocoaFileDialogHelper::~QCocoaFileDialogHelper()
{
    if (!mDelegate)
        return;
    QMacAutoReleasePool pool;
    [mDelegate release];
    mDelegate = nil;
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_selectionChanged(const QString &newPath)
{
    emit currentChanged(QUrl::fromLocalFile(newPath));
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_panelClosed(bool accepted)
{
    if (accepted) {
        emit accept();
    } else {
        emit reject();
    }
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_directoryEntered(const QString &newDir)
{
    // ### fixme: priv->setLastVisitedDirectory(newDir);
    emit directoryEntered(QUrl::fromLocalFile(newDir));
}

void QCocoaFileDialogHelper::QNSOpenSavePanelDelegate_filterSelected(int menuIndex)
{
    const QStringList filters = options()->nameFilters();
    emit filterSelected(menuIndex >= 0 && menuIndex < filters.size() ? filters.at(menuIndex) : QString());
}

void QCocoaFileDialogHelper::setDirectory(const QUrl &directory)
{
    if (mDelegate)
        [mDelegate->mSavePanel setDirectoryURL:[NSURL fileURLWithPath:directory.toLocalFile().toNSString()]];
    else
        mDir = directory;
}

QUrl QCocoaFileDialogHelper::directory() const
{
    if (mDelegate) {
        QString path = QString::fromNSString([[mDelegate->mSavePanel directoryURL] path]).normalized(QString::NormalizationForm_C);
        return QUrl::fromLocalFile(path);
    }
    return mDir;
}

void QCocoaFileDialogHelper::selectFile(const QUrl &filename)
{
    QString filePath = filename.toLocalFile();
    if (QDir::isRelativePath(filePath))
        filePath = QFileInfo(directory().toLocalFile(), filePath).filePath();

    // There seems to no way to select a file once the dialog is running.
    // So do the next best thing, set the file's directory:
    setDirectory(QFileInfo(filePath).absolutePath());
}

QList<QUrl> QCocoaFileDialogHelper::selectedFiles() const
{
    if (mDelegate)
        return [mDelegate selectedFiles];
    return QList<QUrl>();
}

QString QCocoaFileDialogHelper::userFileName() const
{
    if (mDelegate)
         return [mDelegate getCurrentSaveFileName];
    return QString();
}
void QCocoaFileDialogHelper::setFilter()
{
    if (!mDelegate)
        return;
    const SharedPointerFileDialogOptions &opts = options();
    [mDelegate->mSavePanel setTitle:opts->windowTitle().toNSString()];
    if (opts->isLabelExplicitlySet(QFileDialogOptions::Accept))
        [mDelegate->mSavePanel setPrompt:[mDelegate strip:opts->labelText(QFileDialogOptions::Accept)]];
    if (opts->isLabelExplicitlySet(QFileDialogOptions::FileName))
        [mDelegate->mSavePanel setNameFieldLabel:[mDelegate strip:opts->labelText(QFileDialogOptions::FileName)]];

    [mDelegate updateProperties];
}

void QCocoaFileDialogHelper::selectNameFilter(const QString &filter)
{
    if (!options())
        return;
    const int index = options()->nameFilters().indexOf(filter);
    if (index != -1) {
        if (!mDelegate) {
            options()->setInitiallySelectedNameFilter(filter);
            return;
        }
        [mDelegate->mPopUpButton selectItemAtIndex:index];
        [mDelegate filterChanged:nil];
    }
}
void QCocoaFileDialogHelper::QNSEncryptFile()
{
    emit encryptFile();
}

void QCocoaFileDialogHelper::QNSSaveToCloud()
{
    emit saveToCloud();
}

void QCocoaFileDialogHelper::QNSSaveToCloudSwitch()
{
    emit saveToCloudSwitch();
}

void QCocoaFileDialogHelper::onEnableCloudBackupCallback(bool success)
{
    if (mDelegate) {
        [mDelegate onEnableCloudBackupCallback:success];
    }
}

QString QCocoaFileDialogHelper::selectedNameFilter() const
{
    if (!mDelegate)
        return options()->initiallySelectedNameFilter();
    int index = [mDelegate->mPopUpButton indexOfSelectedItem];
    if (index >= options()->nameFilters().count())
        return QString();
    return index != -1 ? options()->nameFilters().at(index) : QString();
}

void QCocoaFileDialogHelper::hide()
{
    hideCocoaFilePanel();
}

bool QCocoaFileDialogHelper::show(Qt::WindowFlags windowFlags, Qt::WindowModality windowModality, QWindow *parent)
{
//    Q_Q(QFileDialog);
    if (windowFlags & Qt::WindowStaysOnTopHint) {
        // The native file dialog tries all it can to stay
        // on the NSModalPanel level. And it might also show
        // its own "create directory" dialog that we cannot control.
        // So we need to use the non-native version in this case...
        return false;
    }

    return showCocoaFilePanel(windowModality, parent);
}

void QCocoaFileDialogHelper::createNSOpenSavePanelDelegate()
{
    QMacAutoReleasePool pool;

    const SharedPointerFileDialogOptions &opts = options();
    const QList<QUrl> selectedFiles = opts->initiallySelectedFiles();
    const QUrl directory = mDir.isEmpty() ? opts->initialDirectory() : mDir;
    const bool selectDir = selectedFiles.isEmpty();
    QString selection(selectDir ? directory.toLocalFile() : selectedFiles.front().toLocalFile());
    QNSOpenSavePanelDelegate *delegate = [[QNSOpenSavePanelDelegate alloc]
        initWithAcceptMode:
            selection
            options:opts
            helper:this];

    [static_cast<QNSOpenSavePanelDelegate *>(mDelegate) release];
    mDelegate = delegate;
}

bool QCocoaFileDialogHelper::showCocoaFilePanel(Qt::WindowModality windowModality, QWindow *parent)
{
    createNSOpenSavePanelDelegate();
    if (!mDelegate)
        return false;
    if (windowModality == Qt::NonModal)
        [mDelegate showModelessPanel];
    else if (windowModality == Qt::WindowModal && parent)
        [mDelegate showWindowModalSheet:parent];
    // no need to show a Qt::ApplicationModal dialog here, since it will be done in _q_platformRunNativeAppModalPanel()
    return true;
}

bool QCocoaFileDialogHelper::hideCocoaFilePanel()
{
    if (!mDelegate){
        // Nothing to do. We return false to leave the question
        // open regarding whether or not to go native:
        return false;
    } else {
        [mDelegate closePanel];
        // Even when we hide it, we are still using a
        // native dialog, so return true:
        return true;
    }
}

void QCocoaFileDialogHelper::exec()
{
    // Note: If NSApp is not running (which is the case if e.g a top-most
    // QEventLoop has been interrupted, and the second-most event loop has not
    // yet been reactivated (regardless if [NSApp run] is still on the stack)),
    // showing a native modal dialog will fail.
    QMacAutoReleasePool pool;
    if ([mDelegate runApplicationModalPanel])
        emit accept();
    else
        emit reject();

}

bool QCocoaFileDialogHelper::defaultNameFilterDisables() const
{
    return true;
}

QT_END_NAMESPACE
