// trie.hpp - a prefix tree (Trie) for autocomplete.
//
// Each edge is one character, so all words that share a prefix share a path.
// To find every word starting with "cou" we walk c -> o -> u (3 steps, no matter
// how many words are stored) and then collect the words below that node.
// Each word has a weight; accepting a suggestion raises it (simple learning).
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Trie {
public:
    // Adds `word`, or raises its weight to `weight` if it is already there. O(L), L = word length.
    void insert(const std::string& word, double weight = 1.0);

    // Adds `amount` to the weight of an existing word. O(L).
    void boost(const std::string& word, double amount);

    bool contains(const std::string& word) const;  // O(L)

    // Up to `limit` words that start with `prefix`, highest weight first, ties alphabetical.
    // O(L + m log m): L steps to reach the prefix, then sort the m words found below it.
    std::vector<std::pair<std::string, double>> complete(const std::string& prefix, size_t limit = 5) const;

    size_t size() const { return count_; }

private:
    struct Node {
        std::map<char, std::unique_ptr<Node>> children;  // std::map keeps children in alphabetical order
        bool isWord = false;
        double weight = 0;
    };
    Node root_;
    size_t count_ = 0;

    const Node* find(const std::string& prefix) const;
    void collect(const Node* node, std::string& path, std::vector<std::pair<std::string, double>>& out) const;
};
