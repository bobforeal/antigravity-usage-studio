cmake_minimum_required(VERSION 3.20)
project(AntigravityUsageStudio VERSION 2.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(antigravity-hud WIN32
    src/main.cpp
)

target_include_directories(antigravity-hud PRIVATE src)

target_link_libraries(antigravity-hud PRIVATE
    d2d1
    dwrite
    dwmapi
    winhttp
    iphlpapi
    ws2_32
    ole32
    uuid
)

if(MINGW)
    target_link_options(antigravity-hud PRIVATE -static -O3 -s)
endif()
