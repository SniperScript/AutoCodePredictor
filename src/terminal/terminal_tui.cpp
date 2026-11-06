#include "terminal_tui.hpp"
#include "context_analyzer.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace autocode {

namespace {

struct RawTermGuard {
    struct termios origTerm{};
    bool active = false;

    RawTermGuard() {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &origTerm) == 0) {
            struct termios raw = origTerm;
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_oflag &= ~(OPOST);
            raw.c_cflag |= (CS8);
            raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            // Enter alternate screen buffer
            std::cout << "\033[?1049h\033[2J\033[H" << std::flush;
            active = true;
        }
    }

    ~RawTermGuard() {
        if (active) {
            std::cout << "\033[?25h\033[0m\033[?1049l" << std::flush;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTerm);
        }
    }
};

void getTerminalSize(int& rows, int& cols) {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    } else {
        rows = 30;
        cols = 100;
    }
}

} // namespace

TerminalTui::TerminalTui(PredictorEngine& engine) : engine_(engine) {}
TerminalTui::~TerminalTui() = default;

long long TerminalTui::currentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void TerminalTui::loadFileOrDefault(const std::string& path) {
    filePath_ = path.empty() ? "playground.cpp" : path;
    lines_.clear();

    std::ifstream in(filePath_);
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines_.push_back(line);
        }
    }

    if (lines_.empty()) {
        // Provide an interactive starter playground so the user can test immediately!
        lines_ = {
            "#include <iostream>",
            "#include <vector>",
            "#include <algorithm>",
            "#include <unordered_map>",
            "",
            "int main() {",
            "    std::vector<int> nums = {42, 7, 19, 3, 88, 12};",
            "    // Try typing 'nums.' or 'std::sort(' or pause on the line below for 3s!",
            "    ",
            "    return 0;",
            "}"
        };
        cursorRow_ = 8;
        cursorCol_ = 4;
    } else {
        cursorRow_ = 0;
        cursorCol_ = 0;
    }
    statusMessage_ = "Ready — Type C++ code, press [Tab] to accept ghost text, or pause 2-4s on any line to learn!";
}

bool TerminalTui::saveFile() {
    std::ofstream out(filePath_);
    if (!out.is_open()) {
        statusMessage_ = "Error: Could not save to " + filePath_;
        return false;
    }
    for (size_t i = 0; i < lines_.size(); ++i) {
        out << lines_[i];
        if (i + 1 < lines_.size()) {
            out << "\n";
        }
    }
    dirty_ = false;
    statusMessage_ = "Saved " + filePath_ + " (" + std::to_string(lines_.size()) + " lines)";
    return true;
}

std::string TerminalTui::joinBuffer() const {
    std::ostringstream ss;
    for (size_t i = 0; i < lines_.size(); ++i) {
        ss << lines_[i];
        if (i + 1 < lines_.size()) {
            ss << "\n";
        }
    }
    return ss.str();
}

void TerminalTui::updatePredictions(int idleMs) {
    PredictionRequest req;
    req.fileContent = joinBuffer();
    req.cursorLine = cursorRow_;
    req.cursorCol = cursorCol_;
    req.idleMs = idleMs;
    req.maxCandidates = 5;

    currentPrediction_ = engine_.predict(req);
    if (selectedCandidateIdx_ >= static_cast<int>(currentPrediction_.candidates.size())) {
        selectedCandidateIdx_ = 0;
    }
}

