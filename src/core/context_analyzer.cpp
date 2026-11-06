#include "context_analyzer.hpp"
#include "ngram_model.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace autocode {

std::vector<std::string> ContextAnalyzer::splitLines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (content.empty() || (!content.empty() && content.back() == '\n')) {
        lines.push_back("");
    }
    return lines;
}

std::string ContextAnalyzer::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string ContextAnalyzer::toLower(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string ContextAnalyzer::normalizeBaseType(const std::string& rawType) {
    std::string t = rawType;
    if (t.rfind("std::", 0) == 0) {
        t = t.substr(5);
    }
    size_t ltPos = t.find('<');
    if (ltPos != std::string::npos) {
        t = t.substr(0, ltPos);
    }
    while (!t.empty() && (t.back() == '*' || t.back() == '&' || t.back() == ' ')) {
        t.pop_back();
    }
    size_t spacePos = t.find_last_of(" \t*&");
    if (spacePos != std::string::npos && spacePos + 1 < t.size()) {
        t = t.substr(spacePos + 1);
    }
    return trim(t);
}

void ContextAnalyzer::parseVariableOrFunction(
    const std::string& trimmedLine,
    int lineNum,
    const std::string& activeUserType,
    ScopeContext& ctx
) {
    if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine.rfind("//", 0) == 0) {
        return;
    }
    if (trimmedLine.rfind("return ", 0) == 0 || trimmedLine.rfind("if ", 0) == 0 ||
        trimmedLine.rfind("if(", 0) == 0 || trimmedLine.rfind("while", 0) == 0 ||
        trimmedLine.rfind("switch", 0) == 0 || trimmedLine.rfind("class ", 0) == 0 ||
        trimmedLine.rfind("struct ", 0) == 0 || trimmedLine.rfind("public:", 0) == 0 ||
        trimmedLine.rfind("private:", 0) == 0 || trimmedLine.rfind("protected:", 0) == 0) {
        return;
    }

    std::vector<std::string> knownTypePrefixes = {
        "std::vector", "vector",
        "std::map", "map",
        "std::unordered_map", "unordered_map",
        "std::set", "set",
        "std::unordered_set", "unordered_set",
        "std::stack", "stack",
        "std::queue", "queue",
        "std::priority_queue", "priority_queue",
        "std::deque", "deque",
        "std::string", "string",
        "std::unique_ptr", "unique_ptr",
        "std::shared_ptr", "shared_ptr",
        "std::ifstream", "ifstream",
        "std::ofstream", "ofstream",
        "std::stringstream", "stringstream",
        "long long", "int", "long", "double", "float", "bool", "char", "size_t", "void", "auto"
    };

    // Append any user-defined struct/class names discovered so far!
    for (const auto& kv : ctx.userTypes) {
        knownTypePrefixes.push_back(kv.first);
    }

    std::string work = trimmedLine;
    bool isForHeader = false;
    if (work.rfind("for", 0) == 0) {
        size_t openParen = work.find('(');
        if (openParen != std::string::npos) {
            work = trim(work.substr(openParen + 1));
            isForHeader = true;
        }
    }
    if (work.rfind("const ", 0) == 0) {
        work = trim(work.substr(6));
    }
    if (work.rfind("static ", 0) == 0) {
        work = trim(work.substr(7));
    }

    for (const auto& prefix : knownTypePrefixes) {
        if (work.rfind(prefix, 0) == 0) {
            size_t pos = prefix.size();
            if (pos < work.size() && work[pos] == '<') {
                int depth = 0;
                while (pos < work.size()) {
                    if (work[pos] == '<') depth++;
                    else if (work[pos] == '>') {
                        depth--;
                        if (depth == 0) {
                            pos++;
                            break;
                        }
                    }
                    pos++;
                }
            }
            if (pos >= work.size() ||
                (!std::isspace(static_cast<unsigned char>(work[pos])) &&
                 work[pos] != '*' && work[pos] != '&')) {
                continue;
            }

            std::string fullType = trim(work.substr(0, pos));
            bool isPtr = (fullType.find("unique_ptr") != std::string::npos ||
                          fullType.find("shared_ptr") != std::string::npos);

            while (pos < work.size() &&
                   (std::isspace(static_cast<unsigned char>(work[pos])) ||
                    work[pos] == '*' || work[pos] == '&')) {
                if (work[pos] == '*') isPtr = true;
                pos++;
            }

            size_t nameStart = pos;
            while (pos < work.size() &&
                   (std::isalnum(static_cast<unsigned char>(work[pos])) || work[pos] == '_')) {
                pos++;
            }
            if (pos <= nameStart) return;

            std::string identName = work.substr(nameStart, pos - nameStart);
            while (pos < work.size() && std::isspace(static_cast<unsigned char>(work[pos]))) {
                pos++;
            }

            // Check if this is a function/method declaration `retType funcName(...)`
            if (!isForHeader && pos < work.size() && work[pos] == '(') {
                size_t closeParen = work.find(')', pos);
                if (closeParen != std::string::npos) {
                    // Distinguish function declaration from constructor-style variable init (e.g. `vector<int> dp(n, 0);`)
                    bool looksLikeFunc = (work.find('{', closeParen) != std::string::npos) ||
                                         (fullType == "void") ||
                                         (!activeUserType.empty());
                    if (looksLikeFunc && identName != "main") {
                        std::string sig = identName + work.substr(pos, closeParen - pos + 1);
                        if (!activeUserType.empty()) {
                            ctx.userTypes[activeUserType].methods.push_back(sig);
                        } else {
                            ctx.userFunctions[identName] = {identName, fullType, sig, lineNum};
                        }
                        return;
                    }
                }
            }

            if (identName == "main" || fullType == "void") {
                return;
            }

            // If inside a struct/class definition, record as a member field of `activeUserType`
            if (!activeUserType.empty()) {
                ctx.userTypes[activeUserType].fields.push_back({identName, fullType});
            }

            VariableInfo info;
            info.name = identName;
            info.type = fullType;
            info.baseType = normalizeBaseType(fullType);
            info.declarationLine = lineNum;
            info.isPointer = isPtr;
            ctx.variables[identName] = info;
            return;
        }
    }
}

