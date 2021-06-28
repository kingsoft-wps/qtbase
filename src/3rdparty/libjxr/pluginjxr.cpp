#include "qimage.h"
#include "qwdphandler_p.h"
#include "JXRGlue.h"

typedef long ERR;

static const char* JXR_ErrorMessage(const int error)
{
    switch(error) {
    case WMP_errNotYetImplemented:
    case WMP_errAbstractMethod:
        return "Not yet implemented";
    case WMP_errOutOfMemory:
        return "Out of memory";
    case WMP_errFileIO:
        return "File I/O error";
    case WMP_errBufferOverflow:
        return "Buffer overflow";
    case WMP_errInvalidParameter:
        return "Invalid parameter";
    case WMP_errInvalidArgument:
        return "Invalid argument";
    case WMP_errUnsupportedFormat:
        return "Unsupported format";
    case WMP_errIncorrectCodecVersion:
        return "Incorrect codec version";
    case WMP_errIndexNotFound:
        return "Format converter: Index not found";
    case WMP_errOutOfSequence:
        return "Metadata: Out of sequence";
    case WMP_errMustBeMultipleOf16LinesUntilLastCall:
        return "Must be multiple of 16 lines until last call";
    case WMP_errPlanarAlphaBandedEncRequiresTempFile:
        return "Planar alpha banded encoder requires temp files";
    case WMP_errAlphaModeCannotBeTranscoded:
        return "Alpha mode cannot be transcoded";
    case WMP_errIncorrectCodecSubVersion:
        return "Incorrect codec subversion";
    case WMP_errFail:
    case WMP_errNotInitialized:
    default:
        return "Invalid instruction - please contact the FreeImage team";
    }

    return NULL;
}

#ifndef QT_NO_EXCEPTIONS
#define JXR_CHECK(error_code)\
    if (error_code < 0) {\
        const char *error_message = JXR_ErrorMessage(error_code);\
        throw error_message;\
    }
#else
#define JXR_CHECK(error_code)\
    if (error_code < 0) {\
        goto on_error;\
    }
#endif

typedef struct tagJXRInputConversion 
{
    BITDEPTH_BITS bdBitDepth;
    U32 cbitUnit;
    QImage::Format imageType;
}JXRInputConversion;

typedef QImage::Format IMAGE_TYPE;

static const JXRInputConversion gs_imageFormatMap[] = 
{
    {BD_1, 1, QImage::Format_Mono},
    {BD_8, 8, QImage::Format_Indexed8},
    {BD_8, 24, QImage::Format_RGB888},
    {BD_8, 32, QImage::Format_ARGB32},
    //{ BD_565, 16, qt absence
    {BD_5, 16, QImage::Format_RGB555},
    //{BD_16, 16, qt absence
    //{ BD_16, 48, qt absence
    //{ BD_16, 64, qt absence
    //{ BD_32F, 32, qt absence
    //{ BD_32F, 96, qt absence
    //{ BD_32F, 128, qt absence
};

static ERR jxrIoRead(WMPStream* pWS, void* pv, size_t cb)
{
    QIODevice* fio = (QIODevice*)pWS->state.pvObj;
    return (fio->read((char*)pv, cb) > 0) ? WMP_errSuccess : WMP_errFileIO;
}

static ERR jxrIoWrite(WMPStream* pWS, const void* pv, size_t cb)
{
    QIODevice* fio = (QIODevice*)pWS->state.pvObj;
    if (0 != cb) {
        qint64 res = fio->write((const char*)pv,cb);
        return (res == 1) ? WMP_errSuccess : WMP_errFileIO;
    }
    return WMP_errFileIO;
}

static ERR jxrIoSetPos(WMPStream* pWS, size_t offPos)
{
    QIODevice* fio = (QIODevice*)pWS->state.pvObj;
    return (fio->seek(offPos)) ? WMP_errSuccess : WMP_errFileIO;
}

static ERR jxrIoGetPos(WMPStream* pWS, size_t* poffPos)
{
    QIODevice* fio = (QIODevice*)pWS->state.pvObj;
    long lOff = fio->pos();
    if (lOff == -1)
        return WMP_errFileIO;

    *poffPos = (size_t)lOff;
    return WMP_errSuccess;
}

static Bool jxrIoEOS(WMPStream* pWS)
{
    QIODevice* fio = (QIODevice*)pWS->state.pvObj;
    qint64 currentPos = fio->pos();
    qint64 fileRemaining = fio->size() - currentPos;
    
    return (fileRemaining > 0);
}

