#include "predictor_engine.hpp"
#include <algorithm>
#include <unordered_set>

namespace autocode {

PredictorEngine::PredictorEngine() {
    KnowledgeBase::initializeModels(trie_, ngram_);
}

IdleStage PredictorEngine::classifyIdleStage(int idleMs) {
    if (idleMs >= 4500) {
        return IdleStage::STUCK;
    }
    if (idleMs >= 2000) {
        return IdleStage::PONDERING;
    }
    return IdleStage::TYPING;
}

void PredictorEngine::recordAcceptedSuggestion(const std::string& trigger) {
    trie_.boostFrequency(trigger, 0.35);
}

PredictionResponse PredictorEngine::predict(const PredictionRequest& req) {
    PredictionResponse resp;
    resp.idleMs = req.idleMs;
    resp.stage = classifyIdleStage(req.idleMs);

    ScopeContext ctx = analyzer_.analyze(req.fileContent, req.cursorLine, req.cursorCol);

    // Dynamically register discovered in-scope variables in the Trie
    for (const auto& kv : ctx.variables) {
        const auto& var = kv.second;
        trie_.insert({
            var.name,
            var.name,
            "",
            3.2,
            "In-Scope Variable `" + var.name + "` (`" + var.type + "`)",
            "Use local variable `" + var.name + "` declared on line " + std::to_string(var.declarationLine + 1) + ".",
            "`" + var.name + "` is an in-scope variable of type `" + var.type + "` declared at line " +
                std::to_string(var.declarationLine + 1) + ". Type `" + var.name + ".` to see its member methods!",
            "O(1) stack/local reference",
            ""
        });
    }

    // Dynamically update N-grams from the file content
    ngram_.learnFromSource(req.fileContent);

    // Build context summary string for UI display
    std::string summary = ctx.insideMain ? "Scope: main()" : (ctx.insideClass ? "Scope: class" : "Scope: global");
    if (ctx.insideLoop) {
        summary += " [in loop]";
    }
    if (!ctx.variables.empty()) {
        summary += " | Vars: ";
        int count = 0;
        for (const auto& kv : ctx.variables) {
            if (count > 0) summary += ", ";
            summary += kv.first + ":" + kv.second.baseType;
            if (++count >= 4) break;
        }
    }
    if (!ctx.recentComment.empty()) {
        summary += " | Goal: \"" + ctx.recentComment + "\"";
    }
    resp.detectedContextSummary = summary;

    std::vector<PredictionCandidate> allCandidates;

    auto appendAll = [&](std::vector<PredictionCandidate>&& items) {
        for (auto& item : items) {
            allCandidates.push_back(std::move(item));
        }
    };

    // 1. Comment intent rules (works on empty lines below a comment or partial lines)
    appendAll(KnowledgeBase::matchCommentIntent(ctx));

    // 2. Member access rules (`nums.`, `mp.fi`, `std::so`)
    appendAll(KnowledgeBase::matchMemberAccess(ctx));

    // 3. Syntax / partial line rules (`for (`, `std::sort(`, `while (`, `if (`)
    appendAll(KnowledgeBase::matchLineSyntaxRules(ctx));

    // 4. Stuck / Next-step workflow rules
    // If the line has text OR if the user has paused (`idleMs >= 1200`) on an empty line, suggest the next logical line!
    if (!ctx.isLineEmptyOrWhitespace || req.idleMs >= 1200 || !ctx.recentComment.empty()) {
        appendAll(KnowledgeBase::matchStuckNextStep(ctx));
    }

    // 5. N-Gram sequence continuations
    if (!ctx.recentTokens.empty() && ctx.accessorOp.empty()) {
        appendAll(ngram_.predict(ctx.recentTokens, ctx.currentWordPrefix, 3));
    }

    // 6. Prefix Trie search (both full trimmed line prefix like `#inc` and current word prefix)
    if (!ctx.trimmedBeforeCursor.empty() && ctx.accessorOp.empty()) {
        appendAll(trie_.searchPrefix(ctx.trimmedBeforeCursor, 4));
    }
    if (!ctx.currentWordPrefix.empty() &&
        ctx.currentWordPrefix != ctx.trimmedBeforeCursor &&
        ctx.accessorOp.empty()) {
        appendAll(trie_.searchPrefix(ctx.currentWordPrefix, 4));
    }

    // Deduplicate by completionSuffix and sort by confidence descending
    std::sort(allCandidates.begin(), allCandidates.end(),
              [](const PredictionCandidate& a, const PredictionCandidate& b) {
                  return a.confidence > b.confidence;
              });

    std::unordered_set<std::string> seenSuffixes;
    int maxOut = std::max(1, req.maxCandidates);
    for (auto& cand : allCandidates) {
        if (cand.completionSuffix.empty() && cand.nextLinePreview.empty()) {
            continue;
        }
        std::string dedupKey = cand.completionSuffix + "|" + cand.fullSuggestion;
        if (seenSuffixes.count(dedupKey) > 0) {
            continue;
        }
        seenSuffixes.insert(dedupKey);
        resp.candidates.push_back(std::move(cand));
        if (static_cast<int>(resp.candidates.size()) >= maxOut) {
            break;
        }
    }

    return resp;
}

} // namespace autocode