void TerminalTui::acceptCurrentSuggestion() {
    if (currentPrediction_.candidates.empty()) {
        // Insert 4 spaces if no suggestion is active
        lines_[cursorRow_].insert(cursorCol_, "    ");
        cursorCol_ += 4;
        dirty_ = true;
        return;
    }

    const auto& cand = currentPrediction_.candidates[selectedCandidateIdx_];
    std::string toInsert = cand.completionSuffix;

    if (toInsert.empty() && !cand.nextLinePreview.empty()) {
        // User is on a comment or finished line; insert the nextLinePreview on a new line below!
        std::string indent;
        for (char c : lines_[cursorRow_]) {
            if (c == ' ' || c == '\t') indent.push_back(c);
            else break;
        }
        lines_.insert(lines_.begin() + cursorRow_ + 1, indent + cand.nextLinePreview);
        cursorRow_++;
        cursorCol_ = static_cast<int>(lines_[cursorRow_].size());
        dirty_ = true;
        engine_.recordAcceptedSuggestion(cand.triggerPrefix);
        statusMessage_ = "Inserted next-step suggestion for: " + cand.conceptTitle;
        return;
    }

    if (toInsert.empty()) {
        return;
    }

    // Handle single-line vs multi-line completionSuffix
    std::string indent;
    for (char c : lines_[cursorRow_]) {
        if (c == ' ' || c == '\t') indent.push_back(c);
        else break;
    }

    auto subLines = ContextAnalyzer::splitLines(toInsert);
    if (subLines.size() <= 1) {
        lines_[cursorRow_].insert(cursorCol_, toInsert);
        cursorCol_ += static_cast<int>(toInsert.size());
    } else {
        std::string tailAfterCursor = lines_[cursorRow_].substr(cursorCol_);
        lines_[cursorRow_] = lines_[cursorRow_].substr(0, cursorCol_) + subLines[0];
        for (size_t i = 1; i < subLines.size(); ++i) {
            std::string newLine = indent + subLines[i];
            if (i + 1 == subLines.size()) {
                cursorCol_ = static_cast<int>(newLine.size());
                newLine += tailAfterCursor;
            }
            lines_.insert(lines_.begin() + cursorRow_ + static_cast<int>(i), newLine);
        }
        cursorRow_ += static_cast<int>(subLines.size()) - 1;
    }

    dirty_ = true;
    engine_.recordAcceptedSuggestion(cand.triggerPrefix);
    statusMessage_ = "Accepted [" + cand.conceptTitle + "]!";
}

std::vector<std::string> TerminalTui::wrapText(const std::string& text, int maxWidth) {
    std::vector<std::string> wrapped;
    if (maxWidth <= 10) {
        wrapped.push_back(text);
        return wrapped;
    }
    std::istringstream words(text);
    std::string word;
    std::string current;
    while (words >> word) {
        if (current.empty()) {
            current = word;
        } else if (static_cast<int>(current.size() + 1 + word.size()) <= maxWidth) {
            current += " " + word;
        } else {
            wrapped.push_back(current);
            current = word;
        }
    }
    if (!current.empty()) {
        wrapped.push_back(current);
    }
    if (wrapped.empty()) {
        wrapped.push_back("");
    }
    return wrapped;
}

