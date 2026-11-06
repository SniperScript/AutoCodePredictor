#include "knowledge_base.hpp"
#include "context_analyzer.hpp"
#include <algorithm>

namespace autocode {

std::string KnowledgeBase::findBestVarOfType(
    const ScopeContext& ctx,
    const std::string& baseType,
    const std::string& fallback
) {
    int bestLine = -1;
    std::string bestName = fallback;
    for (const auto& kv : ctx.variables) {
        if (kv.second.baseType == baseType && kv.second.declarationLine >= bestLine) {
            bestLine = kv.second.declarationLine;
            bestName = kv.first;
        }
    }
    return bestName;
}

void KnowledgeBase::initializeModels(Trie& trie, NGramModel& ngram) {
    // 1. Headers & Preprocessor
    trie.insert({
        "#include <iostream>", "#include <iostream>", "int main() {", 4.0,
        "Standard I/O Stream Header",
        "Include `<iostream>` to use `std::cout`, `std::cin`, and `std::endl`.",
        "`#include <iostream>` brings in the C++ Standard Input/Output stream objects (`std::cin`, `std::cout`, `std::cerr`). Unlike C's `<stdio.h>`, C++ streams are type-safe and extensible via `operator<<` and `operator>>`.",
        "Compile-time header inclusion",
        "For competitive programming or heavy I/O, combine with `std::ios::sync_with_stdio(false);`."
    });
    trie.insert({
        "#include <vector>", "#include <vector>", "std::vector<int> nums;", 3.8,
        "Dynamic Contiguous Array (<vector>)",
        "Include `<vector>` for resizable dynamic arrays (`std::vector<T>`).",
        "`std::vector` is the default sequence container in C++. It stores elements contiguously in memory, offering O(1) random access and amortized O(1) insertion at the back.",
        "O(1) random access, O(1) amortized push_back",
        "Reallocation invalidates all existing pointers and iterators into the vector."
    });
    trie.insert({
        "#include <algorithm>", "#include <algorithm>", "std::sort(v.begin(), v.end());", 3.7,
        "STL Algorithms Header (<algorithm>)",
        "Include `<algorithm>` for `std::sort`, `std::binary_search`, `std::reverse`, `std::max_element`.",
        "`<algorithm>` provides iterator-based generic algorithms (`std::sort`, `std::find`, `std::lower_bound`, `std::reverse`, `std::transform`) that work uniformly across containers.",
        "Varies by algorithm (e.g., std::sort is O(N log N))",
        "Always pass `[begin, end)` half-open iterator ranges."
    });
    trie.insert({
        "#include <unordered_map>", "#include <unordered_map>", "std::unordered_map<int, int> freq;", 3.5,
        "Hash Table Map (<unordered_map>)",
        "Include `<unordered_map>` for average O(1) key-value hash lookups.",
        "`std::unordered_map<Key, Value>` implements a hash table using separate chaining. Ideal for frequency counting, memoization, and O(1) lookups.",
        "Average O(1) lookup/insert, worst-case O(N) on hash collisions",
        "Accessing `mp[key]` inserts a default-constructed value if `key` doesn't exist; use `mp.find(key)` or `mp.count(key)` for read-only checks."
    });
    trie.insert({
        "#include <string>", "#include <string>", "std::string text;", 3.4,
        "C++ String Header (<string>)",
        "Include `<string>` for `std::string` and `std::getline`.",
        "`std::string` manages a dynamically sized character array automatically and provides `.substr()`, `.find()`, `.size()`, and `+` concatenation.",
        "Small-String Optimization (SSO) avoids heap allocation for short strings",
        "Pass `const std::string&` to functions to avoid unnecessary deep copies."
    });
    trie.insert({
        "#include <memory>", "#include <memory>", "auto ptr = std::make_unique<int>(42);", 3.3,
        "Smart Pointers Header (<memory>)",
        "Include `<memory>` for `std::unique_ptr`, `std::shared_ptr`, and `std::make_unique`.",
        "Modern C++ (RAII) uses smart pointers (`std::unique_ptr` and `std::shared_ptr`) from `<memory>` to automatically free heap memory when objects go out of scope, preventing memory leaks.",
        "Zero overhead for std::unique_ptr; atomic ref-count overhead for std::shared_ptr",
        "Prefer `std::make_unique<T>(...)` over raw `new T(...)` for exception safety."
    });

    // 2. Control Structures & Idioms
    trie.insert({
        "for", "for (int i = 0; i < n; ++i) {", "    ", 4.5,
        "Indexed For-Loop",
        "Standard 3-part C++ loop: initialization (`int i = 0`), condition (`i < n`), and increment (`++i`).",
        "An indexed `for` loop gives you direct access to the 0-based index `i`. Useful when you need neighbor comparisons (`v[i]` vs `v[i+1]`), in-place position math, or reverse iteration.",
        "O(N) iterations",
        "Be careful of off-by-one errors (`i <= n` or `i + 1 < n` when accessing `v[i + 1]`)."
    });
    trie.insert({
        "for (", "for (const auto& item : container) {", "    std::cout << item << \" \";", 4.2,
        "Range-Based For-Loop (C++11)",
        "Use `for (const auto& elem : container)` to iterate safely without index bounds bugs.",
        "Range-based `for` loops iterate over any container offering `.begin()` and `.end()`. Using `const auto&` avoids copying elements while preventing accidental modification; use `auto&` if you want to mutate elements in place.",
        "O(N) traversal across container",
        "Do not insert or erase elements from the container while iterating over it in a range-based loop."
    });
    trie.insert({
        "while", "while (!q.empty()) {", "    auto curr = q.front(); q.pop();", 3.8,
        "While Loop (Condition-Driven Iteration)",
        "Use `while (condition)` when the number of iterations isn't fixed upfront (e.g., BFS queue or two pointers).",
        "A `while` loop checks its condition before every iteration. Commonly used for queue processing (`while (!q.empty())`), two-pointer convergence (`while (left < right)`), and stream reading (`while (std::getline(file, line))`).",
        "Depends on loop state progression",
        "Ensure the loop body progresses toward terminating the condition to avoid infinite loops."
    });
    trie.insert({
        "int main", "int main() {\n    \n    return 0;\n}", "    std::ios::sync_with_stdio(false);", 4.5,
        "C++ Program Entry Point (`int main`)",
        "Every standalone C++ program starts execution at `int main()`.",
        "`int main()` is the standard entry point required by the C++ standard. Returning `0` signals successful execution to the operating system.",
        "O(1) startup",
        "In standard C++, `main` must return `int` (never `void main()`)."
    });
    trie.insert({
        "std::sort", "std::sort(v.begin(), v.end());", "", 4.4,
        "Introsort (`std::sort`)",
        "Pass `[v.begin(), v.end())` to sort elements in ascending order in O(N log N) time.",
        "`std::sort` uses Introsort (a hybrid of QuickSort, HeapSort, and InsertionSort). You can pass a custom comparator or lambda as a 3rd argument, e.g., `std::greater<int>()` for descending order.",
        "O(N log N) worst-case and average time, O(log N) stack space",
        "Requires RandomAccessIterator (`std::vector`, `std::deque`, `std::string`); for `std::list`, use `list.sort()`."
    });
    trie.insert({
        "std::binary_search", "std::binary_search(v.begin(), v.end(), target)", "", 3.9,
        "Binary Search (`std::binary_search`)",
        "Checks if `target` exists in a sorted range `[v.begin(), v.end())` in O(log N) time.",
        "`std::binary_search` returns `true` if `target` is found in the sorted range. If you also need the index/iterator of the element, use `std::lower_bound(v.begin(), v.end(), target)` instead!",
        "O(log N) comparisons on random-access iterators",
        "The container MUST be sorted first; otherwise binary search produces undefined/incorrect results."
    });
    trie.insert({
        "std::lower_bound", "auto it = std::lower_bound(v.begin(), v.end(), target);", "if (it != v.end() && *it == target) {", 3.8,
        "Lower Bound Binary Search (`std::lower_bound`)",
        "Returns an iterator to the first element `>= target` in a sorted range.",
        "`std::lower_bound` performs binary search to locate the first element not less than `target`. Subtract `v.begin()` (`int idx = it - v.begin();`) to get the 0-based index!",
        "O(log N) time, O(1) space",
        "Always check `it != v.end() && *it == target` before dereferencing `it` as an exact match."
    });
    trie.insert({
        "std::vector", "std::vector<int> nums = {1, 2, 3, 4, 5};", "std::sort(nums.begin(), nums.end());", 4.3,
        "STL Dynamic Array (`std::vector`)",
        "Declare a typed vector `std::vector<T> name` for contiguous dynamic storage.",
        "`std::vector<T>` manages a dynamically resizing array on the heap via RAII. Use `.push_back(x)` or `.emplace_back(x)` to append, and `.size()` to query length.",
        "O(1) random access, O(1) amortized append",
        "Use `v.reserve(n)` if you know the capacity ahead of time to avoid reallocations."
    });
    trie.insert({
        "std::unordered_map", "std::unordered_map<int, int> freq;", "for (int x : nums) freq[x]++;", 4.1,
        "STL Hash Map (`std::unordered_map`)",
        "Declare `std::unordered_map<Key, Val>` for O(1) average key-value lookups.",
        "`std::unordered_map` stores key-value pairs in hash buckets. It is the go-to data structure for Two-Sum, frequency tables, and caching.",
        "O(1) average time per lookup/insert",
        "`std::pair` has no default `std::hash` specialization out of the box."
    });
    trie.insert({
        "std::cout", "std::cout << result << std::endl;", "", 4.4,
        "Standard Output Stream (`std::cout`)",
        "Use the stream insertion operator `<<` to print values to standard output.",
        "`std::cout` formats and prints variables of any built-in type. Chaining `<<` allows combining strings and variables on a single line.",
        "O(length of output); `\\n` is faster than `std::endl` since `std::endl` forces a buffer flush",
        "Prefer `'\\n'` over `std::endl` inside tight loops to avoid slow I/O flushing."
    });
    trie.insert({
        "std::unique_ptr", "auto ptr = std::make_unique<int>(10);", "", 3.6,
        "Exclusive-Ownership Smart Pointer (`std::unique_ptr`)",
        "Use `std::make_unique<T>(args...)` for automatic, leak-free heap allocation.",
        "`std::unique_ptr<T>` enforces single ownership of a heap object and deletes it automatically when the pointer leaves scope. It cannot be copied, only moved via `std::move(ptr)`.",
        "O(1) creation and zero runtime overhead compared to raw pointers",
        "Never create two `unique_ptr` instances from the same raw pointer."
    });
    trie.insert({
        "class", "class MyClass {\npublic:\n    MyClass() = default;\n    ~MyClass() = default;\nprivate:\n    int data_ = 0;\n};", "", 3.7,
        "C++ Class Definition (Encapsulation & RAII)",
        "Define `public:` methods for the interface and `private:` members for encapsulated state.",
        "A `class` in C++ defaults to `private` access (unlike `struct`, which defaults to `public`). Follow the Rule of Zero/Five when managing resources.",
        "Compile-time type layout",
        "Don't forget the trailing semicolon `;` after the closing brace `};` of a class or struct!"
    });
    trie.insert({
        "return", "return 0;", "", 4.0,
        "Return Statement",
        "Return a value matching the function's return type (or `0` from `main`).",
        "`return` exits the current function immediately and passes the result back to the caller.",
        "O(1) control transfer (supports Named Return Value Optimization / Copy Elision)",
        "Never return a reference or pointer to a local stack variable!"
    });

    // 3. Curated N-Gram token transitions
    ngram.addCuratedPattern("std", "::", {
        "cout << ", 10,
        "Standard Output (`std::cout`)",
        "Stream values to console using `std::cout << value << '\\n';`.",
        "The `::` scope resolution operator accesses identifiers inside the `std` namespace.",
        "O(1) stream operator dispatch"
    });
    ngram.addCuratedPattern("std", "::", {
        "vector<int> ", 9,
        "STL Vector Declaration (`std::vector<int>`)",
        "Declare a dynamic array of integers.",
        "`std::vector<int>` is a contiguous, resizable array in the `std` namespace.",
        "O(1) empty initialization"
    });
    ngram.addCuratedPattern("std", "::", {
        "sort(v.begin(), v.end());", 8,
        "STL Sort (`std::sort`)",
        "Sort a container using `.begin()` and `.end()` iterators.",
        "`std::sort` sorts elements in `[first, last)` in `O(N log N)` time.",
        "O(N log N) time"
    });
    ngram.addCuratedPattern("cout", "<<", {
        "\"Hello, World!\" << std::endl;", 7,
        "Stream Insertion (`<<`)",
        "Chain `<<` with `std::endl` or `\"\\n\"` to finish printing a line.",
        "The `<<` operator sends formatted data to `std::cout` and returns a reference to the stream so you can chain multiple values.",
        "O(length of formatted output)"
    });
    ngram.addCuratedPattern("cin", ">>", {
        "n;", 7,
        "Stream Extraction (`>>`)",
        "Read whitespace-delimited input from `std::cin` into a variable.",
        "`std::cin >> var` skips leading whitespace and parses the next value matching `var`'s type.",
        "O(length of token)"
    });
}

std::vector<PredictionCandidate> KnowledgeBase::matchMemberAccess(const ScopeContext& ctx) {
    std::vector<PredictionCandidate> out;
    if (ctx.objectBeforeAccessor.empty() || ctx.accessorOp.empty()) {
        return out;
    }

    const std::string& obj = ctx.objectBeforeAccessor;
    const std::string& op = ctx.accessorOp;
    const std::string& pfx = ctx.currentWordPrefix;

    auto addIfMatches = [&](const std::string& methodCall,
                            const std::string& nextLine,
                            double conf,
                            const std::string& title,
                            const std::string& hint,
                            const std::string& explanation,
                            const std::string& complexity,
                            const std::string& pitfall) {
        if (!pfx.empty() && methodCall.rfind(pfx, 0) != 0) {
            return;
        }
        std::string suffix = methodCall.substr(pfx.size());
        if (suffix.empty()) return;

        PredictionCandidate c;
        c.triggerPrefix = obj + op + pfx;
        c.completionSuffix = suffix;
        c.fullSuggestion = obj + op + methodCall;
        c.nextLinePreview = nextLine;
        c.confidence = conf;
        c.sourceEngine = "TypeMember";
        c.conceptTitle = title;
        c.progressiveHint = hint;
        c.detailedExplanation = explanation;
        c.complexityNote = complexity;
        c.commonPitfall = pitfall;
        out.push_back(std::move(c));
    };

    // Case 1: `std::` namespace completions
    if (obj == "std" && op == "::") {
        std::string vecName = findBestVarOfType(ctx, "vector", "v");
        addIfMatches(
            "sort(" + vecName + ".begin(), " + vecName + ".end());",
            "for (const auto& x : " + vecName + ") std::cout << x << \" \";",
            0.95,
            "STL Sort (`std::sort`)",
            "Pass `" + vecName + ".begin()` and `" + vecName + ".end()` to sort `" + vecName + "` in ascending order.",
            "`std::sort` sorts the range `[" + vecName + ".begin(), " + vecName + ".end())` in-place using Introsort (QuickSort + HeapSort + InsertionSort). Pass a lambda `[](int a, int b){ return a > b; }` as a 3rd argument for custom ordering.",
            "O(N log N) time, O(log N) auxiliary stack space",
            "Make sure `<algorithm>` is included at the top of your file."
        );
        addIfMatches(
            "cout << ",
            "return 0;",
            0.93,
            "Standard Output (`std::cout`)",
            "Use `std::cout << value << '\\n';` to print output to the terminal.",
            "`std::cout` is the buffered standard output stream in `<iostream>`. Use `<<` to send variables, literals, and expressions to the console.",
            "O(output length)",
            "Use `'\\n'` instead of `std::endl` in performance-critical loops to avoid costly buffer flushes."
        );
        addIfMatches(
            "vector<int> nums = {};",
            "nums.push_back(1);",
            0.91,
            "STL Dynamic Array (`std::vector`)",
            "Declare a resizable contiguous array `std::vector<T>`.",
            "`std::vector` automatically manages heap memory and grows its capacity geometrically (1.5x or 2x) when `.push_back()` exceeds capacity.",
            "O(1) random access, O(1) amortized insertion at end",
            "Remember `#include <vector>`."
        );
        addIfMatches(
            "unordered_map<int, int> freq;",
            "for (int x : " + vecName + ") freq[x]++;",
            0.89,
            "Hash Table Map (`std::unordered_map`)",
            "Use `std::unordered_map<Key, Value>` for O(1) average key lookups and counting.",
            "`std::unordered_map` hashes keys into buckets for fast constant-time access, unlike `std::map` which uses a Red-Black Tree in O(log N).",
            "O(1) average lookup/insert, O(N) space",
            "Remember `#include <unordered_map>`."
        );
        addIfMatches(
            "binary_search(" + vecName + ".begin(), " + vecName + ".end(), target)",
            "",
            0.88,
            "STL Binary Search (`std::binary_search`)",
            "Check if `target` exists in sorted `" + vecName + "` in O(log N) time.",
            "`std::binary_search` halves the search space at each step to test membership in a sorted sequence. Use `std::lower_bound` if you need the index of the match.",
            "O(log N) time, O(1) space",
            "`" + vecName + "` MUST be sorted before calling `std::binary_search`!"
        );
        addIfMatches(
            "make_unique<int>(42);",
            "",
            0.86,
            "Heap Allocation with Smart Pointer (`std::make_unique`)",
            "Create a leak-free `std::unique_ptr` without writing raw `new` or `delete`.",
            "`std::make_unique<T>(args...)` allocates a `T` on the heap and wraps it in a `std::unique_ptr<T>` using RAII so memory is automatically freed when the scope ends.",
            "O(1) allocation, zero reference-counting overhead",
            "Requires C++14 or newer and `#include <memory>`."
        );
        return out;
    }

    // Case 2: Look up `obj` in discovered in-scope variables
    auto it = ctx.variables.find(obj);
    if (it == ctx.variables.end()) {
        return out;
    }

    const std::string& base = it->second.baseType;

    if (base == "vector" || base == "deque") {
        addIfMatches(
            "push_back(val);",
            "std::cout << " + obj + ".size() << std::endl;",
            0.96,
            "Vector Append (`.push_back()`)",
            "Append an element to the back of `" + obj + "` (`" + it->second.type + "`).",
            "`" + obj + ".push_back(val)` copies or moves `val` to the end of the vector, automatically doubling capacity if full. Use `.emplace_back(args...)` to construct the element in-place without a temporary.",
            "Amortized O(1) time; O(N) only when reallocation occurs",
            "Reallocation invalidates any existing iterators or pointers into `" + obj + "`."
        );
        addIfMatches(
            "size()",
            "",
            0.93,
            "Container Length (`.size()`)",
            "Returns the number of elements currently stored in `" + obj + "`.",
            "`" + obj + ".size()` returns an unsigned `size_t` count of elements (`end() - begin()`).",
            "O(1) constant time",
            "`.size()` returns an unsigned `size_t`! Avoid `i < " + obj + ".size() - 1` when `" + obj + "` might be empty (0u - 1 underflows to `SIZE_MAX`)."
        );
        addIfMatches(
            "empty()",
            "",
            0.91,
            "Empty Check (`.empty()`)",
            "Returns `true` if `" + obj + "` contains 0 elements.",
            "Prefer `!" + obj + ".empty()` over `" + obj + ".size() > 0` because `.empty()` is guaranteed O(1) across all STL containers and expresses intent clearly.",
            "O(1) constant time",
            "Always check `!" + obj + ".empty()` before calling `" + obj + ".back()` or `" + obj + ".front()`."
        );
        addIfMatches(
            "begin(), " + obj + ".end()",
            "",
            0.90,
            "Half-Open Iterator Range (`.begin(), .end()`)",
            "Pass `" + obj + ".begin(), " + obj + ".end()` to STL algorithms like `std::sort` or `std::reverse`.",
            "STL algorithms operate on half-open iterator intervals `[begin, end)`, where `.begin()` points to the first element and `.end()` points one past the last element.",
            "O(1) iterator acquisition",
            "Never dereference `" + obj + ".end()` — it points one past the last valid element!"
        );
        addIfMatches(
            "pop_back();",
            "",
            0.87,
            "Remove Last Element (`.pop_back()`)",
            "Removes the last element of `" + obj + "` in O(1) time.",
            "`" + obj + ".pop_back()` destroys the last element and decrements `.size()` by 1 without shrinking allocated capacity.",
            "O(1) constant time",
            "Calling `.pop_back()` on an empty vector is undefined behavior!"
        );
        addIfMatches(
            "clear();",
            "",
            0.84,
            "Clear All Elements (`.clear()`)",
            "Removes all elements from `" + obj + "` so `.size()` becomes 0.",
            "`" + obj + ".clear()` destroys all elements in linear time while keeping the underlying heap buffer allocated for reuse.",
            "O(N) element destruction",
            "Capacity remains unchanged after `.clear()`; call `.shrink_to_fit()` if you need to release memory."
        );
    } else if (base == "map" || base == "unordered_map") {
        addIfMatches(
            "find(key) != " + obj + ".end()",
            "",
            0.96,
            "Key Lookup Without Insertion (`.find()`)",
            "Check if `key` exists in `" + obj + "` without accidentally inserting a default entry.",
            "`" + obj + ".find(key)` searches for `key` and returns an iterator to the key-value pair (`it->first`, `it->second`), or `" + obj + ".end()` if not found.",
            base == "unordered_map" ? "O(1) average time" : "O(log N) tree search time",
            "Using `" + obj + "[key]` to test existence silently inserts `key` with a default value, bloating the map!"
        );
        addIfMatches(
            "insert({key, val});",
            "",
            0.92,
            "Insert Key-Value Pair (`.insert()`)",
            "Insert `{key, val}` into `" + obj + "` if `key` is not already present.",
            "`" + obj + ".insert({key, val})` or `" + obj + "[key] = val;` stores a mapping in the associative container.",
            base == "unordered_map" ? "O(1) average time" : "O(log N) time",
            "Note that `.insert({k, v})` does NOT overwrite an existing key; use `" + obj + "[k] = v` if you want to overwrite."
        );
        addIfMatches(
            "count(key) > 0",
            "",
            0.90,
            "Key Existence Check (`.count()`)",
            "Returns `1` if `key` is in `" + obj + "`, or `0` otherwise.",
            "In `std::map` and `std::unordered_map`, keys are unique, so `.count(key)` always returns `0` or `1` (in C++20, you can also use `.contains(key)`).",
            base == "unordered_map" ? "O(1) average time" : "O(log N) time",
            "Unlike `operator[]`, `.count(key)` never mutates the map."
        );
        addIfMatches(
            "erase(key);",
            "",
            0.86,
            "Remove Key (`.erase()`)",
            "Removes `key` and its associated value from `" + obj + "`.",
            "`" + obj + ".erase(key)` removes the entry matching `key` and returns the number of elements removed (0 or 1).",
            base == "unordered_map" ? "O(1) average time" : "O(log N) time",
            "Erasing by iterator while looping requires `it = " + obj + ".erase(it);`."
        );
    } else if (base == "set" || base == "unordered_set") {
        addIfMatches(
            "insert(val);",
            "",
            0.95,
            "Set Insertion (`.insert()`)",
            "Insert a unique value into `" + obj + "`.",
            "`" + obj + ".insert(val)` adds `val` if it isn't already present, automatically deduplicating elements.",
            base == "unordered_set" ? "O(1) average time" : "O(log N) time",
            "Returns `std::pair<iterator, bool>` where `.second` is `true` if a new element was inserted."
        );
        addIfMatches(
            "find(val) != " + obj + ".end()",
            "",
            0.93,
            "Set Membership Check (`.find()`)",
            "Check if `val` is present in `" + obj + "`.",
            "Compares the returned iterator against `" + obj + ".end()` to verify membership.",
            base == "unordered_set" ? "O(1) average time" : "O(log N) time",
            "Always prefer `" + obj + ".find(val)` over `std::find(" + obj + ".begin(), " + obj + ".end(), val)` (which is O(N))!"
        );
    } else if (base == "string") {
        addIfMatches(
            "substr(pos, len)",
            "",
            0.95,
            "Substring Extraction (`.substr()`)",
            "Extract a substring starting at index `pos` of length `len` (not end index!).",
            "`" + obj + ".substr(pos, len)` returns a new `std::string` starting at `pos` and taking up to `len` characters.",
            "O(len) time and space",
            "In C++, the 2nd argument to `.substr(pos, len)` is the LENGTH, not the ending index (unlike Python/Java)!"
        );
        addIfMatches(
            "find(pattern) != std::string::npos",
            "",
            0.93,
            "Substring Search (`.find()`)",
            "Search for `pattern` in `" + obj + "` and compare against `std::string::npos`.",
            "`" + obj + ".find(pattern)` returns the 0-based starting index of the first match, or `std::string::npos` if not found.",
            "O(N * M) worst-case, O(N) average",
            "Never check `if (" + obj + ".find(pat))` as a boolean — index `0` is falsy! Always compare `!= std::string::npos`."
        );
        addIfMatches(
            "length()",
            "",
            0.90,
            "String Length (`.length()` / `.size()`)",
            "Returns the number of characters in `" + obj + "`.",
            "`.length()` and `.size()` are identical on `std::string` and return the byte count in O(1) time.",
            "O(1) constant time",
            "Returns unsigned `size_t`."
        );
    } else if (base == "stack" || base == "queue" || base == "priority_queue") {
        std::string accessMethod = (base == "queue") ? "front()" : "top()";
        addIfMatches(
            "push(val);",
            "",
            0.95,
            "Push Element (`.push()`)",
            "Push `val` into `" + obj + "` (`" + it->second.type + "`).",
            base == "priority_queue"
                ? "Inserts `val` into the binary heap and bubbles it up to preserve max-heap order."
                : "Pushes `val` onto the container adapter.",
            base == "priority_queue" ? "O(log N) heap insertion" : "O(1) amortized time",
            "Remember to check `!" + obj + ".empty()` before popping."
        );
        addIfMatches(
            accessMethod,
            obj + ".pop();",
            0.93,
            "Peek Next Element (`." + accessMethod + "`)",
            "Inspect the next element of `" + obj + "` before calling `" + obj + ".pop()`.",
            "In C++ STL, `.pop()` returns `void` (for exception safety), so you must read `" + obj + "." + accessMethod + "` first and then call `" + obj + ".pop();`.",
            "O(1) constant time",
            "`std::queue` uses `.front()`, whereas `std::stack` and `std::priority_queue` use `.top()`!"
        );
        addIfMatches(
            "pop();",
            "",
            0.91,
            "Remove Next Element (`.pop()`)",
            "Removes the top/front element from `" + obj + "` (returns `void`).",
            "`" + obj + ".pop()` removes the element without returning it. Save `" + obj + "." + accessMethod + "` in a variable before calling `.pop()`.",
            base == "priority_queue" ? "O(log N) heapify" : "O(1) constant time",
            "Calling `.pop()` on an empty adapter causes undefined behavior."
        );
    }

    return out;
}

std::vector<PredictionCandidate> KnowledgeBase::matchLineSyntaxRules(const ScopeContext& ctx) {
    std::vector<PredictionCandidate> out;
    const std::string& trimmed = ctx.trimmedBeforeCursor;
    if (trimmed.empty()) {
        return out;
    }

    std::string vecVar = findBestVarOfType(ctx, "vector", "nums");
    std::string mapVar = findBestVarOfType(ctx, "unordered_map", findBestVarOfType(ctx, "map", "mp"));
    std::string strVar = findBestVarOfType(ctx, "string", "s");

    auto tryRule = [&](const std::string& prefixPattern,
                       const std::string& fullCompletedLine,
                       const std::string& nextLine,
                       double conf,
                       const std::string& title,
                       const std::string& hint,
                       const std::string& explanation,
                       const std::string& complexity,
                       const std::string& pitfall) {
        if (trimmed.rfind(prefixPattern, 0) == 0 && fullCompletedLine.rfind(trimmed, 0) == 0) {
            std::string suffix = fullCompletedLine.substr(trimmed.size());
            if (suffix.empty()) return;
            PredictionCandidate c;
            c.triggerPrefix = trimmed;
            c.completionSuffix = suffix;
            c.fullSuggestion = fullCompletedLine;
            c.nextLinePreview = nextLine;
            c.confidence = conf;
            c.sourceEngine = "ContextRule";
            c.conceptTitle = title;
            c.progressiveHint = hint;
            c.detailedExplanation = explanation;
            c.complexityNote = complexity;
            c.commonPitfall = pitfall;
            out.push_back(std::move(c));
        }
    };

    // 1. Incomplete `for` loop variations (tailored to discovered vector variable in scope!)
    std::string boundExpr = (ctx.variables.count(vecVar) > 0)
        ? "static_cast<int>(" + vecVar + ".size())"
        : "n";
    std::string bodyPreview = (ctx.variables.count(vecVar) > 0)
        ? "    std::cout << " + vecVar + "[i] << \" \";"
        : "    ";

    tryRule(
        "for (",
        "for (int i = 0; i < " + boundExpr + "; ++i) {",
        bodyPreview,
        0.96,
        "Indexed For-Loop over `" + vecVar + "`",
        "Complete the 3 loop clauses: `int i = 0; i < " + boundExpr + "; ++i) {`.",
        "Iterates index `i` from `0` up to `" + boundExpr + " - 1`. Using `++i` increments `i` after each pass of the loop body.",
        "O(N) linear traversal",
        "Casting `.size()` to `int` (or using `size_t i = 0`) avoids signed/unsigned comparison warnings (`-Wsign-compare`)."
    );

    tryRule(
        "for (",
        "for (const auto& item : " + vecVar + ") {",
        "    std::cout << item << \" \";",
        0.94,
        "Range-Based For-Loop over `" + vecVar + "`",
        "Or use a range-based loop `for (const auto& item : " + vecVar + ")` for clean, bounds-safe iteration.",
        "Range-based `for` automatically iterates from `" + vecVar + ".begin()` to `" + vecVar + ".end()`. Binding as `const auto&` avoids copying elements.",
        "O(N) linear traversal, O(1) space",
        "Use `auto&` (without `const`) if you want to modify elements inside `" + vecVar + "`."
    );

    // 2. Incomplete `std::sort` or `sort`
    tryRule(
        "std::sort(",
        "std::sort(" + vecVar + ".begin(), " + vecVar + ".end());",
        "for (const auto& x : " + vecVar + ") std::cout << x << \" \";",
        0.97,
        "Sorting `" + vecVar + "` with `std::sort`",
        "Pass the iterator range `" + vecVar + ".begin(), " + vecVar + ".end()` to `std::sort`.",
        "`std::sort` rearranges elements in `[" + vecVar + ".begin(), " + vecVar + ".end())` in ascending order using Introsort. For descending order, pass `std::greater<int>()` as a 3rd argument.",
        "O(N log N) time, O(log N) stack space",
        "Requires `#include <algorithm>`."
    );
    tryRule(
        "sort(",
        "sort(" + vecVar + ".begin(), " + vecVar + ".end());",
        "",
        0.95,
        "Sorting `" + vecVar + "` (`sort`)",
        "Pass `" + vecVar + ".begin(), " + vecVar + ".end()` to sort the container in-place.",
        "Sorts `" + vecVar + "` in ascending order in O(N log N) comparisons.",
        "O(N log N) time",
        "Prefer qualifying with `std::sort` when not using `using namespace std;`."
    );

    // 3. Incomplete `std::reverse`
    tryRule(
        "std::reverse(",
        "std::reverse(" + vecVar + ".begin(), " + vecVar + ".end());",
        "",
        0.95,
        "Reversing `" + vecVar + "` In-Place (`std::reverse`)",
        "Pass `" + vecVar + ".begin(), " + vecVar + ".end()` to reverse elements in-place.",
        "`std::reverse` swaps pairs of elements from both ends toward the middle until the range is reversed.",
        "O(N) time (exactly N/2 swaps), O(1) extra space",
        "Requires `#include <algorithm>`."
    );

    // 4. Incomplete `std::binary_search` or `std::lower_bound`
    tryRule(
        "std::binary_search(",
        "std::binary_search(" + vecVar + ".begin(), " + vecVar + ".end(), target);",
        "",
        0.95,
        "Binary Search on `" + vecVar + "`",
        "Pass `" + vecVar + ".begin(), " + vecVar + ".end(), target` to search in O(log N).",
        "Checks whether `target` exists in the sorted range `[" + vecVar + ".begin(), " + vecVar + ".end())`.",
        "O(log N) time, O(1) space",
        "Ensure `" + vecVar + "` is sorted before calling `std::binary_search`!"
    );

    // 5. Incomplete `if (` with context-aware condition
    if (ctx.variables.count(mapVar) > 0) {
        tryRule(
            "if (",
            "if (" + mapVar + ".find(key) != " + mapVar + ".end()) {",
            "    std::cout << " + mapVar + "[key] << std::endl;",
            0.93,
            "Map Key Lookup Guard (`if (" + mapVar + ".find(...) != .end())`)",
            "Check if a key exists in `" + mapVar + "` using `.find(key) != " + mapVar + ".end()`.",
            "Using `.find(key)` checks whether `key` is present in `" + mapVar + "` without inserting a default-constructed element.",
            "O(1) average for unordered_map, O(log N) for map",
            "Avoid `if (" + mapVar + "[key])` if you don't want missing keys to be added to `" + mapVar + "`."
        );
    }
    if (ctx.variables.count(vecVar) > 0) {
        tryRule(
            "if (",
            "if (!" + vecVar + ".empty()) {",
            "    std::cout << " + vecVar + ".front() << std::endl;",
            0.91,
            "Non-Empty Container Guard (`if (!" + vecVar + ".empty())`)",
            "Guard element access by checking `!" + vecVar + ".empty()` first.",
            "Verifies that `" + vecVar + "` has at least one element before accessing `.front()`, `.back()`, or `" + vecVar + "[0]`.",
            "O(1) time",
            "Accessing `" + vecVar + "[0]` on an empty vector is undefined behavior."
        );
    }

    // 6. Incomplete `while (`
    std::string qVar = findBestVarOfType(ctx, "queue", findBestVarOfType(ctx, "priority_queue", findBestVarOfType(ctx, "stack", "")));
    if (!qVar.empty()) {
        std::string peekFn = (ctx.variables.at(qVar).baseType == "queue") ? "front()" : "top()";
        tryRule(
            "while (",
            "while (!" + qVar + ".empty()) {",
            "    auto curr = " + qVar + "." + peekFn + "; " + qVar + ".pop();",
            0.95,
            "Queue / Stack Processing Loop (`while (!" + qVar + ".empty())`)",
            "Process elements from `" + qVar + "` until it becomes empty.",
            "Standard BFS / worklist loop pattern: keep iterating while `!" + qVar + ".empty()`, inspect `." + peekFn + "`, and call `.pop()`.",
            "O(V + E) in graph traversal or O(N) total pops",
            "Don't forget `" + qVar + ".pop();` inside the loop body, or the loop will run forever!"
        );
    } else {
        tryRule(
            "while (",
            "while (left < right) {",
            "    int mid = left + (right - left) / 2;",
            0.90,
            "Two-Pointer / Binary Search Loop (`while (left < right)`)",
            "Loop while the `left` and `right` pointers have not crossed.",
            "Used for two-pointer algorithms (palindrome check, sorted two-sum, container with most water) and binary search.",
            "O(N) for two pointers, O(log N) for binary search",
            "Compute midpoint as `left + (right - left) / 2` to prevent `(left + right)` integer overflow!"
        );
    }

    // 7. Incomplete `std::cout`
    tryRule(
        "std::cout",
        "std::cout << " + vecVar + "[0] << std::endl;",
        "",
        0.89,
        "Printing with `std::cout`",
        "Use `<<` to stream values to `std::cout`, ending with `std::endl` or `'\\n'`.",
        "Outputs the value to standard output.",
        "O(1) amortized I/O",
        "Make sure `<iostream>` is included."
    );

    return out;
}

std::vector<PredictionCandidate> KnowledgeBase::matchCommentIntent(const ScopeContext& ctx) {
    std::vector<PredictionCandidate> out;
    if (ctx.recentComment.empty()) {
        return out;
    }

    std::string commentLower = ContextAnalyzer::toLower(ctx.recentComment);
    bool onCommentLineItself = (ctx.trimmedBeforeCursor.rfind("//", 0) == 0);
    if (onCommentLineItself) {
        // Don't append code onto the end of a `// comment` line itself; wait until user is on the line below,
        // OR provide it as a nextLinePreview!
    }

    std::string vecVar = findBestVarOfType(ctx, "vector", "nums");
    std::string mapVar = findBestVarOfType(ctx, "unordered_map", findBestVarOfType(ctx, "map", "freq"));
    std::string strVar = findBestVarOfType(ctx, "string", "s");

    auto emitIntent = [&](const std::string& targetCode,
                          const std::string& nextLine,
                          const std::string& title,
                          const std::string& hint,
                          const std::string& explanation,
                          const std::string& complexity,
                          const std::string& pitfall) {
        if (onCommentLineItself) {
            PredictionCandidate c;
            c.triggerPrefix = ctx.trimmedBeforeCursor;
            c.completionSuffix = "";
            c.fullSuggestion = targetCode;
            c.nextLinePreview = targetCode;
            c.confidence = 0.94;
            c.sourceEngine = "IntentComment";
            c.conceptTitle = title;
            c.progressiveHint = hint + " (Press Enter to start the next line and Tab to insert!)";
            c.detailedExplanation = explanation;
            c.complexityNote = complexity;
            c.commonPitfall = pitfall;
            out.push_back(std::move(c));
            return;
        }

        // User is on the code line below the comment (either empty or partially typed)
        if (!ctx.trimmedBeforeCursor.empty() && targetCode.rfind(ctx.trimmedBeforeCursor, 0) != 0) {
            return;
        }
        std::string suffix = targetCode.substr(ctx.trimmedBeforeCursor.size());
        if (suffix.empty()) return;

        PredictionCandidate c;
        c.triggerPrefix = ctx.trimmedBeforeCursor;
        c.completionSuffix = suffix;
        c.fullSuggestion = targetCode;
        c.nextLinePreview = nextLine;
        c.confidence = 0.98;
        c.sourceEngine = "IntentComment";
        c.conceptTitle = title;
        c.progressiveHint = hint;
        c.detailedExplanation = explanation;
        c.complexityNote = complexity;
        c.commonPitfall = pitfall;
        out.push_back(std::move(c));
    };

    if (commentLower.find("sort") != std::string::npos) {
        if (commentLower.find("desc") != std::string::npos || commentLower.find("reverse") != std::string::npos) {
            emitIntent(
                "std::sort(" + vecVar + ".begin(), " + vecVar + ".end(), std::greater<int>());",
                "for (int x : " + vecVar + ") std::cout << x << \" \";",
                "Descending Sort (`std::sort` + `std::greater`)",
                "To sort `" + vecVar + "` in descending order, pass `std::greater<int>()` as the 3rd argument to `std::sort`.",
                "`std::sort(first, last, comp)` uses a custom comparator functor or lambda. `std::greater<int>()` compares `a > b` so larger elements come first.",
                "O(N log N) time, O(log N) stack space",
                "You can also use reverse iterators: `std::sort(" + vecVar + ".rbegin(), " + vecVar + ".rend());`."
            );
        } else {
            emitIntent(
                "std::sort(" + vecVar + ".begin(), " + vecVar + ".end());",
                "for (int x : " + vecVar + ") std::cout << x << \" \";",
                "Ascending Sort (`std::sort`)",
                "Use `std::sort(" + vecVar + ".begin(), " + vecVar + ".end());` from `<algorithm>` to sort `" + vecVar + "`.",
                "`std::sort` sorts `" + vecVar + "` in-place in ascending order using Introsort (a hybrid of QuickSort, HeapSort, and InsertionSort).",
                "O(N log N) time, O(log N) stack space",
                "Don't forget `#include <algorithm>` at the top of the file."
            );
        }
    }

    if (commentLower.find("binary search") != std::string::npos || commentLower.find("bsearch") != std::string::npos) {
        emitIntent(
            "auto it = std::lower_bound(" + vecVar + ".begin(), " + vecVar + ".end(), target);",
            "if (it != " + vecVar + ".end() && *it == target) { int idx = it - " + vecVar + ".begin(); }",
            "Binary Search with Index (`std::lower_bound`)",
            "Use `std::lower_bound` on sorted `" + vecVar + "` to find both existence and index in O(log N).",
            "`std::lower_bound` returns an iterator to the first element `>= target`. Subtracting `" + vecVar + ".begin()` gives the 0-based index, and checking `*it == target` confirms an exact match!",
            "O(log N) time, O(1) space",
            "`" + vecVar + "` MUST be sorted before running `std::lower_bound` or `std::binary_search`."
        );
    }

    if (commentLower.find("reverse") != std::string::npos && commentLower.find("sort") == std::string::npos) {
        std::string targetContainer = (ctx.variables.count(strVar) > 0 && commentLower.find("string") != std::string::npos) ? strVar : vecVar;
        emitIntent(
            "std::reverse(" + targetContainer + ".begin(), " + targetContainer + ".end());",
            "",
            "In-Place Reversal (`std::reverse`)",
            "Call `std::reverse(" + targetContainer + ".begin(), " + targetContainer + ".end());` to reverse elements in-place.",
            "`std::reverse` swaps elements symmetrically from `.begin()` and `.end() - 1` toward the center.",
            "O(N) time, O(1) auxiliary space",
            "Works on both `std::vector` and `std::string`."
        );
    }

    if (commentLower.find("print") != std::string::npos || commentLower.find("output") != std::string::npos ||
        commentLower.find("display") != std::string::npos) {
        emitIntent(
            "for (const auto& item : " + vecVar + ") { std::cout << item << \" \"; }\nstd::cout << \"\\n\";",
            "",
            "Print Container Elements (Range-Based Loop)",
            "Iterate over `" + vecVar + "` with `for (const auto& item : " + vecVar + ")` and stream each item to `std::cout`.",
            "C++ does not have a built-in `operator<<` for `std::vector`, so printing a vector requires iterating over its elements with a range-based `for` loop.",
            "O(N) time",
            "Use `const auto&` so large element types (like `std::string`) aren't copied on every iteration."
        );
    }

    if (commentLower.find("freq") != std::string::npos || commentLower.find("count") != std::string::npos ||
        commentLower.find("hash") != std::string::npos) {
        emitIntent(
            "std::unordered_map<int, int> " + mapVar + ";\nfor (int x : " + vecVar + ") { " + mapVar + "[x]++; }",
            "",
            "Frequency Map Counting (`std::unordered_map`)",
            "Use `std::unordered_map` and `freq[x]++` to count occurrences of each element in O(N) time.",
            "When `freq[x]` is accessed for a key `x` not yet in the map, `std::unordered_map` zero-initializes the `int` value to `0` and then `++` increments it to `1`.",
            "O(N) average time, O(U) space for U unique elements",
            "Requires `#include <unordered_map>`."
        );
    }

    if (commentLower.find("sum") != std::string::npos || commentLower.find("accumulate") != std::string::npos) {
        emitIntent(
            "long long total = std::accumulate(" + vecVar + ".begin(), " + vecVar + ".end(), 0LL);",
            "std::cout << total << std::endl;",
            "Summing a Range (`std::accumulate`)",
            "Use `std::accumulate(" + vecVar + ".begin(), " + vecVar + ".end(), 0LL)` from `<numeric>` to sum elements safely.",
            "`std::accumulate` folds the range `[" + vecVar + ".begin(), " + vecVar + ".end())` by adding each element to the initial value `0LL`.",
            "O(N) time, O(1) space",
            "Pass `0LL` (a `long long` literal) instead of `0` (32-bit `int`) as the 3rd argument to prevent 32-bit integer overflow on large sums!"
        );
    }

    if (commentLower.find("max") != std::string::npos || commentLower.find("min") != std::string::npos) {
        std::string fn = (commentLower.find("min") != std::string::npos) ? "min_element" : "max_element";
        emitIntent(
            "int best = *std::" + fn + "(" + vecVar + ".begin(), " + vecVar + ".end());",
            "std::cout << best << std::endl;",
            "Finding Extremum (`std::" + fn + "`)",
            "`std::" + fn + "` returns an ITERATOR to the extremum; dereference it with `*` to get the value!",
            "`std::" + fn + "(first, last)` scans the range in a single pass and returns an iterator pointing to the largest/smallest element.",
            "O(N) comparisons, O(1) space",
            "Never dereference `*std::" + fn + "(...)` if `" + vecVar + "` could be empty!"
        );
    }

    if (commentLower.find("file") != std::string::npos || commentLower.find("read") != std::string::npos) {
        emitIntent(
            "std::ifstream file(\"input.txt\");\nstd::string line;\nwhile (std::getline(file, line)) {\n    std::cout << line << \"\\n\";\n}",
            "",
            "Line-by-Line File Reading (`std::ifstream` + `std::getline`)",
            "Open the file with `std::ifstream` and loop with `while (std::getline(file, line))`.",
            "`std::getline(file, line)` reads an entire line (including spaces) up to `'\\n'` and converts to `false` automatically at EOF, making it the cleanest C++ file-reading idiom.",
            "O(file size) streaming I/O",
            "Requires `#include <fstream>` and `#include <string>`. `std::ifstream` closes the file automatically via RAII when it leaves scope."
        );
    }

    return out;
}

std::vector<PredictionCandidate> KnowledgeBase::matchStuckNextStep(const ScopeContext& ctx) {
    std::vector<PredictionCandidate> out;

    // Only suggest full next-line workflows when the current line is empty or very short
    const std::string& prev = ctx.previousNonEmptyLine;
    std::string vecVar = findBestVarOfType(ctx, "vector", "nums");
    std::string mapVar = findBestVarOfType(ctx, "unordered_map", findBestVarOfType(ctx, "map", "freq"));

    auto addNextStep = [&](const std::string& codeToInsert,
                           const std::string& followUp,
                           double conf,
                           const std::string& title,
                           const std::string& hint,
                           const std::string& explanation,
                           const std::string& complexity,
                           const std::string& pitfall) {
        if (!ctx.trimmedBeforeCursor.empty() && codeToInsert.rfind(ctx.trimmedBeforeCursor, 0) != 0) {
            return;
        }
        std::string suffix = codeToInsert.substr(ctx.trimmedBeforeCursor.size());
        if (suffix.empty()) return;

        PredictionCandidate c;
        c.triggerPrefix = ctx.trimmedBeforeCursor;
        c.completionSuffix = suffix;
        c.fullSuggestion = codeToInsert;
        c.nextLinePreview = followUp;
        c.confidence = conf;
        c.sourceEngine = "StuckCoach";
        c.conceptTitle = title;
        c.progressiveHint = hint;
        c.detailedExplanation = explanation;
        c.complexityNote = complexity;
        c.commonPitfall = pitfall;
        out.push_back(std::move(c));
    };

    // Case A: User just declared or initialized a std::vector on the previous line!
    if (prev.find("vector<") != std::string::npos && ctx.variables.count(vecVar) > 0) {
        addNextStep(
            "std::sort(" + vecVar + ".begin(), " + vecVar + ".end());",
            "for (const auto& x : " + vecVar + ") std::cout << x << \" \";",
            0.91,
            "Next Step: Sort or Traverse `" + vecVar + "`",
            "Stuck after creating `" + vecVar + "`? You can sort it with `std::sort(" + vecVar + ".begin(), " + vecVar + ".end());` or iterate through it.",
            "After initializing a `std::vector` (`" + vecVar + "`), the most common operations are sorting it with `std::sort`, iterating over it with a range-based `for` loop, or appending values with `" + vecVar + ".push_back(...)`.",
            "O(N log N) to sort, O(N) to traverse",
            "Include `<algorithm>` before calling `std::sort`."
        );
        addNextStep(
            "for (const auto& item : " + vecVar + ") {\n    std::cout << item << \" \";\n}",
            "std::cout << std::endl;",
            0.89,
            "Next Step: Traverse `" + vecVar + "` with Range-Based For Loop",
            "Want to inspect or process each element of `" + vecVar + "`? Use `for (const auto& item : " + vecVar + ")`.",
            "Range-based `for` loops visit every element in `" + vecVar + "` from `.begin()` to `.end()` without manual index bookkeeping.",
            "O(N) time, O(1) space",
            "Use `auto&` instead of `const auto&` if you want to modify elements in place."
        );
    }

    // Case B: User just opened a `for` loop `{` on the previous line!
    if (prev.rfind("for", 0) == 0 && prev.find('{') != std::string::npos) {
        if (prev.find("int i") != std::string::npos && ctx.variables.count(vecVar) > 0) {
            addNextStep(
                "std::cout << " + vecVar + "[i] << \" \";",
                "}",
                0.92,
                "Inside Indexed Loop: Access `" + vecVar + "[i]`",
                "Inside your `for (int i = ...)` loop, access the current element via `" + vecVar + "[i]`.",
                "Since `i` ranges from `0` to `" + vecVar + ".size() - 1`, `" + vecVar + "[i]` accesses the `i`-th element in O(1) contiguous memory.",
                "O(1) array indexing per iteration",
                "Use `" + vecVar + ".at(i)` during debugging if you want automatic bounds-checking (`std::out_of_range`)."
            );
        } else {
            addNextStep(
                "std::cout << item << \" \";",
                "}",
                0.88,
                "Inside Loop Body: Process Current Element",
                "Print or accumulate the current loop variable inside the loop body.",
                "Perform your per-element logic here before the closing brace `}` advances to the next iteration.",
                "O(1) per iteration",
                "Variables declared inside the loop body are scoped to a single iteration."
            );
        }
    }

    // Case C: User just declared an `unordered_map` or `map` on the previous line!
    if ((prev.find("unordered_map<") != std::string::npos || prev.find("map<") != std::string::npos) &&
        ctx.variables.count(mapVar) > 0) {
        addNextStep(
            "for (const auto& x : " + vecVar + ") {\n    " + mapVar + "[x]++;\n}",
            "",
            0.91,
            "Next Step: Populate `" + mapVar + "` Frequency Table",
            "Populate `" + mapVar + "` by looping through `" + vecVar + "` and incrementing `" + mapVar + "[x]++`.",
            "`" + mapVar + "[x]++` looks up key `x` in the map (inserting `0` if missing) and increments its count by 1.",
            "O(N) average time for unordered_map",
            "Iterate over `" + mapVar + "` afterward using `for (const auto& [key, count] : " + mapVar + ")` (C++17 structured binding)."
        );
    }

    // Case D: User just opened `int main() {`!
    if (prev.find("int main") != std::string::npos && prev.find('{') != std::string::npos) {
        addNextStep(
            "std::vector<int> nums = {5, 2, 8, 1, 9};",
            "std::sort(nums.begin(), nums.end());",
            0.88,
            "Starter Pattern Inside `main()`",
            "Inside `main()`, start by declaring your input variables or data structures (e.g., `std::vector<int> nums`).",
            "`main()` is where program execution begins. Typically you set up data structures, run your algorithm, print results with `std::cout`, and `return 0;`.",
            "O(N) initialization",
            "Remember to end `main()` with `return 0;`."
        );
    }

    // Case E: User is at the top of an empty file or after `#include` headers
    if (!ctx.insideMain && !ctx.insideClass && ctx.braceDepth == 0 &&
        (prev.empty() || prev.rfind("#include", 0) == 0 || prev.rfind("using ", 0) == 0)) {
        addNextStep(
            "int main() {\n    std::vector<int> nums = {5, 2, 8, 1, 9};\n    return 0;\n}",
            "",
            0.90,
            "Create `int main()` Entry Point",
            "After your `#include` headers, define `int main() { ... }` to start writing executable code.",
            "In C++, statements cannot sit at global file scope outside a function. Define `int main()` as the entry point of your program.",
            "O(1) entry point setup",
            "Make sure every opening `{` has a matching `}`."
        );
    }

    // Case F: User is inside `main()` near the end after printing output
    if (ctx.insideMain && prev.find("std::cout") != std::string::npos) {
        addNextStep(
            "return 0;",
            "}",
            0.85,
            "Finish `main()` with `return 0;`",
            "Done printing output? Return `0` from `main()` to indicate success.",
            "Returning `0` from `int main()` tells the operating system shell that the process exited cleanly without errors.",
            "O(1) exit",
            "Non-zero return codes (like `return 1;`) indicate an error to the OS."
        );
    }

    return out;
}

} // namespace autocode
