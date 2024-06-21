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

#include <QPointF>
#include <QPainterPath>
#include <QDebug>
#include "freetypemac_p.h"

#define FT_Get_Glyph_Outline(face) &((FT_Face)face)->glyph->outline

void _Outline_CheckEmptyContour(OUTLINE_PARAMS* param)
{
    if (param->m_PointCount >= 2 && param->m_pPoints[param->m_PointCount - 2].m_Flag == KSPPT_MOVETO &&
            param->m_pPoints[param->m_PointCount - 2].m_PointX == param->m_pPoints[param->m_PointCount - 1].m_PointX &&
            param->m_pPoints[param->m_PointCount - 2].m_PointY == param->m_pPoints[param->m_PointCount - 1].m_PointY) {
        param->m_PointCount -= 2;
    }
    if (param->m_PointCount >= 4 && param->m_pPoints[param->m_PointCount - 4].m_Flag == KSPPT_MOVETO &&
            param->m_pPoints[param->m_PointCount - 3].m_Flag == KSPPT_BEZIERTO &&
            param->m_pPoints[param->m_PointCount - 3].m_PointX == param->m_pPoints[param->m_PointCount - 4].m_PointX &&
            param->m_pPoints[param->m_PointCount - 3].m_PointY == param->m_pPoints[param->m_PointCount - 4].m_PointY &&
            param->m_pPoints[param->m_PointCount - 2].m_PointX == param->m_pPoints[param->m_PointCount - 4].m_PointX &&
            param->m_pPoints[param->m_PointCount - 2].m_PointY == param->m_pPoints[param->m_PointCount - 4].m_PointY &&
            param->m_pPoints[param->m_PointCount - 1].m_PointX == param->m_pPoints[param->m_PointCount - 4].m_PointX &&
            param->m_pPoints[param->m_PointCount - 1].m_PointY == param->m_pPoints[param->m_PointCount - 4].m_PointY) {
        param->m_PointCount -= 4;
    }
}
extern "C" {
    static int _Outline_MoveTo(const FT_Vector* to, void* user)
    {
        OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;
        if (!param->m_bCount) {
            _Outline_CheckEmptyContour(param);
            param->m_pPoints[param->m_PointCount].m_PointX = to->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_PointY = to->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_Flag = KSPPT_MOVETO;
            param->m_CurX = (int)to->x;
            param->m_CurY = (int)to->y;
            if (param->m_PointCount) {
                param->m_pPoints[param->m_PointCount - 1].m_Flag |= KSPPT_CLOSEFIGURE;
            }
        }
        param->m_PointCount ++;
        return 0;
    }
}
extern "C" {
    static int _Outline_LineTo(const FT_Vector* to, void* user)
    {
        OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;
        if (!param->m_bCount) {
            param->m_pPoints[param->m_PointCount].m_PointX = to->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_PointY = to->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_Flag = KSPPT_LINETO;
            param->m_CurX = (int)to->x;
            param->m_CurY = (int)to->y;
        }
        param->m_PointCount ++;
        return 0;
    }
}
extern "C" {
    static int _Outline_ConicTo(const FT_Vector* control, const FT_Vector* to, void* user)
    {
        OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;
        if (!param->m_bCount) {
            param->m_pPoints[param->m_PointCount].m_PointX = (param->m_CurX + (control->x - param->m_CurX) * 2 / 3) / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_PointY = (param->m_CurY + (control->y - param->m_CurY) * 2 / 3) / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_Flag = KSPPT_BEZIERTO;
            param->m_pPoints[param->m_PointCount + 1].m_PointX = (control->x + (to->x - control->x) / 3) / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 1].m_PointY = (control->y + (to->y - control->y) / 3) / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 1].m_Flag = KSPPT_BEZIERTO;
            param->m_pPoints[param->m_PointCount + 2].m_PointX = to->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 2].m_PointY = to->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 2].m_Flag = KSPPT_BEZIERTO;
            param->m_CurX = (int)to->x;
            param->m_CurY = (int)to->y;
        }
        param->m_PointCount += 3;
        return 0;
    }
}
extern "C" {
    static int _Outline_CubicTo(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user)
    {
        OUTLINE_PARAMS* param = (OUTLINE_PARAMS*)user;
        if (!param->m_bCount) {
            param->m_pPoints[param->m_PointCount].m_PointX = control1->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_PointY = control1->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount].m_Flag = KSPPT_BEZIERTO;
            param->m_pPoints[param->m_PointCount + 1].m_PointX = control2->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 1].m_PointY = control2->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 1].m_Flag = KSPPT_BEZIERTO;
            param->m_pPoints[param->m_PointCount + 2].m_PointX = to->x / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 2].m_PointY = to->y / param->m_CoordUnit;
            param->m_pPoints[param->m_PointCount + 2].m_Flag = KSPPT_BEZIERTO;
            param->m_CurX = (int)to->x;
            param->m_CurY = (int)to->y;
        }
        param->m_PointCount += 3;
        return 0;
    }
}

