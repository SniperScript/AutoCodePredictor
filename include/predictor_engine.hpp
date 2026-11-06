#ifndef AUTOCODE_PREDICTOR_ENGINE_HPP
#define AUTOCODE_PREDICTOR_ENGINE_HPP

#include "context_analyzer.hpp"
#include "knowledge_base.hpp"
#include "ngram_model.hpp"
#include "trie.hpp"
#include "types.hpp"

namespace autocode {

class PredictorEngine {
public:
    PredictorEngine();
    ~PredictorEngine() = default;

    /**
     * Generate ranked code predictions and progressive learning hints for the given request.
     */
    PredictionResponse predict(const PredictionRequest& req);

    /**
     * Notify the engine that the user accepted a suggestion (boosts Trie weights).
     */
    void recordAcceptedSuggestion(const std::string& trigger);

    static IdleStage classifyIdleStage(int idleMs);

private:
    Trie trie_;
    NGramModel ngram_;
    ContextAnalyzer analyzer_;
};

} // namespace autocode

#endif // AUTOCODE_PREDICTOR_ENGINE_HPP
