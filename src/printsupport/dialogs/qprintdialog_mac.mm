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

#include <AppKit/AppKit.h>
#include <PDFKit/PDFKit.h>

#include "qprintdialog.h"
#include "qabstractprintdialog_p.h"

#include <QtCore/private/qcore_mac_p.h>
#include <QtWidgets/private/qapplication_p.h>
#include <QtPrintSupport/qprinter.h>
#include <QtPrintSupport/qprintengine.h>
#include <qpa/qplatformprintdevice.h>

#ifdef Q_OS_MAC
#import "qprintdialogpreview_mac.h"
#include <qpa/qplatformtheme.h>
#endif // Q_OS_MAC

QT_BEGIN_NAMESPACE

extern qreal qt_pointMultiplier(QPageLayout::Unit unit);

class QPrintDialogPrivate : public QAbstractPrintDialogPrivate
{
    Q_DECLARE_PUBLIC(QPrintDialog)

public:
    QPrintDialogPrivate() : printInfo(0), printPanel(0)
       {}

    void openCocoaPrintPanel(Qt::WindowModality modality);
    void closeCocoaPrintPanel();

    inline QPrintDialog *printDialog() { return q_func(); }
#ifdef Q_OS_MAC
    int GetPageCount(bool paperOrientationLand, float rectWidth, float rectHeight, bool isSelectionOnly);
    void GetPaperSize(float *paperWidth, float *paperHeight){}
    void GetPrintJobName(QString &jobName){}
#endif // Q_OS_MAC

    NSPrintInfo *printInfo;
    NSPrintPanel *printPanel;
};

QT_END_NAMESPACE

QT_USE_NAMESPACE


@class QT_MANGLE_NAMESPACE(QCocoaPrintPanelDelegate);

@interface QT_MANGLE_NAMESPACE(QCocoaPrintPanelDelegate) : NSObject
@end

@implementation QT_MANGLE_NAMESPACE(QCocoaPrintPanelDelegate) {
    NSPrintInfo *printInfo;
}

- (instancetype)initWithNSPrintInfo:(NSPrintInfo *)nsPrintInfo
{
    if ((self = [self init])) {
        printInfo = nsPrintInfo;
    }
    return self;
}