std::string TerminalTui::highlightCppLine(
    const std::string& line,
    const std::string& ghostSuffix,
    int cursorCol,
    bool isCurrentLine
) {
    std::string trimmed = ContextAnalyzer::trim(line);
    if (trimmed.rfind("//", 0) == 0) {
        std::string out = "\033[32m" + line + "\033[0m";
        if (isCurrentLine && !ghostSuffix.empty()) {
            out += " \033[90m// Next: " + ghostSuffix + "\033[0m";
        }
        return out;
    }
    if (!trimmed.empty() && trimmed[0] == '#') {
        std::string out = "\033[35m" + line.substr(0, cursorCol) + "\033[0m";
        if (isCurrentLine && !ghostSuffix.empty()) {
            out += "\033[90m" + ghostSuffix + "\033[0m";
        }
        if (cursorCol < static_cast<int>(line.size())) {
            out += "\033[35m" + line.substr(cursorCol) + "\033[0m";
        }
        return out;
    }

    auto colorSegment = [](const std::string& seg) -> std::string {
        static const std::vector<std::string> keywords = {
            "int", "long", "double", "float", "bool", "char", "void", "auto", "const",
            "for", "while", "if", "else", "switch", "case", "return", "class", "struct",
            "public", "private", "protected", "virtual", "override", "true", "false", "nullptr"
        };
        static const std::vector<std::string> stlWords = {
            "std", "vector", "map", "unordered_map", "set", "unordered_set", "string",
            "cout", "cin", "endl", "sort", "binary_search", "lower_bound", "reverse",
            "accumulate", "max_element", "min_element", "unique_ptr", "shared_ptr", "make_unique",
            "queue", "stack", "priority_queue", "ifstream", "ofstream", "getline"
        };

        std::string res;
        size_t i = 0;
        while (i < seg.size()) {
            // Check inline comment
            if (i + 1 < seg.size() && seg[i] == '/' && seg[i + 1] == '/') {
                res += "\033[32m" + seg.substr(i) + "\033[0m";
                break;
            }
            // Check string literal
            if (seg[i] == '\"') {
                size_t endQ = i + 1;
                while (endQ < seg.size() && seg[endQ] != '\"') {
                    if (seg[endQ] == '\\') endQ++;
                    endQ++;
                }
                if (endQ < seg.size()) endQ++;
                res += "\033[33m" + seg.substr(i, endQ - i) + "\033[0m";
                i = endQ;
                continue;
            }
            // Check identifier
            if (std::isalpha(static_cast<unsigned char>(seg[i])) || seg[i] == '_') {
                size_t endId = i + 1;
                while (endId < seg.size() &&
                       (std::isalnum(static_cast<unsigned char>(seg[endId])) || seg[endId] == '_')) {
                    endId++;
                }
                std::string word = seg.substr(i, endId - i);
                if (std::find(keywords.begin(), keywords.end(), word) != keywords.end()) {
                    res += "\033[1;34m" + word + "\033[0m";
                } else if (std::find(stlWords.begin(), stlWords.end(), word) != stlWords.end()) {
                    res += "\033[36m" + word + "\033[0m";
                } else {
                    res += word;
                }
                i = endId;
                continue;
            }
            // Check number
            if (std::isdigit(static_cast<unsigned char>(seg[i]))) {
                size_t endNum = i + 1;
                while (endNum < seg.size() && std::isalnum(static_cast<unsigned char>(seg[endNum]))) {
                    endNum++;
                }
                res += "\033[35m" + seg.substr(i, endNum - i) + "\033[0m";
                i = endNum;
                continue;
            }
            res.push_back(seg[i]);
            i++;
        }
        return res;
    };

    if (!isCurrentLine) {
        return colorSegment(line);
    }

    int safeCol = std::clamp(cursorCol, 0, static_cast<int>(line.size()));
    std::string leftPart = line.substr(0, safeCol);
    std::string rightPart = line.substr(safeCol);

    // Only show first line of multi-line ghostSuffix inline
    std::string inlineGhost = ghostSuffix;
    size_t nlPos = inlineGhost.find('\n');
    if (nlPos != std::string::npos) {
        inlineGhost = inlineGhost.substr(0, nlPos) + " ...";
    }

    std::string out = colorSegment(leftPart);
    if (!inlineGhost.empty()) {
        out += "\033[90m" + inlineGhost + "\033[0m";
    }
    out += colorSegment(rightPart);
    return out;
}

