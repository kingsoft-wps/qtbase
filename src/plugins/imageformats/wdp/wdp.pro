TARGET  = qwdp

SOURCES += wdpmain.cpp qwdphandler.cpp
HEADERS += wdpmain.h qwdphandler_p.h

INCLUDEPATH += $$PWD/../../../3rdparty/libjxr/jxrgluelib
INCLUDEPATH += $$PWD/../../../3rdparty/libjxr/image/sys

include($$PWD/../../../3rdparty/libjxr.pri)

OTHER_FILES += wdp.json

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QWdpPlugin
load(qt_plugin)