- (void)printPanelDidEnd:(NSPrintPanel *)printPanel
        returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    Q_UNUSED(printPanel);

    QPrintDialog *dialog = static_cast<QPrintDialog *>(contextInfo);
    QPrinter *printer = dialog->printer();

    if (returnCode == NSModalResponseOK) {
        PMPrintSession session = static_cast<PMPrintSession>(printInfo.PMPrintSession);
        PMPrintSettings settings = static_cast<PMPrintSettings>(printInfo.PMPrintSettings);

        UInt32 frompage, topage;
        PMGetFirstPage(settings, &frompage);
        PMGetLastPage(settings, &topage);
        topage = qMin(UInt32(INT_MAX), topage);
        dialog->setFromTo(frompage, topage);

        // OK, I need to map these values back let's see
        // If from is 1 and to is INT_MAX, then print it all
        // (Apologies to the folks with more than INT_MAX pages)
        if (dialog->fromPage() == 1 && dialog->toPage() == INT_MAX) {
            dialog->setPrintRange(QPrintDialog::AllPages);
            dialog->setFromTo(0, 0);
        } else {
            dialog->setPrintRange(QPrintDialog::PageRange); // In a way a lie, but it shouldn't hurt.
            // Carbon hands us back a very large number here even for ALL, set it to max
            // in that case to follow the behavior of the other print dialogs.
            if (dialog->maxPage() < dialog->toPage())
                dialog->setFromTo(dialog->fromPage(), dialog->maxPage());
        }

        // Keep us in sync with chosen destination
        PMDestinationType dest;
        PMSessionGetDestinationType(session, settings, &dest);
        if (dest == kPMDestinationFile) {
            // QTBUG-38820
            // If user selected Print to File, leave OSX to generate the PDF,
            // otherwise setting PdfFormat would prevent us showing dialog again.
            // TODO Restore this when QTBUG-36112 is fixed.
            /*
            QCFType<CFURLRef> file;
            PMSessionCopyDestinationLocation(session, settings, &file);
            UInt8 localFile[2048];  // Assuming there's a POSIX file system here.
            CFURLGetFileSystemRepresentation(file, true, localFile, sizeof(localFile));
            printer->setOutputFileName(QString::fromUtf8(reinterpret_cast<const char *>(localFile)));
            */
        } else {
            PMPrinter macPrinter;
            PMSessionGetCurrentPrinter(session, &macPrinter);
            QString printerId = QString::fromCFString(PMPrinterGetID(macPrinter));
            if (printer->printerName() != printerId)
                printer->setPrinterName(printerId);
        }
    }

    // Note this code should be in QCocoaPrintDevice, but that implementation is in the plugin,
    // we need to move the dialog implementation into the plugin first to be able to access it.
    // Need to tell QPrinter/QPageLayout if the page size or orientation has been changed
    PMPageFormat pageFormat = static_cast<PMPageFormat>([printInfo PMPageFormat]);
    PMPaper paper;
    PMGetPageFormatPaper(pageFormat, &paper);
    PMOrientation orientation;
    PMGetOrientation(pageFormat, &orientation);
    QPageSize pageSize;
    CFStringRef key;
    double width = 0;
    double height = 0;
    // If the PPD name is empty then is custom, for some reason PMPaperIsCustom doesn't work here
    PMPaperGetPPDPaperName(paper, &key);
    if (PMPaperGetWidth(paper, &width) == noErr && PMPaperGetHeight(paper, &height) == noErr) {
        QString ppdKey = QString::fromCFString(key);
        if (ppdKey.isEmpty()) {
            // Is using a custom page size as defined in the Print Dialog custom settings using mm or inches.
            // We can't ask PMPaper what those units actually are, we can only get the point size which may return
            // slightly wrong results due to rounding.
            // Testing shows if using metric/mm then is rounded mm, if imperial/inch is rounded to 2 decimal places
            // Even if we pass in our own custom size in mm with decimal places, the dialog will still round it!
            // Suspect internal storage is in rounded mm?
            if (QLocale().measurementSystem() == QLocale::MetricSystem) {
                QSizeF sizef = QSizeF(width, height) / qt_pointMultiplier(QPageLayout::Millimeter);
                // Round to 0 decimal places
                pageSize = QPageSize(sizef.toSize(), QPageSize::Millimeter);
            } else {
                qreal multiplier = qt_pointMultiplier(QPageLayout::Inch);
                const int w = qRound(width * 100 / multiplier);
                const int h = qRound(height * 100 / multiplier);
                pageSize = QPageSize(QSizeF(w / 100.0, h / 100.0), QPageSize::Inch);
            }
        } else {
            pageSize = QPlatformPrintDevice::createPageSize(ppdKey, QSize(width, height), QString());
        }
    }
    if (pageSize.isValid() && !pageSize.isEquivalentTo(printer->pageLayout().pageSize()))
        printer->setPageSize(pageSize);
    printer->setOrientation(orientation == kPMLandscape ? QPrinter::Landscape : QPrinter::Portrait);

    dialog->done((returnCode == NSModalResponseOK) ? QDialog::Accepted : QDialog::Rejected);
}

@end

QT_BEGIN_NAMESPACE

void QPrintDialogPrivate::openCocoaPrintPanel(Qt::WindowModality modality)
{
    Q_Q(QPrintDialog);

    // get the NSPrintInfo from the print engine in the platform plugin
    void *voidp = 0;
    (void) QMetaObject::invokeMethod(qApp->platformNativeInterface(),
                                     "NSPrintInfoForPrintEngine",
                                     Q_RETURN_ARG(void *, voidp),
                                     Q_ARG(QPrintEngine *, printer->printEngine()));
    printInfo = static_cast<NSPrintInfo *>(voidp);
    [printInfo retain];

    // It seems the only way that PM lets you use all is if the minimum
    // for the page range is 1. This _kind of_ makes sense if you think about
    // it. However, calling PMSetFirstPage() or PMSetLastPage() always enforces
    // the range.
    // get print settings from the platform plugin
    PMPrintSettings settings = static_cast<PMPrintSettings>([printInfo PMPrintSettings]);
    PMSetPageRange(settings, q->minPage(), q->maxPage());
    if (q->printRange() == QAbstractPrintDialog::PageRange) {
        PMSetFirstPage(settings, q->fromPage(), false);
        PMSetLastPage(settings, q->toPage(), false);
    }
#ifdef Q_OS_MAC
    QString jobNameFrom;
    q->GetPrintJobName(jobNameFrom);
    CFStringRef refjobNameFrom = CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar *>(jobNameFrom.unicode()), jobNameFrom.length());
    PMPrintSettingsSetJobName(settings, refjobNameFrom);
