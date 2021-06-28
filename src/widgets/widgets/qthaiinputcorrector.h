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

#ifndef __QTHAIINPUTCORRECTOR_H__
#define __QTHAIINPUTCORRECTOR_H__

#include <QtWidgets/qtwidgetsglobal.h>
#include <QtCore/QString>
#include <QtCore/QChar>

QT_BEGIN_NAMESPACE

enum QThaiCharType
{
    QThaiCharTypeInvalid,
    QThaiCharTypeConsonant,        // Consonant
    QThaiCharTypeConsonantVowel,   // independent vowel letter used to write Sanskrit
    QThaiCharTypeVowelLeft,        // Vowel, left
    QThaiCharTypeVowelRight,       // Vowel, right
    QThaiCharTypeVowelTop,         // Vowel, Top
    QThaiCharTypeVowelBottom,      // Vowel, Bottom
    QThaiCharTypeVowelLengthSign,
    QThaiCharTypeToneMark,         // Tone
    QThaiCharTypeSign,
    QThaiCharTypeRepetitionMark,
    QThaiCharTypeCurrencySymbol,
    QThaiCharTypeDigit,
    QThaiCharTypeNone,
};

#define QTHAI_RANGE_START 0x0E00
#define QTHAI_RANGE_END 0x0E7F

#define QTHAI_CHARACTER_SARA_A 0x0E30
#define QTHAI_CHARACTER_SARA_AA 0x0E32
#define QTHAI_CHARACTER_RU 0x0E24
#define QTHAI_CHARACTER_LU 0x0E26
#define QTHAI_CHARACTER_SARA_AM 0x0E33
#define QTHAI_CHARACTER_SARA_I 0x0E34
#define QTHAI_CHARACTER_SARA_II 0x0E35
#define QTHAI_CHARACTER_SARA_UEE 0x0E37
#define QTHAI_CHARACTER_SARA_U 0x0E38
#define QTHAI_CHARACTER_SARA_E 0x0E40
#define QTHAI_CHARACTER_SARA_AE 0x0E41
#define QTHAI_CHARACTER_SARA_O 0x0E42
#define QTHAI_CHARACTER_LAKKHANGYAO 0x0E45
#define QTHAI_CHARACTER_MAITAIKHU 0x0E47
#define QTHAI_CHARACTER_THANTHAKHAT 0x0E4C
#define QTHAI_CHARACTER_NIKHAHIT 0x0E4D
#define QTHAI_CHARACTER_YAMAKKAN 0x0E4E

class QThaiInputCorrector
{
public:
    QThaiInputCorrector()
        : m_vowelLeft(0)
        , m_consonant(0)
        , m_tone(0)
        , m_vowel1(0)
        , m_vowel2(0)
        , m_srcLen(0)
        , m_valid(false)
    {
        memset(&mask, 0, sizeof(mask));
    }

