# Changes: simplified, 100% C++ version

## What changed
- **All C++, terminal only.** Removed the VS Code extension (TypeScript), the JSON server
  and the hand-written JSON parser.
- **Works on Windows.** The old terminal editor used Linux-only APIs. The new, smaller editor
  supports Windows (conio.h) and Linux/macOS (termios); only `src/terminal.cpp` is platform-specific.
- **Smaller and easier to explain:** about 2,450 lines including tests, down from about 4,860.
  One file per idea: trie, scope, ngram, rules, engine, terminal, editor, main.
- **Knowledge is data.** The 1,045-line file of `if` chains became tables of keywords, container
  functions, headers and lessons, plus four short rule functions.
- **A real next-word model.** The N-gram now predicts the next token from bigram/trigram counts
  (sample code plus your file), with backoff, instead of remembering "the rest of a line".
- **Features:** Trie autocomplete with scopes, container functions, N-gram next word, and learning
  hints after a pause (2 s hint, 4.5 s full explanation).
- **Removed comment to code** (`// sort the vector` -> code): rule-based rather than a data
  structure, and the most complex part. This removed about 310 lines, including its tests.

## Kept from the code review
Every relevant fix is still in, with tests: a real `{ }` scope stack; half-typed or renamed names
are never suggested; N-gram counts don't grow per keystroke.

## New
- 44 checks in `--test` (was 6), and each key rule is mutation-tested: breaking it makes a test fail.
