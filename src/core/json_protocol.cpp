#include "json_protocol.hpp"
#include <cctype>
#include <iomanip>
#include <sstream>

namespace autocode {

std::string JsonProtocol::escapeJson(const std::string& input) {
    std::ostringstream ss;
    for (unsigned char c : input) {
        switch (c) {
            case '\"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (c < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << static_cast<char>(c);
                }
                break;
        }
    }
    return ss.str();
}

std::string JsonProtocol::extractStringField(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return "";
    }
    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return "";
    }
    size_t quoteStart = json.find('\"', colonPos + 1);
    if (quoteStart == std::string::npos) {
        return "";
    }

    std::string result;
    for (size_t i = quoteStart + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            char next = json[++i];
            switch (next) {
                case '\"': result.push_back('\"'); break;
                case '\\': result.push_back('\\'); break;
                case '/':  result.push_back('/'); break;
                case 'b':  result.push_back('\b'); break;
                case 'f':  result.push_back('\f'); break;
                case 'n':  result.push_back('\n'); break;
                case 'r':  result.push_back('\r'); break;
                case 't':  result.push_back('\t'); break;
                default:   result.push_back(next); break;
            }
        } else if (c == '\"') {
            break;
        } else {
            result.push_back(c);
        }
    }
    return result;
}

int JsonProtocol::extractIntField(const std::string& json, const std::string& key, int defaultVal) {
    std::string pattern = "\"" + key + "\"";
    size_t keyPos = json.find(pattern);
    if (keyPos == std::string::npos) {
        return defaultVal;
    }
    size_t colonPos = json.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos) {
        return defaultVal;
    }
    size_t pos = colonPos + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        pos++;
    }
    if (pos >= json.size()) {
        return defaultVal;
    }
    bool negative = false;
    if (json[pos] == '-') {
        negative = true;
        pos++;
    }
    if (pos >= json.size() || !std::isdigit(static_cast<unsigned char>(json[pos]))) {
        return defaultVal;
    }
    int val = 0;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return negative ? -val : val;
}

bool JsonProtocol::parseRequest(
    const std::string& jsonLine,
    PredictionRequest& outReq,
    std::string& outAction,
    std::string& outTrigger
) {
    if (jsonLine.find('{') == std::string::npos) {
        return false;
    }
    outAction = extractStringField(jsonLine, "action");
    if (outAction.empty()) {
        outAction = "predict";
    }
    outTrigger = extractStringField(jsonLine, "trigger");

    outReq.fileContent = extractStringField(jsonLine, "fileContent");
    outReq.cursorLine = extractIntField(jsonLine, "cursorLine", 0);
    outReq.cursorCol = extractIntField(jsonLine, "cursorCol", 0);
    outReq.idleMs = extractIntField(jsonLine, "idleMs", 0);
    outReq.maxCandidates = extractIntField(jsonLine, "maxCandidates", 5);
    return true;
}

std::string JsonProtocol::serializeResponse(const PredictionResponse& resp) {
    std::ostringstream ss;
    std::string stageName = "TYPING";
    if (resp.stage == IdleStage::PONDERING) stageName = "PONDERING";
    else if (resp.stage == IdleStage::STUCK) stageName = "STUCK";

    ss << "{";
    ss << "\"stage\":\"" << stageName << "\",";
    ss << "\"stageCode\":" << static_cast<int>(resp.stage) << ",";
    ss << "\"idleMs\":" << resp.idleMs << ",";
    ss << "\"detectedContextSummary\":\"" << escapeJson(resp.detectedContextSummary) << "\",";
    ss << "\"candidates\":[";

    for (size_t i = 0; i < resp.candidates.size(); ++i) {
        const auto& c = resp.candidates[i];
        if (i > 0) ss << ",";
        ss << "{";
        ss << "\"triggerPrefix\":\"" << escapeJson(c.triggerPrefix) << "\",";
        ss << "\"completionSuffix\":\"" << escapeJson(c.completionSuffix) << "\",";
        ss << "\"fullSuggestion\":\"" << escapeJson(c.fullSuggestion) << "\",";
        ss << "\"nextLinePreview\":\"" << escapeJson(c.nextLinePreview) << "\",";
        ss << "\"confidence\":" << std::fixed << std::setprecision(2) << c.confidence << ",";
        ss << "\"sourceEngine\":\"" << escapeJson(c.sourceEngine) << "\",";
        ss << "\"conceptTitle\":\"" << escapeJson(c.conceptTitle) << "\",";
        ss << "\"progressiveHint\":\"" << escapeJson(c.progressiveHint) << "\",";
        ss << "\"detailedExplanation\":\"" << escapeJson(c.detailedExplanation) << "\",";
        ss << "\"complexityNote\":\"" << escapeJson(c.complexityNote) << "\",";
        ss << "\"commonPitfall\":\"" << escapeJson(c.commonPitfall) << "\"";
        ss << "}";
    }

    ss << "]}";
    return ss.str();
}

} // namespace autocode