ScopeContext ContextAnalyzer::analyze(
    const std::string& fileContent,
    int cursorLine,
    int cursorCol
) const {
    ScopeContext ctx;
    auto lines = splitLines(fileContent);
    if (lines.empty()) {
        lines.push_back("");
    }

    ctx.cursorLine = std::clamp(cursorLine, 0, static_cast<int>(lines.size()) - 1);
    ctx.currentLine = lines[ctx.cursorLine];
    ctx.cursorCol = std::clamp(cursorCol, 0, static_cast<int>(ctx.currentLine.size()));

    ctx.lineBeforeCursor = ctx.currentLine.substr(0, ctx.cursorCol);
    ctx.lineAfterCursor = ctx.currentLine.substr(ctx.cursorCol);
    ctx.trimmedBeforeCursor = trim(ctx.lineBeforeCursor);
    ctx.isLineEmptyOrWhitespace = ctx.trimmedBeforeCursor.empty();

    int braceDepth = 0;
    std::string currentUserType;
    int userTypeStartDepth = -1;

    for (int i = 0; i <= ctx.cursorLine; ++i) {
        const std::string& line = (i == ctx.cursorLine) ? ctx.lineBeforeCursor : lines[i];
        std::string trimmed = trim(line);

        if (trimmed.rfind("#include", 0) == 0) {
            size_t lt = trimmed.find('<');
            size_t gt = trimmed.find('>');
            if (lt != std::string::npos && gt != std::string::npos && gt > lt + 1) {
                ctx.includedHeaders.push_back(trimmed.substr(lt + 1, gt - lt - 1));
            }
        }

        if (trimmed.find("int main") != std::string::npos || trimmed.find("void main") != std::string::npos) {
            ctx.insideMain = true;
        }

        // Detect `struct TypeName` or `class TypeName`
        if (trimmed.rfind("struct ", 0) == 0 || trimmed.rfind("class ", 0) == 0) {
            size_t startPos = (trimmed.rfind("struct ", 0) == 0) ? 7 : 6;
            while (startPos < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[startPos]))) {
                startPos++;
            }
            size_t endPos = startPos;
            while (endPos < trimmed.size() &&
                   (std::isalnum(static_cast<unsigned char>(trimmed[endPos])) || trimmed[endPos] == '_')) {
                endPos++;
            }
            if (endPos > startPos) {
                std::string tName = trimmed.substr(startPos, endPos - startPos);
                ctx.userTypes[tName].typeName = tName;
                currentUserType = tName;
                userTypeStartDepth = braceDepth;
                ctx.insideClass = true;
            }
        }

        if (trimmed.rfind("for ", 0) == 0 || trimmed.rfind("for(", 0) == 0 ||
            trimmed.rfind("while ", 0) == 0 || trimmed.rfind("while(", 0) == 0) {
            ctx.insideLoop = true;
        }

        parseVariableOrFunction(trimmed, i, currentUserType, ctx);

        for (char c : line) {
            if (c == '{') braceDepth++;
            else if (c == '}') {
                braceDepth = std::max(0, braceDepth - 1);
                if (!currentUserType.empty() && braceDepth <= userTypeStartDepth) {
                    currentUserType.clear();
                    userTypeStartDepth = -1;
                    ctx.insideClass = false;
                }
                if (braceDepth == 0) {
                    ctx.insideMain = false;
                    ctx.insideClass = false;
                    ctx.insideLoop = false;
                }
            }
        }

        if (i < ctx.cursorLine && !trimmed.empty()) {
            if (trimmed.rfind("//", 0) == 0) {
                if (ctx.cursorLine - i <= 2) {
                    ctx.recentComment = trim(trimmed.substr(2));
                }
            } else {
                ctx.previousNonEmptyLine = trimmed;
                size_t commentPos = trimmed.find("//");
                if (commentPos != std::string::npos && ctx.cursorLine - i == 1) {
                    ctx.recentComment = trim(trimmed.substr(commentPos + 2));
                }
            }
        }
    }

    if (ctx.trimmedBeforeCursor.rfind("//", 0) == 0) {
        ctx.recentComment = trim(ctx.trimmedBeforeCursor.substr(2));
    }

    ctx.braceDepth = braceDepth;

    const std::string& before = ctx.lineBeforeCursor;
    int idx = static_cast<int>(before.size()) - 1;
    while (idx >= 0 && (std::isalnum(static_cast<unsigned char>(before[idx])) ||
                        before[idx] == '_' || before[idx] == '#')) {
        idx--;
    }
    ctx.currentWordPrefix = before.substr(idx + 1);

    if (idx >= 0 && before[idx] == '.') {
        ctx.accessorOp = ".";
        int objEnd = idx;
        int objStart = objEnd - 1;
        while (objStart >= 0 && (std::isalnum(static_cast<unsigned char>(before[objStart])) ||
                                 before[objStart] == '_')) {
            objStart--;
        }
        ctx.objectBeforeAccessor = before.substr(objStart + 1, objEnd - (objStart + 1));
    } else if (idx >= 1 && before[idx - 1] == '-' && before[idx] == '>') {
        ctx.accessorOp = "->";
        int objEnd = idx - 1;
        int objStart = objEnd - 1;
        while (objStart >= 0 && (std::isalnum(static_cast<unsigned char>(before[objStart])) ||
                                 before[objStart] == '_')) {
            objStart--;
        }
        ctx.objectBeforeAccessor = before.substr(objStart + 1, objEnd - (objStart + 1));
    } else if (idx >= 1 && before[idx - 1] == ':' && before[idx] == ':') {
        ctx.accessorOp = "::";
        int objEnd = idx - 1;
        int objStart = objEnd - 1;
        while (objStart >= 0 && (std::isalnum(static_cast<unsigned char>(before[objStart])) ||
                                 before[objStart] == '_')) {
            objStart--;
        }
        ctx.objectBeforeAccessor = before.substr(objStart + 1, objEnd - (objStart + 1));
    }

    ctx.recentTokens = NGramModel::tokenizeLine(ctx.lineBeforeCursor);
    return ctx;
}

} // namespace autocode
