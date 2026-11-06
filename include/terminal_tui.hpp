#ifndef AUTOCODE_TERMINAL_TUI_HPP
#define AUTOCODE_TERMINAL_TUI_HPP

#include "predictor_engine.hpp"
#include <string>
#include <vector>

namespace autocode {

class TerminalTui {
public:
    explicit TerminalTui(PredictorEngine& engine);
    ~TerminalTui();

    /**
     * Launch the interactive Terminal Code Editor with live ghost text and idle learning assistant.
     */
    int runInteractiveEditor(const std::string& filePath = "");

    /**
     * Launch the companion file-watcher mode in the terminal (monitors an external file for changes/pauses).
     */
    int runFileWatcher(const std::string& filePath);

private:
    PredictorEngine& engine_;
    std::vector<std::string> lines_;
    std::string filePath_;
    int cursorRow_ = 0;
    int cursorCol_ = 0;
    int scrollOffsetRow_ = 0;
    int selectedCandidateIdx_ = 0;
    bool running_ = false;
    bool dirty_ = false;
    std::string statusMessage_;

    PredictionResponse currentPrediction_;
    long long lastKeystrokeTimeMs_ = 0;
    int lastRenderedIdleStage_ = -1;

    void loadFileOrDefault(const std::string& path);
    bool saveFile();
    std::string joinBuffer() const;
    void updatePredictions(int idleMs);
    void acceptCurrentSuggestion();
    void renderScreen(int termRows, int termCols, int idleMs);
    static std::string highlightCppLine(const std::string& line, const std::string& ghostSuffix, int cursorCol, bool isCurrentLine);
    static std::vector<std::string> wrapText(const std::string& text, int maxWidth);
    static long long currentTimeMs();
};

} // namespace autocode

#endif // AUTOCODE_TERMINAL_TUI_HPP
