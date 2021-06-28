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

#include <Shlobj.h>
#include "qwindowdragdrophelper.h"

QT_BEGIN_NAMESPACE

/*static*/ bool QDragDropHelper::IsRegisteredFormat(CLIPFORMAT cfFormat, LPCWSTR lpszFormat, bool bRegister /*= false*/)
{
    bool bRet = false;
    if (cfFormat >= 0xC000) {
	    if (bRegister)
		    bRet = (cfFormat == RegisterFormat(lpszFormat));
	    else {
		    WCHAR lpszName[MAX_PATH];
		    if (::GetClipboardFormatName(cfFormat, lpszName, ARRAYSIZE(lpszName)))
			    bRet = (0 == _wcsicmp(lpszFormat, lpszName));
	    }
    }
    return bRet;
}

/*static*/ DROPIMAGETYPE QDragDropHelper::DropEffectToDropImage(DROPEFFECT dwEffect)
{
    DROPIMAGETYPE nImageType = DROPIMAGE_INVALID;
    dwEffect &= ~DROPEFFECT_SCROLL;
    if (DROPEFFECT_NONE == dwEffect)
	    nImageType = DROPIMAGE_NONE;
    else if (dwEffect & DROPEFFECT_MOVE)
	    nImageType = DROPIMAGE_MOVE;
    else if (dwEffect & DROPEFFECT_COPY)
	    nImageType = DROPIMAGE_COPY;
    else if (dwEffect & DROPEFFECT_LINK)
	    nImageType = DROPIMAGE_LINK;
    return nImageType;
}

/*static*/ bool QDragDropHelper::GetGlobalData(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat, FORMATETC& FormatEtc, STGMEDIUM& StgMedium)
{
    FormatEtc.cfFormat = RegisterFormat(lpszFormat);
    FormatEtc.ptd = NULL;
    FormatEtc.dwAspect = DVASPECT_CONTENT;
    FormatEtc.lindex = -1;
    FormatEtc.tymed = TYMED_HGLOBAL;
    HRESULT hr = pIDataObj->QueryGetData(&FormatEtc);
    if (S_OK != hr)
        return false;

    hr = pIDataObj->GetData(&FormatEtc, &StgMedium);
    if (S_OK != hr)
        return false;

    if(TYMED_HGLOBAL != StgMedium.tymed) {
        ::ReleaseStgMedium(&StgMedium);
        return false;
    }

    // STGMEDIUMs with pUnkForRelease need to be copied first
    if (StgMedium.pUnkForRelease != NULL) {
        STGMEDIUM stgMediumDest;
        stgMediumDest.tymed = TYMED_NULL;
        stgMediumDest.pUnkForRelease = NULL;
        if (!CopyStgMedium(FormatEtc.cfFormat, &stgMediumDest, &StgMedium)) {
            ::ReleaseStgMedium(&StgMedium);
            return false;
        }

        ::ReleaseStgMedium(&StgMedium);
        StgMedium = stgMediumDest;
    }

    return true;
}

/*static*/ DWORD QDragDropHelper::GetGlobalDataDWord(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat)
{
    DWORD dwData = 0;
    FORMATETC FormatEtc = { 0 };
    STGMEDIUM StgMedium = { 0 };
    if (GetGlobalData(pIDataObj, lpszFormat, FormatEtc, StgMedium)) {
	    dwData = *(static_cast<LPDWORD>(::GlobalLock(StgMedium.hGlobal)));
	    ::GlobalUnlock(StgMedium.hGlobal);
	    ::ReleaseStgMedium(&StgMedium);
    }
    return dwData;
}

