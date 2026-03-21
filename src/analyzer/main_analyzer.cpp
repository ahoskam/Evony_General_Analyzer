#include "analyzer/json.h"
#include "analyzer/readonly_db.h"
#include "analyzer/ui.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include <GLFW/glfw3.h>

#include <iostream>
#include <string>

namespace {

std::string arg_value(int argc, char** argv, const std::string& key,
                      const std::string& def) {
  for (int i = 1; i < argc - 1; ++i) {
    if (argv[i] == key) {
      return argv[i + 1];
    }
  }
  return def;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string db_path = arg_value(argc, argv, "--db", "data/evony_v2.db");
  const std::string state_path =
      arg_value(argc, argv, "--state", "data/analyzer_owned_state.v1.json");

  AnalyzerDb db;
  try {
    db.open_read_only(db_path);
  } catch (const std::exception& e) {
    std::cerr << "Failed to open DB read-only: " << e.what() << "\n";
    return 1;
  }

  if (!glfwInit()) {
    return 1;
  }

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  GLFWwindow* window = glfwCreateWindow(1400, 820,
                                        "Evony Analyzer (Read Only)", nullptr,
                                        nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  AnalyzerAppState state;
  state.state_path = state_path;
  try {
    state.owned_file = load_owned_state_file(state_path);
    state.owned_file.db_path_hint = db_path;
    state.status_message = "Loaded owned state from " + state_path;
  } catch (const std::exception& e) {
    state.status_message =
        std::string("State file load failed, starting blank: ") + e.what();
    state.owned_file = OwnedStateFile{};
    state.owned_file.db_path_hint = db_path;
  }

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    try {
      analyzer_ui_tick(db, state);
    } catch (const std::exception& e) {
      ImGui::Begin("Evony Analyzer (Read Only)");
      ImGui::TextWrapped("Analyzer error: %s", e.what());
      ImGui::End();
    }

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  if (state.dirty) {
    try {
      save_owned_state_file(state.state_path, state.owned_file);
    } catch (const std::exception& e) {
      std::cerr << "Failed to save owned state on exit: " << e.what() << "\n";
    }
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
