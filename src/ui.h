#pragma once
#include "db.h"
#include "editor_state.h"

struct UiConfig {
  const char* glsl_version = "#version 130";
};

void ui_tick(Db& db, EditorState& st);
