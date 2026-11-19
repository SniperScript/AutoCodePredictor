#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <set>

Stage stageFor(int idleMs) {
    if (idleMs >= kExplainAfterMs) return Stage::Stuck;
    if (idleMs >= kHintAfterMs) return Stage::Pondering;
    return Stage::Typing;
}

const char* stageName(Stage s) {
    switch (s) {
        case Stage::Typing: return "TYPING";
        case Stage::Pondering: return "PAUSED - hint";
        case Stage::Stuck: return "STUCK - full explanation";
    }
    return "";
}

Engine::Engine() {
    for (const Word& w : vocabulary()) {
        words_.insert(w.text, w.weight);
        kinds_[w.text] = w.kind;
    }
    sample_.learn(codeTokens(sampleCorpus(), -1));
}

namespace {

std::string describe(const Context& ctx) {
    std::string s = ctx.insideMain ? "in main()" : (ctx.depth == 0 ? "global scope" : "in a block");
    if (ctx.insideLoop) s += " | in a loop";
    if (!ctx.variables.empty()) {
        s += " | vars:";
        int shown = 0;
        for (const auto& [name, v] : ctx.variables) {
            if (shown++ == 5) {
                s += " ...";
                break;
            }
            s += " " + name + ":" + baseType(v.type);
        }
    }
    return s;
}

// Put a space before the predicted token unless the line already ends with one
// or the token doesn't need it ("std" + "::", "f" + "(").
std::string spacing(const std::string& lineBefore, const std::string& token) {
    if (lineBefore.empty() || lineBefore.back() == ' ' || lineBefore.back() == '(' || lineBefore.back() == '[') return "";
    if (lineBefore.size() >= 2 && (lineBefore.compare(lineBefore.size() - 2, 2, "::") == 0 ||
                                   lineBefore.compare(lineBefore.size() - 2, 2, "->") == 0 || lineBefore.back() == '.'))
        return "";
    static const std::set<std::string> kNoSpace = {"::", "(", ")", "[", "]", ";", ",", ".", "->", "++", "--"};
    return kNoSpace.count(token) ? "" : " ";
}

}  // namespace

Prediction Engine::predict(const std::string& code, int line, int col, int idleMs, size_t limit) const {
    Prediction p;
    p.stage = stageFor(idleMs);
    const Context ctx = scan(code, line, col);
    p.context = describe(ctx);

    std::vector<Suggestion> all;
    auto add = [&](std::vector<Suggestion> more) {
        for (auto& s : more) all.push_back(std::move(s));
    };
    add(memberCompletions(ctx));
    add(syntaxCompletions(ctx));
    const bool emptyLine = trim(ctx.lineBefore).empty();
    if (emptyLine && p.stage != Stage::Typing) add(nextSteps(ctx));  // only once you pause

    const bool includeLine = trim(ctx.lineBefore).rfind("#include", 0) == 0;
    const bool completingName = !ctx.word.empty() && ctx.object.empty() && !includeLine &&
                                !std::isdigit(static_cast<unsigned char>(ctx.word[0]));
    if (completingName) {
        // Variables visible here, from a small Trie built for this request only, so
        // renamed, deleted or half-typed names can never linger between requests.
        Trie visible;
        for (const auto& [name, v] : ctx.variables) visible.insert(name);
        for (const auto& [name, weight] : visible.complete(ctx.word, 5)) {
            if (name == ctx.word) continue;
            all.push_back({name.substr(ctx.word.size()), name, "variable", 0.85, variableLesson(ctx.variables.at(name))});
        }
        for (const auto& [word, weight] : words_.complete(ctx.word, 5)) {
            if (word == ctx.word) continue;
            const std::string& kind = kinds_.at(word);
            double score = 0.5 + 0.04 * std::min(weight, 8.0);  // stays below variables (0.85)
            all.push_back({word.substr(ctx.word.size()), word, kind, score, wordLesson(word, kind)});
        }
    }

    // Next word: the N-gram model, trained on sample code plus this file (rebuilt per
    // request, without the line being typed, so counts never pile up between requests).
    if (ctx.object.empty() && !includeLine && !ctx.tokens.empty()) {
        std::vector<std::string> history = ctx.tokens;
        std::string partial;
        if (!ctx.word.empty() && history.back() == ctx.word) {
            partial = ctx.word;
            history.pop_back();
        }
        if (!history.empty()) {
            NGram model = sample_;
            model.learn(codeTokens(code, line));
            for (const auto& [token, prob] : model.predict(history, 3)) {
                if (token == "\"\"" || token == "''") continue;  // can't usefully insert an empty literal
                if (token.size() <= partial.size() || token.rfind(partial, 0) != 0) continue;
                Suggestion s;
                s.insert = (partial.empty() ? spacing(ctx.lineBefore, token) : "") + token.substr(partial.size());
                s.full = token;
                s.source = "n-gram";
                s.score = 0.55 + 0.35 * prob;
                const int percent = static_cast<int>(prob * 100 + 0.5);
                const std::string after = history.back();
                s.lesson = {"Next word: `" + token + "` (" + std::to_string(percent) + "%)",
                            "After `" + after + "`, code usually continues with `" + token + "`.",
                            "The N-gram model counted which word follows `" + after +
                                "` in sample code and in your file; `" + token + "` came next " +
                                std::to_string(percent) + "% of the time.",
                            "O(1) average lookup (hash map)", "It predicts what is common, not what is correct."};
                all.push_back(s);
            }
        }
    }

    // Best first; equal scores keep the order above. Drop duplicates.
    std::stable_sort(all.begin(), all.end(), [](const Suggestion& a, const Suggestion& b) { return a.score > b.score; });
    std::set<std::string> seen;
    for (auto& s : all) {
        if (p.suggestions.size() >= limit) break;
        if (s.insert.empty() || !seen.insert(s.full).second) continue;
        p.suggestions.push_back(std::move(s));
    }
    return p;
}

void Engine::accept(const Suggestion& s) {
    if (s.source == "keyword" || s.source == "library") words_.boost(s.full, 1.0);
}
