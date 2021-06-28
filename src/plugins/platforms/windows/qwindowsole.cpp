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

#include "qwindowsole.h"
#include "qwindowsmime.h"
#include "qwindowscontext.h"
#include "qwindowsdescription.h"
#include "qwindowdragdrophelper.h"

#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>
#include <QtGui/qpainter.h>
#include <QtGui/qcursor.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/private/qdnd_p.h>

#include <QtCore/qmimedata.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdir.h>
#include <shlobj.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsOleDataObject
    \brief OLE data container

   The following methods are NOT supported for data transfer using the
   clipboard or drag-drop:
   \list
   \li IDataObject::SetData    -- return E_NOTIMPL
   \li IDataObject::DAdvise    -- return OLE_E_ADVISENOTSUPPORTED
   \li ::DUnadvise
   \li ::EnumDAdvise
   \li IDataObject::GetCanonicalFormatEtc -- return E_NOTIMPL
       (NOTE: must set pformatetcOut->ptd = NULL)
   \endlist

    \internal
    \ingroup qt-lighthouse-win
*/

QWindowsOleDataObject::QWindowsOleDataObject(QMimeData *mimeData) :
    data(mimeData),
    CF_PERFORMEDDROPEFFECT(RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT))
{
    qCDebug(lcQpaMime) << __FUNCTION__ << mimeData->formats();
}

QWindowsOleDataObject::~QWindowsOleDataObject() = default;

void QWindowsOleDataObject::releaseQt()
{
    data = nullptr;
}

QMimeData *QWindowsOleDataObject::mimeData() const
{
    return data.data();
}

DWORD QWindowsOleDataObject::reportedPerformedEffect() const
{
    return performedEffect;
}

STDMETHODIMP
QWindowsOleDataObject::GetData(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    HRESULT hr = ResultFromScode(DATA_E_FORMATETC);

    if (data) {
        const QWindowsMimeConverter &mc = QWindowsContext::instance()->mimeConverter();
        if (QWindowsMime *converter = mc.converterFromMime(*pformatetc, data))
            if (converter->convertFromMime(*pformatetc, data, pmedium))
                hr = ResultFromScode(S_OK);
    }

    if (QWindowsContext::verbose > 1 && lcQpaMime().isDebugEnabled())
        qCDebug(lcQpaMime) <<__FUNCTION__ << *pformatetc << "returns" << hex << showbase << quint64(hr);

    return hr;
}

STDMETHODIMP
QWindowsOleDataObject::GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    HRESULT hr = ResultFromScode(DATA_E_FORMATETC);

    if (data) {
        const QWindowsMimeConverter &mc = QWindowsContext::instance()->mimeConverter();
        if (QWindowsMime *converter = mc.converterHereFromMime(*pformatetc, data))
            if (converter->convertHereFromMime(*pformatetc, data, pmedium))
                hr = ResultFromScode(S_OK);
    }

    if (QWindowsContext::verbose > 1 && lcQpaMime().isDebugEnabled())
        qCDebug(lcQpaMime) <<__FUNCTION__ << *pformatetc << "returns" << hex << showbase << quint64(hr);

    return hr;
}

STDMETHODIMP
QWindowsOleDataObject::QueryGetData(LPFORMATETC pformatetc)
{
    HRESULT hr = ResultFromScode(DATA_E_FORMATETC);

    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) << __FUNCTION__;

    if (data) {
        const QWindowsMimeConverter &mc = QWindowsContext::instance()->mimeConverter();
        hr = mc.converterFromMime(*pformatetc, data) ?
             ResultFromScode(S_OK) : ResultFromScode(S_FALSE);
    }
    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) <<  __FUNCTION__ << " returns 0x" << hex << int(hr);
    return hr;
}

STDMETHODIMP
QWindowsOleDataObject::GetCanonicalFormatEtc(LPFORMATETC, LPFORMATETC pformatetcOut)
{
    pformatetcOut->ptd = nullptr;
    return ResultFromScode(E_NOTIMPL);
}

