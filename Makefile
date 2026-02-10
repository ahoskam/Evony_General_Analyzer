# Makefile (updated: GUI is default)
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

INCLUDES_CLI := \
	$(INCLUDES_COMMON) \
	$(PKG_CFLAGS_SQL)

# =========================
# Sources
# =========================
ALL_SRC := $(shell find $(SRC_DIR) -name '*.cpp')

CLI_MAIN := $(SRC_DIR)/main.cpp
GUI_MAIN := $(SRC_DIR)/gui/main.cpp

# CLI: everything except gui folder + gui main
CLI_SRC := $(filter-out $(SRC_DIR)/gui/%,$(ALL_SRC))

# GUI: everything except CLI main + add imgui sources
GUI_SRC := $(filter-out $(CLI_MAIN),$(ALL_SRC))

IMGUI_SRC := \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_demo.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_BACKEND_DIR)/imgui_impl_glfw.cpp \
	$(IMGUI_BACKEND_DIR)/imgui_impl_opengl3.cpp

# Final src lists
CLI_SRCS := $(CLI_SRC)
GUI_SRCS := $(GUI_SRC) $(IMGUI_SRC)

# =========================
# Objects
# =========================
CLI_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CLI_SRCS))
GUI_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(GUI_SRCS))

# For auto-deps include
ALL_OBJS := $(CLI_OBJS) $(GUI_OBJS)

# =========================
# Link libs (Manjaro/Linux)
# =========================
LIBS_CLI := $(PKG_LIBS_SQL)
LIBS_GUI := $(PKG_LIBS_GLFW) $(PKG_LIBS_SQL) -lGL -ldl -lpthread

# =========================
# Flags
# =========================
CXXFLAGS_COMMON := $(CXXSTD) $(WARN) $(OPT) -MMD -MP

CXXFLAGS_CLI := $(CXXFLAGS_COMMON) $(INCLUDES_CLI)
CXXFLAGS_GUI := $(CXXFLAGS_COMMON) $(INCLUDES_GUI)

# =========================
# Targets
# =========================
.PHONY: all clean run run_gui cli gui

all: gui

cli: $(BUILD_DIR)/$(APP)
gui: $(BUILD_DIR)/$(APP)_gui

$(BUILD_DIR)/$(APP): $(CLI_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LIBS_CLI)

$(BUILD_DIR)/$(APP)_gui: $(GUI_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LIBS_GUI)

# Compile rules (separate flags for cli/gui)
$(OBJ_DIR)/$(SRC_DIR)/gui/%.o: $(SRC_DIR)/gui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

# Compile rule for ImGui sources (needs -Iexternal/imgui)
$(OBJ_DIR)/external/imgui/%.o: external/imgui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

$(OBJ_DIR)/external/imgui/backends/%.o: external/imgui/backends/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_GUI) -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_CLI) -c $< -o $@

run: $(BUILD_DIR)/$(APP)
	./$(BUILD_DIR)/$(APP)

run_gui: $(BUILD_DIR)/$(APP)_gui
	env GLFW_PLATFORM=x11 ./$(BUILD_DIR)/$(APP)_gui

clean:
	rm -rf $(BUILD_DIR)

# Auto-deps
-include $(ALL_OBJS:.o=.d)
