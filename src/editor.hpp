// editor.hpp - a small terminal editor that shows the predictor at work.
// Type C++; a suggestion appears in grey at the cursor. Pause and the coach adds a
// hint, then a full explanation. Works on Windows, Linux and macOS.
#pragma once

#include "engine.hpp"

#include <string>

// Opens `path` (or a starter program if it doesn't exist). Returns the exit code.
int runEditor(Engine& engine, const std::string& path);
