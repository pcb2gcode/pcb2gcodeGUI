cmake_minimum_required(VERSION 3.16)

project(
  pcb2gcodeGUI
  VERSION 3.2.0
  DESCRIPTION "A graphical front end for pcb2gcode"
  HOMEPAGE_URL "https://github.com/pcb2gcode/pcb2gcodeGUI"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Svg SvgWidgets)

set(PCB2GCODE_EXECUTABLE "" CACHE FILEPATH
    "pcb2gcode for the tests that run it; looked up on PATH when empty")

add_library(
  pcb2gcodegui_core STATIC
  src/condition.cpp
  src/condition.h
  src/configfile.cpp
  src/configfile.h
  src/graphics_view_zoom.cpp
  src/graphics_view_zoom.h
  src/layout.cpp
  src/layout.h
  src/mainwindow.cpp
  src/mainwindow.h
  src/optioneditors.cpp
  src/optioneditors.h
  src/optionform.cpp
  src/optionform.h
  src/optionschema.cpp
  src/optionschema.h
  src/outputwindow.cpp
  src/outputwindow.h
  src/pcb2gcode.cpp
  src/pcb2gcode.h
  src/previewpane.cpp
  src/previewpane.h
  src/previews.cpp
  src/previews.h
  src/units.cpp
  src/units.h)
target_include_directories(pcb2gcodegui_core PUBLIC src)
target_link_libraries(pcb2gcodegui_core PUBLIC Qt6::Widgets Qt6::Svg Qt6::SvgWidgets)
target_compile_definitions(pcb2gcodegui_core PUBLIC PCB2GCODEGUI_VERSION="${PROJECT_VERSION}"
                                                    QT_NO_CAST_FROM_BYTEARRAY)
if(MSVC)
  target_compile_options(pcb2gcodegui_core PRIVATE /W4)
else()
  target_compile_options(pcb2gcodegui_core PRIVATE -Wall -Wextra)
endif()

# Resources are compiled into each executable: a static library's resources get dropped
# by the linker unless something references them.
set(PCB2GCODEGUI_RESOURCES ${PROJECT_SOURCE_DIR}/resources/pcb2gcodeGUI.qrc)

add_executable(pcb2gcodeGUI src/main.cpp ${PCB2GCODEGUI_RESOURCES})
target_link_libraries(pcb2gcodeGUI PRIVATE pcb2gcodegui_core)
set_target_properties(
  pcb2gcodeGUI
  PROPERTIES WIN32_EXECUTABLE ON
             MACOSX_BUNDLE ON
             MACOSX_BUNDLE_GUI_IDENTIFIER io.github.pcb2gcode.pcb2gcodeGUI
             MACOSX_BUNDLE_BUNDLE_NAME pcb2gcodeGUI
             MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
             MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

install(
  TARGETS pcb2gcodeGUI
  BUNDLE DESTINATION .
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
  install(FILES packaging/pcb2gcodeGUI.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES resources/pcb2gcodeGUI.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# Windows and macOS packages carry their Qt libraries (windeployqt / macdeployqt).
if((WIN32 OR APPLE) AND COMMAND qt_generate_deploy_app_script)
  qt_generate_deploy_app_script(TARGET pcb2gcodeGUI OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${deploy_script})
endif()

set(CPACK_PACKAGE_VENDOR "pcb2gcode")
set(CPACK_PACKAGE_CONTACT "https://github.com/pcb2gcode/pcb2gcodeGUI/issues")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "pcb2gcode")
if(WIN32)
  set(CPACK_GENERATOR ZIP)
elseif(APPLE)
  set(CPACK_GENERATOR DragNDrop)
else()
  set(CPACK_GENERATOR TGZ DEB)
endif()
include(CPack)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
