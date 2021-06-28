TARGET = fcitxplatforminputcontextplugin

QT += dbus gui-private widgets-private

TEMPLATE = lib
CONFIG += plugin

SOURCES += $$PWD/qfcitxplatforminputcontext.cpp \
		   $$PWD/fcitxinputcontextproxy.cpp \
		   $$PWD/fcitxqtdbustypes.cpp \
		   $$PWD/fcitxwatcher.cpp \
		   $$PWD/qtkey.cpp \
		   $$PWD/main.cpp

HEADERS += $$PWD/qfcitxplatforminputcontext.h \
		   $$PWD/fcitxinputcontextproxy.h \
		   $$PWD/fcitxqtdbustypes.h \
		   $$PWD/fcitxwatcher.h \
		   $$PWD/qtkey.h \
		   $$PWD/main.h

inputmethod_interface.files = $$PWD/org.fcitx.Fcitx.InputMethod.xml
inputmethod_interface.header_flags = -i fcitxqtdbustypes.h -c InputMethodProxy
inputmethod_interface.source_flags = -c InputMethodProxy

inputmethod1_interface.files = $$PWD/org.fcitx.Fcitx.InputMethod1.xml
inputmethod1_interface.header_flags = -i fcitxqtdbustypes.h -c InputMethod1Proxy
inputmethod1_interface.source_flags = -c InputMethod1Proxy

inputcontext_interface.files = $$PWD/org.fcitx.Fcitx.InputContext.xml
inputcontext_interface.header_flags = -i fcitxqtdbustypes.h -c InputContextProxy
inputcontext_interface.source_flags = -c InputContextProxy

inputcontext1_interface.files = $$PWD/org.fcitx.Fcitx.InputContext1.xml
inputcontext1_interface.header_flags = -i fcitxqtdbustypes.h -c InputContext1Proxy
inputcontext1_interface.source_flags = -c InputContext1Proxy

DBUS_INTERFACES += inputmethod_interface \
				   inputmethod1_interface \
				   inputcontext_interface \
				   inputcontext1_interface

QMAKE_USE_PRIVATE += xkbcommon
OTHER_FILES += $$PWD/fcitx.json

PLUGIN_TYPE = platforminputcontexts
PLUGIN_EXTENDS = -
PLUGIN_CLASS_NAME = QFcitxPlatformInputContextPlugin
load(qt_plugin)