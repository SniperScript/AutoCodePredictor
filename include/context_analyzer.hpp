#ifndef AUTOCODE_CONTEXT_ANALYZER_HPP
#define AUTOCODE_CONTEXT_ANALYZER_HPP

#include "types.hpp"
#include <string>
#include <vector>

namespace autocode {

class ContextAnalyzer {
public:
    ContextAnalyzer() = default;
    ~ContextAnalyzer() = default;

    /**
     * Analyze the document up to (cursorLine, cursorCol) and return rich ScopeContext.
     */
    ScopeContext analyze(
        const std::string& fileContent,
        int cursorLine,
        int cursorCol
    ) const;

    static std::vector<std::string> splitLines(const std::string& content);
    static std::string trim(const std::string& str);
    static std::string toLower(const std::string& str);

private:
    static std::string normalizeBaseType(const std::string& rawType);
    static void parseVariableOrFunction(
        const std::string& trimmedLine,
        int lineNum,
        const std::string& activeUserType,
        ScopeContext& ctx
    );
};

} // namespace autocode

#endif // AUTOCODE_CONTEXT_ANALYZER_HPP
