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


INCLUDEPATH += $$PWD/lcms
HEADERS += 	\
	$$PWD/lcms/lcms2_internal.h \
	$$PWD/lcms/lcms2_plugin.h \
	$$PWD/lcms/lcms2.h
SOURCES += \
	$$PWD/lcms/cmscam02.c \
	$$PWD/lcms/cmscgats.c \
	$$PWD/lcms/cmscnvrt.c \
	$$PWD/lcms/cmserr.c \
	$$PWD/lcms/cmsgamma.c \
	$$PWD/lcms/cmsgmt.c \
	$$PWD/lcms/cmshalf.c \
	$$PWD/lcms/cmsintrp.c \
	$$PWD/lcms/cmsio0.c \
	$$PWD/lcms/cmsio1.c \
	$$PWD/lcms/cmslut.c \
	$$PWD/lcms/cmsmd5.c \
	$$PWD/lcms/cmsmtrx.c \
	$$PWD/lcms/cmsnamed.c \
	$$PWD/lcms/cmsopt.c \
	$$PWD/lcms/cmspack.c \
	$$PWD/lcms/cmspcs.c \
	$$PWD/lcms/cmsplugin.c \
	$$PWD/lcms/cmsps2.c \
	$$PWD/lcms/cmssamp.c \
	$$PWD/lcms/cmssm.c \
	$$PWD/lcms/cmstypes.c \
	$$PWD/lcms/cmsvirt.c \
	$$PWD/lcms/cmswtpnt.c \
	$$PWD/lcms/cmsxform.c \