#endif // Q_OS_MAC
    [printInfo updateFromPMPrintSettings];

    QPrintDialog::PrintDialogOptions qtOptions = q->options();
    NSPrintPanelOptions macOptions = NSPrintPanelShowsCopies;
    if (qtOptions & QPrintDialog::PrintPageRange)
        macOptions |= NSPrintPanelShowsPageRange;

#ifdef Q_OS_MAC
    if (qtOptions & QPrintDialog::PrintSelection)
        macOptions |= NSPrintPanelShowsPrintSelection;
#endif // Q_OS_MAC
#ifdef Q_OS_MAC
    if (qtOptions & QPrintDialog::PrintShowPageSize)
    {
        //Change to the configuration options below to move the paper direction and size to the top position
        macOptions |= NSPrintPanelShowsPaperSize
                | NSPrintPanelShowsOrientation;
    }
#else
    if (qtOptions & QPrintDialog::PrintShowPageSize)
        macOptions |= NSPrintPanelShowsPaperSize | NSPrintPanelShowsPageSetupAccessory
                      | NSPrintPanelShowsOrientation;
#endif // Q_OS_MAC
#ifdef Q_OS_MAC
    macOptions |= NSPrintPanelShowsPreview;
#endif // Q_OS_MAC
    printPanel = [NSPrintPanel printPanel];
    [printPanel retain];
    [printPanel setOptions:macOptions];
