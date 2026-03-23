cmake_minimum_required(VERSION 3.20)

project(EvonyGeneralAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EVONY_BUILD_GUI "Build the writable GUI editor" ON)
option(EVONY_BUILD_ANALYZER "Build the read-only analyzer" ON)
option(EVONY_BUILD_IMPORTER "Build the CLI importer" ON)

set(EVONY_IS_CROSS_WINDOWS OFF)
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(EVONY_IS_CROSS_WINDOWS ON)
endif()

set(EVONY_BUILD_ANY_GUI_APP OFF)
if(EVONY_BUILD_GUI OR EVONY_BUILD_ANALYZER)
  set(EVONY_BUILD_ANY_GUI_APP ON)
endif()

set(EVONY_NEEDS_PKG_CONFIG OFF)

if(EVONY_IS_CROSS_WINDOWS)
  find_path(EVONY_SQLITE_INCLUDE_DIR sqlite3.h)
  find_library(EVONY_SQLITE_LIBRARY NAMES sqlite3 libsqlite3)

  if(NOT EVONY_SQLITE_INCLUDE_DIR OR NOT EVONY_SQLITE_LIBRARY)
    message(FATAL_ERROR
      "Windows cross-compilation requires a MinGW-targeted SQLite3 install. "
      "Could not find sqlite3.h and libsqlite3 in the MinGW toolchain paths. "
      "Install a Windows SQLite package for MinGW or pass CMAKE_PREFIX_PATH/SQLite3_ROOT.")
  endif()

  add_library(evony_sqlite_dep INTERFACE)
  target_include_directories(evony_sqlite_dep INTERFACE ${EVONY_SQLITE_INCLUDE_DIR})
  target_link_libraries(evony_sqlite_dep INTERFACE ${EVONY_SQLITE_LIBRARY})
  set(EVONY_SQLITE_TARGET evony_sqlite_dep)

  if(EVONY_BUILD_ANY_GUI_APP)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)

    find_path(EVONY_GLFW_INCLUDE_DIR GLFW/glfw3.h)
    find_library(EVONY_GLFW_LIBRARY NAMES glfw3 glfw)

    if(NOT EVONY_GLFW_INCLUDE_DIR OR NOT EVONY_GLFW_LIBRARY)
      message(FATAL_ERROR
        "Windows cross-compilation of the GUI/analyzer requires a MinGW-targeted GLFW3 install. "
        "Could not find GLFW/glfw3.h and a glfw library in the MinGW toolchain paths. "
        "Install a Windows GLFW package for MinGW or pass CMAKE_PREFIX_PATH.")
    endif()

    add_library(evony_glfw_dep INTERFACE)
    target_include_directories(evony_glfw_dep INTERFACE ${EVONY_GLFW_INCLUDE_DIR})
    target_link_libraries(evony_glfw_dep INTERFACE ${EVONY_GLFW_LIBRARY})
    set(EVONY_GLFW_TARGET evony_glfw_dep)
  endif()
else()
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

  if(NOT EVONY_IS_CROSS_WINDOWS)
    add_custom_target(run_gui
      COMMAND $<TARGET_FILE:evony_gui_v2> --db data/evony_v2.db
      DEPENDS evony_gui_v2
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Running evony_gui_v2"
      USES_TERMINAL
    )
  endif()
endif()

if(EVONY_BUILD_ANALYZER)
  add_executable(evony_analyzer_ro
    ${EVONY_ANALYZER_SOURCES}
    ${EVONY_IMGUI_SOURCES}
  )
  target_link_libraries(evony_analyzer_ro PRIVATE evony_build_settings)
  evony_enable_gui_platform_libs(evony_analyzer_ro)

  if(NOT EVONY_IS_CROSS_WINDOWS)
    add_custom_target(run_analyzer
      COMMAND $<TARGET_FILE:evony_analyzer_ro> --db data/evony_v2.db --state data/analyzer_owned_state.v1.json
      DEPENDS evony_analyzer_ro
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Running evony_analyzer_ro"
      USES_TERMINAL
    )
  endif()
endif()

if(EVONY_BUILD_IMPORTER)
  add_executable(importer_v2 ${EVONY_IMPORTER_SOURCES})
  target_link_libraries(importer_v2 PRIVATE
    evony_build_settings
    ${EVONY_SQLITE_TARGET}
  )

  if(NOT EVONY_IS_CROSS_WINDOWS)
    add_custom_target(run_importer
      COMMAND $<TARGET_FILE:importer_v2> --db data/evony_v2.db --path data/import
      DEPENDS importer_v2
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Running importer_v2"
      USES_TERMINAL
    )
  endif()
endif()

if(EVONY_IS_CROSS_WINDOWS)
  set(EVONY_WINDOWS_BUNDLE_DIR ${CMAKE_BINARY_DIR}/windows-bundle)
  set(EVONY_WINDOWS_BUNDLE_DATA_DIR ${EVONY_WINDOWS_BUNDLE_DIR}/data)

  set(EVONY_WINDOWS_DLLS
    /usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll
    /usr/x86_64-w64-mingw32/bin/libatomic-1.dll
    /usr/x86_64-w64-mingw32/bin/libssp-0.dll
    /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll
    /usr/x86_64-w64-mingw32/bin/libsqlite3-0.dll
    /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll
  )

  set(EVONY_WINDOWS_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/evony_v2.db
    ${CMAKE_CURRENT_SOURCE_DIR}/data/canonical_keys.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/data/analyzer_owned_state.template.json
  )

  set(EVONY_WINDOWS_TARGETS)
  if(TARGET evony_gui_v2)
    list(APPEND EVONY_WINDOWS_TARGETS evony_gui_v2)
  endif()
  if(TARGET evony_analyzer_ro)
    list(APPEND EVONY_WINDOWS_TARGETS evony_analyzer_ro)
  endif()
  if(TARGET importer_v2)
    list(APPEND EVONY_WINDOWS_TARGETS importer_v2)
  endif()

  add_custom_target(bundle_windows
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${EVONY_WINDOWS_BUNDLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EVONY_WINDOWS_BUNDLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EVONY_WINDOWS_BUNDLE_DATA_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${EVONY_WINDOWS_BUNDLE_DATA_DIR}/import
    COMMENT "Bundling Windows executables and runtime DLLs"
  )

  foreach(evony_target IN LISTS EVONY_WINDOWS_TARGETS)
    add_dependencies(bundle_windows ${evony_target})
    add_custom_command(TARGET bundle_windows POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:${evony_target}>
      ${EVONY_WINDOWS_BUNDLE_DIR}/
    )
  endforeach()

  foreach(evony_dll IN LISTS EVONY_WINDOWS_DLLS)
    if(EXISTS ${evony_dll})
      add_custom_command(TARGET bundle_windows POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${evony_dll}
        ${EVONY_WINDOWS_BUNDLE_DIR}/
      )
    endif()
  endforeach()

  foreach(evony_data_file IN LISTS EVONY_WINDOWS_DATA_FILES)
    if(EXISTS ${evony_data_file})
      add_custom_command(TARGET bundle_windows POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${evony_data_file}
        ${EVONY_WINDOWS_BUNDLE_DATA_DIR}/
      )
    endif()
  endforeach()
endif()
