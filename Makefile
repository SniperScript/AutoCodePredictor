CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2 -Iinclude

SRCS := src/main.cpp \
        src/core/trie.cpp \
        src/core/ngram_model.cpp \
        src/core/context_analyzer.cpp \
        src/core/knowledge_base.cpp \
        src/core/predictor_engine.cpp \
        src/core/json_protocol.cpp \
        src/terminal/terminal_tui.cpp

TARGET := autocode-predictor

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

test: $(TARGET)
	./$(TARGET) --test

demo: $(TARGET)
	./$(TARGET) --demo

clean:
	rm -f $(TARGET)

.PHONY: all test demo clean
