QT += core gui widgets

TEMPLATE = lib
TARGET = OpenRGBRipplePlugin
CONFIG += plugin c++17 skip_target_version_ext

VERSION = 1.0.0
DEFINES += VERSION_STRING=\\\"$$VERSION\\\"
DEFINES += OPENRGB_PLUGIN_API_VERSION=4

# Headers only — never compile OpenRGB .cpp into the plugin.
# RGBController / SettingsManager live in OpenRGB.exe.
INCLUDEPATH += \
    . \
    OpenRGB \
    OpenRGB/RGBController \
    OpenRGB/dependencies/json \
    OpenRGB/i2c_smbus \
    OpenRGB/net_port \
    OpenRGB/qt

HEADERS += \
    OpenRGBRipplePlugin.h \
    RippleWidget.h \
    RippleEngine.h \
    KeyboardHook.h \
    KeyMap.h

SOURCES += \
    OpenRGBRipplePlugin.cpp \
    RippleWidget.cpp \
    KeyboardHook.cpp \
    KeyMap.cpp

win32 {
    LIBS += -luser32
    DEFINES += _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
}

RESOURCES += OpenRGBRipplePlugin.qrc

win32:CONFIG(release, debug|release): DESTDIR = $$PWD/build
else:win32:CONFIG(debug, debug|release): DESTDIR = $$PWD/build
