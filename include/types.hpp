#ifndef AUTOCODE_TYPES_HPP
#define AUTOCODE_TYPES_HPP

#include <string>
#include <vector>
#include <map>

namespace autocode {

/**
 * Represents the user's current idle/stuck state on a line of code.
 */
enum class IdleStage {
    TYPING = 0,       // < 2000 ms: Instant subtle inline ghost-text completion
    PONDERING = 1,    // 2000 - 4499 ms: Progressive conceptual hint + ghost text (even on empty lines)
    STUCK = 2         // >= 4500 ms: Full learning breakdown ("Why this works", complexity, step-by-step)
};

/**
 * A single ranked code prediction and its educational metadata.
 */
struct PredictionCandidate {
    std::string triggerPrefix;       // The prefix or pattern matched
    std::string completionSuffix;    // The exact ghost text to insert at the cursor
    std::string fullSuggestion;      // Full completed line or snippet
    std::string nextLinePreview;     // Suggested follow-up line (if current line is complete or multi-step)
    double confidence = 0.0;         // 0.0 to 1.0 confidence score
    std::string sourceEngine;        // e.g., "ContextRule", "TypeMember", "IntentComment", "StuckCoach", "DiagnosticFix", "NGram", "Trie"

    // Educational / Learning metadata
    std::string conceptTitle;        // e.g., "STL Vector Dynamic Insertion"
    std::string progressiveHint;     // Level 1 nudge shown when pondering (~2.0s idle)
    std::string detailedExplanation; // Level 2 deep explanation shown when stuck (~4.5s+ idle)
    std::string complexityNote;      // e.g., "Amortized O(1) time, O(1) space"
    std::string commonPitfall;       // e.g., "Watch out for iterator invalidation if reallocation occurs."
};

/**
 * Information about an in-scope variable discovered by the ContextAnalyzer.
 */
struct VariableInfo {
    std::string name;
    std::string type;          // e.g., "std::vector<int>", "std::string", "Student"
    std::string baseType;      // Normalized container/type: "vector", "string", "map", "Student", etc.
    int declarationLine = 0;
    bool isPointer = false;
};

/**
 * Information about a user-defined struct or class discovered in the file.
 */
struct UserTypeInfo {
    std::string typeName;
    std::vector<std::pair<std::string, std::string>> fields; // (fieldName, fieldType)
    std::vector<std::string> methods;                        // method signatures/names
};

/**
 * Information about a user-defined function discovered in the file.
 */
struct FunctionInfo {
    std::string name;
    std::string returnType;
    std::string signature;     // e.g., "calculateSum(const std::vector<int>& nums)"
    int declarationLine = 0;
};

/**
 * Extracted syntactic and semantic context around the cursor.
 */
struct ScopeContext {
    std::vector<std::string> includedHeaders;
    std::map<std::string, VariableInfo> variables;       // varName -> VariableInfo
    std::map<std::string, UserTypeInfo> userTypes;       // typeName -> UserTypeInfo
    std::map<std::string, FunctionInfo> userFunctions;   // funcName -> FunctionInfo
    std::string currentLine;
    std::string lineBeforeCursor;
    std::string lineAfterCursor;
    std::string trimmedBeforeCursor;
    std::string currentWordPrefix;
    std::string objectBeforeAccessor; // e.g., "v" in "v.push" or "ptr" in "ptr->"
    std::string accessorOp;           // "." or "->" or "::"
    std::string previousNonEmptyLine;
    std::string recentComment;        // Most recent "// ..." comment above or on the current line
    std::vector<std::string> recentTokens;
    int cursorLine = 0;               // 0-indexed
    int cursorCol = 0;                // 0-indexed
    int braceDepth = 0;
    bool insideMain = false;
    bool insideClass = false;
    bool insideLoop = false;
    bool isLineEmptyOrWhitespace = false;
};

/**
 * Input request to the PredictorEngine.
 */
struct PredictionRequest {
    std::string fileContent;
    int cursorLine = 0;   // 0-indexed line number
    int cursorCol = 0;    // 0-indexed column number
    int idleMs = 0;       // Milliseconds user has been settled/idle on this line
    int maxCandidates = 5;
};

/**
 * Output response from the PredictorEngine.
 */
struct PredictionResponse {
    IdleStage stage = IdleStage::TYPING;
    int idleMs = 0;
    std::string detectedContextSummary;
    std::vector<PredictionCandidate> candidates;
};

} // namespace autocode

#endif // AUTOCODE_TYPES_HPP
