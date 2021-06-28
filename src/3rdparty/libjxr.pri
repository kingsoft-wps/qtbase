wince*: {
    DEFINES += NO_GETENV
    contains(CE_ARCH,x86):CONFIG -= stl exceptions
    contains(CE_ARCH,x86):CONFIG += exceptions_off
}

#Disable warnings in 3rdparty code due to unused arguments
symbian: {
    QMAKE_CXXFLAGS.CW += -W nounusedarg
    TARGET.UID3=0x2001E61B
} else:contains(QMAKE_CC, gcc): {
    QMAKE_CFLAGS_WARN_ON += -Wno-unused-parameter -Wno-main
}

unix: {
    INCLUDEPATH += $$PWD/libjxr/common/include
    DEFINES += __ANSI__
    DEFINES += DISABLE_PERF_MEASUREMENT
}

HEADERS += $$PWD/libjxr/common/include/guiddef.h
HEADERS += $$PWD/libjxr/common/include/wmsal.h
HEADERS += $$PWD/libjxr/common/include/wmspecstring.h
HEADERS += $$PWD/libjxr/common/include/wmspecstrings_adt.h
HEADERS += $$PWD/libjxr/common/include/wmspecstrings_strict.h
HEADERS += $$PWD/libjxr/common/include/wmspecstrings_undef.h
HEADERS += $$PWD/libjxr/image/decode/decode.h
HEADERS += $$PWD/libjxr/image/encode/encode.h
HEADERS += $$PWD/libjxr/image/sys/ansi.h
HEADERS += $$PWD/libjxr/image/sys/common.h
HEADERS += $$PWD/libjxr/image/sys/perfTimer.h
HEADERS += $$PWD/libjxr/image/sys/strTransform.h
HEADERS += $$PWD/libjxr/image/sys/strcodec.h
HEADERS += $$PWD/libjxr/image/sys/windowsmediaphoto.h
HEADERS += $$PWD/libjxr/image/sys/xplatform_image.h
HEADERS += $$PWD/libjxr/image/x86/x86.h
HEADERS += $$PWD/libjxr/jxrgluelib/JXRGlue.h
HEADERS += $$PWD/libjxr/jxrgluelib/JXRMeta.h

SOURCES += \
$$PWD/libjxr/image/decode/JXRTranscode.c \
$$PWD/libjxr/image/decode/decode.c \
$$PWD/libjxr/image/decode/postprocess.c \
$$PWD/libjxr/image/decode/segdec.c \
$$PWD/libjxr/image/decode/strInvTransform.c \
$$PWD/libjxr/image/decode/strPredQuantDec.c \
$$PWD/libjxr/image/decode/strdec.c \
$$PWD/libjxr/image/decode/strdec_x86.c \
$$PWD/libjxr/image/encode/encode.c \
$$PWD/libjxr/image/encode/segenc.c \
$$PWD/libjxr/image/encode/strFwdTransform.c \
$$PWD/libjxr/image/encode/strPredQuantEnc.c \
$$PWD/libjxr/image/encode/strenc.c \
$$PWD/libjxr/image/encode/strenc_x86.c \
$$PWD/libjxr/image/sys/adapthuff.c \
$$PWD/libjxr/image/sys/image.c \
$$PWD/libjxr/image/sys/perfTimerANSI.c \
$$PWD/libjxr/image/sys/strPredQuant.c \
$$PWD/libjxr/image/sys/strTransform.c \
$$PWD/libjxr/image/sys/strcodec.c \
$$PWD/libjxr/jxrgluelib/JXRGlue.c \
$$PWD/libjxr/jxrgluelib/JXRGlueJxr.c \
$$PWD/libjxr/jxrgluelib/JXRGluePFC.c \
$$PWD/libjxr/jxrgluelib/JXRMeta.c \
$$PWD/libjxr/pluginjxr.cpp