freeTypeFontMgr::freeTypeFontMgr()
    : m_ftLibrary(nullptr)
{

}

freeTypeFontMgr::~freeTypeFontMgr()
{
    if (m_ftLibrary)
    {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }
    for (QMap<QString, FT_Face>::iterator it = m_faceMap.begin(); it != m_faceMap.end(); it++)
    {
        FT_Done_Face(it.value());
    }
    m_faceMap.clear();
    for (QMap<QString, OUTLINE_PARAMS*>::iterator it = m_outlineCacheMap.begin(); it != m_outlineCacheMap.end(); it++)
    {
        delete it.value();
    }
    m_outlineCacheMap.clear();
}

int freeTypeFontMgr::getFreeTypePath(QPainterPath *path, const glyphInfo &info)
{
    if (!path)
        return -1;

    OUTLINE_PARAMS *outlineParams = getGlyphOutline(info);
    if (!outlineParams)
        return -1;
    QPointF pos = info.glyphPos;
    qreal pixelSizeY = info.pixelSize;
    qreal pixelSizeX = info.pixelSize * info.stretch;
    for (int i = 0; i < outlineParams->m_PointCount; i++)
    {
        QPointF point(outlineParams->m_pPoints[i].m_PointX * pixelSizeX, ( - outlineParams->m_pPoints[i].m_PointY) * pixelSizeY);
        point += pos;
        int flag = outlineParams->m_pPoints[i].m_Flag;

        if (KSPPT_MOVETO & flag)
        {
            path->moveTo(point);
        }
        else if (KSPPT_LINETO & flag)
        {
            path->lineTo(point);
        }
        else if (KSPPT_BEZIERTO & flag)
        {
            i++;
            QPointF point1(outlineParams->m_pPoints[i].m_PointX * pixelSizeX,  ( - outlineParams->m_pPoints[i].m_PointY) * pixelSizeY);
            point1 += pos;
            i++;
            QPointF point2(outlineParams->m_pPoints[i].m_PointX * pixelSizeX,  ( - outlineParams->m_pPoints[i].m_PointY) * pixelSizeY);
            point2 += pos;
            path->cubicTo(point, point1, point2);
        }

        if (KSPPT_CLOSEFIGURE & flag)
        {
            path->closeSubpath();
        }
    }
    return 0;
}

