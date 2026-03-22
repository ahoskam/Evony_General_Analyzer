cmake_minimum_required(VERSION 3.20)

project(EvonyGeneralAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EVONY_BUILD_GUI "Build the writable GUI editor" ON)
option(EVONY_BUILD_ANALYZER "Build the read-only analyzer" ON)
option(EVONY_BUILD_IMPORTER "Build the CLI importer" ON)

set(EVONY_BUILD_ANY_GUI_APP OFF)
if(EVONY_BUILD_GUI OR EVONY_BUILD_ANALYZER)
  set(EVONY_BUILD_ANY_GUI_APP ON)
endif()

set(EVONY_NEEDS_PKG_CONFIG OFF)

find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
  set(EVONY_NEEDS_PKG_CONFIG ON)
endif()

if(EVONY_BUILD_ANY_GUI_APP)
  find_package(OpenGL REQUIRED)
  find_package(Threads REQUIRED)
  find_package(glfw3 QUIET)
  if(NOT glfw3_FOUND AND NOT TARGET glfw AND NOT TARGET glfw3)
    set(EVONY_NEEDS_PKG_CONFIG ON)
  endif()
endif()

if(EVONY_NEEDS_PKG_CONFIG)
  find_package(PkgConfig REQUIRED)
endif()

if(NOT SQLite3_FOUND)
  pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
endif()

if(EVONY_BUILD_ANY_GUI_APP AND NOT glfw3_FOUND AND NOT TARGET glfw AND NOT TARGET glfw3)
  pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
endif()

if(EVONY_BUILD_ANY_GUI_APP)
  if(TARGET glfw)
    set(EVONY_GLFW_TARGET glfw)
  elseif(TARGET glfw3)
    set(EVONY_GLFW_TARGET glfw3)
  elseif(TARGET PkgConfig::GLFW3)
    set(EVONY_GLFW_TARGET PkgConfig::GLFW3)
  else()
    message(FATAL_ERROR "Could not find a GLFW target. Install glfw3 or provide a package config.")
  endif()
endif()

if(SQLite3_FOUND)
  set(EVONY_SQLITE_TARGET SQLite::SQLite3)
elseif(TARGET PkgConfig::SQLITE3)
  set(EVONY_SQLITE_TARGET PkgConfig::SQLITE3)
else()
  message(FATAL_ERROR "Could not find an SQLite3 target. Install sqlite3 development files.")
endif()

set(EVONY_COMMON_WARNINGS
  -Wall
  -Wextra
  -Wpedantic
)

set(EVONY_IMGUI_SOURCES
  external/imgui/imgui.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_widgets.cpp
  external/imgui/imgui_demo.cpp
  external/imgui/backends/imgui_impl_glfw.cpp
  external/imgui/backends/imgui_impl_opengl3.cpp
  external/imgui/misc/cpp/imgui_stdlib.cpp
)

set(EVONY_GUI_SOURCES
  src/main_gui.cpp
  src/ui.cpp
  src/model.cpp
  src/db.cpp
  src/db_admin.cpp
  src/db_maintenance.cpp
  src/importer/DbImportV2.cpp
  src/importer/GeneralLoaderV2.cpp
  src/importer/import_service.cpp
)

set(EVONY_ANALYZER_SOURCES
  src/analyzer/main_analyzer.cpp
  src/analyzer/readonly_db.cpp
  src/analyzer/model.cpp
  src/analyzer/compute.cpp
  src/analyzer/json.cpp
  src/analyzer/ui.cpp
)

set(EVONY_IMPORTER_SOURCES
  src/importer/main_importer.cpp
  src/importer/DbImportV2.cpp
  src/importer/GeneralLoaderV2.cpp
  src/importer/import_service.cpp
  src/db_maintenance.cpp
)

add_library(evony_build_settings INTERFACE)
target_compile_features(evony_build_settings INTERFACE cxx_std_20)
target_compile_options(evony_build_settings INTERFACE ${EVONY_COMMON_WARNINGS})
target_include_directories(evony_build_settings INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/misc/cpp
)

function(evony_enable_gui_platform_libs target_name)
  target_link_libraries(${target_name} PRIVATE
    ${EVONY_GLFW_TARGET}
    ${EVONY_SQLITE_TARGET}
    OpenGL::GL
    Threads::Threads
  )

  if(CMAKE_DL_LIBS)
    target_link_libraries(${target_name} PRIVATE ${CMAKE_DL_LIBS})
  endif()
endfunction()

if(EVONY_BUILD_GUI)
  add_executable(evony_gui_v2
    ${EVONY_GUI_SOURCES}
    ${EVONY_IMGUI_SOURCES}
  )
  target_link_libraries(evony_gui_v2 PRIVATE evony_build_settings)
  evony_enable_gui_platform_libs(evony_gui_v2)
endif()

if(EVONY_BUILD_ANALYZER)
  add_executable(evony_analyzer_ro
    ${EVONY_ANALYZER_SOURCES}
    ${EVONY_IMGUI_SOURCES}
  )
  target_link_libraries(evony_analyzer_ro PRIVATE evony_build_settings)
  evony_enable_gui_platform_libs(evony_analyzer_ro)
endif()

if(EVONY_BUILD_IMPORTER)
  add_executable(importer_v2 ${EVONY_IMPORTER_SOURCES})
  target_link_libraries(importer_v2 PRIVATE
    evony_build_settings
    ${EVONY_SQLITE_TARGET}
  )
endif()
