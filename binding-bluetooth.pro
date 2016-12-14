TARGET = settings-bluetooth-binding

HEADERS = bluetooth-api.h bluetooth-manager.h
SOURCES = bluetooth-api.c bluetooth-manager.c

LIBS += -Wl,--version-script=$$PWD/export.map

CONFIG += link_pkgconfig
PKGCONFIG += json-c afb-daemon glib-2.0 gio-2.0 gobject-2.0 zlib

include(binding.pri)
