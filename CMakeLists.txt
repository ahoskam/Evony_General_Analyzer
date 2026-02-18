cmake_minimum_required(VERSION 3.20)
project(evony_gui_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Configure these paths for your repo ----
# If you already have ImGui compiled elsewhere, replace this section with your own targets.
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui" CACHE PATH "Path to Dear ImGui")
set(GLFW_DIR "" CACHE PATH "Optional: custom GLFW path")

add_executable(evony_gui_v2
  src/main_gui.cpp
  src/db.cpp
  src/model.cpp
  src/ui.cpp
)

target_include_directories(evony_gui_v2 PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

# ImGui sources (common minimal set)
target_sources(evony_gui_v2 PRIVATE
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(evony_gui_v2 PRIVATE OpenGL::GL glfw sqlite3)

# Some distros need this for OpenGL loader; adjust as needed.
target_compile_definitions(evony_gui_v2 PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD=0)