static ERR jxrIoClose(WMPStream** ppWS)
{
    WMPStream *pWS = *ppWS;

    if (pWS && pWS->fMem) {
        free(pWS);
        *ppWS = NULL;
    }
    return WMP_errSuccess;
}

static ERR jxrIoCreate(WMPStream** ppWS, QIODevice* jxrIO)
{
    *ppWS = (WMPStream*)calloc(1, sizeof(WMPStream));
    if (*ppWS) {
        WMPStream* pWS = *ppWS;

        pWS->state.pvObj = jxrIO;
        pWS->Close = jxrIoClose;
        pWS->EOS = jxrIoEOS;
        pWS->Read = jxrIoRead;
        pWS->Write = jxrIoWrite;
        pWS->SetPos = jxrIoSetPos;
        pWS->GetPos = jxrIoGetPos;

        pWS->fMem = TRUE;

        return WMP_errSuccess;
    }

    return WMP_errOutOfMemory;
}

static void setDecoderParameters(PKImageDecode* pDecoder, int)
{
    // load image & alpha for formats with alpha
    pDecoder->WMP.wmiSCP.uAlphaMode = 2;

    //more options to come
}

static ERR getQtPixelFormat(const PKPixelInfo* pixelInfo, 
                            PKPixelFormatGUID* out_guid_format, 
                            IMAGE_TYPE* image_type, unsigned* bitDepth)
{
    const unsigned formatCount = (unsigned)sizeof(gs_imageFormatMap) 
                                    / sizeof(gs_imageFormatMap[0]);

    for (unsigned i = 0; i < formatCount; ++i) {
        if ((pixelInfo->bdBitDepth == gs_imageFormatMap[i].bdBitDepth) &&
            (pixelInfo->cbitUnit == gs_imageFormatMap[i].cbitUnit)) {
            memcpy(out_guid_format, 
                    pixelInfo->pGUIDPixFmt, 
                    sizeof(PKPixelFormatGUID));
            *image_type = gs_imageFormatMap[i].imageType;
            *bitDepth = gs_imageFormatMap[i].cbitUnit;

            return WMP_errSuccess;
        }
    }

    return WMP_errFail;
}

