TARGET = qtlibjpeg

CONFIG += \
    static \
    hide_symbols \
    exceptions_off rtti_off warn_off \
    installed

MODULE_INCLUDEPATH = $$PWD
MODULE_INCLUDEPATH += $$PWD/src

load(qt_helper_lib)

include(../libjpeg.pri)
