cmake_minimum_required(VERSION 3.16)
project(AutoCodePredictor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/core/trie.cpp
    src/core/ngram_model.cpp
    src/core/context_analyzer.cpp
    src/core/knowledge_base.cpp
    src/core/predictor_engine.cpp
    src/core/json_protocol.cpp
    src/terminal/terminal_tui.cpp
)

add_executable(autocode-predictor ${SOURCES})
target_compile_options(autocode-predictor PRIVATE -Wall -Wextra -O2)
