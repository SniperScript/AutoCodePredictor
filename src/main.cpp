#include "json_protocol.hpp"
#include "predictor_engine.hpp"
#include "terminal_tui.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void printUsage(const char* prog) {
    std::cout << "AutoCodePredictor — C++ Code Predictor & Progressive Learning Assistant\n\n"
              << "Usage:\n"
              << "  " << prog << " [--tui] [file.cpp]       Launch interactive Terminal TUI Editor (default)\n"
              << "  " << prog << " --watch <file.cpp>       Watch a C++ file edited externally and show live coaching\n"
              << "  " << prog << " --server                 Run persistent JSON-RPC stdio server (for VS Code extension)\n"
              << "  " << prog << " --demo                   Run showcase of progressive idle & learning predictions\n"
              << "  " << prog << " --test                   Run automated unit & integration test suite\n"
              << "  " << prog << " --predict [options]      One-shot CLI prediction:\n"
              << "      --file <path>       Path to C++ source file\n"
              << "      --line <int>        0-indexed cursor line\n"
              << "      --col <int>         0-indexed cursor column\n"
              << "      --idle <ms>         Simulated idle time in milliseconds (e.g. 0, 2500, 5000)\n"
              << "      --json              Output raw JSON response\n";
}

int runJsonServer(autocode::PredictorEngine& engine) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        autocode::PredictionRequest req;
        std::string action;
        std::string trigger;
        if (!autocode::JsonProtocol::parseRequest(line, req, action, trigger)) {
            continue;
        }
        if (action == "accept") {
            engine.recordAcceptedSuggestion(trigger);
            std::cout << "{\"status\":\"ok\"}" << std::endl;
            continue;
        }
        if (action == "quit") {
            break;
        }
        autocode::PredictionResponse resp = engine.predict(req);
        std::cout << autocode::JsonProtocol::serializeResponse(resp) << std::endl;
    }
    return 0;
}

int runShowcaseDemo(autocode::PredictorEngine& engine) {
    std::cout << "\033[1;36m========================================================================\033[0m\n";
    std::cout << "\033[1;37m   AutoCodePredictor C++ — Progressive Learning & Prediction Showcase   \033[0m\n";
    std::cout << "\033[1;36m========================================================================\033[0m\n\n";

    struct Scenario {
        std::string name;
        std::string code;
        int line;
        int col;
        int idleMs;
    };

    std::vector<Scenario> scenarios = {
        {
            "Scenario 1: Active Typing — Scope & Type-Aware Member Completion (`nums.pu`)",
            "#include <vector>\nint main() {\n    std::vector<int> nums = {10, 20, 30};\n    nums.pu\n}",
            3, 11, 200
        },
        {
            "Scenario 2: Paused 2.5s Mid-Line — Context-Aware Loop Completion (`for (`)",
            "#include <vector>\nint main() {\n    std::vector<int> scores = {95, 82, 88};\n    for (\n}",
            3, 9, 2500
        },
        {
            "Scenario 3: Stuck 5.0s on Blank Line Below Comment (`// sort in descending order`)",
            "#include <vector>\n#include <algorithm>\nint main() {\n    std::vector<int> prices = {40, 10, 90, 20};\n    // sort in descending order\n    \n}",
            5, 4, 5000
        },
        {
            "Scenario 4: Stuck 5.0s After Declaring Vector (No Comment — Workflow Next-Step Coach)",
            "#include <iostream>\n#include <vector>\nint main() {\n    std::vector<int> data = {8, 4, 2, 6};\n    \n}",
            4, 4, 5000
        }
    };

    for (const auto& sc : scenarios) {
        autocode::PredictionRequest req{sc.code, sc.line, sc.col, sc.idleMs, 3};
        autocode::PredictionResponse resp = engine.predict(req);

        std::cout << "\033[1;33m>>> " << sc.name << "\033[0m\n";
        std::cout << "    \033[90m" << resp.detectedContextSummary << " | Idle: " << sc.idleMs << "ms\033[0m\n";
        if (!resp.candidates.empty()) {
            const auto& top = resp.candidates[0];
            std::cout << "    \033[1;32mGhost Completion:\033[0m \033[36m" << top.completionSuffix << "\033[0m\n";
            std::cout << "    \033[1;37mFull Line:\033[0m        " << top.fullSuggestion << "\n";
            std::cout << "    \033[1;33mConcept:\033[0m          " << top.conceptTitle
                      << " [" << top.sourceEngine << ", " << static_cast<int>(top.confidence * 100) << "%]\n";
            std::cout << "    \033[1;35mProgressive Hint:\033[0m " << top.progressiveHint << "\n";
            if (resp.stage == autocode::IdleStage::STUCK) {
                std::cout << "    \033[1;36mWhy It Works:\033[0m     " << top.detailedExplanation << "\n";
                std::cout << "    \033[1;34mComplexity:\033[0m       " << top.complexityNote << "\n";
                if (!top.commonPitfall.empty()) {
                    std::cout << "    \033[1;31mCommon Pitfall:\033[0m   " << top.commonPitfall << "\n";
                }
            }
        }
        std::cout << "\n";
    }
    return 0;
}

