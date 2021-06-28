HEADERS += \
        $$PWD/qfreetypefontdatabase_p.h \
        $$PWD/qfontengine_ft_p.h

SOURCES += \
        $$PWD/qfreetypefontdatabase.cpp \
        $$PWD/qfontengine_ft.cpp

QMAKE_USE_PRIVATE += freetype
qtConfig(harfbuzz) {
    QMAKE_USE_PRIVATE += harfbuzz
}