STDMETHODIMP
QWindowsOleDataObject::SetData(LPFORMATETC pFormatetc, STGMEDIUM *pMedium, BOOL fRelease)
{
    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) << __FUNCTION__;

    HRESULT hr = ResultFromScode(E_NOTIMPL);

    if (pFormatetc->cfFormat == CF_PERFORMEDDROPEFFECT && pMedium->tymed == TYMED_HGLOBAL) {
        DWORD * val = (DWORD*)GlobalLock(pMedium->hGlobal);
        performedEffect = *val;
        GlobalUnlock(pMedium->hGlobal);
        if (fRelease)
            ReleaseStgMedium(pMedium);
        hr = ResultFromScode(S_OK);
    }
    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) <<  __FUNCTION__ << " returns 0x" << hex << int(hr);
    return hr;
}


STDMETHODIMP
QWindowsOleDataObject::EnumFormatEtc(DWORD dwDirection, LPENUMFORMATETC FAR* ppenumFormatEtc)
{
     if (QWindowsContext::verbose > 1)
         qCDebug(lcQpaMime) << __FUNCTION__ << "dwDirection=" << dwDirection;

    if (!data)
        return ResultFromScode(DATA_E_FORMATETC);

    SCODE sc = S_OK;

    QVector<FORMATETC> fmtetcs;
    if (dwDirection == DATADIR_GET) {
        QWindowsMimeConverter &mc = QWindowsContext::instance()->mimeConverter();
        fmtetcs = mc.allFormatsForMime(data);
    } else {
        FORMATETC formatetc;
        formatetc.cfFormat = CLIPFORMAT(CF_PERFORMEDDROPEFFECT);
        formatetc.dwAspect = DVASPECT_CONTENT;
        formatetc.lindex = -1;
        formatetc.ptd = nullptr;
        formatetc.tymed = TYMED_HGLOBAL;
        fmtetcs.append(formatetc);
    }

    QWindowsOleEnumFmtEtc *enumFmtEtc = new QWindowsOleEnumFmtEtc(fmtetcs);
    *ppenumFormatEtc = enumFmtEtc;
    if (enumFmtEtc->isNull()) {
        delete enumFmtEtc;
        *ppenumFormatEtc = nullptr;
        sc = E_OUTOFMEMORY;
    }

    return ResultFromScode(sc);
}

STDMETHODIMP
QWindowsOleDataObject::DAdvise(FORMATETC FAR*, DWORD,
                       LPADVISESINK, DWORD FAR*)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}


STDMETHODIMP
QWindowsOleDataObject::DUnadvise(DWORD)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}

STDMETHODIMP
QWindowsOleDataObject::EnumDAdvise(LPENUMSTATDATA FAR*)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}


//QWindowsOleDataObjectEx
QWindowsOleDataObjectEx::QWindowsOleDataObjectEx(QMimeData *mimeData)
    : QWindowsOleDataObject(mimeData),
      m_pDragSourceHelper(nullptr),
      m_pDropDescription(nullptr),
      m_bUseDescription(false)
{
    m_pDataCache = NULL;
    m_nMaxSize = 0;
    m_nSize = 0;
    m_nGrowBy = 10;

    init();
}

QWindowsOleDataObjectEx::~QWindowsOleDataObjectEx()
{
    if (m_pDragSourceHelper)
        m_pDragSourceHelper->Release();

    if (m_pDropDescription)
        delete m_pDropDescription;
    EmptyCacheEntry();
}

bool QWindowsOleDataObjectEx::init()
{
    m_pDropDescription = new QDropDescription();
#if defined(IID_PPV_ARGS)
    ::CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&m_pDragSourceHelper));
#else
    ::CoCreateInstance(CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, IID_IDragSourceHelper,
                       reinterpret_cast<LPVOID *>(&m_pDragSourceHelper));
#endif
    if (!m_pDragSourceHelper)
        return false;

    IDragSourceHelper2 *pDragSourceHelper2 = nullptr;
#if defined(IID_PPV_ARGS) && defined(__IDragSourceHelper2_INTERFACE_DEFINED__)
    m_pDragSourceHelper->QueryInterface(IID_PPV_ARGS(&pDragSourceHelper2));