void TerminalTui::renderScreen(int termRows, int termCols, int idleMs) {
    std::ostringstream frame;
    // Hide cursor during redraw and jump home
    frame << "\033[?25l\033[H";

    // Determine bottom panel height based on IdleStage
    IdleStage stage = currentPrediction_.stage;
    int coachPanelHeight = 9;
    if (stage == IdleStage::STUCK) {
        coachPanelHeight = std::min(13, std::max(9, termRows / 3 + 3));
    } else if (stage == IdleStage::PONDERING) {
        coachPanelHeight = 10;
    }

    int editorRows = std::max(5, termRows - coachPanelHeight - 3);

    // Adjust vertical scroll so cursorRow_ is visible
    if (cursorRow_ < scrollOffsetRow_) {
        scrollOffsetRow_ = cursorRow_;
    } else if (cursorRow_ >= scrollOffsetRow_ + editorRows) {
        scrollOffsetRow_ = cursorRow_ - editorRows + 1;
    }

    // 1. Top Title & Idle Status Bar
    std::string stageBadge;
    std::string badgeColor;
    if (stage == IdleStage::TYPING) {
        stageBadge = " ⚡ ACTIVE TYPING (Inline Ghost Mode) ";
        badgeColor = "\033[42;30;1m";
    } else if (stage == IdleStage::PONDERING) {
        stageBadge = " 💡 PAUSED " + std::to_string(idleMs / 1000) + "." + std::to_string((idleMs % 1000) / 100) + "s (Progressive Hint Unlocked) ";
        badgeColor = "\033[43;30;1m";
    } else {
        stageBadge = " 🎓 STUCK ON LINE " + std::to_string(idleMs / 1000) + "s+ (Full Learning Guide Active) ";
        badgeColor = "\033[46;30;1m";
    }

    frame << "\033[44;37;1m AutoCodePredictor C++ \033[0m"
          << badgeColor << stageBadge << "\033[0m"
          << "\033[100;37m File: " << filePath_ << (dirty_ ? " [*]" : "")
          << " | Ln " << (cursorRow_ + 1) << ", Col " << (cursorCol_ + 1)
          << "\033[K\033[0m\r\n";

    // 2. Context Bar
    frame << "\033[90m " << currentPrediction_.detectedContextSummary << "\033[K\033[0m\r\n";

    // Determine active ghost suffix
    std::string activeGhost;
    if (!currentPrediction_.candidates.empty()) {
        const auto& cand = currentPrediction_.candidates[selectedCandidateIdx_];
        activeGhost = !cand.completionSuffix.empty() ? cand.completionSuffix : cand.nextLinePreview;
    }

    // 3. Render Editor Lines
    int gutterWidth = 6;
    for (int r = 0; r < editorRows; ++r) {
        int lineIdx = scrollOffsetRow_ + r;
        if (lineIdx < static_cast<int>(lines_.size())) {
            bool isCurr = (lineIdx == cursorRow_);
            char numBuf[16];
            std::snprintf(numBuf, sizeof(numBuf), "%4d ", lineIdx + 1);
            if (isCurr) {
                frame << "\033[1;36m>" << numBuf << "\033[0m";
            } else {
                frame << "\033[90m " << numBuf << "\033[0m";
            }
            frame << highlightCppLine(lines_[lineIdx], isCurr ? activeGhost : "", cursorCol_, isCurr);
        } else {
            frame << "\033[90m   ~  \033[0m";
        }
        frame << "\033[K\r\n";
    }

    // 4. Divider Header for the Learning Coach Panel
    std::string sep(std::max(10, termCols), '-');
    frame << "\033[1;34m" << sep.substr(0, termCols) << "\033[0m\r\n";

    int linesUsedInCoach = 1;
    if (currentPrediction_.candidates.empty()) {
        frame << " \033[90mNo active suggestion on this token. Try typing a C++ keyword, `nums.`, `std::`, or `// sort`.\033[K\033[0m\r\n";
        linesUsedInCoach++;
    } else {
        const auto& cand = currentPrediction_.candidates[selectedCandidateIdx_];
        int totalCands = static_cast<int>(currentPrediction_.candidates.size());

        // Header row of Learning Coach
        frame << " \033[1;33m[" << (selectedCandidateIdx_ + 1) << "/" << totalCands << "] "
              << cand.conceptTitle << "\033[0m"
              << " \033[90m(" << cand.sourceEngine << " | "
              << static_cast<int>(cand.confidence * 100) << "% match)\033[K\033[0m\r\n";
        linesUsedInCoach++;

        // Code Suggestion row
        std::string singleLineFull = cand.fullSuggestion;
        std::replace(singleLineFull.begin(), singleLineFull.end(), '\n', ' ');
        frame << " \033[1;32mCode:\033[0m \033[36m" << singleLineFull << "\033[0m";
        if (!cand.nextLinePreview.empty() && cand.nextLinePreview != cand.fullSuggestion) {
            frame << "  \033[90m-> Next: " << cand.nextLinePreview << "\033[0m";
        }
        frame << "\033[K\r\n";
        linesUsedInCoach++;

        // Progressive Hint (Always shown in PONDERING & STUCK, and compactly in TYPING)
        if (stage == IdleStage::TYPING) {
            frame << " \033[33mHint:\033[0m " << cand.progressiveHint
                  << " \033[90m(Pause 2s on this line for deeper learning explanation)\033[K\033[0m\r\n";
            linesUsedInCoach++;
        } else if (stage == IdleStage::PONDERING) {
            frame << " \033[1;33m💡 Progressive Hint:\033[0m " << cand.progressiveHint << "\033[K\r\n";
            linesUsedInCoach++;
            if (!cand.complexityNote.empty()) {
                frame << " \033[1;35m⏱ Complexity:\033[0m " << cand.complexityNote
                      << "  \033[90m(Stay settled 2s more or press Ctrl+H for full breakdown)\033[K\033[0m\r\n";
                linesUsedInCoach++;
            }
        } else {
            // STUCK stage: show full educational explanation + complexity + pitfall!
            frame << " \033[1;33m💡 Hint:\033[0m " << cand.progressiveHint << "\033[K\r\n";
            linesUsedInCoach++;

            auto wrappedExp = wrapText("📖 Why It Works: " + cand.detailedExplanation, termCols - 4);
            for (const auto& wline : wrappedExp) {
                if (linesUsedInCoach >= coachPanelHeight - 2) break;
                frame << " \033[37m" << wline << "\033[K\033[0m\r\n";
                linesUsedInCoach++;
            }
            if (!cand.complexityNote.empty() && linesUsedInCoach < coachPanelHeight - 1) {
                frame << " \033[1;35m⏱ Complexity:\033[0m " << cand.complexityNote;
                if (!cand.commonPitfall.empty()) {
                    frame << "   \033[1;31m⚠️ Pitfall:\033[0m " << cand.commonPitfall;
                }
                frame << "\033[K\r\n";
                linesUsedInCoach++;
            }
        }
    }

    // Pad remaining coach lines
    while (linesUsedInCoach < coachPanelHeight) {
        frame << "\033[K\r\n";
        linesUsedInCoach++;
    }

    // 5. Bottom Keybindings & Status Bar
    frame << "\033[48;5;236;37m [Tab] Accept Suggestion  [Ctrl+N] Cycle ("
          << (currentPrediction_.candidates.empty() ? 0 : selectedCandidateIdx_ + 1) << "/"
          << currentPrediction_.candidates.size()
          << ")  [Ctrl+H] Explain Now  [Ctrl+S] Save  [Ctrl+Q] Quit | "
          << statusMessage_ << "\033[K\033[0m";

    // Reposition hardware cursor to the active editor line/col
    int screenCursorRow = 3 + (cursorRow_ - scrollOffsetRow_);
    int screenCursorCol = gutterWidth + 1 + cursorCol_;
    frame << "\033[" << screenCursorRow << ";" << screenCursorCol << "H\033[?25h";

    std::cout << frame.str() << std::flush;
}