/*static*/ bool QDragDropHelper::SetGlobalDataDWord(LPDATAOBJECT pIDataObj, LPCWSTR lpszFormat, DWORD dwData, bool bForce /*= true*/)
{
    bool bSet = false;
    FORMATETC FormatEtc;
    STGMEDIUM StgMedium;
    if (GetGlobalData(pIDataObj, lpszFormat, FormatEtc, StgMedium)) {
	    LPDWORD pData = static_cast<LPDWORD>(::GlobalLock(StgMedium.hGlobal));
	    bSet = (*pData != dwData);
	    *pData = dwData;
	    ::GlobalUnlock(StgMedium.hGlobal);
	    if (bSet)
		    bSet = (S_OK == pIDataObj->SetData(&FormatEtc, &StgMedium, TRUE));
	    if (!bSet)									// not changed or setting failed
            ::ReleaseStgMedium(&StgMedium);
    }
    else if (dwData || bForce) {
	    StgMedium.hGlobal = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
	    if (StgMedium.hGlobal) {
		    LPDWORD pData = static_cast<LPDWORD>(::GlobalLock(StgMedium.hGlobal));
		    *pData = dwData;
		    ::GlobalUnlock(StgMedium.hGlobal);
		    StgMedium.tymed = TYMED_HGLOBAL;
		    StgMedium.pUnkForRelease = NULL;
		    bSet = (S_OK == pIDataObj->SetData(&FormatEtc, &StgMedium, TRUE));
		    if (!bSet)
			    ::GlobalFree(StgMedium.hGlobal);
	    }
    }
    return bSet;
}

/*static*/void QDragDropHelper::ClearDescription(DROPDESCRIPTION *pDropDescription)
{
	if (!pDropDescription)
        return;

	ZeroMemory(pDropDescription, sizeof(DROPDESCRIPTION));
    pDropDescription->type = DROPIMAGE_INVALID;
}

LPFORMATETC QDragDropHelper::FillFormatEtc(
	LPFORMATETC lpFormatEtc, CLIPFORMAT cfFormat, LPFORMATETC lpFormatEtcFill)
{
    //Q_ASSERT(lpFormatEtcFill != NULL);
    if (lpFormatEtc == NULL && cfFormat != 0) {
	    lpFormatEtc = lpFormatEtcFill;
	    lpFormatEtc->cfFormat = cfFormat;
	    lpFormatEtc->ptd = NULL;
	    lpFormatEtc->dwAspect = DVASPECT_CONTENT;
	    lpFormatEtc->lindex = -1;
	    lpFormatEtc->tymed = (DWORD)-1;
    }
    return lpFormatEtc;
}

