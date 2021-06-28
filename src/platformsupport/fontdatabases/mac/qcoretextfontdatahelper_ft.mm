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

#include "qglobal.h"

#if defined(Q_OS_MACX)
#import <Cocoa/Cocoa.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#elif defined(Q_OS_IOS)
#import <UIKit/UIFont.h>
#endif

#include "qcoretextfontdatahelper_ft_p.h"
#ifndef QT_NO_FREETYPE
#include <QtFontDataBaseSupport/private/qfontengine_ft_p.h>
#include <ft2build.h>
#include FT_TRUETYPE_IDS_H
#include FT_SFNT_NAMES_H
#endif

QT_BEGIN_NAMESPACE

#ifndef QT_NO_FREETYPE
static QString FTGetUnicodeStringMacintosh(FT_UShort encodingId, void* string, uint32_t stringLen)
{
	if (encodingId == TT_MAC_ID_ROMAN)
		return QString::fromNSString([[[NSString alloc] initWithBytes:string length:stringLen encoding:NSMacOSRomanStringEncoding] autorelease]);

	QString name;

	while (true)
	{
		OSStatus error = 0;
		TextEncoding textEncoding;
		error = UpgradeScriptInfoToTextEncoding(encodingId,
			kTextLanguageDontCare,
			kTextRegionDontCare,
			NULL,
			&textEncoding);
		if (error) break;

		TextToUnicodeInfo textToUnicodeInfo;
		error = CreateTextToUnicodeInfoByEncoding(textEncoding, &textToUnicodeInfo);
		if (error) break;

		ByteCount sourceRead = 0, unicodeLen = 0;

		int bufLen = stringLen * 4;
		UniChar * buf = (UniChar*)malloc(bufLen * sizeof(UniChar));
		error = ConvertFromTextToUnicode(textToUnicodeInfo,
			stringLen, string,
			0, 0, 0, 0, 0, // no font offset
			bufLen, &sourceRead, &unicodeLen,
			buf);

		if (!error)
			name = QString::fromNSString([NSString stringWithCharacters:buf length:unicodeLen/2]);

		DisposeTextToUnicodeInfo(&textToUnicodeInfo);
		free(buf);
		break;
	}

	return name;
}

static QString FTGetUnicodeStringAppleUnicode(FT_UShort encodingId, void* string, uint32_t stringLen)
{
	return QString::fromNSString([[[NSString alloc] initWithBytes:string length:stringLen encoding:NSUnicodeStringEncoding] autorelease]);
}

static QString FTGetUnicodeStringMicrosoft(FT_UShort encodingId, void* string, uint32_t stringLen)
{
	NSStringEncoding enc = kCFStringEncodingInvalidId;
	bool mbs = false;
	switch(encodingId)
	{
	case TT_MS_ID_UNICODE_CS: // UTF16-BE
	case TT_MS_ID_SYMBOL_CS:
		enc = NSUTF16BigEndianStringEncoding;
		break;
	case TT_MS_ID_UCS_4:
		enc = NSUTF32BigEndianStringEncoding;
		break;
//	case TT_MS_ID_PRC:
//		enc = (NSStringEncoding)CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingGB_18030_2000);
//		mbs = true;
//		break;
	case TT_MS_ID_SJIS:
		enc = NSShiftJISStringEncoding;
		mbs = true;
		break;
	case TT_MS_ID_BIG_5:
		enc = (NSStringEncoding)CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingBig5);
		mbs = true;
		break;
	default:
		break;
	}

	if (enc == kCFStringEncodingInvalidId)
		return {};

	if (mbs)
	{
		// byte 0 is not valid in MBS, so remove all byte 0
		unsigned char * trimed = (unsigned char *) malloc(stringLen);
		uint32_t trimedLen = 0;
		for (uint32_t i = 0; i < stringLen; ++ i)
		{
			unsigned char c = ((unsigned char *)string)[i];
			if (c) trimed[trimedLen++] = c;
		}
		if (trimedLen)
			return QString::fromNSString([[[NSString alloc] initWithBytes:trimed length:trimedLen encoding:enc] autorelease]);
		else
			return {};
	}
	return QString::fromNSString([[[NSString alloc] initWithBytes:string length:stringLen encoding:enc] autorelease]);
}

static QString FTGetUnicodeString(FT_UShort platformId, FT_UShort encodingId, void* string, quint32 stringLen)
{
	QString str;
	switch(platformId)
	{
	case TT_PLATFORM_MACINTOSH:
		str = FTGetUnicodeStringMacintosh(encodingId, string, stringLen);
		break;
	case TT_PLATFORM_MICROSOFT:
		str = FTGetUnicodeStringMicrosoft(encodingId, string, stringLen);
		break;
	case TT_PLATFORM_APPLE_UNICODE:
		str = FTGetUnicodeStringAppleUnicode(encodingId, string, stringLen);
		break;
	default: break;
	}

	if (str.isEmpty())
		str = QString::fromNSString([[[NSString alloc] initWithBytes:string length:stringLen encoding:NSISOLatin1StringEncoding] autorelease]); // IEC_8859-1
	return str;
}

