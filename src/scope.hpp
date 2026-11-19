// scope.hpp - reads the code up to the cursor and works out what is visible there.
//
// Data structure: a STACK of scopes. Every `{` pushes a scope, every `}` pops one,
// and popping removes the variables declared inside that block (restoring any
// outer variable they were hiding). That is how C++ itself decides which names
// you can use at a given point.
#pragma once

#include <map>
#include <string>
#include <vector>

// ---------- small text helpers, shared by the other modules ----------

std::string trim(const std::string& s);
std::vector<std::string> splitLines(const std::string& code);

// Splits code into tokens: names, numbers, two-character operators (:: << -> ...)
// and single symbols. A string literal becomes the single token "" and a char
// literal ''. Stops at a // comment.
std::vector<std::string> tokenize(const std::string& line);

// All tokens of a file, skipping comments and the line `skipLine` (-1 = none).
std::vector<std::string> codeTokens(const std::string& code, int skipLine);

// "std::vector<int>" -> "vector",  "const std::string&" -> "string"
std::string baseType(const std::string& type);

// "std::unordered_map<int, std::string>" -> {"int", "std::string"}
std::vector<std::string> templateArgs(const std::string& type);

// ---------- scope analysis ----------

struct Variable {
    std::string name;
    std::string type;  // as written, e.g. "std::vector<int>"
    int line = 0;      // 0-based line of the declaration
};

// Everything the suggestion rules need to know about the cursor position.
struct Context {
    std::map<std::string, Variable> variables;  // only the ones visible at the cursor
    std::string lineBefore;    // the current line, up to the cursor
    std::string word;          // the name being typed: "cou" in "int x = cou"
    std::string object;        // "nums" when typing "nums.pu"
    std::string previousLine;  // the last line of code above the cursor
    std::vector<std::string> tokens;  // tokens of the current line before the cursor
    int depth = 0;             // how many { } blocks are open
    bool insideMain = false;
    bool insideLoop = false;
};

// Reads `code` up to (line, col), both 0-based, and returns the context there.
Context scan(const std::string& code, int line, int col);