#ifdef Q_OS_MAC
    if (qtOptions & QPrintDialog::PrintNeedViewController)
    {
        NSViewController *pViewCtroller = (NSViewController*)q->GetViewController();
        [printPanel addAccessoryController:pViewCtroller];
        [pViewCtroller release];
    }

    [[NSColor blackColor] set];
    //Use A4 paper size by default
    float paperW = 0;
    float paperH = 0;
    float paperWFromSetup = 0;
    float paperHFromSetup = 0;
    q->GetPaperSize(&paperWFromSetup, &paperHFromSetup);
    if (qtOptions & QPrintDialog::PaperOrientationLand)
    {

        paperW = qMax(paperWFromSetup, paperHFromSetup);
        paperH = qMin(paperWFromSetup, paperHFromSetup);
    }
    else
    {
        paperW = qMin(paperWFromSetup, paperHFromSetup);
        paperH = qMax(paperWFromSetup, paperHFromSetup);
    }
    NSRect frameRect = NSMakeRect(0, 0, paperW, paperH);

    [printInfo setPaperSize:frameRect.size];//The size of the initial paper, the same as the frame, A4 paper is used by default
    [printInfo setHorizontalPagination: NSFitPagination];
    [printInfo setVerticalPagination: NSAutoPagination];
    //Set from the business layer to see if the default is horizontal paper
    if (qtOptions & QPrintDialog::PaperOrientationLand)
    {
       [printInfo setOrientation: NSPaperOrientationLandscape];
    }
    else
    {
        [printInfo setOrientation: NSPaperOrientationPortrait];
    }

    [printInfo setVerticallyCentered:YES];
    NSPrintOperation *op = nil;
    QPrintDialogPreview *previewView = nil;
    PDFDocument *document = nil;
    QString pdfFilePath;
    QString userKey;
    QString ownerKey;
    q->GetPDFFilePathAndPassword(pdfFilePath, userKey, ownerKey);

    if (!pdfFilePath.isEmpty())
    {
        //Printing process of PDF
        CFStringRef cfPath = pdfFilePath.toCFString();
        NSString *str = (__bridge NSString *)cfPath;
        NSString *newString = [str stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet URLQueryAllowedCharacterSet]];
        NSURL* url = [NSURL URLWithString:newString];
        document = [[PDFDocument alloc] initWithURL:url];
        if ([document isLocked])
        {
            CFStringRef cfUserKey = userKey.toCFString();
            NSString *nsUserKey = (__bridge NSString *)cfUserKey;
            [document unlockWithPassword:nsUserKey];
            CFRelease(cfUserKey);
        }
        if ([document isEncrypted])
        {
            CFStringRef cfOwnerKey = ownerKey.toCFString();
            NSString *nsOwnerKey = (__bridge NSString *)cfOwnerKey;
            [document unlockWithPassword:nsOwnerKey];
            CFRelease(cfOwnerKey);
        }

        op = [document printOperationForPrintInfo:printInfo scalingMode:kPDFPrintPageScaleToFit autoRotate:YES];
        CFRelease(cfPath);
    }
    else
    {
        //Printing process of the other three components
        previewView = [[QPrintDialogPreview alloc] initWithFrame: frameRect];
        previewView.wantsLayer = YES;
        [previewView setFatherDiag:q];
        op = [NSPrintOperation printOperationWithView:previewView printInfo:printInfo];
    }

    [op setShowsProgressPanel:YES];
    [op setShowsPrintPanel:YES];
    [op setPrintPanel:printPanel];
    [op setJobTitle:jobNameFrom.toNSString()];
	
	// Call processEvents in case the event dispatcher has been interrupted, and needs to do
	// cleanup of modal sessions. Do this before showing the native dialog, otherwise it will
	// close down during the cleanup.
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
	
	// Make sure we don't interrupt the runModalWithPrintInfo call.
	(void) QMetaObject::invokeMethod(qApp->platformNativeInterface(),
									 "clearCurrentThreadCocoaEventDispatcherInterruptFlag");

    BOOL ret = [op runOperation];
    if (document)
        [document release];
    if (previewView)
        [previewView release];

#else
    // Call processEvents in case the event dispatcher has been interrupted, and needs to do
    // cleanup of modal sessions. Do this before showing the native dialog, otherwise it will
    // close down during the cleanup (QTBUG-17913):
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
#endif // Q_OS_MAC

    QT_MANGLE_NAMESPACE(QCocoaPrintPanelDelegate) *delegate = [[QT_MANGLE_NAMESPACE(QCocoaPrintPanelDelegate) alloc] initWithNSPrintInfo:printInfo];
    if (modality == Qt::ApplicationModal || !q->parentWidget()) {
        if (modality == Qt::NonModal)
            qWarning("QPrintDialog is required to be modal on OS X");

#ifdef Q_OS_MAC
		//int rval = [printPanel runModalWithPrintInfo:printInfo];
		[delegate printPanelDidEnd:printPanel returnCode:ret contextInfo:q];
#else
        // Make sure we don't interrupt the runModalWithPrintInfo call.
        (void) QMetaObject::invokeMethod(qApp->platformNativeInterface(),
                                         "clearCurrentThreadCocoaEventDispatcherInterruptFlag");

        int rval = [printPanel runModalWithPrintInfo:printInfo];
        [delegate printPanelDidEnd:printPanel returnCode:rval contextInfo:q];
#endif // Q_OS_MAC
    } else {
        Q_ASSERT(q->parentWidget());
        QWindow *parentWindow = q->parentWidget()->windowHandle();
        NSWindow *window = static_cast<NSWindow *>(qApp->platformNativeInterface()->nativeResourceForWindow("nswindow", parentWindow));
        [printPanel beginSheetWithPrintInfo:printInfo
                             modalForWindow:window
                                   delegate:delegate
                             didEndSelector:@selector(printPanelDidEnd:returnCode:contextInfo:)
                                contextInfo:q];
    }
}

