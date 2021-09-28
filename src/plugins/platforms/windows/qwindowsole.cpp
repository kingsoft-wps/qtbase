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
    : QWindowsOleDataObject(mimeData)
{
    init();
}

QWindowsOleDataObjectEx::~QWindowsOleDataObjectEx()
{
    if (m_pDragSourceHelper)
        m_pDragSourceHelper->Release();
    if (m_dataObj)
        m_dataObj->Release();
}

bool QWindowsOleDataObjectEx::init()
{

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

    ParseDraggedData();
    QPoint hotPoint(-1, -1);
    QDragManager *manager = QDragManager::self();
    if (manager->object())
        hotPoint = manager->object()->hotSpot();
    SetDragImage(hotPoint);

    return true;
}


STDMETHODIMP
QWindowsOleDataObjectEx::GetData(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    if (m_dataObj) {
        return m_dataObj->GetData(pformatetc, pmedium);
    }

    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM pmedium)
{
    if (m_dataObj) {
        return m_dataObj->GetDataHere(pformatetc, pmedium);
    }

    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::QueryGetData(LPFORMATETC pformatetc)
{
    if (m_dataObj) {
        return m_dataObj->QueryGetData(pformatetc);
    }
    return ResultFromScode(DATA_E_FORMATETC);
}

STDMETHODIMP
QWindowsOleDataObjectEx::SetData(LPFORMATETC pFormatetc, STGMEDIUM *pMedium, BOOL fRelease)
{
    if (m_dataObj) {
        return m_dataObj->SetData(pFormatetc, pMedium, fRelease);
    }
    return ResultFromScode(E_NOTIMPL);
}

STDMETHODIMP
QWindowsOleDataObjectEx::EnumFormatEtc(DWORD dwDirection, LPENUMFORMATETC FAR *ppenumFormatEtc)
{
    if (m_dataObj) {
        return m_dataObj->EnumFormatEtc(dwDirection, ppenumFormatEtc);
    }

   return ResultFromScode(DATA_E_FORMATETC);
}


void QWindowsOleDataObjectEx::ParseDraggedData()
{
    QList<QUrl> urls = data->urls();
    int fileCount = urls.size();
    if (fileCount == 0)
        return;

    QMap<QString, int> systemItem;
    QString computerGuid = QStringLiteral("{20D04FE0-3AEA-1069-A2D8-08002B30309D}");
    QString recycleGuid =  QStringLiteral("{645FF040-5081-101B-9F08-00AA002F954E}");
    QString documentGuid = QStringLiteral("{59031a47-3f72-44a7-89c5-5595fe6b30ee}");
    QString controlPaneGuid = QStringLiteral("{5399E694-6CE5-4D6C-8FCE-1D8870FDCBA0}");
    QString networkGuid = QStringLiteral("{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}");

    systemItem.insert(computerGuid,CSIDL_DRIVES); 
    systemItem.insert(recycleGuid,CSIDL_BITBUCKET);
    systemItem.insert(documentGuid,CSIDL_PROFILE);
    systemItem.insert(controlPaneGuid,CSIDL_CONTROLS);
    systemItem.insert(networkGuid,CSIDL_COMPUTERSNEARME);

    QVector<PCIDLIST_ABSOLUTE> pidls;
    pidls.reserve(fileCount);
    for (int i = 0; i < urls.size(); i++) {
        QString fn = QDir::toNativeSeparators(urls.at(i).toLocalFile());
        if (systemItem.contains(fn)) {
            PIDLIST_ABSOLUTE pidl = NULL;
            if (SUCCEEDED(SHGetFolderLocation(NULL, systemItem.value(fn), NULL, 0, &pidl))) {
                pidls.append(pidl);
            }
           
        }
        else if (!fn.isEmpty()) {
            pidls.append(::ILCreateFromPathW(fn.toStdWString().c_str()));
        }
    }


    IShellItemArray *shellItemArray = nullptr;
    if (QWindowsContext::shell32dll.sHCreateShellItemArrayFromIDLists) {
        QWindowsContext::shell32dll.sHCreateShellItemArrayFromIDLists(static_cast<UINT>(pidls.size()), pidls.data(), &shellItemArray);
    }

    if (shellItemArray) {
        shellItemArray->BindToHandler(nullptr, BHID_DataObject,IID_PPV_ARGS(&m_dataObj));
        shellItemArray->Release();
    }
    for (size_t i = 0; i < pidls.size(); ++i) {
        ::ILFree((PIDLIST_RELATIVE)pidls[i]);
    }
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