    void parse(const ushort* pText, const int sel, ushort ch)
    {
        init();

        QThaiCharType chType = getThaiCharType(ch);
        switch (chType)
        {
        case QThaiCharTypeDigit:
        case QThaiCharTypeRepetitionMark:
        case QThaiCharTypeCurrencySymbol:
        case QThaiCharTypeConsonantVowel:
            if (sel <= 0 || getThaiCharType(pText[sel - 1]) != QThaiCharTypeVowelLeft)
            {
                setValid(true);
                setConsonant(ch);
            }
            break;
        case QThaiCharTypeNone:
        case QThaiCharTypeInvalid:
        case QThaiCharTypeConsonant:
            setValid(true);
            setConsonant(ch);
            break;
        case QThaiCharTypeToneMark:
            parseSyllable(pText, sel);
            if (valid())
                inputTone(ch);
            break;
        case QThaiCharTypeVowelLengthSign:
            if (sel > 0 && getThaiCharType(pText[sel - 1]) == QThaiCharTypeConsonantVowel)
            {
                setValid(true);
                setVowel1(ch);
            }
            else if (sel > 1 && pText[sel - 1] == QTHAI_CHARACTER_SARA_AA &&
                getThaiCharType(pText[sel - 2]) == QThaiCharTypeConsonantVowel)
            {
                setValid(true);
                setVowel1(ch);
                setSrcLen(1);
            }
            break;
        case QThaiCharTypeSign:
            correctSign(pText, sel, ch);
            break;
        case QThaiCharTypeVowelLeft:
        {
            int len = 0;
            ushort dst = ch;
            int prev = sel - 1;
            if (prev >= 0)
            {
                QThaiCharType prevType = getThaiCharType(pText[prev]);
                if (prevType == QThaiCharTypeVowelLeft)
                {
                    len = 1;
                    if (pText[prev] == QTHAI_CHARACTER_SARA_E && ch == pText[prev])
                        dst = QTHAI_CHARACTER_SARA_AE;
                }
            }
            setValid(true);
            setSrcLen(len);
            setVowelLeft(dst);
        }
        break;
        case QThaiCharTypeVowelTop:
        case QThaiCharTypeVowelRight:
        case QThaiCharTypeVowelBottom:
            parseSyllable(pText, sel);
            if (valid())
            {
                if (mask.vowel1 &&
                    m_vowel1 == QTHAI_CHARACTER_NIKHAHIT &&
                    ch == QTHAI_CHARACTER_SARA_AA)
                    inputVowel(QTHAI_CHARACTER_SARA_AM);
                else
                    inputVowel(ch);
            }
            else if (ch == QTHAI_CHARACTER_SARA_AA)
            {
                if (sel > 0 && getThaiCharType(pText[sel - 1]) == QThaiCharTypeConsonantVowel)
                {
                    setValid(true);
                    setVowel1(ch);
                }
                else if (sel > 1 && pText[sel - 1] == QTHAI_CHARACTER_LAKKHANGYAO &&
                    getThaiCharType(pText[sel - 2]) == QThaiCharTypeConsonantVowel)
                {
                    setValid(true);
                    setVowel1(ch);
                    setSrcLen(1);
                }
            }
            break;
        default:
            Q_ASSERT(false);
            break;;
        }
    }

    QString value()
    {
        const int maxLen = 6;
        QChar txt[maxLen] = { 0 };
        int i = 0;
        if (mask.vowelLeft)
            txt[i++] = m_vowelLeft;
        if (mask.consonant)
            txt[i++] = m_consonant;
        if (mask.vowel1)
        {
            if (getThaiCharType(m_vowel1) == QThaiCharTypeVowelRight)
            {
                if (mask.tone)
                    txt[i++] = m_tone;
                txt[i++] = m_vowel1;
            }
            else
            {
                txt[i++] = m_vowel1;
                if (mask.tone)
                    txt[i++] = m_tone;
            }

            if (mask.vowel2)
                txt[i++] = m_vowel2;
        }
        else if (mask.tone)
        {
            txt[i++] = m_tone;
        }
        return QString(txt);
    }