#else
    m_pDragSourceHelper->QueryInterface(IID_IDragSourceHelper2,
                                        reinterpret_cast<LPVOID *>(&m_pDragSourceHelper2));
#endif
    if (pDragSourceHelper2) {
        m_pDragSourceHelper->Release();
        m_pDragSourceHelper = static_cast<IDragSourceHelper *>(pDragSourceHelper2);
        HRESULT hr = pDragSourceHelper2->SetFlags(DSH_ALLOWDROPDESCRIPTIONTEXT);
        Q_ASSERT(hr == S_OK);
    }

    m_bUseDescription = (pDragSourceHelper2 != NULL) && QDragDropHelper::isAppThemed();
    if (m_bUseDescription) {
        CLIPFORMAT cfDS = QDragDropHelper::RegisterFormat(CFSTR_DROPDESCRIPTION);
        HGLOBAL hGlobal = NULL;
        if (cfDS
            && (hGlobal = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPDESCRIPTION)))) {
            DROPDESCRIPTION *pDropDescription =
                    static_cast<DROPDESCRIPTION *>(::GlobalLock(hGlobal));
            pDropDescription->type = DROPIMAGE_INVALID;
            ::GlobalUnlock(hGlobal);
            CacheGlobalData(cfDS, hGlobal);
        }
    }

    // CACHE drop data
    ParseDraggedData();
    QPoint hotPoint(-1, -1);
    QDragManager *manager = QDragManager::self();
    if (manager->object())
        hotPoint = manager->object()->hotSpot();
    SetDragImage(hotPoint);

    return true;
}

//---------------------------------------------------------------------
//                    IDataObject Methods
//
// The following methods are NOT supported for data transfer using the
// clipboard or drag-drop:
//
//      IDataObject::SetData    -- return E_NOTIMPL
//      IDataObject::DAdvise    -- return OLE_E_ADVISENOTSUPPORTED
//                 ::DUnadvise
//                 ::EnumDAdvise
//      IDataObject::GetCanonicalFormatEtc -- return E_NOTIMPL
//                     (NOTE: must set pformatetcOut->ptd = NULL)
//---------------------------------------------------------------------

