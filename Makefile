CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic -Iinclude

SRC := src/main.cpp src/Stat.cpp src/Stats.cpp
OBJ := $(SRC:.cpp=.o)
BIN := general_analyzer


all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

run: all
	./$(BIN)

clean:
	rm -rf build

.PHONY: all run clean