BOOL QDragDropHelper::CopyStgMedium(
    CLIPFORMAT cfFormat, LPSTGMEDIUM lpDest, LPSTGMEDIUM lpSource)
{
    if (lpDest->tymed == TYMED_NULL) {
	    //Q_ASSERT(lpSource->tymed != TYMED_NULL);
	    switch (lpSource->tymed)
	    {
	    case TYMED_ENHMF:
	    case TYMED_HGLOBAL:
		    //Q_ASSERT(sizeof(HGLOBAL) == sizeof(HENHMETAFILE));
		    lpDest->tymed = lpSource->tymed;
		    lpDest->hGlobal = NULL;
		    break;  // fall through to CopyGlobalMemory case

	    case TYMED_ISTREAM:
		    lpDest->pstm = lpSource->pstm;
		    lpDest->pstm->AddRef();
		    lpDest->tymed = TYMED_ISTREAM;
		    return TRUE;

	    case TYMED_ISTORAGE:
		    lpDest->pstg = lpSource->pstg;
		    lpDest->pstg->AddRef();
		    lpDest->tymed = TYMED_ISTORAGE;
		    return TRUE;

	    case TYMED_MFPICT:
	    {
		    // copy LPMETAFILEPICT struct + embedded HMETAFILE
		    HGLOBAL hDest = CopyGlobalMemory(NULL, lpSource->hGlobal);
		    if (hDest == NULL)
			    return FALSE;
		    LPMETAFILEPICT lpPict = (LPMETAFILEPICT)::GlobalLock(hDest);
		    //Q_ASSERT(lpPict != NULL);
		    lpPict->hMF = ::CopyMetaFile(lpPict->hMF, NULL);
		    if (lpPict->hMF == NULL) {
			    ::GlobalUnlock(hDest);
			    ::GlobalFree(hDest);
			    return FALSE;
		    }
		    ::GlobalUnlock(hDest);

		    lpDest->hGlobal = hDest;
		    lpDest->tymed = TYMED_MFPICT;
	    }
	    return TRUE;

	    case TYMED_GDI:
		    lpDest->tymed = TYMED_GDI;
		    lpDest->hGlobal = NULL;
		    break;

	    case TYMED_FILE:
	    {
		    lpDest->tymed = TYMED_FILE;
		    //Q_ASSERT(lpSource->lpszFileName != NULL);
		    if (lpSource->lpszFileName == NULL) {
			    return FALSE;
		    }
		    UINT cbSrc = static_cast<UINT>(wcslen(lpSource->lpszFileName));
		    LPOLESTR szFileName = (LPOLESTR)::CoTaskMemAlloc((cbSrc + 1) * sizeof(OLECHAR));
		    lpDest->lpszFileName = szFileName;
		    if (szFileName == NULL)
			    return FALSE;

		    memcpy_s(szFileName, (cbSrc + 1) * sizeof(OLECHAR),
			    lpSource->lpszFileName, (cbSrc + 1) * sizeof(OLECHAR));
		    return TRUE;
	    }

	    default:
		    return FALSE;
	    }
    }
    //Q_ASSERT(lpDest->tymed == lpSource->tymed);

    switch (lpSource->tymed)
    {
    case TYMED_HGLOBAL:
    {
	    HGLOBAL hDest = CopyGlobalMemory(lpDest->hGlobal,
		    lpSource->hGlobal);
	    if (hDest == NULL)
		    return FALSE;

	    lpDest->hGlobal = hDest;
    }
    return TRUE;

    case TYMED_ISTREAM:
    {
	    //Q_ASSERT(lpDest->pstm != NULL);
	    //Q_ASSERT(lpSource->pstm != NULL);
	    STATSTG stat;
	    if (lpSource->pstm->Stat(&stat, STATFLAG_NONAME) != S_OK)
		    return FALSE;
	    //Q_ASSERT(stat.pwcsName == NULL);

	    LARGE_INTEGER zero = { 0, 0 };
	    lpDest->pstm->Seek(zero, STREAM_SEEK_SET, NULL);
	    lpSource->pstm->Seek(zero, STREAM_SEEK_SET, NULL);

	    if (lpSource->pstm->CopyTo(lpDest->pstm, stat.cbSize,
		    NULL, NULL) != NULL)
		    return FALSE;

	    lpDest->pstm->Seek(zero, STREAM_SEEK_SET, NULL);
	    lpSource->pstm->Seek(zero, STREAM_SEEK_SET, NULL);
    }
    return TRUE;

    case TYMED_ISTORAGE:
    {
	    if (lpSource->pstg->CopyTo(0, NULL, NULL, lpDest->pstg) != S_OK)
		    return FALSE;
    }
    return TRUE;

    case TYMED_FILE:
    {
	    return CopyFile(lpSource->lpszFileName, lpDest->lpszFileName, FALSE);
    }

    case TYMED_ENHMF:
    case TYMED_GDI:
    {
	    if (lpDest->hGlobal != NULL)
		    return FALSE;

	    lpDest->hGlobal = OleDuplicateData(lpSource->hGlobal, cfFormat, 0);
	    if (lpDest->hGlobal == NULL)
		    return FALSE;
	    return TRUE;
    }

    default:
	    return FALSE;
    }
}

HGLOBAL QDragDropHelper::CopyGlobalMemory(HGLOBAL hDest, HGLOBAL hSource)
{
    //Q_ASSERT(hSource != NULL);
    ULONG_PTR nSize = ::GlobalSize(hSource);
    if (hDest == NULL) {
	    hDest = ::GlobalAlloc(GMEM_SHARE | GMEM_MOVEABLE, nSize);
	    if (hDest == NULL)
		    return NULL;
    }
    else if (nSize > ::GlobalSize(hDest)) {
	    return NULL;
    }

    // copy the bits
    LPVOID lpSource = ::GlobalLock(hSource);
    LPVOID lpDest = ::GlobalLock(hDest);
    memcpy_s(lpDest, ::GlobalSize(hDest), lpSource, (ULONG)nSize);
    ::GlobalUnlock(hDest);
    ::GlobalUnlock(hSource);

    return hDest;
}

void QDragDropHelper::CopyFormatEtc(LPFORMATETC petcDest, LPFORMATETC petcSrc)
{
    petcDest->cfFormat = petcSrc->cfFormat;
    petcDest->ptd = CopyTargetDevice(petcSrc->ptd);
    petcDest->dwAspect = petcSrc->dwAspect;
    petcDest->lindex = petcSrc->lindex;
    petcDest->tymed = petcSrc->tymed;
}

