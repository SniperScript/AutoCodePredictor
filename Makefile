CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SOURCES := src/main.cpp src/engine.cpp src/rules.cpp src/scope.cpp src/trie.cpp src/ngram.cpp src/editor.cpp src/terminal.cpp tests/tests.cpp
TARGET := autocode-predictor

all: $(TARGET)

$(TARGET): $(SOURCES) $(wildcard src/*.hpp) tests/tests.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

test: $(TARGET)
	./$(TARGET) --test

demo: $(TARGET)
	./$(TARGET) --demo

clean:
	rm -f $(TARGET)

.PHONY: all test demo clean
