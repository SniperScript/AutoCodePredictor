#ifndef AUTOCODE_KNOWLEDGE_BASE_HPP
#define AUTOCODE_KNOWLEDGE_BASE_HPP

#include "ngram_model.hpp"
#include "trie.hpp"
#include "types.hpp"
#include <string>
#include <vector>

namespace autocode {

class KnowledgeBase {
public:
    KnowledgeBase() = default;
    ~KnowledgeBase() = default;

    /**
     * Populate the Trie and NGramModel with comprehensive C++ patterns and learning notes.
     */
    static void initializeModels(Trie& trie, NGramModel& ngram);

    /**
     * Match member access on typed variables (e.g., `nums.push` or `mp.` or `std::`).
     */
    static std::vector<PredictionCandidate> matchMemberAccess(const ScopeContext& ctx);

    /**
     * Match incomplete line syntax patterns (e.g., `for (`, `std::sort(`, `#include`, `while (`).
     */
    static std::vector<PredictionCandidate> matchLineSyntaxRules(const ScopeContext& ctx);

    /**
     * Match comment intent (e.g., `// sort vector`, `// binary search`, `// count frequency`)
     * using real variable names from the current ScopeContext.
     */
    static std::vector<PredictionCandidate> matchCommentIntent(const ScopeContext& ctx);

    /**
     * Predict next logical step when the user is stuck/settled on an empty or minimal line,
     * analyzing the preceding code lines and scope variables.
     */
    static std::vector<PredictionCandidate> matchStuckNextStep(const ScopeContext& ctx);

private:
    static std::string findBestVarOfType(const ScopeContext& ctx, const std::string& baseType, const std::string& fallback);
};

} // namespace autocode

#endif // AUTOCODE_KNOWLEDGE_BASE_HPP