int TerminalTui::runInteractiveEditor(const std::string& filePath) {
    loadFileOrDefault(filePath);

    if (!isatty(STDIN_FILENO)) {
        std::cerr << "Error: Interactive TUI requires a TTY terminal. Use --predict or --server for non-interactive mode.\n";
        return 1;
    }

    RawTermGuard guard;
    running_ = true;
    lastKeystrokeTimeMs_ = currentTimeMs();
    int forceFullExplainBonusMs = 0;

    updatePredictions(0);

    while (running_) {
        int termRows = 30, termCols = 100;
        getTerminalSize(termRows, termCols);

        long long now = currentTimeMs();
        int idleMs = static_cast<int>(now - lastKeystrokeTimeMs_) + forceFullExplainBonusMs;

        // Re-run prediction when crossing idle thresholds (1200ms, 2000ms, 4500ms)
        int stageBucket = (idleMs >= 4500) ? 3 : (idleMs >= 2000 ? 2 : (idleMs >= 1200 ? 1 : 0));
        if (stageBucket != lastRenderedIdleStage_) {
            updatePredictions(idleMs);
            lastRenderedIdleStage_ = stageBucket;
        }

        renderScreen(termRows, termCols, idleMs);

        struct pollfd pfd{};
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, 100); // 100ms tick for live idle timer

        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buf[32];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) continue;

            bool resetIdle = true;
            unsigned char c = static_cast<unsigned char>(buf[0]);

            // Ctrl+Q (17) -> Quit
            if (c == 17) {
                running_ = false;
                break;
            }
            // Ctrl+S (19) -> Save
            if (c == 19) {
                saveFile();
                resetIdle = false;
            }
            // Ctrl+N (14) -> Cycle next candidate
            else if (c == 14) {
                if (!currentPrediction_.candidates.empty()) {
                    selectedCandidateIdx_ = (selectedCandidateIdx_ + 1) %
                        static_cast<int>(currentPrediction_.candidates.size());
                    statusMessage_ = "Switched to suggestion " + std::to_string(selectedCandidateIdx_ + 1);
                }
                resetIdle = false;
            }
            // Ctrl+H (8) -> Note: 8 can be Ctrl+H or Backspace depending on terminal; let's check 127 vs 8
            // Let's also bind Ctrl+E (5) or F1 to "Explain Now"!
            else if (c == 5) {
                forceFullExplainBonusMs = 5000;
                statusMessage_ = "Learning Mode Unlocked!";
                resetIdle = false;
            }
            // Tab (9) -> Accept suggestion
            else if (c == '\t') {
                acceptCurrentSuggestion();
            }
            // Enter (13 or 10) -> Split line with auto-indentation
            else if (c == '\r' || c == '\n') {
                std::string current = lines_[cursorRow_];
                std::string indent;
                for (char ch : current) {
                    if (ch == ' ' || ch == '\t') indent.push_back(ch);
                    else break;
                }
                std::string before = current.substr(0, cursorCol_);
                std::string after = current.substr(cursorCol_);
                if (!before.empty() && before.back() == '{') {
                    indent += "    ";
                }
                lines_[cursorRow_] = before;
                lines_.insert(lines_.begin() + cursorRow_ + 1, indent + after);
                cursorRow_++;
                cursorCol_ = static_cast<int>(indent.size());
                dirty_ = true;
            }
            // Backspace (127 or 8)
            else if (c == 127 || c == 8) {
                if (cursorCol_ > 0) {
                    lines_[cursorRow_].erase(cursorCol_ - 1, 1);
                    cursorCol_--;
                    dirty_ = true;
                } else if (cursorRow_ > 0) {
                    int prevLen = static_cast<int>(lines_[cursorRow_ - 1].size());
                    lines_[cursorRow_ - 1] += lines_[cursorRow_];
                    lines_.erase(lines_.begin() + cursorRow_);
                    cursorRow_--;
                    cursorCol_ = prevLen;
                    dirty_ = true;
                }
            }
            // Escape sequences (Arrow keys, Home, End, Delete)
            else if (c == 27) {
                if (n == 1) {
                    // Bare ESC: unlock full explanation immediately
                    forceFullExplainBonusMs = 5000;
                    statusMessage_ = "Full Learning Guide expanded (Press Ctrl+Q to quit)";
                    resetIdle = false;
                } else if (n >= 3 && buf[1] == '[') {
                    if (buf[2] == 'A') { // Up
                        if (cursorRow_ > 0) {
                            cursorRow_--;
                            cursorCol_ = std::min(cursorCol_, static_cast<int>(lines_[cursorRow_].size()));
                        }
                    } else if (buf[2] == 'B') { // Down
                        if (cursorRow_ + 1 < static_cast<int>(lines_.size())) {
                            cursorRow_++;
                            cursorCol_ = std::min(cursorCol_, static_cast<int>(lines_[cursorRow_].size()));
                        }
                    } else if (buf[2] == 'C') { // Right
                        if (cursorCol_ < static_cast<int>(lines_[cursorRow_].size())) {
                            cursorCol_++;
                        } else if (cursorRow_ + 1 < static_cast<int>(lines_.size())) {
                            cursorRow_++;
                            cursorCol_ = 0;
                        }
                    } else if (buf[2] == 'D') { // Left
                        if (cursorCol_ > 0) {
                            cursorCol_--;
                        } else if (cursorRow_ > 0) {
                            cursorRow_--;
                            cursorCol_ = static_cast<int>(lines_[cursorRow_].size());
                        }
                    } else if (buf[2] == 'H') { // Home
                        cursorCol_ = 0;
                    } else if (buf[2] == 'F') { // End
                        cursorCol_ = static_cast<int>(lines_[cursorRow_].size());
                    }
                }
            }
            // Printable character
            else if (c >= 32 && c < 127) {
                lines_[cursorRow_].insert(cursorCol_, 1, static_cast<char>(c));
                cursorCol_++;
                dirty_ = true;
            }

            if (resetIdle) {
                lastKeystrokeTimeMs_ = currentTimeMs();
                forceFullExplainBonusMs = 0;
                selectedCandidateIdx_ = 0;
                lastRenderedIdleStage_ = -1;
                updatePredictions(0);
            }
        }
    }

    return 0;
}

