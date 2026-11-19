// tests.cpp - checks for every feature, plus regression checks for the bugs found in
// the code review (marked "review"). Uses plain if-checks, so tests also run in
// optimised builds where assert() would be switched off.

#include "tests.hpp"

#include "../src/engine.hpp"
#include "../src/ngram.hpp"
#include "../src/scope.hpp"
#include "../src/trie.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

namespace {

int total = 0, passed = 0;

void check(const std::string& name, bool ok) {
    ++total;
    if (ok) ++passed;
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << name << "\n";
}

void section(const std::string& name) { std::cout << "\n" << name << "\n"; }

// Prediction with the cursor at the end of `line` (0-based).
Prediction at(const Engine& e, const std::string& code, int line, int idleMs = 0) {
    return e.predict(code, line, static_cast<int>(splitLines(code)[line].size()), idleMs, 8);
}

bool has(const Prediction& p, const std::string& full, const std::string& source = "") {
    return std::any_of(p.suggestions.begin(), p.suggestions.end(), [&](const Suggestion& s) {
        return s.full == full && (source.empty() || s.source == source);
    });
}

// Returns a COPY: a pointer into a temporary Prediction would dangle after the line ends.
std::optional<Suggestion> first(const Prediction& p, const std::string& source) {
    for (const auto& s : p.suggestions)
        if (s.source == source) return s;
    return std::nullopt;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void testTrie() {
    section("Trie");
    Trie t;
    for (const char* w : {"count", "counter", "country"}) t.insert(w, 1.0);
    t.insert("cout", 3.0);
    t.insert("count", 1.0);
    check("insert + contains + size (duplicates counted once)", t.contains("count") && !t.contains("cou") && t.size() == 4);
    auto r = t.complete("cou", 10);
    check("highest weight first, then alphabetical", r.size() == 4 && r[0].first == "cout" && r[1].first == "count" &&
                                                         r[2].first == "counter" && r[3].first == "country");
    check("the limit keeps the best ones", t.complete("cou", 2).size() == 2 && t.complete("cou", 2)[1].first == "count");
    t.boost("country", 5.0);
    check("boost moves a word to the top", t.complete("cou", 1)[0].first == "country");
    check("no match gives nothing", t.complete("xyz").empty());
}

void testScopes() {
    section("Scopes (review: the original kept one flat list of every variable)");
    Engine e;
    {
        const std::string code = "void helper() {\n    int tempTotal = 5;\n}\nint main() {\n    for (int idx = 0; idx < 3; ++idx) { }\n    te\n}";
        Prediction p = at(e, code, 5);
        check("another function's local is not suggested", !has(p, "tempTotal"));
        check("a finished loop's variable is gone", !contains(p.context, "idx:"));
        check("not 'in a loop' after the loop closed", !contains(p.context, "in a loop"));
    }
    {
        const std::string code = "int main() {\n    for (int idx = 0; idx < 3; ++idx) {\n        id\n    }\n}";
        Prediction p = at(e, code, 2);
        check("the loop variable is suggested inside its loop", has(p, "idx", "variable"));
        check("'in a loop' inside the loop", contains(p.context, "in a loop"));
    }
    {
        const std::string code = "int main() {\n    for (int k = 0; k < 3; ++k)\n    {\n        k\n    }\n}";
        check("a loop whose { is on the next line still scopes its variable", contains(at(e, code, 3).context, "k:int"));
        const std::string braceless = "int main() {\n    for (int k = 0; k < 3; ++k) k++;\n    \n}";
        check("a brace-less loop's variable doesn't leak", !contains(at(e, braceless, 2).context, "k:"));
    }
    {
        const std::string code = "int main() {\n    std::vector<int> data;\n    {\n        std::string data;\n        data.\n    }\n    data.\n}";
        check("shadowing: inside the block `data` is the string", contains(at(e, code, 4).context, "data:string"));
        check("shadowing: after the block `data` is the vector again", contains(at(e, code, 6).context, "data:vector"));
    }
    {
        const std::string code = "struct Student {\n    int grade;\n};\nint main() {\n    gr\n}";
        check("a struct field is not a free variable", !has(at(e, code, 4), "grade"));
        const std::string user = "struct Point { int x; int y; };\nint main() {\n    Point p;\n    \n}";
        check("a variable of a struct type is recognised", contains(at(e, user, 3).context, "p:Point"));
    }
    {
        const std::string code = "int main() {\n    int beforeString = 1;\n    std::string s = \"{ not a block\";\n"
                                 "    char c = '{';  // { in a comment\n}\nbef";
        Prediction p = at(e, code, 5);
        check("braces in strings, chars and comments are ignored", !has(p, "beforeString") && contains(p.context, "global scope"));
    }
    {
        const std::string code = "int main() {\n    std::vector<int> nums = {1, 2};\n    std::vector<int> par\n}";
        Prediction p = at(e, code, 2);
        check("a half-typed declaration is not a variable yet", !contains(p.context, "par:") && contains(p.context, "nums:vector"));
    }
}

void testTyping() {
    section("Typing (review: the original stored half-typed names forever)");
    Engine e;
    const std::string decl = "int counter = 0;";
    for (size_t k = 1; k <= decl.size(); ++k) e.predict("int main() {\n    " + decl.substr(0, k) + "\n}", 1, int(4 + k), 0, 8);
    Prediction p = at(e, "int main() {\n    int counter = 0;\n    co\n}", 2);
    bool fragment = false;
    for (const auto& s : p.suggestions)
        if (s.source == "variable" && s.full != "counter") fragment = true;
    check("no half-typed fragments (cou, coun, ...) are suggested as variables", !fragment);
    check("the real variable `counter` is suggested, first", !p.suggestions.empty() && p.suggestions[0].full == "counter");
    e.predict("int main() {\n    int alphaValue = 1;\n    \n}", 2, 4, 0, 8);
    check("a renamed variable is gone", !has(at(e, "int main() {\n    int betaValue = 1;\n    alp\n}", 2), "alphaValue"));
    check("...and its new name is suggested", has(at(e, "int main() {\n    int betaValue = 1;\n    be\n}", 2), "betaValue"));
}

void testVocabularyAndLearning() {
    section("Keywords, library names and learning");
    Engine e;
    const std::string code = "int main() {\n    pri\n}";
    Prediction before = at(e, code, 1);
    check("C/C++ words are suggested (printf, priority_queue, private)",
          has(before, "printf") && has(before, "priority_queue") && has(before, "private"));
    check("common words rank first (`cout` before `count` for `cou`)",
          [&] { auto p = at(e, "int main() {\n    cou\n}", 1); return !p.suggestions.empty() && p.suggestions[0].full == "cout"; }());
    const Suggestion* pq = nullptr;
    for (const auto& s : before.suggestions)
        if (s.full == "priority_queue") pq = &s;
    if (pq) e.accept(*pq);
    Prediction after = at(e, code, 1);
    check("accepting a word moves it up next time", pq && !after.suggestions.empty() && after.suggestions[0].full == "priority_queue");
}

void testNgram() {
    section("Next-word prediction (N-gram)");
    NGram m;
    m.learn(tokenize("alpha beta gamma ; alpha beta gamma ;"));
    m.learn(tokenize("zeta beta delta ; zeta beta delta ; zeta beta delta ;"));
    auto tri = m.predict({"alpha", "beta"});
    check("trigram: after `alpha beta` comes `gamma` (even though `beta delta` is more common)", !tri.empty() && tri[0].first == "gamma");
    auto bi = m.predict({"nothing", "beta"});
    check("backoff to bigram: after `beta` alone, `delta` (3 of 5)", !bi.empty() && bi[0].first == "delta" && bi[0].second == 0.6);

    Engine e;
    const std::string code = "int main() {\n    int total = 5;\n    std::cout << total << std::endl;\n    std::cout << total <<\n}";
    Prediction p = at(e, code, 3);
    const auto n = first(p, "n-gram");
    check("in a file: after `total <<` it predicts `std`", n && n->full == "std" && n->insert == " std");
    double c1 = n ? n->score : -1;
    for (int k = 0; k < 7; ++k) at(e, code, 3);
    const auto n8 = first(at(e, code, 3), "n-gram");
    check("review: confidence doesn't grow with repeated requests", n8 && n8->score == c1);
    Prediction same = at(e, "int main() {\n    qqone qqtwo qqthree; qqone qqtwo \n}", 1);  // space: next word
    check("the line being typed is not learned from", !has(same, "qqthree"));
    Prediction other = at(e, "int main() {\n    qqone qqtwo qqthree;\n    qqone qqtwo \n}", 2);
    check("...but earlier lines are", has(other, "qqthree", "n-gram"));
}

void testMembersAndSyntax() {
    section("Container functions and syntax");
    Engine e;
    Prediction v = at(e, "int main() {\n    std::vector<int> nums;\n    nums.pu\n}", 2);
    check("`nums.pu` -> push_back(val)", !v.suggestions.empty() && v.suggestions[0].full == "nums.push_back(val)" &&
                                            v.suggestions[0].insert == "sh_back(val)");
    Prediction m = at(e, "int main() {\n    std::map<int, int> m;\n    m.fi\n}", 2);
    Prediction u = at(e, "int main() {\n    std::unordered_map<int, int> m;\n    m.fi\n}", 2);
    check("map.find costs O(log n), unordered_map.find O(1) average",
          !m.suggestions.empty() && m.suggestions[0].lesson.cost == "O(log n)" && !u.suggestions.empty() &&
              contains(u.suggestions[0].lesson.cost, "O(1) average"));
    check("`s.su` on a string -> substr", has(at(e, "int main() {\n    std::string s;\n    s.su\n}", 2), "s.substr(pos, len)"));
    check("an unknown object gives no member suggestions", !first(at(e, "int main() {\n    mystery.pu\n}", 1), "member"));
    Prediction inc = at(e, "#include <io", 0);
    check("`#include <io` -> stream>", !inc.suggestions.empty() && inc.suggestions[0].insert == "stream>");
    Prediction loop = at(e, "int main() {\n    std::vector<int> items;\n    for (\n}", 2);
    check("`for (` uses the vector in scope (range loop and index loop)",
          has(loop, "for (const auto& x : items) {") && has(loop, "for (size_t i = 0; i < items.size(); ++i) {"));
    check("`while (` with a stack in scope -> !st.empty()",
          has(at(e, "int main() {\n    std::stack<int> st;\n    while (\n}", 2), "while (!st.empty()) {"));
}

void testStagesAndNextSteps() {
    section("Learning stages and next steps");
    check("stage thresholds: 0 s typing, 2 s hint, 4.5 s full explanation",
          stageFor(1999) == Stage::Typing && stageFor(2000) == Stage::Pondering && stageFor(4500) == Stage::Stuck);
    Engine e;
    const std::string code = "int main() {\n    std::vector<int> data = {8, 4};\n    \n}";
    check("no next-step suggestion while typing", !first(at(e, code, 2, 0), "next step"));
    const auto s = first(at(e, code, 2, 2500), "next step");
    check("after a pause: a next step that uses `data`", s && contains(s->full, "data"));
    check("every lesson part is filled in for it", s && !s->lesson.hint.empty() && !s->lesson.explanation.empty() && !s->lesson.cost.empty());
    const auto m = first(at(e, "int main() {\n    \n}", 1, 2500), "next step");
    check("right after `int main() {` -> declare some data", m && contains(m->full, "std::vector<int>"));
    const auto body = first(at(e, "int main() {\n    std::vector<int> v;\n    for (const auto& item : v) {\n        \n    }\n}", 3, 2500), "next step");
    check("inside a range loop -> use the current element", body && body->full == "std::cout << item << \" \";");
}

}  // namespace

int runTests() {
    total = passed = 0;
    std::cout << "Running tests...\n";
    testTrie();
    testScopes();
    testTyping();
    testVocabularyAndLearning();
    testNgram();
    testMembersAndSyntax();
    testStagesAndNextSteps();
    std::cout << "\nResult: " << passed << "/" << total << " checks passed.\n";
    return total - passed;
}
