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

#include "qwindowsfontengine_ft_p.h"
#include "qwindowsfontdatabase_p.h"
#include <QtCore/qt_windows.h>

#include <QtCore/QDebug>
#include <private/qstringiterator_p.h>
#include <qguiapplication.h>
#include <qscreen.h>
#include <qpa/qplatformscreen.h>
#include <QtCore/QUuid>
#include <limits.h>

QT_BEGIN_NAMESPACE

QWindowsFontEngineFT::QWindowsFontEngineFT(const QFontDef &fd, HFONT _hfont)
    : QFontEngineFT(fd), hfont(_hfont)
{
    default_hint_style = QFontEngineFT::HintLight;
}

QWindowsFontEngineFT::~QWindowsFontEngineFT()
{
    if (hfont)
        DeleteObject(hfont);
}

glyph_metrics_t QWindowsFontEngineFT::alphaMapBoundingBox(glyph_t glyph, QFixed subPixelPosition,
                                                          const QTransform &matrix,
                                                          QFontEngine::GlyphFormat format)
{
    glyph_metrics_t gm = QFontEngineFT::alphaMapBoundingBox(glyph, subPixelPosition, matrix, format);
    gm.width += 4;
    gm.height += 4;
    return gm;
}


QFontEngine* QWindowsFontEngineFT::cloneWithSize(qreal pixelSize) const
{
    QFontDef fontDef(this->fontDef);
    fontDef.pixelSize = pixelSize;
    LOGFONT lf = QWindowsFontDatabase::fontDefToLOGFONT(fontDef, QString());
    HFONT _h = CreateFontIndirect(&lf);
    QWindowsFontEngineFT* fe = new QWindowsFontEngineFT(fontDef, _h);
    if (!fe->initFromFontEngine(this)) {
        delete fe;
        return 0;
    }
    else {
        return fe;
    }
}

QWindowsFontEngineFT *QWindowsFontEngineFT::createWindowsFontEngineFT(
        HFONT _font, const QFontDef &fontDef, const QString &faceName,
        const QSharedPointer<QWindowsFontEngineData> &fontEngineData)
{
    HDC hdc = fontEngineData->hdc;
    HGDIOBJ hOldObj = SelectObject(hdc, _font);
    DWORD fontSize = GetFontData(hdc, 0, 0, 0, 0);
    QByteArray fontData;
    if (fontSize != GDI_ERROR) {
        fontData.resize(fontSize);
        if (GetFontData(hdc, 0, 0, fontData.data(), fontSize) == GDI_ERROR)
            fontData.clear();
    }
    SelectObject(hdc, hOldObj);
    if (fontData.isEmpty())
        return nullptr;

    QFontEngine::FaceId faceId;
    faceId.filename = faceName.isEmpty() ? 
        QFile::encodeName(fontDef.family) :
        QFile::encodeName(faceName);

    QScopedPointer<QWindowsFontEngineFT> engine(new QWindowsFontEngineFT(fontDef, _font));

    QFontEngineFT::GlyphFormat format = QFontEngineFT::Format_Mono;
    const bool antialias = !(fontDef.styleStrategy & QFont::NoAntialias);

    if (antialias) {
        QFontEngine::SubpixelAntialiasingType subpixelType = QFontEngineFT::subpixelAntialiasingTypeHint();
        if (subpixelType == QFontEngine::Subpixel_None
            || (fontDef.styleStrategy & QFont::NoSubpixelAntialias)) {
            format = QFontEngineFT::Format_A8;
            engine->subpixelType = QFontEngine::Subpixel_None;
        } else {
            format = QFontEngineFT::Format_A32;
            engine->subpixelType = subpixelType;
        }
    }

    if (!engine->init(faceId, antialias, format, fontData) || engine->invalid()) {
        return nullptr;
    }

    engine->setQtDefaultHintStyle(static_cast<QFont::HintingPreference>(fontDef.hintingPreference));
    return engine.take();
}

namespace {
class QWindowsFontEngineFTRawData : public QWindowsFontEngineFT
{
public:
    QWindowsFontEngineFTRawData(const QFontDef &fontDef, HFONT _hfont)
        : QWindowsFontEngineFT(fontDef, _hfont)
    {
    }

    void updateFamilyNameAndStyle()
    {
        fontDef.family = QString::fromLatin1(freetype->face->family_name);

        if (freetype->face->style_flags & FT_STYLE_FLAG_ITALIC)
            fontDef.style = QFont::StyleItalic;

        if (freetype->face->style_flags & FT_STYLE_FLAG_BOLD)
            fontDef.weight = QFont::Bold;
    }

    void updateHFont()
    {
        LOGFONT lf = QWindowsFontDatabase::fontDefToLOGFONT(fontDef, QString());
        hfont = CreateFontIndirect(&lf);
    }

    bool initFromData(const QByteArray &fontData)
    {
        FaceId faceId;
        faceId.filename = "";
        faceId.index = 0;
        faceId.uuid = QUuid::createUuid().toByteArray();

        return init(faceId, true, Format_None, fontData);
    }
};
}

QWindowsFontEngineFT *
QWindowsFontEngineFT::createWindowsFontEngineFT(const QByteArray &fontData, qreal pixelSize,
                                                QFont::HintingPreference hintingPreference)
{
    QFontDef fontDef;
    fontDef.pixelSize = pixelSize;
    fontDef.stretch = QFont::Unstretched;
    fontDef.hintingPreference = hintingPreference;
    QWindowsFontEngineFTRawData *fe = new QWindowsFontEngineFTRawData(fontDef, nullptr);
    if (!fe->initFromData(fontData) || !fe->isColorFont()) {
        delete fe;
        return 0;
    }

    fe->updateFamilyNameAndStyle();
    fe->setQtDefaultHintStyle(static_cast<QFont::HintingPreference>(fontDef.hintingPreference));
    fe->updateHFont();
    return fe;
}

QT_END_NAMESPACE
