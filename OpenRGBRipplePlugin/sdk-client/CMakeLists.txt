cmake_minimum_required(VERSION 3.16)
project(OpenRGBRipple LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OpenRGBRipple
    OpenRGBRippleClient.cpp
    ../KeyMap.cpp
    ../KeyMap.h
    ../RippleEngine.h
    ../KeyboardHook.h
)

target_include_directories(OpenRGBRipple PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(OpenRGBRipple PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(OpenRGBRipple PRIVATE /EHsc)
    target_link_libraries(OpenRGBRipple PRIVATE ws2_32 user32)
endif()