void QPrintDialogPrivate::closeCocoaPrintPanel()
{
    [printInfo release];
    printInfo = 0;
    [printPanel release];
    printPanel = 0;

#ifdef Q_OS_MAC
    // Bug#422555 temporary plan
    if (qt_mac_applicationIsInDarkMode())
    {
        if (@available(macOS 10.14, *)) 
        {
            [NSAppearance setCurrentAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
        }
        QPlatformTheme *theme = QGuiApplicationPrivate::instance()->platformTheme();
        if (theme)
        {
            theme->handleSystemThemeChange();
        }
    }
#endif // Q_OS_MAC
}

static bool warnIfNotNative(QPrinter *printer)
{
    if (printer->outputFormat() != QPrinter::NativeFormat) {
        qWarning("QPrintDialog: Cannot be used on non-native printers");
        return false;
    }
    return true;
}


QPrintDialog::QPrintDialog(QPrinter *printer, QWidget *parent)
    : QAbstractPrintDialog(*(new QPrintDialogPrivate), printer, parent)
{
    Q_D(QPrintDialog);
    if (!warnIfNotNative(d->printer))
        return;
    setAttribute(Qt::WA_DontShowOnScreen);
}

QPrintDialog::QPrintDialog(QWidget *parent)
    : QAbstractPrintDialog(*(new QPrintDialogPrivate), 0, parent)
{
    Q_D(QPrintDialog);
    if (!warnIfNotNative(d->printer))
        return;
    setAttribute(Qt::WA_DontShowOnScreen);
}

QPrintDialog::~QPrintDialog()
{
}

int QPrintDialog::exec()
{
    Q_D(QPrintDialog);
    if (!warnIfNotNative(d->printer))
        return QDialog::Rejected;

    QDialog::setVisible(true);

    QMacAutoReleasePool pool;
    d->openCocoaPrintPanel(Qt::ApplicationModal);
    d->closeCocoaPrintPanel();

    QDialog::setVisible(false);

    return result();
}


/*!
    \reimp
*/
void QPrintDialog::setVisible(bool visible)
{
    Q_D(QPrintDialog);

    bool isCurrentlyVisible = (d->printPanel != 0);

    if (!visible == !isCurrentlyVisible)
        return;

    if (d->printer->outputFormat() != QPrinter::NativeFormat)
        return;

    QDialog::setVisible(visible);

    if (visible) {
        Qt::WindowModality modality = windowModality();
        if (modality == Qt::NonModal) {
            // NSPrintPanels can only be modal, so we must pick a type
            modality = parentWidget() ? Qt::WindowModal : Qt::ApplicationModal;
        }
        d->openCocoaPrintPanel(modality);
        return;
    } else {
        if (d->printPanel) {
            d->closeCocoaPrintPanel();
            return;
        }
    }
}
#if defined (Q_OS_MAC)

void QPrintDialog::PrintPage(void* ctx, int pageNum, const PrintPageInfo &info)
{
    // override
}

int QPrintDialog::GetPageCount(bool paperOrientationLand, float rectWidth, float rectHeight, bool isSelectionOnly)
{
    return 1;
}

void* QPrintDialog::GetViewController()
{
    //Subclass to achieve
    return NULL;
}

void QPrintDialog::GetPaperSize(float *paperWidth, float *paperHeight)
{
    //Subclass to achieve
    return;
}

void QPrintDialog::GetPrintJobName(QString &jobName)
{
    //Subclass to achieve
    return;
}

void QPrintDialog::GetPDFFilePathAndPassword(QString &filePath, QString &userKey, QString &ownerKey)
{
    //Subclass to achieve
    return;
}

//CGImageRef to NSImage
NSImage *qt_mac_cgimage_to_nsimage(CGImageRef image)
{
    NSImage *newImage = [[NSImage alloc] initWithCGImage:image size:NSZeroSize];
    return newImage;
}
static void qt_mac_deleteImage(void *image, const void *, size_t)
{
    delete static_cast<QImage *>(image);
}

CGDataProviderRef qt_mac_CGDataProvider(const QImage &image)
{
    return CGDataProviderCreateWithData(new QImage(image), image.bits(),
                                        image.byteCount(), qt_mac_deleteImage);
}

CGImageRef qt_mac_toCGImage(const QImage &inImage)
{
    if (inImage.isNull())
        return 0;

    QImage image = inImage;

    uint cgflags = kCGImageAlphaNone;
    switch (image.format()) {
    case QImage::Format_ARGB32:
        cgflags = kCGImageAlphaFirst | kCGBitmapByteOrder32Host;
        break;
    case QImage::Format_RGB32:
        cgflags = kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host;
        break;
    case QImage::Format_RGB888:
        cgflags = kCGImageAlphaNone | kCGBitmapByteOrder32Big;
        break;
    case QImage::Format_RGBA8888_Premultiplied:
        cgflags = kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big;
        break;
    case QImage::Format_RGBA8888:
        cgflags = kCGImageAlphaLast | kCGBitmapByteOrder32Big;
        break;
    case QImage::Format_RGBX8888:
        cgflags = kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big;
        break;
    default:
        // Everything not recognized explicitly is converted to ARGB32_Premultiplied.
        image = inImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // no break;
    case QImage::Format_ARGB32_Premultiplied:
        cgflags = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
        break;
    }

    CGDataProviderRef dataProvider = qt_mac_CGDataProvider(image);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGImageRef  imageRef = CGImageCreate(image.width(), image.height(), 8, 32,
                                         image.bytesPerLine(),
                                         colorSpace,
                                         cgflags, dataProvider, 0, false, kCGRenderingIntentDefault);
   CGDataProviderRelease(dataProvider);
   CGColorSpaceRelease(colorSpace);
   return imageRef;
}

void QPrintDialog::DrawImageToContext(void *ctx, const QImage &img, const QRect &destRc)
{
    CGContextRef context = (CGContextRef)ctx;
    QPrintDialog::PrintDialogOptions qtOptions = this->options();

    //If the current is WPP and it is vertical, the paper needs to be converted accordingly
    QRect rc = destRc;
    if (qtOptions & QPrintDialog::PaperPortraitSpecial)
    {
        Q_D(QPrintDialog);
        if ([d->printInfo orientation] == NSPaperOrientationPortrait)
        {
            int newHeight = img.height() * destRc.width() / img.width();
            rc = QRect(rc.left(), rc.top() + (rc.height() - newHeight) / 2, rc.width(), newHeight);
        }
        else
        {
            int newWidth = img.width() * destRc.height() / img.height();
            rc = QRect(rc.left() + (rc.width() - newWidth)/2, rc.top(), newWidth, rc.height());
        }
    }

    //Convert the QImage drawn by the kernel to NSImage
    CGImageRef imageRef = qt_mac_toCGImage(img);
    CGContextDrawImage(context, CGRectMake(rc.left(), rc.top(), rc.width(), rc.height()), imageRef);
    CGImageRelease(imageRef);
}

void QPrintDialog::DrawPDFDocumentToContext(void *ctx, QString *filename, int rotate /*= 0*/, unsigned int pageIndex /*= 1*/, const char* password /*= nullptr*/)
{
    CFStringRef path;
    CFURLRef url;
    CGPDFDocumentRef document;
    size_t count;

    path = filename->toCFString();

    url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, 0);

    CFRelease(path);
    document = CGPDFDocumentCreateWithURL(url);
    CFRelease(url);
    if(CGPDFDocumentIsEncrypted(document))
    {
        CGPDFDocumentUnlockWithPassword(document, password);
    }
    count = CGPDFDocumentGetNumberOfPages(document);
    if (count != 0)
    {
        CGPDFPageRef pageRef;
        CGContextRef context = (CGContextRef)ctx;
        CGContextSaveGState(context);
	pageRef = CGPDFDocumentGetPage(document, pageIndex);

        CGAffineTransform pdfTransform = CGPDFPageGetDrawingTransform(pageRef, kCGPDFCropBox, CGContextGetClipBoundingBox(context), rotate, true);
        CGContextConcatCTM(context, pdfTransform);

        CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
        CGContextSetRenderingIntent(context, kCGRenderingIntentDefault);
        CGContextDrawPDFPage(context, pageRef);
        CGContextRestoreGState(context);

        CFRelease(document);
    }

    return;
}

#endif
QT_END_NAMESPACE

#include "moc_qprintdialog.cpp"
