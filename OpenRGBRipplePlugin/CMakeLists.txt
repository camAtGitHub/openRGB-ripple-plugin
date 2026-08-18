cmake_minimum_required(VERSION 3.16)
project(OpenRGBRipplePlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)

set(OPENRGB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/OpenRGB")
if(NOT EXISTS "${OPENRGB_DIR}/OpenRGBPluginInterface.h")
    message(FATAL_ERROR
        "Clone OpenRGB 1.0rc2 into ${OPENRGB_DIR}\n"
        "git clone --branch release_candidate_1.0rc2 --depth 1 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB")
endif()

add_library(OpenRGBRipplePlugin SHARED
    OpenRGBRipplePlugin.cpp
    OpenRGBRipplePlugin.h
    RippleWidget.cpp
    RippleWidget.h
    DeviceSession.cpp
    DeviceSession.h
    RippleSettingsIO.h
    DetectionLifecycle.h
    KeyboardHook.cpp
    KeyboardHook.h
    KeyMap.cpp
    KeyMap.h
    RippleEngine.h
    OpenRGBRipplePlugin.qrc
)

target_include_directories(OpenRGBRipplePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENRGB_DIR}
    ${OPENRGB_DIR}/RGBController
    ${OPENRGB_DIR}/dependencies/json
    ${OPENRGB_DIR}/i2c_smbus
    ${OPENRGB_DIR}/net_port
    ${OPENRGB_DIR}/qt
)

target_compile_definitions(OpenRGBRipplePlugin PRIVATE
    VERSION_STRING="1.0.6"
    OPENRGB_PLUGIN_API_VERSION=4
)

target_link_libraries(OpenRGBRipplePlugin PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
if(WIN32)
    target_link_libraries(OpenRGBRipplePlugin PRIVATE user32)
    target_compile_definitions(OpenRGBRipplePlugin PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set_target_properties(OpenRGBRipplePlugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "OpenRGBRipplePlugin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
