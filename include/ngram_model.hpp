#ifndef AUTOCODE_NGRAM_MODEL_HPP
#define AUTOCODE_NGRAM_MODEL_HPP

#include "types.hpp"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace autocode {

struct NGramContinuation {
    std::string continuationText;
    int count = 1;
    std::string conceptTitle;
    std::string progressiveHint;
    std::string detailedExplanation;
    std::string complexityNote;
};

class NGramModel {
public:
    NGramModel();
    ~NGramModel() = default;

    /**
     * Train a curated 2-token or 1-token context to continuation mapping with learning notes.
     */
    void addCuratedPattern(
        const std::string& prevToken1,
        const std::string& prevToken2,
        const NGramContinuation& continuation
    );

    /**
     * Dynamically learn token transitions from raw C++ source code (e.g., current file).
     */
    void learnFromSource(const std::string& sourceCode);

    /**
     * Predict continuations given recent tokens before the cursor and an optional partial word.
     */
    std::vector<PredictionCandidate> predict(
        const std::vector<std::string>& recentTokens,
        const std::string& currentPartialToken,
        size_t maxResults = 3
    ) const;

    static std::vector<std::string> tokenizeLine(const std::string& text);

private:
    // Key: "tok1|tok2" -> list of continuations
    std::unordered_map<std::string, std::vector<NGramContinuation>> trigramTable_;
    // Key: "tok2" -> list of continuations
    std::unordered_map<std::string, std::vector<NGramContinuation>> bigramTable_;
};

} // namespace autocode

#endif // AUTOCODE_NGRAM_MODEL_HPP
