TARGET  = qjpeg

QT += core-private gui-private

SOURCES += main.cpp qjpeghandler.cpp
HEADERS += main.h qjpeghandler_p.h

qtConfig(system-jpeg) {
    QMAKE_USE += libjpeg
} else {
    QMAKE_USE_PRIVATE += libjpeg
}

include($$PWD/../../../3rdparty/lcms.pri)

win32 {
    !winrt {
        LIBS_PRIVATE += -luser32 -lgdi32
    }
}
OTHER_FILES += jpeg.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QJpegPlugin
load(qt_plugin)
