#ifndef AUTOCODE_TRIE_HPP
#define AUTOCODE_TRIE_HPP

#include "types.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocode {

struct TrieEntry {
    std::string trigger;
    std::string fullCompletion;
    std::string nextLinePreview;
    double weight = 1.0;
    std::string conceptTitle;
    std::string progressiveHint;
    std::string detailedExplanation;
    std::string complexityNote;
    std::string commonPitfall;
};

class Trie {
public:
    Trie();
    ~Trie() = default;

    void insert(const TrieEntry& entry);
    void boostFrequency(const std::string& trigger, double delta = 0.25);

    std::vector<PredictionCandidate> searchPrefix(
        const std::string& prefix,
        size_t maxResults = 5
    ) const;

private:
    struct Node {
        std::unordered_map<char, std::unique_ptr<Node>> children;
        std::vector<TrieEntry> entries;
    };

    std::unique_ptr<Node> root_;

    void collectEntries(const Node* node, std::vector<TrieEntry>& out) const;
};

} // namespace autocode

#endif // AUTOCODE_TRIE_HPP