int TerminalTui::runFileWatcher(const std::string& filePath) {
    std::cout << "\033[1;36m=== AutoCodePredictor Companion File Watcher ===\033[0m\n";
    std::cout << "Watching file: \033[1;33m" << filePath << "\033[0m\n";
    std::cout << "Edit the file in any editor (vim, nano, VS Code). When you save or pause, live predictions & learning guides appear here!\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    std::vector<std::string> prevLines;
    long long lastChangeTimeMs = currentTimeMs();
    int activeLine = 0;
    int lastStagePrinted = -1;

    while (true) {
        std::ifstream in(filePath);
        if (in.is_open()) {
            std::ostringstream ss;
            ss << in.rdbuf();
            std::string content = ss.str();
            auto currLines = ContextAnalyzer::splitLines(content);

            if (currLines != prevLines) {
                // Find the line that changed most recently
                activeLine = static_cast<int>(currLines.size()) - 1;
                for (size_t i = 0; i < currLines.size(); ++i) {
                    if (i >= prevLines.size() || currLines[i] != prevLines[i]) {
                        activeLine = static_cast<int>(i);
                        break;
                    }
                }
                prevLines = currLines;
                lastChangeTimeMs = currentTimeMs();
                lastStagePrinted = -1;
            }

            int idleMs = static_cast<int>(currentTimeMs() - lastChangeTimeMs);
            int stageBucket = (idleMs >= 4500) ? 2 : (idleMs >= 2000 ? 1 : 0);

            if (stageBucket != lastStagePrinted && !currLines.empty()) {
                lastStagePrinted = stageBucket;
                PredictionRequest req;
                req.fileContent = content;
                req.cursorLine = activeLine;
                req.cursorCol = static_cast<int>(currLines[activeLine].size());
                req.idleMs = idleMs;
                req.maxCandidates = 3;

                PredictionResponse resp = engine_.predict(req);

                std::cout << "\033[2J\033[H";
                std::cout << "\033[44;37;1m AutoCodePredictor Companion Watcher \033[0m "
                          << "File: " << filePath << " | Active Line: " << (activeLine + 1)
                          << " | Idle: " << (idleMs / 1000.0) << "s\n";
                std::cout << "\033[90m" << resp.detectedContextSummary << "\033[0m\n\n";

                std::cout << "Current Line (" << (activeLine + 1) << "): \033[1;37m"
                          << currLines[activeLine] << "\033[0m";
                if (!resp.candidates.empty()) {
                    std::cout << "\033[90m" << resp.candidates[0].completionSuffix << "\033[0m";
                }
                std::cout << "\n\n";

                if (resp.candidates.empty()) {
                    std::cout << "\033[90mNo active completion needed on this line.\033[0m\n";
                } else {
                    const auto& top = resp.candidates[0];
                    std::cout << "\033[1;33m🎯 Concept:\033[0m " << top.conceptTitle
                              << " (" << static_cast<int>(top.confidence * 100) << "% confidence)\n";
                    std::cout << "\033[1;32m✨ Suggested Completion:\033[0m\n   \033[36m"
                              << top.fullSuggestion << "\033[0m\n\n";
                    std::cout << "\033[1;33m💡 Progressive Hint:\033[0m\n   "
                              << top.progressiveHint << "\n\n";
                    if (stageBucket >= 1) {
                        std::cout << "\033[1;36m📖 Deep Learning Explanation:\033[0m\n   "
                                  << top.detailedExplanation << "\n\n";
                        if (!top.complexityNote.empty()) {
                            std::cout << "\033[1;35m⏱ Complexity:\033[0m " << top.complexityNote << "\n";
                        }
                        if (!top.commonPitfall.empty()) {
                            std::cout << "\033[1;31m⚠️ Common Pitfall:\033[0m " << top.commonPitfall << "\n";
                        }
                    }
                }
                std::cout << std::flush;
            }
        }
        usleep(250 * 1000);
    }
    return 0;
}

} // namespace autocode
