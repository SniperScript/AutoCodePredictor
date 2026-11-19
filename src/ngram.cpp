#include "ngram.hpp"

#include <algorithm>

void NGram::learn(const std::vector<std::string>& t) {
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        afterOne_[t[i]][t[i + 1]]++;
        if (i + 2 < t.size()) afterTwo_[t[i] + " " + t[i + 1]][t[i + 2]]++;
    }
}

std::vector<std::pair<std::string, double>> NGram::predict(const std::vector<std::string>& history,
                                                           size_t limit) const {
    const Counts* counts = nullptr;
    const size_t n = history.size();
    if (n >= 2) {  // trigram: the last two tokens
        auto it = afterTwo_.find(history[n - 2] + " " + history[n - 1]);
        if (it != afterTwo_.end()) counts = &it->second;
    }
    if (!counts && n >= 1) {  // back off to the bigram: the last token only
        auto it = afterOne_.find(history[n - 1]);
        if (it != afterOne_.end()) counts = &it->second;
    }
    std::vector<std::pair<std::string, double>> result;
    if (!counts) return result;

    int total = 0;
    for (const auto& [token, count] : *counts) total += count;
    for (const auto& [token, count] : *counts) result.emplace_back(token, static_cast<double>(count) / total);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    if (result.size() > limit) result.resize(limit);
    return result;
}
