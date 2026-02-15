# Makefile (v2: GUI + Importer v2 + optional legacy CLI)
# ======================================================

# =========================
# Project
# =========================
APP := evony_generals

CXX := g++
CXXSTD := -std=c++20
WARN := -Wall -Wextra -Wpedantic
OPT  := -O0

# =========================
# Folders
# =========================
SRC_DIR := src
INC_DIR := include

IMGUI_DIR := external/imgui
IMGUI_BACKEND_DIR := $(IMGUI_DIR)/backends

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

# =========================
# pkg-config
# =========================
PKG_CFLAGS_GLFW := $(shell pkg-config --cflags glfw3 2>/dev/null)
PKG_LIBS_GLFW   := $(shell pkg-config --libs glfw3 2>/dev/null)

PKG_CFLAGS_SQL  := $(shell pkg-config --cflags sqlite3 2>/dev/null)
PKG_LIBS_SQL    := $(shell pkg-config --libs sqlite3 2>/dev/null)

# =========================
# Includes
# =========================
INCLUDES_COMMON := -I$(INC_DIR)

INCLUDES_GUI := \
	$(INCLUDES_COMMON) \
	-I$(IMGUI_DIR) \
	-I$(IMGUI_BACKEND_DIR) \
	$(PKG_CFLAGS_GLFW) \
	$(PKG_CFLAGS_SQL)

INCLUDES_IMPORTER := \
	$(INCLUDES_COMMON) \
	$(PKG_CFLAGS_SQL)

INCLUDES_CLI := \
	$(INCLUDES_COMMON) \
	$(PKG_CFLAGS_SQL)

# =========================
# Link libs (Manjaro/Linux)
# =========================
LIBS_GUI := $(PKG_LIBS_GLFW) $(PKG_LIBS_SQL) -lGL -ldl -lpthread
LIBS_IMPORTER := $(PKG_LIBS_SQL)
LIBS_CLI := $(PKG_LIBS_SQL)

# =========================
# Flags
# =========================
CXXFLAGS_COMMON := $(CXXSTD) $(WARN) $(OPT) -MMD -MP
CXXFLAGS_GUI := $(CXXFLAGS_COMMON) $(INCLUDES_GUI)
CXXFLAGS_IMPORTER := $(CXXFLAGS_COMMON) $(INCLUDES_IMPORTER)
CXXFLAGS_CLI := $(CXXFLAGS_COMMON) $(INCLUDES_CLI)

# =========================
# Sources
# =========================
ALL_SRC := $(shell find $(SRC_DIR) -name '*.cpp')

GUI_MAIN := $(SRC_DIR)/gui/main.cpp
IMPORTER_MAIN := $(SRC_DIR)/importer/main_importer.cpp
CLI_MAIN := $(SRC_DIR)/main.cpp

# ImGui sources (explicit)
IMGUI_SRC := \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_demo.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_BACKEND_DIR)/imgui_impl_glfw.cpp \
	$(IMGUI_BACKEND_DIR)/imgui_impl_opengl3.cpp

# Importer v2: ONLY src/importer/*
IMPORTER_SRCS := $(filter $(SRC_DIR)/importer/%,$(ALL_SRC))

# GUI: everything under src except src/importer/* AND except legacy CLI main (src/main.cpp), plus ImGui
GUI_SRCS := $(filter-out $(SRC_DIR)/importer/%,$(ALL_SRC))
GUI_SRCS := $(filter-out $(CLI_MAIN),$(GUI_SRCS))
GUI_SRCS := $(GUI_SRCS) $(IMGUI_SRC)

# Optional legacy CLI: everything except gui/* and importer/*
CLI_SRCS := $(filter-out $(SRC_DIR)/gui/%,$(ALL_SRC))
CLI_SRCS := $(filter-out $(SRC_DIR)/importer/%,$(CLI_SRCS))

# =========================
# Object mapping helpers
# =========================
# Map "path/to/file.cpp" -> "build/obj/path/to/file.o"
to_obj = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(1))

GUI_OBJS := $(call to_obj,$(GUI_SRCS))
IMPORTER_OBJS := $(call to_obj,$(IMPORTER_SRCS))
CLI_OBJS := $(call to_obj,$(CLI_SRCS))

ALL_OBJS := $(GUI_OBJS) $(IMPORTER_OBJS) $(CLI_OBJS)

# =========================
# Targets
# =========================
.PHONY: all clean gui importer cli run_gui run_importer run_cli

# Build BOTH by default (this is the main usability fix)
all: gui importer

# --- GUI ---
gui: $(BUILD_DIR)/$(APP)_gui

$(BUILD_DIR)/$(APP)_gui: $(GUI_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LIBS_GUI)

run_gui: $(BUILD_DIR)/$(APP)_gui
	env GLFW_PLATFORM=x11 ./$(BUILD_DIR)/$(APP)_gui

# --- Importer v2 ---
importer: $(BUILD_DIR)/importer_v2

$(BUILD_DIR)/importer_v2: $(IMPORTER_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LIBS_IMPORTER)

run_importer: $(BUILD_DIR)/importer_v2
	./$(BUILD_DIR)/importer_v2 --db data/evony_v2.db --path data/import

# --- Optional legacy CLI ---
# Only build if src/main.cpp exists (prevents accidental "no input files" behavior)
ifeq ($(wildcard $(CLI_MAIN)),)
cli:
	@echo "No legacy CLI main found at $(CLI_MAIN); skipping."
run_cli: cli
else
cli: $(BUILD_DIR)/$(APP)_cli

$(BUILD_DIR)/$(APP)_cli: $(CLI_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LIBS_CLI)

run_cli: $(BUILD_DIR)/$(APP)_cli
	./$(BUILD_DIR)/$(APP)_cli
endif

# =========================
# Compile rules (deterministic)
# =========================
# We compile GUI objects with GUI flags, importer with importer flags, cli with cli flags.
# This avoids pattern-rule precedence surprises.

# GUI objects
$(OBJ_DIR)/$(SRC_DIR)/gui/%.o: $(SRC_DIR)/gui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

# ImGui objects
$(OBJ_DIR)/external/imgui/%.o: external/imgui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

$(OBJ_DIR)/external/imgui/backends/%.o: external/imgui/backends/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

# Importer objects
$(OBJ_DIR)/$(SRC_DIR)/importer/%.o: $(SRC_DIR)/importer/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_IMPORTER) -c $< -o $@

# Everything else (core sources used by GUI/CLI) compiled with CLI flags.
# NOTE: GUI still links its own objects built with GUI flags above; this rule is only for non-gui/non-importer paths.
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_CLI) -c $< -o $@

# =========================
# Housekeeping
# =========================
clean:
	rm -rf $(BUILD_DIR)

# Auto-deps
-include $(ALL_OBJS:.o=.d)
