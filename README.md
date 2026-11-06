# AutoCodePredictor C++ — Code Predictor & Progressive Learning Coach

**AutoCodePredictor** is a dual-mode C++ code completion and educational coaching system built in **C++20** with both:
1. **An Interactive Terminal TUI Editor & Companion Watcher** (`./autocode-predictor`)
2. **A Native VS Code Extension** (`vscode-extension/`) powered by the C++ backend over a persistent JSON-RPC stdio stream (`--server`).

Unlike standard autocomplete tools that only show raw text, **AutoCodePredictor** tracks how long you have been settled/stuck on a line of code and progressively unlocks **conceptual hints**, **complete line/next-step predictions**, **"Why This Works" explanations**, **Big-O complexity notes**, and **common C++ pitfalls**.

---

## Architecture & C++ Data Structures

The core engine (`src/core/`, `include/`) has zero external dependencies and combines 5 specialized C++ sub-engines:

1. **Weighted Prefix Trie (`Trie`)** — [`include/trie.hpp`](include/trie.hpp) / [`src/core/trie.cpp`](src/core/trie.cpp)
   - Stores C++ keywords, headers, idioms, and dynamically discovered local variables with adaptive frequency boosting when suggestions are accepted.
2. **Bi-gram & Tri-gram Markov Model (`NGramModel`)** — [`include/ngram_model.hpp`](include/ngram_model.hpp) / [`src/core/ngram_model.cpp`](src/core/ngram_model.cpp)
   - Tokenizes C++ code and learns sequence transitions both from curated STL patterns and dynamically from the user's current file buffer.
3. **Lexical & Scope Context Analyzer (`ContextAnalyzer`)** — [`include/context_analyzer.hpp`](include/context_analyzer.hpp) / [`src/core/context_analyzer.cpp`](src/core/context_analyzer.cpp)
   - Tracks `#include` headers, brace scope depth, and local variable declarations with their normalized types (`std::vector`, `std::unordered_map`, `std::map`, `std::set`, `std::string`, `std::queue`, `std::stack`, `std::priority_queue`, smart pointers).
4. **Pedagogical Knowledge Base & Intent Matcher (`KnowledgeBase`)** — [`include/knowledge_base.hpp`](include/knowledge_base.hpp) / [`src/core/knowledge_base.cpp`](src/core/knowledge_base.cpp)
   - **Type-Aware Member Completion**: Typing `nums.` or `mp.fi` inspects the variable's declared type and suggests valid STL methods tailored to that variable name.
   - **Mid-Line Syntax Completion**: Typing `for (`, `while (`, `if (`, or `std::sort(` automatically uses the actual container variable names in scope (e.g., `for (int i = 0; i < static_cast<int>(nums.size()); ++i)`).
   - **Comment-to-Code Intent Matcher**: Writing `// sort in descending order`, `// binary search`, `// frequency count`, `// print`, `// sum`, or `// read file` and pausing on the line below generates the exact C++ implementation using your in-scope variables.
   - **Stuck Next-Step Workflow Coach**: If you pause on an empty line right after declaring a `std::vector`, opening a `for` loop, or opening `int main()`, it predicts the next logical step in your program.
5. **Progressive Idle Orchestrator (`PredictorEngine`)** — [`include/predictor_engine.hpp`](include/predictor_engine.hpp) / [`src/core/predictor_engine.cpp`](src/core/predictor_engine.cpp)
   - **Stage 0 — `TYPING` (`< 2.0s` idle)**: Subtle inline gray ghost-text completion (`Tab` to accept).
   - **Stage 1 — `PONDERING` (`2.0s – 4.5s` idle)**: Unlocks a **Progressive Conceptual Hint** nudging you on how to complete the line without overwhelming you.
   - **Stage 2 — `STUCK` (`>= 4.5s` idle)**: Expands the **Full Learning Guide** with the complete code suggestion, deep explanation of *why* it works, time/space complexity, and common pitfalls.

---

## Quick Start

### 1. Build & Run Tests
```bash
make
./autocode-predictor --test
./autocode-predictor --demo
```

### 2. Run the Terminal Code Predictor (Interactive TUI Editor)
```bash
./autocode-predictor playground.cpp
```
**Terminal TUI Controls:**
- **Type C++ code**: Live inline gray ghost text appears automatically at the cursor.
- **Pause for 2 seconds**: Status bar turns amber (`💡 PAUSED — Progressive Hint Unlocked`) and displays a conceptual hint in the bottom Learning Coach panel.
- **Stay settled on a line for 4.5 seconds**: Status bar turns cyan (`🎓 STUCK ON LINE — Full Learning Guide Active`) and expands the full explanation, complexity, and pitfall guide (even on a blank line!).
- `Tab`: Accept the current ghost-text completion (or insert the suggested next line).
- `Ctrl+N`: Cycle through ranked alternative suggestions (`1/4`, `2/4`, ...).
- `ESC` or `Ctrl+E`: Immediately expand the Full Learning Guide without waiting 4.5 seconds.
- `Ctrl+S`: Save file.
- `Ctrl+Q`: Quit.

### 3. Run Terminal Companion Watcher Mode (For Vim / Nano / External Editors)
If you prefer editing in `vim` or `nano`, open a second split terminal and run:
```bash
./autocode-predictor --watch playground.cpp
```
Whenever you save or pause in your editor, the companion terminal displays live completions and learning explanations for your active line.

### 4. Run the VS Code Extension
1. Open this folder (`/home/xczn/Documents/Coding backup/AutoCodePredictor`) in **VS Code**.
2. Press **`F5`** (uses [`.vscode/launch.json`](.vscode/launch.json) to launch an **Extension Development Host** window with `playground.cpp` open).
3. In the Extension Development Host window:
   - Open the **AutoCode Learning Coach** icon (`🎓` mortar-board) in the Activity Bar on the left.
   - Type C++ code to see real-time **inline ghost text** (`Tab` to accept, `Alt+]` to cycle suggestions).
   - **Pause on any line for 2–4.5 seconds**:
     - Inline **CodeLens badges** (`💡 AutoCode Coach` & `✨ Insert`) appear above the line.
     - The **AutoCode Learning Coach** sidebar automatically transitions from `⚡ Active Typing` → `💡 Hint Unlocked` → `🎓 Stuck Coach Active (Full Learning Guide)`.
     - Press `Ctrl+Shift+E` anytime to explain and complete the current line immediately.
