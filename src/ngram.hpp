// ngram.hpp - next-word prediction with an N-gram (Markov) model.
//
// It counts, in code it has seen, which token follows each token (bigram) and each
// pair of tokens (trigram). To predict, it looks up the last two tokens you typed;
// if that pair was never seen, it "backs off" to just the last token.
// Data structure: hash maps from context -> (next token -> count).
#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class NGram {
public:
    // Counts every bigram and trigram in `tokens`. O(n).
    void learn(const std::vector<std::string>& tokens);

    // The most likely next tokens after `history`, with their probability (0..1),
    // best first. O(1) average lookup + O(k log k) to sort the k candidates.
    std::vector<std::pair<std::string, double>> predict(const std::vector<std::string>& history,
                                                        size_t limit = 3) const;

private:
    using Counts = std::unordered_map<std::string, int>;    // next token -> how often
    std::unordered_map<std::string, Counts> afterOne_;      // "<<"      -> {"std": 5, "x": 2}
    std::unordered_map<std::string, Counts> afterTwo_;      // "cout <<" -> {"x": 3, ...}
};