int runUnitTests(autocode::PredictorEngine& engine) {
    int passed = 0;
    int total = 0;

    auto check = [&](const std::string& name, bool cond) {
        total++;
        if (cond) {
            passed++;
            std::cout << "  \033[32m[PASS]\033[0m " << name << "\n";
        } else {
            std::cout << "  \033[31m[FAIL]\033[0m " << name << "\n";
        }
    };

    std::cout << "Running AutoCodePredictor C++ Test Suite...\n";

    // Test 1: Prefix Trie completion for #include
    {
        autocode::PredictionRequest req{"#include <io", 0, 12, 100, 3};
        auto resp = engine.predict(req);
        check("Trie completes '#include <io' -> 'stream>'",
              !resp.candidates.empty() && resp.candidates[0].completionSuffix == "stream>");
    }

    // Test 2: Type-aware vector member completion
    {
        std::string code = "int main() {\n    std::vector<int> myVec;\n    myVec.pu\n}";
        autocode::PredictionRequest req{code, 2, 12, 100, 3};
        auto resp = engine.predict(req);
        check("TypeMember completes 'myVec.pu' -> 'sh_back(val);'",
              !resp.candidates.empty() && resp.candidates[0].completionSuffix == "sh_back(val);");
    }

    // Test 3: Context-aware for-loop uses variable name in scope
    {
        std::string code = "int main() {\n    std::vector<int> items;\n    for (\n}";
        autocode::PredictionRequest req{code, 2, 9, 2500, 3};
        auto resp = engine.predict(req);
        check("ContextRule completes 'for (' using in-scope vector 'items'",
              !resp.candidates.empty() &&
              resp.candidates[0].fullSuggestion.find("items.size()") != std::string::npos &&
              resp.stage == autocode::IdleStage::PONDERING);
    }

    // Test 4: Comment-driven intent on empty line when stuck
    {
        std::string code = "int main() {\n    std::vector<int> arr = {3, 1, 2};\n    // sort the vector\n    \n}";
        autocode::PredictionRequest req{code, 3, 4, 5000, 3};
        auto resp = engine.predict(req);
        check("IntentComment predicts 'std::sort(arr.begin(), arr.end());' when stuck on line below comment",
              !resp.candidates.empty() &&
              resp.candidates[0].fullSuggestion == "std::sort(arr.begin(), arr.end());" &&
              resp.stage == autocode::IdleStage::STUCK &&
              !resp.candidates[0].detailedExplanation.empty());
    }

    // Test 5: Stuck next-step coach on blank line after declaring vector (no comment)
    {
        std::string code = "int main() {\n    std::vector<int> values = {9, 2, 5};\n    \n}";
        autocode::PredictionRequest req{code, 2, 4, 4800, 3};
        auto resp = engine.predict(req);
        check("StuckCoach suggests next logical step on blank line after vector declaration",
              !resp.candidates.empty() &&
              resp.candidates[0].fullSuggestion.find("values") != std::string::npos);
    }

    // Test 6: JSON protocol round-trip
    {
        std::string reqJson = "{\"action\":\"predict\",\"fileContent\":\"int main() {\\n    std::so\\n}\",\"cursorLine\":1,\"cursorCol\":11,\"idleMs\":4600,\"maxCandidates\":2}";
        autocode::PredictionRequest parsedReq;
        std::string action, trigger;
        bool ok = autocode::JsonProtocol::parseRequest(reqJson, parsedReq, action, trigger);
        auto resp = engine.predict(parsedReq);
        std::string outJson = autocode::JsonProtocol::serializeResponse(resp);
        check("JsonProtocol parses multi-line request and emits valid STUCK JSON response",
              ok && outJson.find("\"stage\":\"STUCK\"") != std::string::npos &&
              outJson.find("sort(") != std::string::npos);
    }

    std::cout << "\nResult: " << passed << "/" << total << " tests passed.\n";
    return (passed == total) ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    autocode::PredictorEngine engine;

    if (argc == 1) {
        autocode::TerminalTui tui(engine);
        return tui.runInteractiveEditor("playground.cpp");
    }

    std::string mode = argv[1];

    if (mode == "--help" || mode == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    if (mode == "--server") {
        return runJsonServer(engine);
    }
    if (mode == "--demo") {
        return runShowcaseDemo(engine);
    }
    if (mode == "--test") {
        return runUnitTests(engine);
    }
    if (mode == "--tui") {
        std::string file = (argc >= 3) ? argv[2] : "playground.cpp";
        autocode::TerminalTui tui(engine);
        return tui.runInteractiveEditor(file);
    }
    if (mode == "--watch") {
        if (argc < 3) {
            std::cerr << "Error: --watch requires a file path (e.g. ./autocode-predictor --watch playground.cpp)\n";
            return 1;
        }
        autocode::TerminalTui tui(engine);
        return tui.runFileWatcher(argv[2]);
    }
    if (mode == "--predict") {
        std::string filePath;
        int line = 0;
        int col = 0;
        int idleMs = 0;
        bool jsonOut = false;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--file" && i + 1 < argc) filePath = argv[++i];
            else if (arg == "--line" && i + 1 < argc) line = std::stoi(argv[++i]);
            else if (arg == "--col" && i + 1 < argc) col = std::stoi(argv[++i]);
            else if (arg == "--idle" && i + 1 < argc) idleMs = std::stoi(argv[++i]);
            else if (arg == "--json") jsonOut = true;
        }

        std::string content;
        if (!filePath.empty()) {
            std::ifstream in(filePath);
            std::ostringstream ss;
            ss << in.rdbuf();
            content = ss.str();
        }

        autocode::PredictionRequest req{content, line, col, idleMs, 5};
        auto resp = engine.predict(req);
        if (jsonOut) {
            std::cout << autocode::JsonProtocol::serializeResponse(resp) << "\n";
        } else {
            std::cout << "Context: " << resp.detectedContextSummary << "\n";
            for (size_t i = 0; i < resp.candidates.size(); ++i) {
                const auto& c = resp.candidates[i];
                std::cout << "[" << (i + 1) << "] " << c.conceptTitle << " (" << static_cast<int>(c.confidence * 100) << "%)\n"
                          << "    Ghost Suffix: " << c.completionSuffix << "\n"
                          << "    Full Line:    " << c.fullSuggestion << "\n"
                          << "    Hint:         " << c.progressiveHint << "\n"
                          << "    Explanation:  " << c.detailedExplanation << "\n\n";
            }
        }
        return 0;
    }

    // If user passed a filename directly (`./autocode-predictor myfile.cpp`), open it in TUI
    autocode::TerminalTui tui(engine);
    return tui.runInteractiveEditor(mode);
}
