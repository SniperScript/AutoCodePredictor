#include "editor.hpp"

#include "terminal.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace {

const char* const kGrey = "\x1b[90m";
const char* const kBold = "\x1b[1m";
const char* const kGreen = "\x1b[32m";
const char* const kYellow = "\x1b[33m";
const char* const kCyan = "\x1b[36m";
const char* const kInverse = "\x1b[7m";
const char* const kReset = "\x1b[0m";

long long nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string indentOf(const std::string& line) {  // the leading spaces
    size_t n = line.find_first_not_of(' ');
    return line.substr(0, n == std::string::npos ? line.size() : n);
}

// Breaks `text` into lines of at most `width` characters, at spaces.
std::vector<std::string> wrap(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string word, cur;
    while (in >> word) {
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back(cur);
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

struct Editor {
    Editor(Engine& e, std::string file) : engine(e), path(std::move(file)) {}

    Engine& engine;
    std::string path;
    std::vector<std::string> lines;
    int row = 0;              // the cursor is always at the end of this line
    int selected = 0;         // which suggestion Tab will insert
    bool explainNow = false;  // Ctrl+E: show the full explanation without waiting
    bool changed = false;
    std::string status;
    Prediction prediction;

    void load() {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        if (lines.empty())
            lines = {"#include <iostream>", "#include <vector>", "", "int main() {",
                     "    std::vector<int> nums = {42, 7, 19, 3};", "    ", "",
                     "    return 0;", "}"};
        // Start on the first indented empty line (inside main), else at the end.
        row = static_cast<int>(lines.size()) - 1;
        for (size_t i = 0; i < lines.size(); ++i)
            if (!lines[i].empty() && trim(lines[i]).empty()) {
                row = static_cast<int>(i);
                break;
            }
    }

    bool save() {
        std::ofstream out(path);
        for (const auto& l : lines) out << l << "\n";
        changed = !out;
        status = out ? "Saved " + path : "Could not save " + path;
        return static_cast<bool>(out);
    }

    std::string text() const {
        std::string all;
        for (size_t i = 0; i < lines.size(); ++i) all += lines[i] + (i + 1 < lines.size() ? "\n" : "");
        return all;
    }

    void predict(int idleMs) {
        prediction = engine.predict(text(), row, static_cast<int>(lines[row].size()), idleMs, 4);
        if (selected >= static_cast<int>(prediction.suggestions.size())) selected = 0;
    }

    void accept() {
        if (prediction.suggestions.empty()) {
            status = "Nothing to insert yet.";
            return;
        }
        const Suggestion s = prediction.suggestions[selected];
        lines[row] += s.insert;
        engine.accept(s);
        changed = true;
        selected = 0;
        status = "Inserted: " + s.lesson.title;
    }

    void edit(int key) {
        std::string& line = lines[row];
        switch (key) {
            case term::kEnter: {
                std::string indent = indentOf(line);
                if (!trim(line).empty() && trim(line).back() == '{') indent += "    ";
                lines.insert(lines.begin() + row + 1, indent);
                ++row;
                break;
            }
            case term::kBackspace:
                if (!line.empty()) line.pop_back();
                else if (row > 0) lines.erase(lines.begin() + row--);
                break;
            case term::kUp:
                if (row > 0) --row;
                break;
            case term::kDown:
                if (row + 1 < static_cast<int>(lines.size())) ++row;
                break;
            default:  // a printable character; `}` on an indented empty line moves back one level
                if (key == '}' && trim(line).empty() && line.size() >= 4) line.resize(line.size() - 4);
                line += static_cast<char>(key);
        }
        changed = changed || (key != term::kUp && key != term::kDown);
    }

    void render() const {
        const Stage stage = prediction.stage;
        const bool showHint = explainNow || stage != Stage::Typing;
        const bool showAll = explainNow || stage == Stage::Stuck;
        const Suggestion* top = prediction.suggestions.empty() ? nullptr : &prediction.suggestions[selected];

        term::clearScreen();
        std::cout << kBold << " AutoCodePredictor" << kReset << "   " << path << (changed ? " (unsaved)" : "") << "\n"
                  << kGrey << " Tab insert | Ctrl+N next | Ctrl+E explain | arrows: change line | Ctrl+S save | Esc quit"
                  << kReset << "\n" << std::string(80, '-') << "\n";

        const int first = std::max(0, row - 12);
        const int last = std::min(static_cast<int>(lines.size()), first + 16);
        for (int i = first; i < last; ++i) {
            std::cout << kGrey << (i + 1 < 10 ? "   " : i + 1 < 100 ? "  " : " ") << i + 1 << " | " << kReset << lines[i];
            if (i == row) {
                std::cout << kInverse << " " << kReset;  // the cursor
                if (top) std::cout << kGrey << top->insert << kReset;
            }
            std::cout << "\n";
        }

        std::cout << std::string(80, '-') << "\n";
        const char* colour = stage == Stage::Typing ? kGreen : stage == Stage::Pondering ? kYellow : kCyan;
        std::cout << " " << colour << "[" << stageName(stage) << "]" << kReset << kGrey << "  " << prediction.context
                  << kReset << "\n";
        if (!top) std::cout << "   (no suggestion - keep typing, or pause for help)\n";
        for (size_t k = 0; k < prediction.suggestions.size(); ++k) {
            const Suggestion& s = prediction.suggestions[k];
            std::string shown = s.full;
            if (shown.size() > 42) shown = shown.substr(0, 39) + "...";
            std::cout << (static_cast<int>(k) == selected ? " > " : "   ") << k + 1 << ". " << shown
                      << std::string(44 - shown.size(), ' ') << kGrey << "[" << s.source << "]" << kReset << "\n";
        }
        if (top) {
            std::cout << "\n " << kBold << top->lesson.title << kReset << "\n";
            if (showHint)
                for (const auto& l : wrap("Hint: " + top->lesson.hint, 76)) std::cout << " " << kYellow << l << kReset << "\n";
            if (showAll) {
                for (const auto& l : wrap("Why: " + top->lesson.explanation, 76)) std::cout << " " << kCyan << l << kReset << "\n";
                if (!top->lesson.cost.empty()) std::cout << " " << kCyan << "Cost: " << top->lesson.cost << kReset << "\n";
                if (!top->lesson.pitfall.empty())
                    for (const auto& l : wrap("Watch out: " + top->lesson.pitfall, 76)) std::cout << " " << kCyan << l << kReset << "\n";
            } else if (!showHint) {
                std::cout << kGrey << " (pause 2 s for a hint, 4.5 s for the full explanation)" << kReset << "\n";
            }
        }
        if (!status.empty()) std::cout << "\n " << kGrey << status << kReset << "\n";
        std::cout << std::flush;
    }
};

}  // namespace

int runEditor(Engine& engine, const std::string& path) {
    if (!term::isInteractive()) {
        std::cerr << "The editor needs a terminal window. Try --demo instead.\n";
        return 1;
    }
    Editor ed{engine, path};
    ed.load();

    term::start();
    struct RestoreTerminal {
        ~RestoreTerminal() { term::stop(); }
    } restore;

    long long lastEdit = nowMs();
    ed.predict(0);
    ed.render();
    for (;;) {
        const int key = term::readKey(100);
        const int idle = static_cast<int>(nowMs() - lastEdit);
        if (key == term::kNone) {  // nothing typed: has the pause reached the next stage?
            if (stageFor(idle) != ed.prediction.stage) {
                ed.predict(idle);
                ed.render();
            }
            continue;
        }
        if (key == term::kEscape || key == term::kCtrlQ) break;
        ed.status.clear();
        if (key == term::kCtrlN) {  // next suggestion (not an edit: the pause continues)
            if (!ed.prediction.suggestions.empty())
                ed.selected = (ed.selected + 1) % static_cast<int>(ed.prediction.suggestions.size());
        } else if (key == term::kCtrlE) {
            ed.explainNow = true;
        } else if (key == term::kCtrlS) {
            ed.save();
        } else {
            if (key == term::kTab) ed.accept();
            else ed.edit(key);
            ed.explainNow = false;
            lastEdit = nowMs();
            ed.predict(0);
        }
        ed.render();
    }
    term::clearScreen();
    std::cout << (ed.changed ? "Closed without saving the last changes.\n" : "Closed.\n");
    return 0;
}
