TEMPLATE = subdirs
QT_FOR_CONFIG += gui-private

android:!android-embedded: SUBDIRS += android

!android: SUBDIRS += minimal

!android:qtConfig(freetype): SUBDIRS += offscreen

qtConfig(xcb) {
    SUBDIRS += xcb
}

uikit:!watchos: SUBDIRS += ios
osx: SUBDIRS += cocoa

win32:!winrt: SUBDIRS += windows
winrt: SUBDIRS += winrt

qtConfig(direct2d1_1):qtConfig(directwrite1) {
    SUBDIRS += direct2d
}

qnx {
    SUBDIRS += qnx
}

qtConfig(eglfs) {
    SUBDIRS += eglfs
    SUBDIRS += minimalegl
}

qtConfig(directfb) {
    SUBDIRS += directfb
}

qtConfig(linuxfb): SUBDIRS += linuxfb

qtHaveModule(network):qtConfig(vnc): SUBDIRS += vnc

freebsd {
    SUBDIRS += bsdfb
}

haiku {
    SUBDIRS += haiku
}

wasm: SUBDIRS += wasm

qtConfig(mirclient): SUBDIRS += mirclient

qtConfig(integrityfb): SUBDIRS += integrity
