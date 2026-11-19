// main.cpp - AutoCodePredictor: C++ code completion with a learning coach.
//
//   autocode-predictor [file.cpp]           interactive editor (default: playground.cpp)
//   autocode-predictor --demo               scripted showcase of every feature
//   autocode-predictor --predict FILE LINE [PAUSE_MS]
//   autocode-predictor --test               run the test suite

#include "../tests/tests.hpp"
#include "editor.hpp"
#include "engine.hpp"
#include "terminal.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

bool useColor = false;

std::string paint(const char* code, const std::string& text) {
    return useColor ? std::string(code) + text + "\x1b[0m" : text;
}

const char* const kGrey = "\x1b[90m";
const char* const kTitle = "\x1b[1;36m";

const char* stageColour(Stage s) {
    return s == Stage::Typing ? "\x1b[32m" : s == Stage::Pondering ? "\x1b[33m" : "\x1b[36m";
}

void usage() {
    std::cout << "AutoCodePredictor - C++ code completion with a learning coach\n\n"
                 "  autocode-predictor [file.cpp]           interactive editor (default: playground.cpp)\n"
                 "  autocode-predictor --demo               scripted showcase of every feature\n"
                 "  autocode-predictor --predict FILE LINE [PAUSE_MS]\n"
                 "                                          suggestions at the end of LINE (1 = first line)\n"
                 "  autocode-predictor --test               run the test suite\n";
}

// Prints the code (cursor at the end of `line`) and what the coach shows after `idleMs`.
void show(const Engine& engine, const std::string& code, int line, int idleMs) {
    std::vector<std::string> lines = splitLines(code);
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        std::cout << "   " << paint(kGrey, std::to_string(i + 1) + " |") << " " << lines[i]
                  << (i == line ? paint("\x1b[7m", " ") + paint(kGrey, "  <- cursor") : "") << "\n";

    const Prediction p = engine.predict(code, line, static_cast<int>(lines[line].size()), idleMs, 3);
    std::ostringstream pause;
    pause << std::fixed << std::setprecision(1) << idleMs / 1000.0;
    std::cout << "  " << paint(stageColour(p.stage), std::string("[") + stageName(p.stage) + "]") << " after a "
              << pause.str() << " s pause   " << paint(kGrey, p.context) << "\n";
    if (p.suggestions.empty()) std::cout << "    (no suggestions)\n";
    for (size_t k = 0; k < p.suggestions.size(); ++k) {
        const Suggestion& s = p.suggestions[k];
        std::cout << "    " << k + 1 << ". " << s.full << "  "
                  << paint(kGrey, "[" + s.source + "] " + s.lesson.title) << "\n";
    }
    if (p.suggestions.empty()) return;
    const Lesson& l = p.suggestions[0].lesson;
    if (p.stage != Stage::Typing) std::cout << "    Hint: " << l.hint << "\n";
    if (p.stage == Stage::Stuck) {
        std::cout << "    Why: " << l.explanation << "\n";
        if (!l.cost.empty()) std::cout << "    Cost: " << l.cost << "\n";
        if (!l.pitfall.empty()) std::cout << "    Watch out: " << l.pitfall << "\n";
    }
}

void heading(const std::string& title, const std::string& what) {
    std::cout << "\n" << paint(kTitle, "=== " + title + " ===") << "\n  " << what << "\n";
}

int runDemo(const Engine& engine) {
    heading("1. Autocomplete (Trie)", "Typing `cou`: variables in scope come first, then C++ words.");
    show(engine, "int main() {\n    int counter = 0;\n    int country = 91;\n    cou\n}", 3, 0);

    heading("2. Scope-aware", "The loop variable `index` exists inside the loop...");
    const std::string loop = "int main() {\n    for (int index = 0; index < 3; ++index) {\n        ind\n    }\n    ind\n}";
    show(engine, loop, 2, 0);
    std::cout << "  ...but not after it closes:\n";
    show(engine, loop, 4, 0);

    heading("3. Container functions", "After `nums.` the predictor knows nums is a vector.");
    show(engine, "int main() {\n    std::vector<int> nums = {3, 1, 2};\n    nums.pu\n}", 2, 0);

    heading("4. Next-word prediction (N-gram)", "After `std::cout << total <<` - what usually comes next?");
    show(engine, "int main() {\n    int total = 5;\n    std::cout << total << std::endl;\n    std::cout << total <<\n}", 3, 0);

    const std::string queueCode = "int main() {\n    std::queue<int> line;\n    line.po\n}";
    heading("5. Learning hints", "The same line after a pause of 0 s, 2.5 s and 5 s:");
    show(engine, queueCode, 2, 0);
    std::cout << "\n";
    show(engine, queueCode, 2, 2500);
    std::cout << "\n";
    show(engine, queueCode, 2, 5000);

    heading("6. Stuck on an empty line", "Pause on an empty line and it suggests a sensible next step:");
    show(engine, "int main() {\n    std::vector<int> data = {8, 4, 2, 6};\n    \n}", 2, 5000);
    std::cout << "\n";
    return 0;
}

int runPredict(const Engine& engine, int argc, char** argv) {
    if (argc < 4) {
        usage();
        return 1;
    }
    std::ifstream in(argv[2]);
    if (!in) {
        std::cerr << "Error: cannot open " << argv[2] << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    int line = 0, idle = 0;
    try {
        line = std::stoi(argv[3]);
        idle = argc > 4 ? std::stoi(argv[4]) : 0;
    } catch (const std::exception&) {
        std::cerr << "Error: LINE and PAUSE_MS must be whole numbers\n";
        return 1;
    }
    int count = static_cast<int>(splitLines(ss.str()).size());
    if (line < 1 || line > count) {
        std::cerr << "Error: LINE must be between 1 and " << count << "\n";
        return 1;
    }
    show(engine, ss.str(), line - 1, idle);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    useColor = term::outputIsTerminal();
    if (useColor) term::enableColors();

    if (mode == "--help" || mode == "-h") {
        usage();
        return 0;
    }
    if (mode == "--test") return runTests() == 0 ? 0 : 1;

    Engine engine;
    if (mode == "--demo") return runDemo(engine);
    if (mode == "--predict") return runPredict(engine, argc, argv);
    if (!mode.empty() && mode[0] == '-') {
        std::cerr << "Unknown option: " << mode << "\n\n";
        usage();
        return 1;
    }
    return runEditor(engine, mode.empty() ? "playground.cpp" : mode);
}
