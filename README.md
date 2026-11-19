# AutoCodePredictor

**C++ code completion with a learning coach, written entirely in C++ (standard library only).**

You type C++ in a small terminal editor. A suggestion appears in grey at the cursor.
If you **pause**, the coach helps more:

| Pause | What you see |
|---|---|
| typing | the suggestion only |
| 2 seconds | + a **hint** |
| 4.5 seconds | + **why it works**, its **cost** (Big-O) and a **common pitfall** |

Works on **Windows, Linux and macOS**.

## Features

1. **Autocomplete (Trie):** variables in scope, C++ keywords and standard library names.
   Words you accept rank higher next time.
2. **Scope-aware:** only variables visible at the cursor are suggested. A variable from another
   function, or from a loop that has finished, is not.
3. **Next-word prediction (N-gram):** after `std::cout << total <<` it predicts `std`, learning
   from sample code and from your own file.
4. **Container functions:** after `nums.` it lists the functions of that container type
   (`push_back`, `size`, ...), with their real cost for that type.
5. **Learning hints:** every suggestion comes with a hint, an explanation, its cost and a pitfall.

## How it works (the data structures)

| Idea | Data structure | Cost |
|---|---|---|
| Find all words starting with `cou` | **Trie** (prefix tree). Children are kept in a `std::map`, so words come out alphabetically; then they are ordered by weight | reach the prefix O(L), then sort the m matches O(m log m) |
| Which variables exist here | **Stack of scopes**: `{` pushes, `}` pops and removes that block's variables (restoring any outer one they hid) | O(1) per brace, O(file length) per scan |
| Most likely next word | **N-gram model**: hash maps of counts, (last 1 or 2 tokens) -> (next token -> count). Uses the last 2 tokens if that pair was seen ("trigram"), else the last 1 ("bigram") | O(1) average lookup |
| What C++ offers | **Tables** of keywords, library names, container functions, headers and lessons (`rules.cpp`) | O(table size) |

## Files: where each part is

| File | What it contains |
|---|---|
| `src/trie.hpp/.cpp` | The Trie: `insert`, `complete`, `boost` (learning) |
| `src/scope.hpp/.cpp` | Reads the code up to the cursor: the scope stack, declarations, the comment above, the word being typed. Also the tokenizer |
| `src/ngram.hpp/.cpp` | The N-gram next-word model: `learn`, `predict` |
| `src/rules.hpp/.cpp` | The knowledge (keyword lists, container functions, headers, lessons) and the three rules: container functions, syntax (`for (`, `#include <`), next steps |
| `src/engine.hpp/.cpp` | Combines everything, ranks the suggestions, decides the learning stage from the pause |
| `src/terminal.hpp/.cpp` | The only platform-specific code: reading single keys (Windows `conio.h`, Linux/macOS `termios`) |
| `src/editor.hpp/.cpp` | The interactive editor |
| `src/main.cpp` | Program modes (editor, `--demo`, `--predict`, `--test`) and the showcase |
| `tests/tests.cpp` | All tests |
| `playground.cpp` | A starter program the editor opens by default |

## Build and run

**Windows** (g++ 10 or newer, for example WinLibs), from Command Prompt in this folder:
```bat
build.bat
autocode-predictor.exe --demo
autocode-predictor.exe
```
The first command builds, the second shows every feature, the third opens the editor on
`playground.cpp`. Or just run `demo.bat`: it builds, runs the tests and the showcase.

**Linux / macOS:**
```bash
make
./autocode-predictor --demo
./autocode-predictor
```

Other commands:
```
autocode-predictor myfile.cpp                  open another file in the editor
autocode-predictor --predict FILE LINE [MS]    suggestions at the end of LINE after a pause of MS milliseconds
autocode-predictor --test                      run the tests
```

## Editor keys

| Key | Action |
|---|---|
| type | edit the current line (the cursor is always at the end of the line) |
| **Tab** | insert the selected suggestion |
| **Ctrl+N** | select the next suggestion |
| **Ctrl+E** | show the full explanation now, without waiting |
| Up / Down | move to another line |
| Enter | new line (indents automatically) |
| **Ctrl+S** | save |
| **Esc** | quit |

## Tests

`--test` runs 44 checks: every feature, plus regression checks for bugs found in an earlier
code review (scopes, half-typed or renamed names, N-gram counts).

## Limits

- The scope analysis reads declarations line by line; it is not a full C++ parser
  (for example, function parameters are not tracked as variables).
- Container suggestions use placeholders such as `push_back(val)` that you then edit.
- Comment-to-code generation (`// sort the vector` -> code) was removed to keep the project focused
  on its data structures.
- What the coach learns (accepted words) is kept in memory only and resets when you quit.
- The editor is deliberately simple: the cursor stays at the end of the line.
