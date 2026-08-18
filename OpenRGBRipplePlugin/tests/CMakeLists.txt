cmake_minimum_required(VERSION 3.16)
project(OpenRGBRippleHostlessTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(test_ripple_engine
    hostless/test_ripple_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KeyMap.cpp
)
target_include_directories(test_ripple_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
add_custom_target(check COMMAND test_ripple_engine DEPENDS test_ripple_engine)
