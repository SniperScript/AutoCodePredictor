// engine.hpp - combines everything into one ranked list of suggestions.
//
//   scope analysis  -> which variables are visible, what is being typed
//   rules           -> container functions, syntax, next steps
//   Trie            -> words that start with what you typed (variables, keywords, library)
//   N-gram          -> the most likely next word
//
// It also decides the learning stage from how long you have paused on the line.
#pragma once

#include "ngram.hpp"
#include "rules.hpp"
#include "trie.hpp"

#include <map>
#include <string>
#include <vector>

enum class Stage {
    Typing,     // < 2 s:   just the suggestion
    Pondering,  // 2-4.5 s: + a hint
    Stuck       // >= 4.5 s: + why it works, cost and a common pitfall
};
constexpr int kHintAfterMs = 2000;
constexpr int kExplainAfterMs = 4500;
Stage stageFor(int idleMs);
const char* stageName(Stage s);

struct Prediction {
    Stage stage = Stage::Typing;
    std::string context;                  // e.g. "in main() | vars: nums:vector"
    std::vector<Suggestion> suggestions;  // best first
};

class Engine {
public:
    Engine();

    // `line` and `col` are 0-based; `idleMs` is how long the user has paused on this line.
    Prediction predict(const std::string& code, int line, int col, int idleMs, size_t limit = 5) const;

    // Learning: an accepted keyword or library name ranks higher from now on.
    void accept(const Suggestion& s);

private:
    Trie words_;                                // keywords and library names, with weights
    std::map<std::string, std::string> kinds_;  // word -> "keyword" / "library"
    NGram sample_;                              // next-word model trained on sample code
};
