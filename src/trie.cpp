#include "trie.hpp"

#include <algorithm>

void Trie::insert(const std::string& word, double weight) {
    if (word.empty()) return;
    Node* node = &root_;
    for (char c : word) {
        auto& child = node->children[c];
        if (!child) child = std::make_unique<Node>();
        node = child.get();
    }
    if (!node->isWord) {
        node->isWord = true;
        ++count_;
    }
    node->weight = std::max(node->weight, weight);
}

const Trie::Node* Trie::find(const std::string& prefix) const {
    const Node* node = &root_;
    for (char c : prefix) {
        auto it = node->children.find(c);
        if (it == node->children.end()) return nullptr;
        node = it->second.get();
    }
    return node;
}

void Trie::boost(const std::string& word, double amount) {
    Node* node = &root_;
    for (char c : word) {
        auto it = node->children.find(c);
        if (it == node->children.end()) return;  // not in the Trie
        node = it->second.get();
    }
    if (node->isWord) node->weight += amount;
}

bool Trie::contains(const std::string& word) const {
    const Node* node = find(word);
    return node && node->isWord;
}

// Depth-first walk: children are visited in alphabetical order.
void Trie::collect(const Node* node, std::string& path, std::vector<std::pair<std::string, double>>& out) const {
    if (node->isWord) out.emplace_back(path, node->weight);
    for (const auto& [c, child] : node->children) {
        path.push_back(c);
        collect(child.get(), path, out);
        path.pop_back();
    }
}

std::vector<std::pair<std::string, double>> Trie::complete(const std::string& prefix, size_t limit) const {
    std::vector<std::pair<std::string, double>> found;
    const Node* start = find(prefix);
    if (!start) return found;
    std::string path = prefix;
    collect(start, path, found);  // alphabetical
    // Highest weight first; stable_sort keeps alphabetical order among equal weights.
    std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (found.size() > limit) found.resize(limit);
    return found;
}
