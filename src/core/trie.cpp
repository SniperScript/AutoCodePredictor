#include "trie.hpp"
#include <algorithm>
#include <cmath>

namespace autocode {

Trie::Trie() : root_(std::make_unique<Node>()) {}

void Trie::insert(const TrieEntry& entry) {
    if (entry.trigger.empty()) {
        return;
    }
    Node* current = root_.get();
    for (char ch : entry.trigger) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            auto nextNode = std::make_unique<Node>();
            Node* rawPtr = nextNode.get();
            current->children[ch] = std::move(nextNode);
            current = rawPtr;
        } else {
            current = it->second.get();
        }
    }

    // Check if an identical fullCompletion already exists at this trigger node
    for (auto& existing : current->entries) {
        if (existing.fullCompletion == entry.fullCompletion) {
            existing.weight = std::max(existing.weight, entry.weight);
            if (!entry.conceptTitle.empty()) {
                existing.conceptTitle = entry.conceptTitle;
                existing.progressiveHint = entry.progressiveHint;
                existing.detailedExplanation = entry.detailedExplanation;
                existing.complexityNote = entry.complexityNote;
                existing.commonPitfall = entry.commonPitfall;
            }
            return;
        }
    }
    current->entries.push_back(entry);
}

void Trie::boostFrequency(const std::string& trigger, double delta) {
    if (trigger.empty()) return;
    Node* current = root_.get();
    for (char ch : trigger) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            return;
        }
        current = it->second.get();
    }
    for (auto& entry : current->entries) {
        entry.weight += delta;
    }
}

void Trie::collectEntries(const Node* node, std::vector<TrieEntry>& out) const {
    if (!node) return;
    for (const auto& entry : node->entries) {
        out.push_back(entry);
    }
    for (const auto& kv : node->children) {
        collectEntries(kv.second.get(), out);
    }
}

std::vector<PredictionCandidate> Trie::searchPrefix(
    const std::string& prefix,
    size_t maxResults
) const {
    std::vector<PredictionCandidate> results;
    if (prefix.empty()) {
        return results;
    }

    const Node* current = root_.get();
    for (char ch : prefix) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            return results;
        }
        current = it->second.get();
    }

    std::vector<TrieEntry> matched;
    collectEntries(current, matched);

    // Sort by weight descending, then prefer shorter trigger lengths (more direct completions)
    std::sort(matched.begin(), matched.end(), [](const TrieEntry& a, const TrieEntry& b) {
        if (std::abs(a.weight - b.weight) > 1e-6) {
            return a.weight > b.weight;
        }
        return a.trigger.size() < b.trigger.size();
    });

    for (const auto& entry : matched) {
        if (results.size() >= maxResults) break;

        // Compute the suffix to insert after `prefix`
        std::string suffix;
        if (entry.fullCompletion.rfind(prefix, 0) == 0) {
            suffix = entry.fullCompletion.substr(prefix.size());
        } else if (entry.trigger.rfind(prefix, 0) == 0) {
            suffix = entry.trigger.substr(prefix.size());
            if (entry.fullCompletion.rfind(entry.trigger, 0) == 0) {
                suffix += entry.fullCompletion.substr(entry.trigger.size());
            }
        } else {
            continue;
        }

        if (suffix.empty()) {
            continue;
        }

        PredictionCandidate cand;
        cand.triggerPrefix = prefix;
        cand.completionSuffix = suffix;
        cand.fullSuggestion = entry.fullCompletion;
        cand.nextLinePreview = entry.nextLinePreview;
        cand.confidence = std::min(0.92, 0.55 + 0.08 * entry.weight);
        cand.sourceEngine = "Trie";
        cand.conceptTitle = entry.conceptTitle.empty()
            ? "C++ Identifier / Keyword Completion"
            : entry.conceptTitle;
        cand.progressiveHint = entry.progressiveHint.empty()
            ? "Complete `" + prefix + "` into `" + entry.fullCompletion + "`."
            : entry.progressiveHint;
        cand.detailedExplanation = entry.detailedExplanation.empty()
            ? "Standard C++ construct matched from prefix `" + prefix + "`."
            : entry.detailedExplanation;
        cand.complexityNote = entry.complexityNote;
        cand.commonPitfall = entry.commonPitfall;
        results.push_back(std::move(cand));
    }

    return results;
}

} // namespace autocode
