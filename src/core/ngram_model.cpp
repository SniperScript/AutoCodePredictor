#include "ngram_model.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace autocode {

NGramModel::NGramModel() = default;

void NGramModel::addCuratedPattern(
    const std::string& prevToken1,
    const std::string& prevToken2,
    const NGramContinuation& continuation
) {
    if (!prevToken1.empty() && !prevToken2.empty()) {
        std::string key = prevToken1 + "|" + prevToken2;
        trigramTable_[key].push_back(continuation);
    }
    if (!prevToken2.empty()) {
        bigramTable_[prevToken2].push_back(continuation);
    }
}

std::vector<std::string> NGramModel::tokenizeLine(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        // Check 2-char operators: ::, ->, <<, >>, <=, >=, ==, !=, &&, ||, ++, --
        if (i + 1 < text.size()) {
            std::string two = text.substr(i, 2);
            if (two == "::" || two == "->" || two == "<<" || two == ">>" ||
                two == "<=" || two == ">=" || two == "==" || two == "!=" ||
                two == "&&" || two == "||" || two == "++" || two == "--") {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                tokens.push_back(two);
                ++i;
                continue;
            }
        }
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '#') {
            current.push_back(c);
        } else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.push_back(std::string(1, c));
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void NGramModel::learnFromSource(const std::string& sourceCode) {
    std::istringstream stream(sourceCode);
    std::string line;
    while (std::getline(stream, line)) {
        // Ignore pure comments
        size_t firstNonSpace = line.find_first_not_of(" \t\r\n");
        if (firstNonSpace == std::string::npos) continue;
        if (line.compare(firstNonSpace, 2, "//") == 0) continue;

        auto tokens = tokenizeLine(line);
        if (tokens.size() < 3) continue;

        for (size_t i = 0; i + 2 < tokens.size(); ++i) {
            std::string t1 = tokens[i];
            std::string t2 = tokens[i + 1];
            // Reconstruct the remainder of the statement from i+2 to end of line
            std::string remainder;
            for (size_t j = i + 2; j < tokens.size(); ++j) {
                if (!remainder.empty() &&
                    tokens[j] != ";" && tokens[j] != ")" && tokens[j] != "," &&
                    tokens[j] != "." && tokens[j] != "::" && tokens[j] != "->" &&
                    tokens[j - 1] != "(" && tokens[j - 1] != "." &&
                    tokens[j - 1] != "::" && tokens[j - 1] != "->") {
                    remainder += " ";
                }
                remainder += tokens[j];
            }
            if (remainder.size() < 2 || remainder.size() > 80) continue;

            std::string triKey = t1 + "|" + t2;
            auto& vec = trigramTable_[triKey];
            bool found = false;
            for (auto& item : vec) {
                if (item.continuationText == remainder) {
                    item.count += 1;
                    found = true;
                    break;
                }
            }
            if (!found && vec.size() < 12) {
                NGramContinuation c;
                c.continuationText = remainder;
                c.count = 1;
                c.conceptTitle = "Learned File Pattern (N-Gram)";
                c.progressiveHint = "Follows pattern `" + t1 + " " + t2 + " ...` seen earlier in your code.";
                c.detailedExplanation = "The N-Gram sequence model noticed you previously wrote `" +
                    t1 + " " + t2 + " " + remainder + "` in this file and predicts a similar continuation.";
                c.complexityNote = "Context-adaptive local Markov transition";
                vec.push_back(std::move(c));
            }
        }
    }
}

std::vector<PredictionCandidate> NGramModel::predict(
    const std::vector<std::string>& recentTokens,
    const std::string& currentPartialToken,
    size_t maxResults
) const {
    std::vector<PredictionCandidate> results;
    if (recentTokens.empty()) {
        return results;
    }

    std::vector<NGramContinuation> pool;

    // If currentPartialToken is non-empty, the last token in recentTokens might be the partial token itself
    size_t tokenCount = recentTokens.size();
    bool lastIsPartial = (!currentPartialToken.empty() &&
                          !recentTokens.empty() &&
                          recentTokens.back() == currentPartialToken);

    size_t effectiveEnd = lastIsPartial ? tokenCount - 1 : tokenCount;
    if (effectiveEnd == 0) {
        return results;
    }

    if (effectiveEnd >= 2) {
        std::string t1 = recentTokens[effectiveEnd - 2];
        std::string t2 = recentTokens[effectiveEnd - 1];
        auto it = trigramTable_.find(t1 + "|" + t2);
        if (it != trigramTable_.end()) {
            for (const auto& item : it->second) {
                pool.push_back(item);
            }
        }
    }

    if (pool.size() < maxResults && effectiveEnd >= 1) {
        std::string t2 = recentTokens[effectiveEnd - 1];
        auto it = bigramTable_.find(t2);
        if (it != bigramTable_.end()) {
            for (const auto& item : it->second) {
                pool.push_back(item);
            }
        }
    }

    std::sort(pool.begin(), pool.end(), [](const NGramContinuation& a, const NGramContinuation& b) {
        return a.count > b.count;
    });

    for (const auto& item : pool) {
        if (results.size() >= maxResults) break;

        std::string suffix = item.continuationText;
        if (!currentPartialToken.empty()) {
            if (suffix.rfind(currentPartialToken, 0) == 0) {
                suffix = suffix.substr(currentPartialToken.size());
            } else {
                continue;
            }
        } else {
            // Add a leading space if the previous token wasn't an opening delimiter or scope op
            std::string prevTok = recentTokens[effectiveEnd - 1];
            if (prevTok != "(" && prevTok != "::" && prevTok != "." && prevTok != "->" &&
                prevTok != "[" && !suffix.empty() && suffix[0] != '(' && suffix[0] != ';') {
                // Only prepend space if the caller's line doesn't already end with space;
                // PredictorEngine normalizes leading whitespace based on lineBeforeCursor.
            }
        }

        if (suffix.empty()) continue;

        PredictionCandidate cand;
        cand.triggerPrefix = currentPartialToken;
        cand.completionSuffix = suffix;
        cand.fullSuggestion = item.continuationText;
        cand.confidence = std::min(0.88, 0.60 + 0.05 * item.count);
        cand.sourceEngine = "NGram";
        cand.conceptTitle = item.conceptTitle.empty() ? "C++ Sequence Continuation" : item.conceptTitle;
        cand.progressiveHint = item.progressiveHint;
        cand.detailedExplanation = item.detailedExplanation;
        cand.complexityNote = item.complexityNote;
        results.push_back(std::move(cand));
    }

    return results;
}

} // namespace autocode