static QString SFNTNameGetValue(FT_SfntName * sfntName)
{
	return FTGetUnicodeString(sfntName->platform_id, sfntName->encoding_id, sfntName->string, sfntName->string_len);
}

struct QFT_FaceLiveHelper
{
    QFT_FaceLiveHelper(FT_Face _face)
        : face(_face) {}
    ~QFT_FaceLiveHelper() { FT_Done_Face(face); }

    FT_Face face;
};

QCoreTextFontDataHelper_FT::QCoreTextFontDataHelper_FT()
    : m_ftLib(NULL)
{
    FT_Init_FreeType(&m_ftLib);
}

QCoreTextFontDataHelper_FT::~QCoreTextFontDataHelper_FT()
{
    if (m_ftLib)
		FT_Done_FreeType(m_ftLib);
}

QCoreTextFontDataHelper_FT* QCoreTextFontDataHelper_FT::instance()
{
    static QCoreTextFontDataHelper_FT helper;
    return &helper;
}

QMap<QString, QStringList> QCoreTextFontDataHelper_FT::localizedFamilyNames(CFArrayRef fonts)
{
    QMap<QString, QString> postsript2Family;
    QSet<QString> fontpaths;
    if (fonts) {
        const int numFonts = CFArrayGetCount(fonts);
        for (int i = 0; i < numFonts; ++i) {
            CTFontDescriptorRef fontDescriptor = CTFontDescriptorRef(CFArrayGetValueAtIndex(fonts, i));
            QCFType<CFStringRef> familyName = CFStringRef(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontFamilyNameAttribute));
            QCFType<CFStringRef> postscript = CFStringRef(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontNameAttribute));
            QCFType<CFURLRef> cfurl(static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(fontDescriptor, kCTFontURLAttribute)));

            postsript2Family.insert((QString) QCFString(postscript), (QString) QCFString(familyName));

            QUrl url = QUrl::fromCFURL(cfurl);
            QString fontpath = url.toLocalFile();
            fontpaths.insert(fontpath);
        }
    }

    QMap<QString, QStringList> localizedMap;
    for (const QString& fontpath : fontpaths)
    {
        FT_Error ft_err = FT_Err_Ok;

        FT_Face  face;
        FT_Long  idx, num_faces;

        FT_Open_Args  args;
        args.flags    = FT_OPEN_PATHNAME;
        QByteArray pathname = fontpath.toUtf8();
        args.pathname = pathname.data();
        args.stream   = NULL;

        // NSFontManager tells 'Skia' has a few styles, but tag of /Libarary/Fonts/Skia.ttf is 'true'.
        // Mostlikely it's a TrueType GX Variations, which Freetype can't handle right now.
        ft_err = FT_Open_Face(m_ftLib, &args, -1, &face);
        if (ft_err)
            continue;

        num_faces = face->num_faces;
        FT_Done_Face(face);

        for ( idx = 0; idx < num_faces; idx++ )
        {
            ft_err = FT_Open_Face(m_ftLib, &args, idx, &face);
            if (ft_err)
                continue;

            QFT_FaceLiveHelper faceLiveHelper(face);

            FT_UInt count = FT_Get_Sfnt_Name_Count(face);
            if (count == 0)
                continue;

            QString postscript = QString::fromUtf8(FT_Get_Postscript_Name(face));
            if (postscript.isEmpty())
                continue;

            if (!postsript2Family.contains(postscript))
            {
                Q_ASSERT(false);
                continue;
            }

            if (localizedMap.contains(postsript2Family.value(postscript)))
                continue;

            static const FT_UShort nameIDOrder[] =
            {
                TT_NAME_ID_WWS_FAMILY,
                TT_NAME_ID_PREFERRED_FAMILY,
                TT_NAME_ID_FONT_FAMILY,
            };

            QSet<QString> localizeds;
            for (FT_UShort nameId : nameIDOrder)
            {
                for (FT_UInt sfntIdx = 0; sfntIdx < count; ++ sfntIdx)
                {
                    FT_SfntName sfntName;
                    ft_err = FT_Get_Sfnt_Name(face, sfntIdx, &sfntName);
                    if (ft_err)
                        continue;

                    if (nameId != sfntName.name_id)
                        continue;

                    switch(nameId) {
                    case TT_NAME_ID_WWS_FAMILY:
                    case TT_NAME_ID_PREFERRED_FAMILY:
                    case TT_NAME_ID_FONT_FAMILY:
                        localizeds.insert(SFNTNameGetValue(&sfntName));
                        break;
                    }
                }
            }
            localizedMap.insert(postsript2Family.value(postscript), localizeds.toList());
        }
    }
    return  localizedMap;
}
#endif

QT_END_NAMESPACE

