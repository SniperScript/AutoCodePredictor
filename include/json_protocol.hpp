#ifndef AUTOCODE_JSON_PROTOCOL_HPP
#define AUTOCODE_JSON_PROTOCOL_HPP

#include "types.hpp"
#include <string>

namespace autocode {

class JsonProtocol {
public:
    /**
     * Parse a single-line JSON request into a PredictionRequest.
     * Expected fields: "fileContent", "cursorLine", "cursorCol", "idleMs", "maxCandidates"
     */
    static bool parseRequest(const std::string& jsonLine, PredictionRequest& outReq, std::string& outAction, std::string& outTrigger);

    /**
     * Serialize a PredictionResponse into a compact single-line JSON string.
     */
    static std::string serializeResponse(const PredictionResponse& resp);

    /**
     * Escape a string for safe JSON emission.
     */
    static std::string escapeJson(const std::string& input);

    /**
     * Extract a string value for a given key from a flat JSON object.
     */
    static std::string extractStringField(const std::string& json, const std::string& key);

    /**
     * Extract an integer value for a given key from a flat JSON object.
     */
    static int extractIntField(const std::string& json, const std::string& key, int defaultVal = 0);
};

} // namespace autocode

#endif // AUTOCODE_JSON_PROTOCOL_HPP
