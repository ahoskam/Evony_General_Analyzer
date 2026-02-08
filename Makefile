CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -g
SRC := $(wildcard src/*.cpp)
BIN := build/app

all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@

run: all
	./$(BIN)

clean:
	rm -rf build

.PHONY: all run clean