    bool valid() { return m_valid; }
    long getSrcLen() { return m_valid ? m_srcLen : 0; }

public:
    static QThaiCharType getThaiCharType(ushort ch)
    {
        static QThaiCharType s_QThaiCharType[128] = {
            QThaiCharTypeNone/*0x0E00*/, QThaiCharTypeConsonant/*0x0E10*/, QThaiCharTypeConsonant/*0x0E20*/, QThaiCharTypeVowelRight/*0x0E30*/, QThaiCharTypeVowelLeft/*0x0E40*/, QThaiCharTypeDigit/*0x0E50*/, QThaiCharTypeNone/*0x0E60*/, QThaiCharTypeNone/*0x0E70*/,
            QThaiCharTypeConsonant/*0x0E01*/, QThaiCharTypeConsonant/*0x0E11*/, QThaiCharTypeConsonant/*0x0E21*/, QThaiCharTypeVowelTop/*0x0E31*/, QThaiCharTypeVowelLeft/*0x0E41*/, QThaiCharTypeDigit/*0x0E51*/, QThaiCharTypeNone/*0x0E61*/, QThaiCharTypeNone/*0x0E71*/,
            QThaiCharTypeConsonant/*0x0E02*/, QThaiCharTypeConsonant/*0x0E12*/, QThaiCharTypeConsonant/*0x0E22*/, QThaiCharTypeVowelRight/*0x0E32*/, QThaiCharTypeVowelLeft/*0x0E42*/, QThaiCharTypeDigit/*0x0E52*/, QThaiCharTypeNone/*0x0E62*/, QThaiCharTypeNone/*0x0E72*/,
            QThaiCharTypeConsonant/*0x0E03*/, QThaiCharTypeConsonant/*0x0E13*/, QThaiCharTypeConsonant/*0x0E23*/, QThaiCharTypeVowelRight/*0x0E33*/, QThaiCharTypeVowelLeft/*0x0E43*/, QThaiCharTypeDigit/*0x0E53*/, QThaiCharTypeNone/*0x0E63*/, QThaiCharTypeNone/*0x0E73*/,
            QThaiCharTypeConsonant/*0x0E04*/, QThaiCharTypeConsonant/*0x0E14*/, QThaiCharTypeConsonantVowel/*0x0E24*/, QThaiCharTypeVowelTop/*0x0E34*/, QThaiCharTypeVowelLeft/*0x0E44*/, QThaiCharTypeDigit/*0x0E54*/, QThaiCharTypeNone/*0x0E64*/, QThaiCharTypeNone/*0x0E74*/,
            QThaiCharTypeConsonant/*0x0E05*/, QThaiCharTypeConsonant/*0x0E15*/, QThaiCharTypeConsonant/*0x0E25*/, QThaiCharTypeVowelTop/*0x0E35*/, QThaiCharTypeVowelLengthSign/*0x0E45*/, QThaiCharTypeDigit/*0x0E55*/, QThaiCharTypeNone/*0x0E65*/, QThaiCharTypeNone/*0x0E75*/,
            QThaiCharTypeConsonant/*0x0E06*/, QThaiCharTypeConsonant/*0x0E16*/, QThaiCharTypeConsonantVowel/*0x0E26*/, QThaiCharTypeVowelTop/*0x0E36*/, QThaiCharTypeRepetitionMark/*0x0E46*/, QThaiCharTypeDigit/*0x0E56*/, QThaiCharTypeNone/*0x0E66*/, QThaiCharTypeNone/*0x0E76*/,
            QThaiCharTypeConsonant/*0x0E07*/, QThaiCharTypeConsonant/*0x0E17*/, QThaiCharTypeConsonant/*0x0E27*/, QThaiCharTypeVowelTop/*0x0E37*/, QThaiCharTypeVowelTop/*0x0E47*/, QThaiCharTypeDigit/*0x0E57*/, QThaiCharTypeNone/*0x0E67*/, QThaiCharTypeNone/*0x0E77*/,
            QThaiCharTypeConsonant/*0x0E08*/, QThaiCharTypeConsonant/*0x0E18*/, QThaiCharTypeConsonant/*0x0E28*/, QThaiCharTypeVowelBottom/*0x0E38*/, QThaiCharTypeToneMark/*0x0E48*/, QThaiCharTypeDigit/*0x0E58*/, QThaiCharTypeNone/*0x0E68*/, QThaiCharTypeNone/*0x0E78*/,
            QThaiCharTypeConsonant/*0x0E09*/, QThaiCharTypeConsonant/*0x0E19*/, QThaiCharTypeConsonant/*0x0E29*/, QThaiCharTypeVowelBottom/*0x0E39*/, QThaiCharTypeToneMark/*0x0E49*/, QThaiCharTypeDigit/*0x0E59*/, QThaiCharTypeNone/*0x0E69*/, QThaiCharTypeNone/*0x0E79*/,
            QThaiCharTypeConsonant/*0x0E0A*/, QThaiCharTypeConsonant/*0x0E1A*/, QThaiCharTypeConsonant/*0x0E2A*/, QThaiCharTypeVowelBottom/*0x0E3A*/, QThaiCharTypeToneMark/*0x0E4A*/, QThaiCharTypeSign/*0x0E5A*/, QThaiCharTypeNone/*0x0E6A*/, QThaiCharTypeNone/*0x0E7A*/,
            QThaiCharTypeConsonant/*0x0E0B*/, QThaiCharTypeConsonant/*0x0E1B*/, QThaiCharTypeConsonant/*0x0E2B*/, QThaiCharTypeNone/*0x0E3B*/, QThaiCharTypeToneMark/*0x0E4B*/, QThaiCharTypeSign/*0x0E5B*/, QThaiCharTypeNone/*0x0E6B*/, QThaiCharTypeNone/*0x0E7B*/,
            QThaiCharTypeConsonant/*0x0E0C*/, QThaiCharTypeConsonant/*0x0E1C*/, QThaiCharTypeConsonant/*0x0E2C*/, QThaiCharTypeNone/*0x0E3C*/, QThaiCharTypeSign/*0x0E4C*/, QThaiCharTypeNone/*0x0E5C*/, QThaiCharTypeNone/*0x0E6C*/, QThaiCharTypeNone/*0x0E7C*/,
            QThaiCharTypeConsonant/*0x0E0D*/, QThaiCharTypeConsonant/*0x0E1D*/, QThaiCharTypeConsonant/*0x0E2D*/, QThaiCharTypeNone/*0x0E3D*/, QThaiCharTypeSign/*0x0E4D*/, QThaiCharTypeNone/*0x0E5D*/, QThaiCharTypeNone/*0x0E6D*/, QThaiCharTypeNone/*0x0E7D*/,
            QThaiCharTypeConsonant/*0x0E0E*/, QThaiCharTypeConsonant/*0x0E1E*/, QThaiCharTypeConsonant/*0x0E2E*/, QThaiCharTypeNone/*0x0E3E*/, QThaiCharTypeSign/*0x0E4E*/, QThaiCharTypeNone/*0x0E5E*/, QThaiCharTypeNone/*0x0E6E*/, QThaiCharTypeNone/*0x0E7E*/,
            QThaiCharTypeConsonant/*0x0E0F*/, QThaiCharTypeConsonant/*0x0E1F*/, QThaiCharTypeSign/*0x0E2F*/, QThaiCharTypeCurrencySymbol/*0x0E3F*/, QThaiCharTypeSign/*0x0E4F*/, QThaiCharTypeNone/*0x0E5F*/, QThaiCharTypeNone/*0x0E6F*/, QThaiCharTypeNone/*0x0E7F*/,
        };

        if (ch < QTHAI_RANGE_START || ch > QTHAI_RANGE_END)
            return QThaiCharTypeInvalid;
        int offset = ch - QTHAI_RANGE_START;
        offset = (offset % 16) * 8 + offset / 16;
        return s_QThaiCharType[offset];
    }