STDMETHODIMP
QWindowsOleDataObjectEx::GetData(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    if (!data)
        return ResultFromScode(DATA_E_FORMATETC);

    AFX_DATACACHE_ENTRY *pCache = Lookup(pformatetc, DATADIR_GET);
    if (pCache == NULL)
        return ResultFromScode(DATA_E_FORMATETC);

    memset(pmedium, 0, sizeof(STGMEDIUM));
    if (pCache->m_stgMedium.tymed != TYMED_NULL) {
        if (!QDragDropHelper::CopyStgMedium(pformatetc->cfFormat, pmedium, &pCache->m_stgMedium)) {
            return ResultFromScode(DATA_E_FORMATETC);
        }
        return ResultFromScode(S_OK);
    }

    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    if (!data)
        return ResultFromScode(DATA_E_FORMATETC);

    pformatetc->tymed = pmedium->tymed; // just in case.
    AFX_DATACACHE_ENTRY *pCache = Lookup(pformatetc, DATADIR_GET);
    if (pCache == NULL)
        return ResultFromScode(DATA_E_FORMATETC);

    if (pCache->m_stgMedium.tymed != TYMED_NULL) {
        Q_ASSERT(pCache->m_stgMedium.tymed == pmedium->tymed);
        if (!QDragDropHelper::CopyStgMedium(pformatetc->cfFormat, pmedium, &pCache->m_stgMedium)) {
            return ResultFromScode(DATA_E_FORMATETC);
        }

        return ResultFromScode(S_OK);
    }
    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::QueryGetData(LPFORMATETC pformatetc)
{
    if (!data)
        return ResultFromScode(DATA_E_FORMATETC);

    AFX_DATACACHE_ENTRY *pCache = Lookup(pformatetc, DATADIR_GET);
    if (pCache)
        return ResultFromScode(S_OK);

    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::SetData(LPFORMATETC pFormatetc, STGMEDIUM *pMedium, BOOL fRelease)
{
    UNREFERENCED_PARAMETER(fRelease);
    if (pFormatetc == NULL || pMedium == NULL)
        return E_INVALIDARG;

    Q_ASSERT(pFormatetc->tymed == pMedium->tymed);
    AFX_DATACACHE_ENTRY *pCache = Lookup(pFormatetc, DATADIR_SET);
    if (NULL == pCache && // Error: Invalid FORMATETC structure
        (pFormatetc->tymed & (TYMED_HGLOBAL | TYMED_ISTREAM)) && pFormatetc->cfFormat >= 0xC000) {
        CacheData(pFormatetc->cfFormat, pMedium, pFormatetc);
        return ResultFromScode(S_OK);
    }
    return ResultFromScode(E_NOTIMPL);
}

STDMETHODIMP
QWindowsOleDataObjectEx::EnumFormatEtc(DWORD dwDirection, LPENUMFORMATETC FAR *ppenumFormatEtc)
{
    if (!data)
        return ResultFromScode(DATA_E_FORMATETC);

    SCODE sc = S_OK;
    QVector<FORMATETC> fmtetcs;
    for (UINT nIndex = 0; nIndex < m_nSize; nIndex++) {
        AFX_DATACACHE_ENTRY *pCache = &m_pDataCache[nIndex];
        if ((DWORD)pCache->m_nDataDir & dwDirection) {
            // entry should be enumerated -- add it to the list
            FORMATETC formatEtc;
            QDragDropHelper::CopyFormatEtc(&formatEtc, &pCache->m_formatEtc);
            fmtetcs.append(formatEtc);
        }
    }

    QWindowsOleEnumFmtEtc *enumFmtEtc = new QWindowsOleEnumFmtEtc(fmtetcs);
    *ppenumFormatEtc = enumFmtEtc;
    if (enumFmtEtc->isNull()) {
        delete enumFmtEtc;
        *ppenumFormatEtc = NULL;
        sc = E_OUTOFMEMORY;
    }

    return ResultFromScode(sc);
}

AFX_DATACACHE_ENTRY *QWindowsOleDataObjectEx::Lookup(LPFORMATETC lpFormatEtc, DATADIR nDataDir) const
{
    AFX_DATACACHE_ENTRY *pLast = NULL;
    for (UINT nIndex = 0; nIndex < m_nSize; nIndex++) {
        AFX_DATACACHE_ENTRY *pCache = &m_pDataCache[nIndex];
        FORMATETC *pCacheFormat = &pCache->m_formatEtc;
        if (pCacheFormat->cfFormat == lpFormatEtc->cfFormat
            && (pCacheFormat->tymed & lpFormatEtc->tymed) != 0
            && (pCacheFormat->dwAspect == DVASPECT_THUMBNAIL
                || pCacheFormat->dwAspect == DVASPECT_ICON
                || pCache->m_stgMedium.tymed == TYMED_NULL
                || pCacheFormat->lindex == lpFormatEtc->lindex
                || (pCacheFormat->lindex == 0 && lpFormatEtc->lindex == -1)
                || (pCacheFormat->lindex == -1 && lpFormatEtc->lindex == 0))
            && pCacheFormat->dwAspect == lpFormatEtc->dwAspect && pCache->m_nDataDir == nDataDir) {
            pLast = pCache;
            DVTARGETDEVICE *ptd1 = pCacheFormat->ptd;
            DVTARGETDEVICE *ptd2 = lpFormatEtc->ptd;

            if (ptd1 == NULL && ptd2 == NULL)
                break;
            if (ptd1 != NULL && ptd2 != NULL && ptd1->tdSize == ptd2->tdSize
                && memcmp(ptd1, ptd2, ptd1->tdSize) == 0) {
                break;
            }
        }
    }
    return pLast;
}

void QWindowsOleDataObjectEx::CacheGlobalData(CLIPFORMAT cfFormat, HGLOBAL hGlobal,
                                       LPFORMATETC lpFormatEtc)
{
    FORMATETC formatEtc;
    lpFormatEtc = QDragDropHelper::FillFormatEtc(lpFormatEtc, cfFormat, &formatEtc);
    lpFormatEtc->tymed = TYMED_HGLOBAL;

    AFX_DATACACHE_ENTRY *pEntry = GetCacheEntry(lpFormatEtc, DATADIR_GET);
    pEntry->m_stgMedium.tymed = TYMED_HGLOBAL;
    pEntry->m_stgMedium.hGlobal = hGlobal;
    pEntry->m_stgMedium.pUnkForRelease = NULL;
}

void QWindowsOleDataObjectEx::CacheData(CLIPFORMAT cfFormat, LPSTGMEDIUM lpStgMedium,
                                 LPFORMATETC lpFormatEtc)
{
    FORMATETC formatEtc;
    lpFormatEtc = QDragDropHelper::FillFormatEtc(lpFormatEtc, cfFormat, &formatEtc);
    Q_ASSERT(lpStgMedium->tymed != TYMED_GDI || lpFormatEtc->cfFormat == CF_METAFILEPICT
             || lpFormatEtc->cfFormat == CF_PALETTE || lpFormatEtc->cfFormat == CF_BITMAP);
    lpFormatEtc->tymed = lpStgMedium->tymed;

    AFX_DATACACHE_ENTRY *pEntry = GetCacheEntry(lpFormatEtc, DATADIR_GET);
    pEntry->m_stgMedium = *lpStgMedium;
}

AFX_DATACACHE_ENTRY *QWindowsOleDataObjectEx::GetCacheEntry(LPFORMATETC lpFormatEtc, DATADIR nDataDir)
{
    AFX_DATACACHE_ENTRY *pEntry = Lookup(lpFormatEtc, nDataDir);
    if (pEntry != NULL) {
        CoTaskMemFree(pEntry->m_formatEtc.ptd);
        ::ReleaseStgMedium(&pEntry->m_stgMedium);
    } else {
        if (m_pDataCache == NULL || m_nSize == m_nMaxSize) {
            Q_ASSERT(m_nGrowBy != 0);
            AFX_DATACACHE_ENTRY *pCache = new AFX_DATACACHE_ENTRY[m_nMaxSize + m_nGrowBy];
            m_nMaxSize += m_nGrowBy;
            if (m_pDataCache != NULL) {
                memcpy_s(pCache, (m_nMaxSize + m_nGrowBy) * sizeof(AFX_DATACACHE_ENTRY),
                         m_pDataCache, m_nSize * sizeof(AFX_DATACACHE_ENTRY));
                delete[] m_pDataCache;
            }
            m_pDataCache = pCache;
        }
        Q_ASSERT(m_pDataCache != NULL);
        Q_ASSERT(m_nMaxSize != 0);

        pEntry = &m_pDataCache[m_nSize++];
    }

    pEntry->m_nDataDir = nDataDir;
    pEntry->m_formatEtc = *lpFormatEtc;
    return pEntry;
}

void QWindowsOleDataObjectEx::EmptyCacheEntry()
{
    if (m_pDataCache != NULL) {
        Q_ASSERT(m_nMaxSize != 0);
        Q_ASSERT(m_nSize != 0);

        for (UINT nIndex = 0; nIndex < m_nSize; nIndex++) {
            CoTaskMemFree(m_pDataCache[nIndex].m_formatEtc.ptd);
            ::ReleaseStgMedium(&m_pDataCache[nIndex].m_stgMedium);
        }

        // delete the cache
        delete[] m_pDataCache;
        m_pDataCache = NULL;
        m_nMaxSize = 0;
        m_nSize = 0;
    }
    Q_ASSERT(m_pDataCache == NULL);
    Q_ASSERT(m_nMaxSize == 0);
    Q_ASSERT(m_nSize == 0);
}

void QWindowsOleDataObjectEx::ParseDraggedData()
{
    QList<QUrl> urls = data->urls();
    if (!urls.size())
        return;

    QStringList fileNames;
    int size = sizeof(DROPFILES) + 2;
    for (int i = 0; i < urls.size(); i++) {
        QString fn = QDir::toNativeSeparators(urls.at(i).toLocalFile());
        if (!fn.isEmpty()) {
            size += sizeof(ushort) * (fn.length() + 1);
            fileNames.append(fn);
        }
    }

    QByteArray result(size, '\0');
    DROPFILES *d = (DROPFILES *)result.data();
    d->pFiles = sizeof(DROPFILES);
    GetCursorPos(&d->pt);
    d->fNC = true;
    char *files = ((char *)d) + d->pFiles;

    d->fWide = true;
    wchar_t *f = (wchar_t *)files;
    for (int i = 0; i < fileNames.size(); i++) {
        int l = fileNames.at(i).length();
        memcpy(f, fileNames.at(i).utf16(), l * sizeof(ushort));
        f += l;
        *f++ = 0;
    }
    *f = 0;

    HGLOBAL hData = GlobalAlloc(0, result.size());
    if (!hData)
        return;

    void *out = GlobalLock(hData);
    memcpy(out, result.data(), result.size());
    GlobalUnlock(hData);

    FORMATETC etc = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    CacheGlobalData(CF_HDROP, hData, &etc);
}

Q_GUI_EXPORT HBITMAP qt_pixmapToWinHBITMAP(const QPixmap &p, int hbitmapFormat = 0);

bool QWindowsOleDataObjectEx::SetDragImage(const QPoint &hotpt)
{
    QPixmap pm;
    QDragManager *manager = QDragManager::self();
    if (manager && manager->object()) {
        pm = manager->object()->pixmap();
        if (pm.isNull())
            return false;
    }

    HRESULT hRes = E_NOINTERFACE;
    HBITMAP hBitmap = qt_pixmapToWinHBITMAP(pm, 2); // QtWin::HBitmapAlpha)
    if (hBitmap && m_pDragSourceHelper) {
        BITMAP bm;
        SHDRAGIMAGE di;
        ::GetObject(hBitmap, sizeof(bm), &bm);
        di.sizeDragImage.cx = bm.bmWidth;
        di.sizeDragImage.cy = bm.bmHeight;

        di.ptOffset.x = hotpt.x();
        di.ptOffset.y = hotpt.y();
        if (di.ptOffset.x < 0)
            di.ptOffset.x = di.sizeDragImage.cx / 2;
        else if (di.ptOffset.x > di.sizeDragImage.cx)
            di.ptOffset.x = di.sizeDragImage.cx;
        if (di.ptOffset.y < 0)
            di.ptOffset.y = di.sizeDragImage.cy / 2;
        else if (di.ptOffset.y > di.sizeDragImage.cy)
            di.ptOffset.y = di.sizeDragImage.cy;

        di.hbmpDragImage = hBitmap;
        di.crColorKey = RGB(255, 0, 0);
        // di.crColorKey = ::GetSysColor(COLOR_BACKGROUND);
        hRes = m_pDragSourceHelper->InitializeFromBitmap(&di, this);
    }
    if (FAILED(hRes) && hBitmap)
        ::DeleteObject(hBitmap);

    return SUCCEEDED(hRes);
}

/*!
    \class QWindowsOleEnumFmtEtc
    \brief Enumerates the FORMATETC structures supported by QWindowsOleDataObject.
    \internal
    \ingroup qt-lighthouse-win
*/

QWindowsOleEnumFmtEtc::QWindowsOleEnumFmtEtc(const QVector<FORMATETC> &fmtetcs)
{
    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) << __FUNCTION__ << fmtetcs;
    m_lpfmtetcs.reserve(fmtetcs.count());
    for (int idx = 0; idx < fmtetcs.count(); ++idx) {
        LPFORMATETC destetc = new FORMATETC();
        if (copyFormatEtc(destetc, &(fmtetcs.at(idx)))) {
            m_lpfmtetcs.append(destetc);
        } else {
            m_isNull = true;
            delete destetc;
            break;
        }
    }
}

QWindowsOleEnumFmtEtc::QWindowsOleEnumFmtEtc(const QVector<LPFORMATETC> &lpfmtetcs)
{
    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaMime) << __FUNCTION__;
    m_lpfmtetcs.reserve(lpfmtetcs.count());
    for (int idx = 0; idx < lpfmtetcs.count(); ++idx) {
        LPFORMATETC srcetc = lpfmtetcs.at(idx);
        LPFORMATETC destetc = new FORMATETC();
        if (copyFormatEtc(destetc, srcetc)) {
            m_lpfmtetcs.append(destetc);
        } else {
            m_isNull = true;
            delete destetc;
            break;
        }
    }
}

