cmake_minimum_required(VERSION 3.21)

project(qScreenRec VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT WIN32)
    message(FATAL_ERROR "qScreenRec 只支援 Windows（Windows 10 1903 / Windows 11）")
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
qt_standard_project_setup()

set(APP_COMPANY   "SAISAI.Dev")
set(APP_COPYRIGHT "© 2026 SAISAI.Dev")
set(APP_EMAIL     "aisais.dev@gmail.com")

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)
configure_file(resources/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)

set(SOURCES
    src/main.cpp
    src/app.cpp                    src/app.h
    src/core/settings.cpp          src/core/settings.h
    src/core/hotkey.cpp            src/core/hotkey.h
    src/core/ffmpeg.cpp            src/core/ffmpeg.h
    src/core/recorder.cpp          src/core/recorder.h
    src/core/watermark.cpp         src/core/watermark.h
    src/core/license.cpp           src/core/license.h
    src/core/autostart.cpp         src/core/autostart.h
    src/capture/capturer.cpp       src/capture/capturer.h
    src/capture/dxgicapturer.cpp   src/capture/dxgicapturer.h
    src/capture/gdicapturer.cpp    src/capture/gdicapturer.h
    src/ui/theme.cpp               src/ui/theme.h
    src/ui/settingsdialog.cpp      src/ui/settingsdialog.h
    src/ui/aboutdialog.cpp         src/ui/aboutdialog.h
    src/ui/helpdialog.cpp          src/ui/helpdialog.h
    src/ui/hotkeyedit.cpp          src/ui/hotkeyedit.h
    resources/resources.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

qt_add_executable(qScreenRec WIN32 ${SOURCES})

qt_add_translations(qScreenRec
    TS_FILES i18n/qscreenrec_zh_TW.ts
    RESOURCE_PREFIX /i18n
)

target_include_directories(qScreenRec PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(qScreenRec PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(qScreenRec PRIVATE /utf-8 /W3)
endif()

target_link_libraries(qScreenRec PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    d3d11 dxgi dwmapi user32 gdi32 shell32 advapi32
)

# windeployqt after build:  cmake --build build --target deploy
find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT)
    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT}" --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler
                "$<TARGET_FILE:qScreenRec>"
        DEPENDS qScreenRec
        COMMENT "Running windeployqt")
endif()
