# Note: OpenGL32 must precede Gdi32 as it overwrites some functions.
LIBS += -lole32 -luser32 -lwinspool -limm32 -lwinmm -loleaut32

QT_FOR_CONFIG += gui

qtConfig(opengl):!qtConfig(dynamicgl): LIBS *= -lopengl32

mingw: LIBS *= -luuid
LIBS += -lshlwapi -lshell32 -ladvapi32 -lwtsapi32

DEFINES *= QT_NO_CAST_FROM_ASCII QT_NO_FOREACH

SOURCES += \
    $$PWD/qwindowswindow.cpp \
    $$PWD/qwindowsintegration.cpp \
    $$PWD/qwindowscontext.cpp \
    $$PWD/qwindowsscreen.cpp \
    $$PWD/qwindowskeymapper.cpp \
    $$PWD/qwindowsmousehandler.cpp \
    $$PWD/qwindowspointerhandler.cpp \
    $$PWD/qwindowsole.cpp \
    $$PWD/qwindowsdropdataobject.cpp \
    $$PWD/qwindowsmime.cpp \
    $$PWD/qwindowsinternalmimedata.cpp \
    $$PWD/qwindowscursor.cpp \
    $$PWD/qwindowsinputcontext.cpp \
    $$PWD/qwindowstheme.cpp \
    $$PWD/qwindowsmenu.cpp \
    $$PWD/qwindowsdialoghelpers.cpp \
    $$PWD/qwindowsservices.cpp \
    $$PWD/qwindowsnativeinterface.cpp \
    $$PWD/qwindowsopengltester.cpp \
    $$PWD/qwin10helpers.cpp \
    $$PWD/qwindowdragdrophelper.cpp \
    $$PWD/qwindowsdescription.cpp

HEADERS += \
    $$PWD/qwindowscombase.h \
    $$PWD/qwindowswindow.h \
    $$PWD/qwindowsintegration.h \
    $$PWD/qwindowscontext.h \
    $$PWD/qwindowsscreen.h \
    $$PWD/qwindowskeymapper.h \
    $$PWD/qwindowsmousehandler.h \
    $$PWD/qwindowspointerhandler.h \
    $$PWD/qtwindowsglobal.h \
    $$PWD/qwindowsole.h \
    $$PWD/qwindowsdropdataobject.h \
    $$PWD/qwindowsmime.h \
    $$PWD/qwindowsinternalmimedata.h \
    $$PWD/qwindowscursor.h \
    $$PWD/qwindowsinputcontext.h \
    $$PWD/qwindowstheme.h \
    $$PWD/qwindowsmenu.h \
    $$PWD/qwindowsdialoghelpers.h \
    $$PWD/qwindowsservices.h \
    $$PWD/qwindowsnativeinterface.h \
    $$PWD/qwindowsopengltester.h \
    $$PWD/qwindowsthreadpoolrunner.h \
    $$PWD/qwin10helpers.h \
    $$PWD/qwindowdragdrophelper.h \
    $$PWD/qwindowsdescription.h

INCLUDEPATH += $$PWD

qtConfig(opengl): HEADERS += $$PWD/qwindowsopenglcontext.h

qtConfig(opengl) {
    SOURCES += $$PWD/qwindowsglcontext.cpp
    HEADERS += $$PWD/qwindowsglcontext.h
}

qtConfig(systemtrayicon) {
    SOURCES += $$PWD/qwindowssystemtrayicon.cpp
    HEADERS += $$PWD/qwindowssystemtrayicon.h
}

qtConfig(vulkan) {
    SOURCES += $$PWD/qwindowsvulkaninstance.cpp
    HEADERS += $$PWD/qwindowsvulkaninstance.h
}

qtConfig(clipboard) {
    SOURCES += $$PWD/qwindowsclipboard.cpp
    HEADERS += $$PWD/qwindowsclipboard.h
    # drag and drop on windows only works if a clipboard is available
    qtConfig(draganddrop) {
        HEADERS += $$PWD/qwindowsdrag.h
        SOURCES += $$PWD/qwindowsdrag.cpp
    }
}

qtConfig(tabletevent) {
    INCLUDEPATH += $$QT_SOURCE_TREE/src/3rdparty/wintab
    HEADERS += $$PWD/qwindowstabletsupport.h
    SOURCES += $$PWD/qwindowstabletsupport.cpp
}

qtConfig(sessionmanager) {
    SOURCES += $$PWD/qwindowssessionmanager.cpp
    HEADERS += $$PWD/qwindowssessionmanager.h
}

qtConfig(imageformat_png):RESOURCES += $$PWD/cursors.qrc

RESOURCES += $$PWD/openglblacklists.qrc

qtConfig(accessibility): include($$PWD/uiautomation/uiautomation.pri)