QWindowsOleEnumFmtEtc::~QWindowsOleEnumFmtEtc()
{
    LPMALLOC pmalloc;

    if (CoGetMalloc(MEMCTX_TASK, &pmalloc) == NOERROR) {
        for (int idx = 0; idx < m_lpfmtetcs.count(); ++idx) {
            LPFORMATETC tmpetc = m_lpfmtetcs.at(idx);
            if (tmpetc->ptd)
                pmalloc->Free(tmpetc->ptd);
            delete tmpetc;
        }

        pmalloc->Release();
    }
    m_lpfmtetcs.clear();
}

bool QWindowsOleEnumFmtEtc::isNull() const
{
    return m_isNull;
}

// IEnumFORMATETC methods
STDMETHODIMP
QWindowsOleEnumFmtEtc::Next(ULONG celt, LPFORMATETC rgelt, ULONG FAR* pceltFetched)
{
    ULONG i=0;
    ULONG nOffset;

    if (rgelt == nullptr)
        return ResultFromScode(E_INVALIDARG);

    while (i < celt) {
        nOffset = m_nIndex + i;

        if (nOffset < ULONG(m_lpfmtetcs.count())) {
            copyFormatEtc(rgelt + i, m_lpfmtetcs.at(int(nOffset)));
            i++;
        } else {
            break;
        }
    }

    m_nIndex += i;

    if (pceltFetched != nullptr)
        *pceltFetched = i;

    if (i != celt)
        return ResultFromScode(S_FALSE);

    return NOERROR;
}