OUTLINE_PARAMS* freeTypeFontMgr::getGlyphOutline(const glyphInfo &info)
{
    QString glyphKey = QString::fromUtf8("%1-%2-%3-%4-%5").arg(info.fontFamily)
                                            .arg(info.fontFaceName)
                                            .arg(info.glyphId)
                                            .arg(info.italic)
                                            .arg(info.verticalMetric);
    if (m_outlineCacheMap.find(glyphKey) != m_outlineCacheMap.end())
        return m_outlineCacheMap[glyphKey];

    int error;
    FT_Face face = getFontFace(info.fontPath, info.fontFamily, info.fontFaceName);
    if (!face)
        return nullptr;
    FT_Matrix  ft_matrix = {65536, 0, 0, 65536};
    if (info.italic)
    {
        ft_matrix.xy = info.italicFactor * 65536;
    }
    if (info.verticalMetric)
    {
        FT_Matrix  ft_matrix_rotate = {0, -65536, 65536, 0};
        FT_Matrix_Multiply(&ft_matrix_rotate, &ft_matrix);
    }
    if (!face->internal)
        return nullptr;
    FT_Set_Transform(face, &ft_matrix, 0);
    error = FT_Set_Pixel_Sizes(face, 0, 64);
    if (FT_Err_Ok != error)
    {
        return nullptr;
    }

    int load_flags = FT_LOAD_NO_BITMAP;
    if(!(face->face_flags & FT_FACE_FLAG_SFNT) || !FT_IS_TRICKY(face)) {
        load_flags |= FT_LOAD_NO_HINTING;
    }

    error = FT_Load_Glyph(face, info.glyphId, load_flags);
    if (FT_Err_Ok != error)
    {
        return nullptr;
    }

    FT_Outline_Funcs funcs;
    funcs.move_to = _Outline_MoveTo;
    funcs.line_to = _Outline_LineTo;
    funcs.conic_to = _Outline_ConicTo;
    funcs.cubic_to = _Outline_CubicTo;
    funcs.shift = 0;
    funcs.delta = 0;
    OUTLINE_PARAMS *params = new OUTLINE_PARAMS;
    params->m_bCount = 1;
    params->m_PointCount = 0;
    error = FT_Outline_Decompose(FT_Get_Glyph_Outline(face), &funcs, params);
    if (params->m_PointCount == 0 || FT_Err_Ok != error) {
        delete params;
        return nullptr;
    }

    KSP_PATHPOINT* pPoints = new KSP_PATHPOINT[params->m_PointCount];
    params->m_bCount = 0;
    params->m_PointCount = 0;
    params->m_pPoints = pPoints;
    params->m_CurX = params->m_CurY = 0;
    params->m_CoordUnit = 64 * 64.0;
    error = FT_Outline_Embolden(FT_Get_Glyph_Outline(face), 110);
    if (FT_Err_Ok != error)
    {
        delete[] pPoints;
        delete params;
        return nullptr;
    }
    error = FT_Outline_Decompose(FT_Get_Glyph_Outline(face), &funcs, params);
    if (FT_Err_Ok != error)
    {
        delete[] pPoints;
        delete params;
        return nullptr;
    }
    m_outlineCacheMap.insert(glyphKey, params);
    return params;
}

FT_Face freeTypeFontMgr::getFontFace(const QString &fontFile, const QString &family, const QString &styleName)
{
    if (m_faceMap.find(fontFile) != m_faceMap.end())
        return m_faceMap[fontFile];

    int iError = 0;
    if (!m_ftLibrary)
    {
        iError = FT_Init_FreeType(&m_ftLibrary);
        if(iError)
            return nullptr;
    }
    FT_Face face;
    bool bFind = false;
    iError = FT_New_Face(m_ftLibrary, fontFile.toUtf8().data(), -1, &face);
    if(iError || !face)
        return nullptr;
    int num_faces = face->num_faces;
    FT_Done_Face(face);

    std::vector<FT_Face> vecFaces;
    for (int faceIndex = 0; faceIndex < num_faces; faceIndex++)
    {
        iError = FT_New_Face(m_ftLibrary, fontFile.toUtf8().data(), faceIndex, &face);
        if(iError || !face)
        {
            if (face)
                FT_Done_Face(face);
            return nullptr;
        }

        if (face->family_name)
        {
            if(family == QString::fromUtf8(face->family_name))
            {
                vecFaces.push_back(face);
                continue;
            }
        }
        FT_Done_Face(face);
    }

    if (vecFaces.size() > 1)
    {
        if (styleName.isEmpty())
        {
            for(FT_Face iter : vecFaces)
            {
                FT_Done_Face(iter);
            }
            return nullptr;
        }
        
        for (int i = 0; i < vecFaces.size(); i++)
        {
            if (styleName == QString::fromUtf8(vecFaces[i]->style_name))
            {
                face = vecFaces[i];
                bFind = true;
            }
            else
            {
                FT_Done_Face(vecFaces[i]);
            }
        }
    }
    else if (vecFaces.size() == 1)
    {
        face = vecFaces[0];
        bFind = true;
    }

    if (!bFind)
        return nullptr;

    m_faceMap.insert(fontFile, face);
    return face;
}

freeTypeFontMgr* freeTypeFontMgr::instance()
{
    static freeTypeFontMgr *_instance = nullptr;
    if (nullptr == _instance)
    {
        _instance = new freeTypeFontMgr();
    }
    return _instance;
}

