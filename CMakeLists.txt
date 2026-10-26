cmake_minimum_required(VERSION 3.24)
project(MokouScreenshot VERSION 0.4.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets DBus Network WaylandClient)
find_package(KF6GlobalAccel REQUIRED)
find_package(KPipeWire 6.7 REQUIRED)
find_package(LayerShellQt REQUIRED)
find_package(PlasmaWaylandProtocols REQUIRED)
qt_add_library(mokou-core STATIC
    src/ai.cpp
    src/annotationinput.cpp
    src/application.cpp
    src/audiocapture.cpp
    src/capture.cpp
    src/editortoolbar.cpp
    src/document.cpp
    src/imageoutput.cpp
    src/ocr.cpp
    src/overlay.cpp
    src/panels.cpp
    src/readingpanel.cpp
    src/recognition.cpp
    src/recorder.cpp
    src/settings.cpp
    src/shortcuts.cpp
    src/textoverlay.cpp
    src/textresult.cpp
    src/unixsignals.cpp
)
target_include_directories(mokou-core PUBLIC src PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(mokou-core PUBLIC Qt6::Widgets Qt6::DBus Qt6::Network
    Qt6::WaylandClient KF6::GlobalAccel K::KPipeWireRecord LayerShellQt::Interface)
qt_generate_wayland_protocol_client_sources(mokou-core FILES
    ${PLASMA_WAYLAND_PROTOCOLS_DIR}/zkde-screencast-unstable-v1.xml)
target_compile_options(mokou-core PRIVATE -Wall -Wextra -Wpedantic)
qt_add_executable(mokou-screenshot src/main.cpp resources/resources.qrc)
target_link_libraries(mokou-screenshot PRIVATE mokou-core)
target_compile_definitions(mokou-screenshot PRIVATE MOKOU_VERSION="${PROJECT_VERSION}")
target_compile_options(mokou-screenshot PRIVATE -Wall -Wextra -Wpedantic)
include(GNUInstallDirs)
install(TARGETS mokou-screenshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
configure_file(resources/io.github.fedokou.MokouScreenshot.desktop.in io.github.fedokou.MokouScreenshot.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.fedokou.MokouScreenshot.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/mokou-screenshot.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ocr/worker.py ocr/requirements.txt ocr/requirements.lock ocr/extra-models.json DESTINATION ${CMAKE_INSTALL_DATADIR}/mokou-screenshot/ocr)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/mokou-screenshot)
install(DIRECTORY LICENSES/ DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/mokou-screenshot)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    foreach(check core editor lifecycle audio ocr)
        qt_add_executable(${check}-check tests/${check}_check.cpp)
        target_link_libraries(${check}-check PRIVATE mokou-core Qt6::Test)
        target_compile_definitions(${check}-check PRIVATE MOKOU_TEST_PYTHON="${Python3_EXECUTABLE}")
    endforeach()
    target_sources(lifecycle-check PRIVATE resources/resources.qrc)
    add_test(NAME core-check COMMAND core-check -platform offscreen)
    add_test(NAME editor-check COMMAND editor-check -platform offscreen)
    add_test(NAME audio-check COMMAND audio-check)
    add_test(NAME ocr-check COMMAND ocr-check -platform offscreen)
    find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
    add_test(NAME lifecycle-check COMMAND ${DBUS_RUN_SESSION}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/tests/dbus-isolated.conf --
        $<TARGET_FILE:lifecycle-check> -platform offscreen)
    add_test(NAME repository-check COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/repository_check.py)
endif()