static ERR getInputPixelFormat(PKImageDecode* pDecoder,
                                PKPixelFormatGUID* guid_format,
                                IMAGE_TYPE* imageType,
                                unsigned* bitDepth)
{
    ERR error_code = 0;
    PKPixelInfo pixelInfo;

#ifndef QT_NO_EXCEPTIONS
    try {
#else
    {
#endif
        PKPixelFormatGUID pguidSourcePF;
        error_code = pDecoder->GetPixelFormat(pDecoder, &pguidSourcePF);
        JXR_CHECK(error_code);
        pixelInfo.pGUIDPixFmt = &pguidSourcePF;
        error_code = PixelFormatLookup(&pixelInfo, LOOKUP_FORWARD);
        JXR_CHECK(error_code);

        error_code = getQtPixelFormat(&pixelInfo, guid_format, imageType, bitDepth);

        if (error_code != WMP_errSuccess) {
            const PKPixelFormatGUID* ppguidTargetPF = NULL;
            unsigned iIndex = 0;

            do {
                error_code = PKFormatConverter_EnumConversions(&pguidSourcePF,
                                                                iIndex,
                                                                &ppguidTargetPF);
                if (error_code == WMP_errSuccess) {
                    pixelInfo.pGUIDPixFmt = ppguidTargetPF;
                    error_code = PixelFormatLookup(&pixelInfo, LOOKUP_FORWARD);
                    JXR_CHECK(error_code);
                    error_code = getQtPixelFormat(&pixelInfo, guid_format, imageType, bitDepth);

                    if (error_code == WMP_errSuccess)
                        break;
                }
                ++iIndex;
            } while (error_code != WMP_errIndexNotFound);
        }
        return (error_code == WMP_errSuccess) ? WMP_errSuccess : WMP_errUnsupportedFormat;
    }
#ifndef QT_NO_EXCEPTIONS
    catch(...) {
        return error_code;
    }
#else
on_error:
    return error_code;
#endif
}

static ERR copyPixels(PKImageDecode* pDecoder, 
                        PKPixelFormatGUID out_guid_format, 
                        QImage* dib, int width, int height)
{
    PKFormatConverter* pConverter = NULL;
    ERR error_code = 0;
    uchar* pb = NULL;

    const PKRect rect = {0, 0, width, height};

#ifndef QT_NO_EXCEPTIONS
    try {
#else
    {
#endif
        PKPixelFormatGUID in_guid_format;
        error_code = pDecoder->GetPixelFormat(pDecoder, &in_guid_format);
        JXR_CHECK(error_code);
        
        if (IsEqualGUID(out_guid_format, in_guid_format)) {
            PKPixelInfo pPiInfo;

            pPiInfo.pGUIDPixFmt = &out_guid_format;
            error_code = PixelFormatLookup(&pPiInfo, LOOKUP_FORWARD);
            JXR_CHECK(error_code);

            size_t decoderBytePerLine = ((pPiInfo.cbitUnit + 7) >> 3) * width;
            error_code = PKAllocAligned((void**)&pb, decoderBytePerLine * height, 128);
            JXR_CHECK(error_code);

            error_code = pDecoder->Copy(pDecoder, &rect, pb, decoderBytePerLine);
            JXR_CHECK(error_code);

            size_t memsetSize = 0;

            if ((size_t)dib->bytesPerLine() <= decoderBytePerLine)
                memsetSize = dib->bytesPerLine();
            else
                memsetSize = decoderBytePerLine;

            //qt Qimage bytperline compute method is different from FreeImage,
            //so here only can be copy memory of each line, but can not directly copy whole memory.
            for (int y = 0; y < height; ++y) {
                uchar* srcBits = (uchar*)(pb + y * decoderBytePerLine);
                uchar* dstBits = (uchar*)dib->scanLine(y);
                memcpy(dstBits, srcBits, memsetSize);
            }
            PKFreeAligned((void**)&pb);
        }
        else {
            // we need to use the conversion API
            error_code = PKCodecFactory_CreateFormatConverter(&pConverter);
            JXR_CHECK(error_code);

            error_code = pConverter->Initialize(pConverter, pDecoder, NULL, out_guid_format);
            JXR_CHECK(error_code);

            size_t byteperlineMax = 0;
            size_t byteperlineTo = 0;
            
            {
                PKPixelInfo pPiFrom;
                PKPixelInfo pPiTo;

                pPiFrom.pGUIDPixFmt = &in_guid_format;
                error_code = PixelFormatLookup(&pPiFrom, LOOKUP_FORWARD);
                JXR_CHECK(error_code);

                pPiTo.pGUIDPixFmt = &out_guid_format;
                error_code = PixelFormatLookup(&pPiTo, LOOKUP_FORWARD);
                JXR_CHECK(error_code);

                size_t byteperlineForm = ((pPiFrom.cbitUnit + 7) >> 3) * width;
                byteperlineTo = ((pPiTo.cbitUnit + 7) >> 3) * width;
                byteperlineMax = qMax(byteperlineForm, byteperlineTo);
            }
            error_code = PKAllocAligned((void**)&pb, byteperlineMax * height, 128);
            JXR_CHECK(error_code);

            error_code = pConverter->Copy(pConverter, &rect, pb, byteperlineMax);
            JXR_CHECK(error_code);

            size_t memsetSize = 0;
            if ((size_t)dib->bytesPerLine() <= byteperlineTo)
                memsetSize = dib->bytesPerLine();
            else
                memsetSize = byteperlineTo;

            for (int y = 0; y < height; ++y) {
                uchar* srcBits = (uchar*)(pb + y * byteperlineMax);
                uchar* dstBits = (uchar*)dib->scanLine(y);
                memcpy(dstBits, srcBits, memsetSize);
            }
            PKFreeAligned((void**)&pb);
            PKFormatConverter_Release(&pConverter);
        }

        if (IsEqualGUID(out_guid_format, GUID_PKPixelFormat24bppBGR) 
         || IsEqualGUID(out_guid_format, GUID_PKPixelFormat32bppBGR)) {
            int dpiX = dib->dotsPerMeterX();
            int dpiY = dib->dotsPerMeterY();
            *dib = dib->rgbSwapped();
            dib->setDotsPerMeterX(dpiX);
            dib->setDotsPerMeterY(dpiY);
        }

        return WMP_errSuccess;
    }
#ifndef QT_NO_EXCEPTIONS
    catch(...) {
#else
on_error:
#endif
        PKFreeAligned((void **)&pb);
        PKFormatConverter_Release(&pConverter);
#ifndef QT_NO_EXCEPTIONS
    }
#endif

    return error_code;
}

bool jxrReadHeader(QIODevice* pDevice, int* width, int* height, float* resolutionX, float* resolutionY, QImage::Format* Imgformat)
{
    WMPStream* pWS = NULL;
    jxrIoCreate(&pWS, pDevice);

    PKImageDecode *pDecoder = NULL;
    PKPixelFormatGUID guid_format;
    ERR error_code = 0;

    if (!pWS)
        return false;

    if (!pDevice) {
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);
        return false;
    }

#ifndef QT_NO_EXCEPTIONS
    try {
#else
    {
#endif
        error_code = PKImageDecode_Create_WMP(&pDecoder);
        JXR_CHECK(error_code);

        error_code = pDecoder->Initialize(pDecoder, pWS);
        JXR_CHECK(error_code);

        setDecoderParameters(pDecoder, 0);
        {
            unsigned int bitDepth = 0;
            error_code = getInputPixelFormat(pDecoder, &guid_format, Imgformat, &bitDepth);
            JXR_CHECK(error_code);
        }

        pDecoder->GetSize(pDecoder, width, height);
		pDecoder->GetResolution(pDecoder, resolutionX, resolutionY);
        pDecoder->Release(&pDecoder);
        Q_ASSERT(pDecoder == NULL);
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);

        return true;
    }
#ifndef QT_NO_EXCEPTIONS
    catch(const char *message) {
        Q_ASSERT(message);
#else
on_error:
#endif

        pDecoder->Release(&pDecoder);
        Q_ASSERT(pDecoder == NULL);
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);
#ifndef QT_NO_EXCEPTIONS
    }
#endif

    return false;
}

