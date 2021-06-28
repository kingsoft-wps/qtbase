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

#ifndef FREETPYEMAC_H
#define FREETPYEMAC_H
#include <QMap>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
QT_BEGIN_NAMESPACE
class QPainterPath;
class QString;
QT_END_NAMESPACE
#define KSPPT_CLOSEFIGURE		0x01
#define KSPPT_LINETO                    0x02
#define KSPPT_BEZIERTO			0x04
#define KSPPT_MOVETO                    0x08
#define KSPPT_BEZIERTO_V		0x10

typedef struct {
    float			m_PointX;
    float			m_PointY;
    int                         m_Flag;
} KSP_PATHPOINT;

typedef struct {
    int			m_bCount;
    int                 m_PointCount;
    KSP_PATHPOINT*	m_pPoints;
    int                 m_CurX;
    int                 m_CurY;
    float		m_CoordUnit;
} OUTLINE_PARAMS;

typedef struct glyphInfo_
{
    QString fontPath;
    QString fontFamily;
    QString fontFaceName;
    QPointF glyphPos;
    quint32 glyphId;
    qreal   pixelSize;
    bool    italic;
    qreal   italicFactor;
    bool    isStretch;
    qreal   stretch;
    bool    verticalMetric;
} glyphInfo;

class freeTypeFontMgr
{
public:
    freeTypeFontMgr();
    virtual ~freeTypeFontMgr();
    static freeTypeFontMgr* instance();

    int getFreeTypePath(QPainterPath *path, const glyphInfo &info);
    OUTLINE_PARAMS* getGlyphOutline(const glyphInfo &info);
private:
    FT_Face getFontFace(const QString &fontFile, const QString &family);

private:
    FT_Library m_ftLibrary;
    QMap<QString, FT_Face> m_faceMap;
    QMap<QString, OUTLINE_PARAMS*> m_outlineCacheMap;
};

#endif // FREETPYEMAC_H