DVTARGETDEVICE* QDragDropHelper::CopyTargetDevice(DVTARGETDEVICE* ptdSrc)
{
    if (ptdSrc == NULL)
	    return NULL;

    DVTARGETDEVICE* ptdDest =
	    (DVTARGETDEVICE*)CoTaskMemAlloc(ptdSrc->tdSize);
    if (ptdDest == NULL)
	    return NULL;

    memcpy_s(ptdDest, (size_t)ptdSrc->tdSize,
	    ptdSrc, (size_t)ptdSrc->tdSize);
    return ptdDest;
}

HBITMAP QDragDropHelper::ReplaceBlackBit(HBITMAP hBitmap)
{
    DIBSECTION ds;
    int nSize = ::GetObject(hBitmap, sizeof(ds), &ds);
    if (nSize && nSize < sizeof(ds) && ds.dsBm.bmBitsPixel >= 16) {
	    HBITMAP hCopy = (HBITMAP)::CopyImage(
		    hBitmap, IMAGE_BITMAP, 0, 0,
		    LR_CREATEDIBSECTION | LR_COPYDELETEORG);
	    if (NULL != hCopy) {
		    hBitmap = hCopy;
		    nSize = ::GetObject(hBitmap, sizeof(ds), &ds);
	    }
    }
    if (sizeof(ds) == nSize && ds.dsBm.bmBits && ds.dsBm.bmBitsPixel >= 16) {
	    DWORD dwColor = 0x010101;

	    if (ds.dsBitfields[0] && ds.dsBitfields[1] && ds.dsBitfields[2]) {
		    auto GetLowestColorBit = [](DWORD dwBitField) -> DWORD {
			    if (0 == dwBitField)
				    return 0;
			    DWORD dwMask = 1;
			    while (0 == (dwBitField & 1)) {
				    dwBitField >>= 1;
				    dwMask <<= 1;
			    }
			    return dwMask;
		    };
		    dwColor = GetLowestColorBit(ds.dsBitfields[0]) |
			    GetLowestColorBit(ds.dsBitfields[1]) |
			    GetLowestColorBit(ds.dsBitfields[2]);
	    }
	    LPBYTE pBits = static_cast<LPBYTE>(ds.dsBm.bmBits);
	    if (16 == ds.dsBm.bmBitsPixel) {
		    WORD wColor = static_cast<WORD>(dwColor);
		    for (long i = 0; i < ds.dsBm.bmHeight; i++) {
			    LPWORD pPixel = reinterpret_cast<LPWORD>(pBits);
			    for (long j = 0; j < ds.dsBm.bmWidth; j++) {
				    if (0 == *pPixel)
					    *pPixel = wColor;
				    ++pPixel;
			    }
			    pBits += ds.dsBm.bmWidthBytes;		// begin of next/previous line
		    }
	    }
	    else {
		    int nBytePerPixel = ds.dsBm.bmBitsPixel / 8;
		    BYTE ColorLow = static_cast<BYTE>(dwColor);
		    BYTE ColorMid = static_cast<BYTE>(dwColor >> 8);
		    BYTE ColorHigh = static_cast<BYTE>(dwColor >> 16);
		    for (long i = 0; i < ds.dsBm.bmHeight; i++) {
			    LPBYTE pPixel = pBits;
			    for (long j = 0; j < ds.dsBm.bmWidth; j++) {
				    if (0 == pPixel[0] && 0 == pPixel[1] && 0 == pPixel[2]) {
					    pPixel[0] = ColorLow;
					    pPixel[1] = ColorMid;
					    pPixel[2] = ColorHigh;
				    }
				    pPixel += nBytePerPixel;
			    }
			    pBits += ds.dsBm.bmWidthBytes;
		    }
	    }
    }
    return hBitmap;
}

BOOL QDragDropHelper::isAppThemed()
{
    typedef BOOL(__stdcall *PFNISAPPTHEMED)();

	static BOOL bAppThemed = FALSE;
    static bool bLoaded = false;
    if (bLoaded)
        return bAppThemed;

	bLoaded = true;
    HMODULE hModule = ::LoadLibraryW(L"UxTheme.dll");
    if (hModule) {
        PFNISAPPTHEMED funcIsAppThemed = (PFNISAPPTHEMED)::GetProcAddress(hModule, "IsAppThemed");
        if (funcIsAppThemed) {
            bAppThemed = funcIsAppThemed();
		}

        FreeLibrary(hModule);
    }
    return bAppThemed;
}

QT_END_NAMESPACE