QImage jxrImageRead(QIODevice* pDevice, ERR* retErr, long flags)
{
    WMPStream* pWS = NULL;
    jxrIoCreate(&pWS, pDevice);

    PKImageDecode *pDecoder = NULL;
    ERR error_code = 0;
    PKPixelFormatGUID guid_format;

    IMAGE_TYPE imageType = (IMAGE_TYPE)0;
    unsigned bitDepth = 0;

    if (!pWS) {
        *retErr = WMP_errFail;
        return QImage();
    }

    if (!pDevice) {
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);
        *retErr = WMP_errFail;
        return QImage();
    }

#ifndef QT_NO_EXCEPTIONS
    try {
#else
    {
#endif
        int width, height;
        // create a JXR decoder interface and initialize function pointers
        error_code = PKImageDecode_Create_WMP(&pDecoder);
        JXR_CHECK(error_code);

        // attach the stream to the decoder ...
        error_code = pDecoder->Initialize(pDecoder, pWS);
        JXR_CHECK(error_code);

        // set decoder parameters
        setDecoderParameters(pDecoder, flags);

        error_code = getInputPixelFormat(pDecoder, &guid_format, &imageType, &bitDepth);
        JXR_CHECK(error_code);
        pDecoder->GetSize(pDecoder, &width, &height);

        QImage dstImg(width, height, (QImage::Format)imageType);

        {
            float resX, resY;
            pDecoder->GetResolution(pDecoder, &resX, &resY);
            dstImg.setDotsPerMeterX((unsigned)(resX / 0.0254F + 0.5F));
            dstImg.setDotsPerMeterY((unsigned)(resY / 0.0254F + 0.5F));
        }

        if (bitDepth == 8) {
            QVector<QRgb> colorTable;
            colorTable.resize(256);
            
            for (int i = 0; i < 256; ++i)
                colorTable[i] = qRgb(i, i, i);

            dstImg.setColorTable(colorTable);
        }
        else if (bitDepth == 1) {
            QVector<QRgb> colorTable;
            colorTable.resize(2);

            colorTable[0] = 0;
            colorTable[1] = 255;
        }

        error_code = copyPixels(pDecoder, guid_format, &dstImg, width, height);
        JXR_CHECK(error_code);

        pDecoder->Release(&pDecoder);
        Q_ASSERT(pDecoder == NULL);
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);

        *retErr = WMP_errSuccess;
        return dstImg;
    }
#ifndef QT_NO_EXCEPTIONS
    catch (const char* message) {
        Q_ASSERT(message);
#else
on_error:
#endif
        *retErr = error_code;

        pDecoder->Release(&pDecoder);
        Q_ASSERT(pDecoder == NULL);
        jxrIoClose(&pWS);
        Q_ASSERT(pWS == NULL);
#ifndef QT_NO_EXCEPTIONS
    }
#endif

    return QImage();
}