    static bool reserveLeftVowel(ushort v, ushort lv)
    {
        if (v == QTHAI_CHARACTER_SARA_A)
            return lv == QTHAI_CHARACTER_SARA_AE ||
            lv == QTHAI_CHARACTER_SARA_O ||
            lv == QTHAI_CHARACTER_SARA_E;

        if (v == QTHAI_CHARACTER_MAITAIKHU)
            return lv == QTHAI_CHARACTER_SARA_E ||
            lv == QTHAI_CHARACTER_SARA_AE;

        if (v == QTHAI_CHARACTER_SARA_AA ||
            v == QTHAI_CHARACTER_SARA_I ||
            v == QTHAI_CHARACTER_SARA_II ||
            v == QTHAI_CHARACTER_SARA_UEE)
            return lv == QTHAI_CHARACTER_SARA_E;

        return false;
    }

private:
    void parseSyllable(const ushort* pText, int sel)
    {
        if (sel <= 0)
            return;

        int len = 0;
        int idx = sel;
        while (--idx >= 0)
        {
            QThaiCharType type = getThaiCharType(pText[idx]);
            if (type == QThaiCharTypeVowelLeft)
            {
                if (!mask.vowelLeft)
                {
                    setVowelLeft(pText[idx]);
                    ++len;
                }
                break;
            }
            if (valid())
            {
                break;
            }
            else if (type == QThaiCharTypeConsonant)
            {
                setConsonant(pText[idx]);
                setValid(true);
            }
            else if (type == QThaiCharTypeToneMark)
            {
                if (!mask.tone)
                {
                    setTone(pText[idx]);
                }
                else
                {
                    break;
                }
            }
            else if (type == QThaiCharTypeVowelLeft ||
                type == QThaiCharTypeVowelRight ||
                type == QThaiCharTypeVowelTop ||
                type == QThaiCharTypeVowelBottom)
            {
                if (!mask.vowel1)
                {
                    setVowel1(pText[idx]);
                }
                else if (idx == sel - 2 &&
                    pText[idx] == QTHAI_CHARACTER_SARA_AA &&
                    pText[idx + 1] == QTHAI_CHARACTER_SARA_A)
                {
                    setVowel1(pText[idx]);
                    setVowel2(pText[idx + 1]);
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (!mask.tone && pText[idx] == QTHAI_CHARACTER_THANTHAKHAT)
                    setTone(pText[idx]);
                else if (!mask.vowel1 &&
                    (pText[idx] == QTHAI_CHARACTER_NIKHAHIT ||
                        pText[idx] == QTHAI_CHARACTER_YAMAKKAN))
                    setVowel1(pText[idx]);
                else
                    break;
            }
            ++len;
        }
        setSrcLen(len);
    }

    void correctSign(const ushort* pText, const int sel, ushort ch)
    {
        switch (ch)
        {
        case QTHAI_CHARACTER_THANTHAKHAT:
            parseSyllable(pText, sel);
            if (!valid())
                return;
            setTone(ch);
            if (mask.vowel1 &&
                m_vowel1 != QTHAI_CHARACTER_SARA_U &&
                m_vowel1 != QTHAI_CHARACTER_SARA_I)
                removeVowel1();
            removeVowel2();
            removeVowelLeft();
            break;
        case QTHAI_CHARACTER_NIKHAHIT:
        case QTHAI_CHARACTER_YAMAKKAN:
            parseSyllable(pText, sel);
            if (!valid())
                return;
            setVowel1(ch);
            removeTone();
            removeVowel2();
            removeVowelLeft();
            break;
        default:
            if (sel > 0 && getThaiCharType(pText[sel - 1]) == QThaiCharTypeVowelLeft)
                return;
            setValid(true);
            setConsonant(ch);
            break;
        }
    }

    void inputVowel(ushort ch)
    {
        Q_ASSERT(getThaiCharType(ch) == QThaiCharTypeVowelRight
            || getThaiCharType(ch) == QThaiCharTypeVowelTop
            || getThaiCharType(ch) == QThaiCharTypeVowelBottom);

        if (mask.vowel1 && m_vowel1 == QTHAI_CHARACTER_SARA_AA &&
            ch == QTHAI_CHARACTER_SARA_A &&
            (!mask.tone || (mask.vowelLeft && m_vowelLeft == QTHAI_CHARACTER_SARA_E)))
        {
            if (mask.vowel2)
            {
                setValid(false);
                return;
            }
            else
            {
                setVowel2(ch);
            }
        }
        else
        {
            setVowel1(ch);
            removeVowel2();
            if (mask.tone &&
                (ch == QTHAI_CHARACTER_MAITAIKHU ||
                (m_tone == QTHAI_CHARACTER_THANTHAKHAT &&
                    ch != QTHAI_CHARACTER_SARA_I &&
                    ch != QTHAI_CHARACTER_SARA_U)))
                removeTone();
        }
        if (mask.vowelLeft && !reserveLeftVowel(ch, m_vowelLeft))
        {
            removeVowelLeft();
        }
    }

    void inputTone(ushort ch)
    {
        Q_ASSERT(getThaiCharType(ch) == QThaiCharTypeToneMark);

        setTone(ch);
        if (mask.vowel1)
        {
            if (m_vowel1 == QTHAI_CHARACTER_MAITAIKHU ||
                getThaiCharType(m_vowel1) == QThaiCharTypeSign)
                removeVowel1();
            else if (mask.vowel2 &&
                (!mask.vowelLeft ||
                    m_vowelLeft != QTHAI_CHARACTER_SARA_E))
                removeVowel2();
        }
    }

private:
    void init()
    {
        memset(&mask, 0, sizeof(mask));
        m_srcLen = 0;
    }

    void setConsonant(ushort ch)
    {
        mask.consonant = true;
        m_consonant = ch;
    }

    void setTone(ushort ch)
    {
        mask.tone = true;
        m_tone = ch;
    }

    void removeTone()
    {
        mask.tone = false;
        m_tone = 0;
    }

    void setVowelLeft(ushort ch)
    {
        mask.vowelLeft = true;
        m_vowelLeft = ch;
    }

    void removeVowelLeft()
    {
        mask.vowelLeft = false;
        m_vowelLeft = 0;
    }

    void setVowel1(ushort ch)
    {
        mask.vowel1 = true;
        m_vowel1 = ch;
    }

    void removeVowel1()
    {
        mask.vowel1 = false;
        m_vowel1 = 0;
    }

    void setVowel2(ushort ch)
    {
        mask.vowel2 = true;
        m_vowel2 = ch;
    }

    void removeVowel2()
    {
        mask.vowel2 = false;
        m_vowel2 = 0;
    }

    void setSrcLen(long v) { m_srcLen = v; }
    void setValid(bool v) { m_valid = v; }

private:
    struct
    {
        quint32 vowelLeft : 1;
        quint32 consonant : 1;
        quint32 tone : 1;
        quint32 vowel1 : 1;
        quint32 vowel2 : 1;
        quint32 reserved : 27;
    } mask;

    ushort m_vowelLeft;
    ushort m_consonant;
    ushort m_tone;
    ushort m_vowel1;
    ushort m_vowel2;
    long m_srcLen;
    bool m_valid;
};

QT_END_NAMESPACE

#endif /* __QTHAIINPUTCORRECTOR_H__ */
