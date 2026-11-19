// rules.hpp - what the predictor knows about C++, and the rules that turn it into suggestions.
//
// Most of the knowledge is plain DATA (tables of keywords, container functions, headers
// and lessons in rules.cpp), so adding to it doesn't require writing new logic.
#pragma once

#include "scope.hpp"

#include <string>
#include <vector>

// What the learning coach says about a suggestion. The hint appears after a short
// pause; the explanation, cost and pitfall after a longer one.
struct Lesson {
    std::string title;        // "Add to the end"
    std::string hint;         // a nudge
    std::string explanation;  // why it works
    std::string cost;         // time/space complexity
    std::string pitfall;      // a common mistake
};

struct Suggestion {
    std::string insert;   // text to add at the cursor
    std::string full;     // the whole word or line, for display
    std::string source;   // "member", "syntax", "next step", "variable", "keyword", "library", "n-gram"
    double score = 0;     // 0..1, higher is shown first
    Lesson lesson;
};

// Built-in vocabulary: C++ keywords and C/C++ standard library names (checked with a
// compiler). Common words have a higher weight.
struct Word {
    std::string text;
    std::string kind;  // "keyword" or "library"
    double weight;
};
std::vector<Word> vocabulary();
Lesson wordLesson(const std::string& word, const std::string& kind);
Lesson variableLesson(const Variable& v);

// The rules. Each returns nothing when it doesn't apply.
std::vector<Suggestion> memberCompletions(const Context& ctx);  // "nums.pu"             -> push_back(val)
std::vector<Suggestion> syntaxCompletions(const Context& ctx);  // "for (", "while (", "#include <"
std::vector<Suggestion> nextSteps(const Context& ctx);          // what usually comes next (empty line)

// Sample C++ code the next-word model learns from before it has seen your file.
const std::string& sampleCorpus();