STDMETHODIMP
QWindowsOleEnumFmtEtc::Skip(ULONG celt)
{
    ULONG i=0;
    ULONG nOffset;

    while (i < celt) {
        nOffset = m_nIndex + i;

        if (nOffset < ULONG(m_lpfmtetcs.count())) {
            i++;
        } else {
            break;
        }
    }

    m_nIndex += i;

    if (i != celt)
        return ResultFromScode(S_FALSE);

    return NOERROR;
}

STDMETHODIMP
QWindowsOleEnumFmtEtc::Reset()
{
    m_nIndex = 0;
    return NOERROR;
}

STDMETHODIMP
QWindowsOleEnumFmtEtc::Clone(LPENUMFORMATETC FAR* newEnum)
{
    if (newEnum == nullptr)
        return ResultFromScode(E_INVALIDARG);

    QWindowsOleEnumFmtEtc *result = new QWindowsOleEnumFmtEtc(m_lpfmtetcs);
    result->m_nIndex = m_nIndex;

    if (result->isNull()) {
        delete result;
        return ResultFromScode(E_OUTOFMEMORY);
    }

    *newEnum = result;
    return NOERROR;
}

bool QWindowsOleEnumFmtEtc::copyFormatEtc(LPFORMATETC dest, const FORMATETC *src) const
{
    if (dest == nullptr || src == nullptr)
        return false;

    *dest = *src;

    if (src->ptd) {
        LPMALLOC pmalloc;

        if (CoGetMalloc(MEMCTX_TASK, &pmalloc) != NOERROR)
            return false;

        pmalloc->Alloc(src->ptd->tdSize);
        memcpy(dest->ptd, src->ptd, size_t(src->ptd->tdSize));

        pmalloc->Release();
    }

    return true;
}

QT_END_NAMESPACE
