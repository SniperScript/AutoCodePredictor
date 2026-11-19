#include "scope.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>
#include <set>

// ---------- text helpers ----------

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitLines(const std::string& code) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : code) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur.push_back(c);
        }
    }
    lines.push_back(cur);
    return lines;
}

namespace {

bool isNameStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isNameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// `line` with comments removed (and string/char contents blanked if `blankStrings`).
// `inBlock` carries a /* ... */ comment from one line to the next.
std::string withoutComments(const std::string& line, bool& inBlock, bool blankStrings) {
    std::string out(line.size(), ' ');
    for (size_t i = 0; i < line.size(); ++i) {
        if (inBlock) {
            if (line.compare(i, 2, "*/") == 0) {
                inBlock = false;
                ++i;
            }
            continue;
        }
        if (line.compare(i, 2, "//") == 0) break;
        if (line.compare(i, 2, "/*") == 0) {
            inBlock = true;
            ++i;
            continue;
        }
        const char c = line[i];
        // a ' right after a digit or letter is a digit separator (1'000), not a char literal
        if (c == '"' || (c == '\'' && !(i > 0 && isNameChar(line[i - 1])))) {
            size_t start = i++;
            while (i < line.size() && line[i] != c) {
                if (line[i] == '\\') ++i;
                ++i;
            }
            if (!blankStrings) {
                for (size_t k = start; k <= i && k < line.size(); ++k) out[k] = line[k];
            }
            continue;
        }
        out[i] = c;
    }
    return out;
}

}  // namespace

std::vector<std::string> tokenize(const std::string& line) {
    static const char* const kTwoChar[] = {"::", "->", "<<", ">>", "<=", ">=", "==", "!=",
                                           "&&", "||", "++", "--", "+=", "-=", "*=", "/="};
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        } else if (line.compare(i, 2, "//") == 0) {
            break;
        } else if (c == '"' || c == '\'') {  // a whole literal is one token: "" or ''
            ++i;
            while (i < line.size() && line[i] != c) {
                if (line[i] == '\\') ++i;
                ++i;
            }
            ++i;
            tokens.push_back(std::string(2, c));
        } else if (isNameStart(c) || std::isdigit(static_cast<unsigned char>(c))) {
            const bool number = std::isdigit(static_cast<unsigned char>(c));
            size_t start = i;
            while (i < line.size() && (isNameChar(line[i]) || (number && line[i] == '.'))) ++i;  // 3.14
            tokens.push_back(line.substr(start, i - start));
        } else {
            std::string two = line.substr(i, 2);
            bool isTwo = std::any_of(std::begin(kTwoChar), std::end(kTwoChar), [&](const char* op) { return two == op; });
            tokens.push_back(isTwo ? two : std::string(1, c));
            i += isTwo ? 2 : 1;
        }
    }
    return tokens;
}

std::vector<std::string> codeTokens(const std::string& code, int skipLine) {
    std::vector<std::string> all;
    bool inBlock = false;
    std::vector<std::string> lines = splitLines(code);
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        std::string clean = withoutComments(lines[i], inBlock, /*blankStrings=*/false);
        if (i == skipLine) continue;  // the line being typed isn't finished yet
        for (auto& t : tokenize(clean)) all.push_back(std::move(t));
    }
    return all;
}

std::string baseType(const std::string& type) {
    std::string t = trim(type);
    if (t.rfind("const ", 0) == 0) t = trim(t.substr(6));
    if (t.rfind("std::", 0) == 0) t = t.substr(5);
    size_t lt = t.find('<');
    if (lt != std::string::npos) t = t.substr(0, lt);
    while (!t.empty() && (t.back() == '&' || t.back() == '*' || t.back() == ' ')) t.pop_back();
    return t;
}

std::vector<std::string> templateArgs(const std::string& type) {
    std::vector<std::string> args;
    size_t open = type.find('<'), close = type.rfind('>');
    if (open == std::string::npos || close == std::string::npos || close <= open) return args;
    int depth = 0;
    std::string cur;
    for (size_t i = open + 1; i < close; ++i) {
        if (type[i] == '<') ++depth;
        if (type[i] == '>') --depth;
        if (type[i] == ',' && depth == 0) {
            args.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(type[i]);
        }
    }
    args.push_back(trim(cur));
    return args;
}

// ---------- declarations ----------

