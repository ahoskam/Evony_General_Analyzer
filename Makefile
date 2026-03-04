CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O0 -MMD -MP

BUILD := build
OBJ := $(BUILD)/obj

INC := -Isrc -Iexternal/imgui -Iexternal/imgui/backends -Iexternal/imgui/misc/cpp

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null)

SQL_CFLAGS := $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQL_LIBS   := $(shell pkg-config --libs sqlite3 2>/dev/null)

GUI_CXXFLAGS := $(CXXFLAGS) $(INC) $(GLFW_CFLAGS) $(SQL_CFLAGS)
IMP_CXXFLAGS := $(CXXFLAGS) -Isrc $(SQL_CFLAGS)

GUI_LIBS := $(GLFW_LIBS) $(SQL_LIBS) -lGL -ldl -lpthread
IMP_LIBS := $(SQL_LIBS)

# -----------------------
# GUI v2 sources (ONLY)
# -----------------------
GUI_SRCS := \
  src/main_gui.cpp \
  src/ui.cpp \
  src/model.cpp \
  src/db.cpp

IMGUI_SRCS := \
  external/imgui/imgui.cpp \
  external/imgui/imgui_draw.cpp \
  external/imgui/imgui_tables.cpp \
  external/imgui/imgui_widgets.cpp \
  external/imgui/imgui_demo.cpp \
  external/imgui/backends/imgui_impl_glfw.cpp \
  external/imgui/backends/imgui_impl_opengl3.cpp \
  external/imgui/misc/cpp/imgui_stdlib.cpp

GUI_OBJS := $(patsubst %.cpp,$(OBJ)/%.o,$(GUI_SRCS) $(IMGUI_SRCS))

# -----------------------
# Importer v2 sources (ONLY)
# -----------------------
IMP_SRCS := \
  src/importer/main_importer.cpp \
  src/importer/DbImportV2.cpp \
  src/importer/GeneralLoaderV2.cpp

IMP_OBJS := $(patsubst %.cpp,$(OBJ)/%.o,$(IMP_SRCS))

# -----------------------
# Targets
# -----------------------
.PHONY: all clean gui importer run_gui run_importer

all: gui importer

gui: $(BUILD)/evony_gui_v2
importer: $(BUILD)/importer_v2

run_gui: gui
	./$(BUILD)/evony_gui_v2 --db data/evony_v2.db

run_importer: importer
	./$(BUILD)/importer_v2 --db data/evony_v2.db --path data/import

$(BUILD)/evony_gui_v2: $(GUI_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) $^ -o $@ $(GUI_LIBS)

$(BUILD)/importer_v2: $(IMP_OBJS)
	@mkdir -p $(BUILD)
	$(CXX) $^ -o $@ $(IMP_LIBS)

# -----------------------
# Compile rules (SEPARATE FLAGS!)
# -----------------------
$(OBJ)/src/importer/%.o: src/importer/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(IMP_CXXFLAGS) -c $< -o $@

$(OBJ)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(GUI_OBJS:.o=.d)
-include $(IMP_OBJS:.o=.d)
