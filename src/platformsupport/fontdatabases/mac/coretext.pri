HEADERS += $$PWD/qcoretextfontdatabase_p.h $$PWD/qfontengine_coretext_p.h
OBJECTIVE_SOURCES += $$PWD/qfontengine_coretext.mm $$PWD/qcoretextfontdatabase.mm

HEADERS += $$PWD/qcoretextfontdatahelper_ft_p.h
OBJECTIVE_SOURCES += $$PWD/qcoretextfontdatahelper_ft.mm
HEADERS += $$PWD/freetypemac_p.h

SOURCES += $$PWD/freetypemac.cpp

LIBS_PRIVATE += \
    -framework CoreFoundation \
    -framework CoreGraphics \
    -framework CoreText \
    -framework Foundation

macos: \
    LIBS_PRIVATE += -framework AppKit
else: \
    LIBS_PRIVATE += -framework UIKit

CONFIG += watchos_coretext
QMAKE_USE_PRIVATE += freetype