namespace {

const std::set<std::string> kTypeNames = {
    "int", "long", "short", "unsigned", "signed", "double", "float", "bool", "char", "auto",
    "size_t", "int64_t", "int32_t", "uint64_t", "uint32_t", "string", "vector", "map",
    "unordered_map", "set", "unordered_set", "multiset", "multimap", "stack", "queue",
    "priority_queue", "deque", "list", "pair", "array", "ifstream", "ofstream", "stringstream",
    "unique_ptr", "shared_ptr"};

std::string readName(const std::string& s, size_t& i) {
    size_t start = i;
    if (i < s.size() && isNameStart(s[i])) {
        while (i < s.size() && isNameChar(s[i])) ++i;
    }
    return s.substr(start, i - start);
}

void skipSpaces(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

// Does the text after `(` look like a parameter list ("int a, ...") or "()"?
bool looksLikeParameters(const std::string& s, size_t open, const std::set<std::string>& userTypes) {
    size_t i = open + 1;
    skipSpaces(s, i);
    if (i < s.size() && s[i] == ')') return true;
    if (s.compare(i, 6, "const ") == 0) return true;
    if (s.compare(i, 5, "std::") == 0) i += 5;
    std::string first = readName(s, i);
    return kTypeNames.count(first) > 0 || userTypes.count(first) > 0;
}

// Reads a declaration such as `std::vector<int> nums = {...}` or `const auto& x : v`
// at the start of `s`. Returns nothing for statements and function definitions.
std::optional<Variable> parseDeclaration(const std::string& s, const std::set<std::string>& userTypes) {
    size_t i = 0;
    skipSpaces(s, i);
    for (;;) {  // qualifiers
        size_t save = i;
        std::string w = readName(s, i);
        if (w == "const" || w == "static" || w == "constexpr") {
            skipSpaces(s, i);
            continue;
        }
        i = save;
        break;
    }
    const size_t typeStart = i;
    if (s.compare(i, 5, "std::") == 0) i += 5;
    std::string word = readName(s, i);
    if (!kTypeNames.count(word) && !userTypes.count(word)) return std::nullopt;
    while (word == "long" || word == "unsigned" || word == "signed" || word == "short") {  // "long long", "unsigned int"
        size_t save = i;
        skipSpaces(s, i);
        std::string next = readName(s, i);
        if (next == "long" || next == "int" || next == "double" || next == "char" || next == "short") {
            word = next;
            continue;
        }
        i = save;
        break;
    }
    if (i < s.size() && s[i] == '<') {  // template arguments: vector<pair<int, int>>
        int depth = 0;
        for (; i < s.size(); ++i) {
            if (s[i] == '<') ++depth;
            if (s[i] == '>' && --depth == 0) {
                ++i;
                break;
            }
        }
        if (depth != 0) return std::nullopt;
    }
    Variable v;
    v.type = trim(s.substr(typeStart, i - typeStart));
    skipSpaces(s, i);
    while (i < s.size() && (s[i] == '*' || s[i] == '&')) {
        ++i;
        skipSpaces(s, i);
    }
    v.name = readName(s, i);
    if (v.name.empty() || v.name == "main") return std::nullopt;
    skipSpaces(s, i);
    if (i < s.size() && s[i] == '(') {
        // `int sum(int a) {...}` is a function; `vector<int> v(n, 0);` is a variable
        if (s.find('{', i) != std::string::npos || looksLikeParameters(s, i, userTypes)) return std::nullopt;
    }
    if (i < s.size() && !std::strchr("=;({[,:)", s[i])) return std::nullopt;
    return v;
}

std::string firstWord(const std::string& s) {
    size_t i = 0;
    return readName(s, i);
}

bool isMainHeader(const std::string& s) {
    std::string w = firstWord(s);
    if (w != "int" && w != "void" && w != "auto") return false;
    size_t p = s.find("main");
    if (p == std::string::npos) return false;
    p += 4;
    while (p < s.size() && s[p] == ' ') ++p;
    return p < s.size() && s[p] == '(';
}

}  // namespace

// ---------- the scanner ----------

Context scan(const std::string& code, int line, int col) {
    Context ctx;
    std::vector<std::string> lines = splitLines(code);
    line = std::clamp(line, 0, static_cast<int>(lines.size()) - 1);
    col = std::clamp(col, 0, static_cast<int>(lines[line].size()));
    ctx.lineBefore = lines[line].substr(0, col);

    // One entry per open { } block. `declared` remembers (name, the variable it hid),
    // so closing the block can undo its declarations.
    struct Scope {
        bool isMain = false;
        bool isLoop = false;
        std::vector<std::pair<std::string, std::optional<Variable>>> declared;
    };
    std::vector<Scope> scopes(1);  // scopes[0] = the whole file
    std::set<std::string> userTypes;

    // `for (int i ...)` and `int main()` may have their `{` on the next line, and a loop's
    // own variables belong INSIDE its block - so they wait here until the `{` appears.
    std::vector<Variable> waiting;
    bool headerWaiting = false, waitingIsMain = false, waitingIsLoop = false;
    bool inBlockComment = false;

    auto declare = [&](const Variable& v) {
        auto old = ctx.variables.find(v.name);
        scopes.back().declared.emplace_back(
            v.name, old == ctx.variables.end() ? std::nullopt : std::optional<Variable>(old->second));
        ctx.variables[v.name] = v;
    };
    auto closeScope = [&]() {
        if (scopes.size() == 1) return;  // a stray `}` never closes the file scope
        auto& declared = scopes.back().declared;
        for (auto it = declared.rbegin(); it != declared.rend(); ++it) {
            if (it->second) ctx.variables[it->first] = *it->second;  // un-hide the outer one
            else ctx.variables.erase(it->first);
        }
        scopes.pop_back();
    };
    auto stopWaiting = [&]() {
        waiting.clear();
        headerWaiting = waitingIsMain = waitingIsLoop = false;
    };

    for (int i = 0; i <= line; ++i) {
        const bool atCursor = (i == line);
        const std::string text = trim(withoutComments(atCursor ? ctx.lineBefore : lines[i], inBlockComment, true));
        if (text.empty()) continue;
        if (headerWaiting && text[0] != '{') stopWaiting();  // header without braces: `for (...) x++;`

        const std::string w = firstWord(text);
        if (w == "struct" || w == "class") {  // `struct Student` makes Student a type name
            size_t p = w.size();
            skipSpaces(text, p);
            std::string name = readName(text, p);
            if (!name.empty()) userTypes.insert(name);
        }
        const bool loopHeader = (w == "for" || w == "while" || w == "do");
        const bool mainHeader = isMainHeader(text);

        // Declarations. The line being typed is skipped: `int cou` is not a variable yet.
        std::optional<Variable> found;
        if (!atCursor) {
            size_t paren = text.find('(');
            std::string declText = (w == "for" && paren != std::string::npos) ? text.substr(paren + 1) : text;
            found = parseDeclaration(declText, userTypes);
            if (found) found->line = i;
        }
        if (found && loopHeader) waiting.push_back(*found);
        else if (found) declare(*found);

        bool headerUsed = false;
        for (char c : text) {
            if (c == '{') {
                Scope s;
                if (!headerUsed && (loopHeader || mainHeader)) {
                    s.isLoop = loopHeader;
                    s.isMain = mainHeader;
                    headerUsed = true;
                } else if (headerWaiting) {
                    s.isLoop = waitingIsLoop;
                    s.isMain = waitingIsMain;
                }
                scopes.push_back(s);
                for (const auto& v : waiting) declare(v);  // loop variables live inside the loop
                stopWaiting();
            } else if (c == '}') {
                closeScope();
            }
        }
        if ((loopHeader || mainHeader) && !headerUsed) {
            if (text.back() == ')') {  // the `{` should come on the next line
                headerWaiting = true;
                waitingIsLoop = loopHeader;
                waitingIsMain = mainHeader;
            } else {
                stopWaiting();
            }
        }
    }

    for (const auto& s : scopes) {
        ctx.insideMain = ctx.insideMain || s.isMain;
        ctx.insideLoop = ctx.insideLoop || s.isLoop;
    }
    ctx.depth = static_cast<int>(scopes.size()) - 1;

    // The last line of code above the cursor (blank and comment-only lines skipped).
    for (int i = line - 1; i >= 0; --i) {
        std::string t = trim(lines[i]);
        if (t.empty() || t.rfind("//", 0) == 0) continue;
        size_t c = t.find("//");
        ctx.previousLine = trim(c == std::string::npos ? t : t.substr(0, c));
        break;
    }

    // The name being typed, and the object before a `.` or `->`.
    size_t k = ctx.lineBefore.size();
    while (k > 0 && isNameChar(ctx.lineBefore[k - 1])) --k;
    ctx.word = ctx.lineBefore.substr(k);
    size_t dot = std::string::npos;
    if (k >= 1 && ctx.lineBefore[k - 1] == '.') dot = k - 1;
    else if (k >= 2 && ctx.lineBefore.compare(k - 2, 2, "->") == 0) dot = k - 2;
    if (dot != std::string::npos) {
        size_t j = dot;
        while (j > 0 && isNameChar(ctx.lineBefore[j - 1])) --j;
        ctx.object = ctx.lineBefore.substr(j, dot - j);
    }
    ctx.tokens = tokenize(ctx.lineBefore);
    return ctx;
}
