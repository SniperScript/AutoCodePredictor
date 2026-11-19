#include "rules.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <optional>
#include <sstream>

// =====================================================================
// Data: vocabulary (verified: each keyword is rejected as a variable name
// by g++ -std=c++20; each library name compiles against the real headers)
// =====================================================================

namespace {

const char* const kKeywords =
    "alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t "
    "char32_t class compl concept const consteval constexpr constinit const_cast continue co_await "
    "co_return co_yield decltype default delete do double dynamic_cast else enum explicit export "
    "extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq "
    "nullptr operator or or_eq private protected public register reinterpret_cast requires return "
    "short signed sizeof static static_assert static_cast struct switch template this thread_local "
    "throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while "
    "xor xor_eq";

const char* const kLibrary =
    // C++ (std::)
    "std cout cin cerr endl getline string vector map unordered_map set unordered_set multimap "
    "multiset list deque queue priority_queue stack pair tuple array bitset unique_ptr shared_ptr "
    "weak_ptr make_unique make_shared make_pair make_tuple move swap sort stable_sort reverse find "
    "find_if count count_if accumulate min max min_element max_element lower_bound upper_bound "
    "binary_search fill transform for_each iota to_string stoi stod optional variant "
    // C library (also valid in C++)
    "printf scanf fprintf fscanf sprintf snprintf sscanf puts fgets fputs getchar putchar fopen "
    "fclose fread fwrite fseek ftell rewind feof malloc calloc realloc free memcpy memmove memset "
    "memcmp strlen strcpy strncpy strcat strncat strcmp strncmp strchr strrchr strstr strtok atoi "
    "atof atol strtol strtod abs labs rand srand qsort bsearch exit atexit system time clock sqrt "
    "pow fabs floor ceil sin cos tan log log10 exp isalpha isdigit isalnum isspace isupper islower "
    "toupper tolower assert";

// Words beginners type most often get a higher weight, so they come first.
const char* const kCommon =
    "int for if else return while const auto void bool double char struct class true false break "
    "continue std cout cin endl vector string sort max min swap getline";

// Real lessons for the most useful words (everything else gets a generic one).
struct WordLesson { const char* word; Lesson lesson; };
const WordLesson kWordLessons[] = {
    {"cout", {"Print with std::cout", "Send values to the screen with <<, e.g. std::cout << x;",
              "std::cout is the standard output stream; each << appends one value to the output.",
              "O(length of the output)", "Needs #include <iostream>. Prefer \"\\n\" over std::endl inside loops."}},
    {"cin", {"Read input with std::cin", "Read into a variable with >>, e.g. std::cin >> x;",
             "std::cin reads the next whitespace-separated value from the keyboard and converts it to the variable's type.",
             "O(length of the input)", "std::cin >> s stops at the first space; use std::getline for a whole line."}},
    {"endl", {"End the line (std::endl)", "Writes a newline and flushes the output.",
              "Flushing forces the text out immediately, which is slower than a plain \"\\n\".",
              "A flush per call", "In a loop, \"\\n\" is much faster than std::endl."}},
    {"getline", {"Read a whole line (std::getline)", "std::getline(std::cin, line) reads up to the Enter key.",
                 "Unlike >>, getline keeps the spaces inside the line.", "O(length of the line)",
                 "After std::cin >> x, call std::cin.ignore() first or getline reads an empty line."}},
    {"vector", {"Dynamic array (std::vector)", "Declare with std::vector<int> nums;",
                "A vector stores elements next to each other in memory and grows automatically.",
                "Index O(1), push_back amortized O(1)", "Needs #include <vector>. nums[i] does not check bounds; nums.at(i) does."}},
    {"string", {"Text (std::string)", "Declare with std::string name = \"text\";",
                "std::string manages its own memory and knows its length, unlike a char array.",
                "size O(1), concatenation O(length)", "Needs #include <string>. Compare with ==, not strcmp."}},
    {"map", {"Sorted key-value map (std::map)", "std::map<std::string, int> ages; ages[\"Ana\"] = 20;",
             "A balanced binary search tree (red-black tree): keys stay sorted.",
             "find/insert/erase O(log n)", "m[key] inserts the key if it's missing; use m.count(key) or m.find(key) to only check."}},
    {"unordered_map", {"Hash map (std::unordered_map)", "std::unordered_map<int, int> freq; freq[x]++;",
                       "A hash table: keys are spread over buckets for constant-time access, in no particular order.",
                       "find/insert/erase O(1) average, O(n) worst", "Iteration order is not sorted; use std::map if you need order."}},
    {"set", {"Sorted unique values (std::set)", "std::set<int> seen; seen.insert(x);",
             "A balanced binary search tree that keeps each value once, in sorted order.", "insert/find O(log n)",
             "Duplicates are silently ignored; use std::multiset to keep them."}},
    {"unordered_set", {"Hash set (std::unordered_set)", "std::unordered_set<int> seen; seen.count(x);",
                       "A hash table of unique values with constant-time membership tests.", "insert/count O(1) average",
                       "No ordering: printing it gives an arbitrary order."}},
    {"stack", {"Last in, first out (std::stack)", "push to add, top to look, pop to remove.",
               "A stack only exposes its top element: the last one pushed comes out first.", "push/pop/top O(1)",
               "pop() returns nothing; read top() before popping."}},
    {"queue", {"First in, first out (std::queue)", "push to join the back, front to look, pop to leave.",
               "A queue serves elements in arrival order, as in breadth-first search (BFS).", "push/pop/front O(1)",
               "pop() returns nothing; read front() first."}},
    {"priority_queue", {"Heap (std::priority_queue)", "push values; top() is always the largest.",
                        "A binary heap keeps the largest element on top.", "push/pop O(log n), top O(1)",
                        "For the smallest on top: std::priority_queue<int, std::vector<int>, std::greater<int>>."}},
    {"pair", {"Two values together (std::pair)", "std::pair<int, std::string> p = {1, \"a\"}; p.first, p.second",
              "A pair groups two values; maps store their entries as pairs.", "O(1)", "Needs #include <utility>."}},
    {"sort", {"Sort a range (std::sort)", "std::sort(v.begin(), v.end());",
              "std::sort uses introsort (quicksort + heapsort + insertion sort) to sort in place.",
              "O(n log n) time, O(log n) stack", "Needs #include <algorithm>. Pass std::greater<>() for descending order."}},
    {"reverse", {"Reverse a range (std::reverse)", "std::reverse(v.begin(), v.end());",
                 "Swaps the first and last element, then moves inward.", "O(n), exactly n/2 swaps", "Needs #include <algorithm>."}},
    {"find", {"Linear search (std::find)", "std::find(v.begin(), v.end(), x) returns an iterator.",
              "Checks elements one by one until it finds x; returns end() if it isn't there.", "O(n)",
              "For sets and maps use their own .find(): O(log n) or O(1) instead of O(n)."}},
    {"count", {"Count matches (std::count)", "std::count(v.begin(), v.end(), x)",
               "Visits every element and counts those equal to x.", "O(n)", "Needs #include <algorithm>."}},
    {"accumulate", {"Sum a range (std::accumulate)", "std::accumulate(v.begin(), v.end(), 0LL)",
                    "Adds every element to a starting value and returns the total.", "O(n)",
                    "Needs #include <numeric>. The starting value sets the result type: 0 sums in int."}},
    {"max", {"The larger of two (std::max)", "std::max(a, b)", "Returns the larger argument.", "O(1)",
             "Both arguments must have the same type: std::max(1, 2.5) does not compile."}},
    {"min", {"The smaller of two (std::min)", "std::min(a, b)", "Returns the smaller argument.", "O(1)",
             "Both arguments must have the same type."}},
    {"swap", {"Exchange two values (std::swap)", "std::swap(a, b);", "Swaps the contents of a and b.", "O(1) for numbers",
              "Swapping containers is O(1): it swaps their internals."}},
    {"lower_bound", {"Binary search (std::lower_bound)", "std::lower_bound(v.begin(), v.end(), x)",
                     "Returns the first position whose element is >= x, halving the range each step.", "O(log n)",
                     "The range must be sorted first."}},
    {"printf", {"C-style printing (printf)", "printf(\"%d\\n\", x);", "Prints using a format string: %d int, %f double, %s text.",
                "O(length of the output)", "Needs #include <cstdio>. A wrong % code is undefined behaviour."}},
    {"scanf", {"C-style input (scanf)", "scanf(\"%d\", &x);", "Reads formatted input into the addresses you pass.",
               "O(length of the input)", "Forgetting the & before the variable is a classic crash."}},
    {"strlen", {"Length of a C string (strlen)", "strlen(text)", "Counts characters up to the terminating '\\0'.",
                "O(n): it scans the whole string", "Calling it in a loop condition makes the loop O(n^2)."}},
    {"malloc", {"Allocate raw memory (malloc)", "int* a = (int*)malloc(n * sizeof(int));", "Reserves n bytes on the heap.",
                "O(1) amortized", "In C++ prefer std::vector; every malloc needs exactly one free."}},
    {"for", {"for loop", "for (int i = 0; i < n; ++i) { ... }", "Runs the body while the condition holds; the update runs after each pass.",
             "O(number of iterations)", "Off-by-one: i <= n runs n+1 times."}},
    {"while", {"while loop", "while (condition) { ... }", "Repeats the body as long as the condition is true.",
               "O(number of iterations)", "Make sure something in the body changes the condition, or it never ends."}},
    {"if", {"if statement", "if (condition) { ... } else { ... }", "Runs the block only when the condition is true.", "O(1)",
            "= assigns, == compares: if (x = 5) is always true."}},
    {"return", {"return", "return value;", "Ends the function and hands the value back to the caller.", "O(1)",
                "In main, return 0; means the program finished successfully."}},
    {"auto", {"auto", "auto x = expression;", "The compiler works out the type from the value.", "No runtime cost",
              "auto drops const and &: use const auto& in range loops to avoid copies."}},
    {"const", {"const", "const int limit = 10;", "A const value can't be changed after it is set.", "No runtime cost",
               "Pass big objects as const T& to avoid copying them."}},
    {"nullptr", {"nullptr", "int* p = nullptr;", "The null pointer: a pointer that points to nothing.", "O(1)",
                 "Dereferencing a null pointer crashes the program."}},
};

// =====================================================================
// Data: container functions, by type (the "members" table)
// =====================================================================

struct Member {
    const char* types;  // space-separated container types it applies to
    const char* text;   // what gets inserted after the dot
    Lesson lesson;
};

const Member kMembers[] = {
    {"vector deque string", "push_back(val)", {"Add to the end", "Appends val after the last element.",
        "The container grows automatically to make room.", "Amortized O(1)", "For a vector, this can invalidate iterators."}},
    {"vector deque string", "pop_back()", {"Remove the last element", "Deletes the last element.",
        "size() goes down by one.", "O(1)", "Undefined on an empty container: check empty() first."}},
    {"vector deque string map unordered_map set unordered_set stack queue priority_queue", "size()",
        {"Number of elements", "How many elements are stored.", "Containers keep a count, so this is instant.", "O(1)",
         "size() is unsigned: size() - 1 on an empty container is a huge number."}},
    {"vector deque string map unordered_map set unordered_set stack queue priority_queue", "empty()",
        {"Is it empty?", "true when there are no elements.", "Clearer than size() == 0.", "O(1)", ""}},
    {"vector deque string map unordered_map set unordered_set", "clear()",
        {"Remove everything", "Deletes all elements.", "size() becomes 0.", "O(n)", ""}},
    {"vector deque string", "back()", {"Last element", "The element at the end.", "Same as c[c.size() - 1].", "O(1)",
        "Undefined on an empty container."}},
    {"vector deque string", "front()", {"First element", "The element at the start.", "Same as c[0].", "O(1)",
        "Undefined on an empty container."}},
    {"vector deque string map unordered_map set unordered_set", "begin()", {"Iterator to the first element",
        "Where a range starts, e.g. std::sort(v.begin(), v.end()).", "Algorithms work on [begin, end) ranges.", "O(1)", ""}},
    {"vector deque string map unordered_map set unordered_set", "end()", {"Iterator past the last element",
        "Where a range stops.", "end() points one past the last element, not at it.", "O(1)", "Never dereference end()."}},
    {"vector", "insert(it, val)", {"Insert before a position", "Inserts val before the iterator it.",
        "Every element after it shifts one place right.", "O(n)", "Use push_back to add at the end: it's much faster."}},
    {"vector", "erase(it)", {"Remove at a position", "Removes the element at iterator it.",
        "Every element after it shifts one place left.", "O(n)", "Invalidates iterators at and after it."}},
    {"vector", "reserve(n)", {"Reserve capacity", "Makes room for n elements in advance.",
        "Avoids repeated reallocation when you know the final size.", "O(n)", "reserve doesn't change size(); resize does."}},
    {"string", "substr(pos, len)", {"Part of a string", "len characters starting at pos.",
        "Returns a new string; the original is unchanged.", "O(len)", "Throws std::out_of_range if pos > size()."}},
    {"string", "find(text)", {"Search inside a string", "Index of text, or std::string::npos.",
        "Scans for the first occurrence.", "O(n * m) worst case", "Compare with std::string::npos, not -1."}},
    {"string", "length()", {"Length of the string", "Same as size().", "Strings store their length.", "O(1)", ""}},
    {"map", "find(key)", {"Look up a key", "Iterator to key, or end() if missing.",
        "std::map is a balanced tree, so it halves the search space at each step.", "O(log n)",
        "m[key] would insert a missing key; find doesn't."}},
    {"unordered_map", "find(key)", {"Look up a key", "Iterator to key, or end() if missing.",
        "std::unordered_map hashes the key straight to its bucket.", "O(1) average, O(n) worst",
        "m[key] would insert a missing key; find doesn't."}},
    {"map", "count(key)", {"Is the key there?", "1 if key is present, else 0.", "Keys are unique, so it's 0 or 1.",
        "O(log n)", ""}},
    {"unordered_map", "count(key)", {"Is the key there?", "1 if key is present, else 0.", "Keys are unique, so it's 0 or 1.",
        "O(1) average", ""}},
    {"map unordered_map", "insert({key, val})", {"Add a key-value pair", "Adds key -> val if key isn't there yet.",
        "insert never overwrites an existing value.", "map O(log n), unordered_map O(1) average", "Use m[key] = val to overwrite."}},
    {"map unordered_map set unordered_set", "erase(key)", {"Remove a key", "Deletes key if present.",
        "Returns how many elements were removed (0 or 1).", "ordered O(log n), unordered O(1) average", ""}},
    {"set", "insert(val)", {"Add to the set", "Adds val if it isn't there.", "Keeps values sorted and unique (balanced tree).",
        "O(log n)", "Duplicates are ignored; use std::multiset to keep them."}},
    {"unordered_set", "insert(val)", {"Add to the set", "Adds val if it isn't there.", "A hash set: unique values, no order.",
        "O(1) average", ""}},
    {"set", "count(val)", {"Is it in the set?", "1 if present, else 0.", "Values are unique.", "O(log n)", ""}},
    {"unordered_set", "count(val)", {"Is it in the set?", "1 if present, else 0.", "Values are unique.", "O(1) average", ""}},
    {"stack", "push(val)", {"Push on top", "Puts val on top of the stack.", "The last value pushed is the first one out.", "O(1)", ""}},
    {"stack", "pop()", {"Remove the top", "Removes the top value.", "pop() returns nothing: read top() first.", "O(1)",
        "pop() on an empty stack is undefined."}},
    {"stack priority_queue", "top()", {"Look at the top", "The value that pop() would remove.",
        "Only the top element can be read.", "O(1)", "Undefined when empty."}},
    {"queue", "push(val)", {"Join the back of the queue", "Adds val at the back.", "Elements leave in arrival order.", "O(1)", ""}},
    {"queue", "pop()", {"Leave the front of the queue", "Removes the front value.", "pop() returns nothing: read front() first.",
        "O(1)", "pop() on an empty queue is undefined."}},
    {"queue", "front()", {"Look at the front", "The value that pop() would remove.", "The oldest element.", "O(1)",
        "Undefined when empty."}},
    {"priority_queue", "push(val)", {"Insert by priority", "Adds val; the largest stays on top.",
        "A binary heap sifts the new value up to its place.", "O(log n)", ""}},
    {"priority_queue", "pop()", {"Remove the largest", "Removes top().", "The heap re-orders itself afterwards.", "O(log n)", ""}},
    {"deque", "push_front(val)", {"Add to the front", "Inserts val before the first element.",
        "A deque grows at both ends.", "O(1)", ""}},
    {"deque", "pop_front()", {"Remove the first element", "Deletes the first element.", "", "O(1)",
        "Undefined on an empty deque."}},
};

// =====================================================================
// Data: headers for #include
// =====================================================================

struct Header { const char* name; const char* gives; };
const Header kHeaders[] = {
    {"iostream", "std::cout, std::cin, std::endl"}, {"vector", "std::vector"}, {"string", "std::string, std::getline"},
    {"algorithm", "std::sort, std::reverse, std::find, std::max_element"}, {"map", "std::map"},
    {"unordered_map", "std::unordered_map"}, {"set", "std::set"}, {"unordered_set", "std::unordered_set"},
    {"stack", "std::stack"}, {"queue", "std::queue, std::priority_queue"}, {"deque", "std::deque"},
    {"numeric", "std::accumulate, std::iota"}, {"fstream", "std::ifstream, std::ofstream"},
    {"sstream", "std::stringstream"}, {"cmath", "std::sqrt, std::pow, std::abs"}, {"climits", "INT_MAX, INT_MIN"},
    {"utility", "std::pair, std::swap"}, {"iomanip", "std::setprecision, std::setw"},
    {"cstdio", "printf, scanf"}, {"cstring", "strlen, strcpy, strcmp"},
};

// =====================================================================
// Helpers
// =====================================================================

bool listHas(const char* spaceSeparated, const std::string& item) {
    std::istringstream in(spaceSeparated);
    std::string w;
    while (in >> w)
        if (w == item) return true;
    return false;
}

// The most recently declared visible variable of a base type ("vector", "map", ...), or "".
std::string latestOfType(const Context& ctx, std::initializer_list<const char*> types) {
    std::string best;
    int bestLine = -1;
    for (const auto& [name, v] : ctx.variables) {
        std::string b = baseType(v.type);
        for (const char* t : types)
            if (b == t && v.line >= bestLine) {
                best = name;
                bestLine = v.line;
            }
    }
    return best;
}

// The name that ends just before position `pos` in `s` (spaces skipped): nameBefore("i < v.size()", 5) -> "v"
std::string nameBefore(const std::string& s, size_t pos) {
    if (pos == std::string::npos) return "";
    size_t end = pos;
    while (end > 0 && s[end - 1] == ' ') --end;
    size_t start = end;
    while (start > 0 && (std::isalnum(static_cast<unsigned char>(s[start - 1])) || s[start - 1] == '_')) --start;
    return s.substr(start, end - start);
}

// A line-level suggestion: `full` must continue what is already typed on the line.
std::optional<Suggestion> lineSuggestion(const Context& ctx, const std::string& full, const std::string& source,
                                         double score, const Lesson& lesson) {
    std::string typed = trim(ctx.lineBefore);
    if (full.rfind(typed, 0) != 0 || full.size() == typed.size()) return std::nullopt;
    Suggestion s;
    s.insert = full.substr(typed.size());
    s.full = full;
    s.source = source;
    s.score = score;
    s.lesson = lesson;
    return s;
}

}  // namespace

// =====================================================================
// Vocabulary
// =====================================================================

std::vector<Word> vocabulary() {
    std::vector<Word> words;
    for (auto [list, kind] : {std::pair{kKeywords, "keyword"}, std::pair{kLibrary, "library"}}) {
        std::istringstream in(list);
        std::string w;
        while (in >> w) words.push_back({w, kind, listHas(kCommon, w) ? 2.0 : 1.0});
    }
    return words;
}

Lesson wordLesson(const std::string& word, const std::string& kind) {
    for (const auto& wl : kWordLessons)
        if (word == wl.word) return wl.lesson;
    if (kind == "keyword")
        return {"C++ keyword `" + word + "`", "`" + word + "` is a reserved word of the language.",
                "Keywords have a fixed meaning in C++, so they can't be used as variable names.", "", ""};
    return {"Standard library: `" + word + "`", "`" + word + "` comes from the C/C++ standard library.",
            "Library names are declared in headers; the right #include makes them available.", "",
            "C++ library names live in namespace std: write std::" + word + "."};
}

Lesson variableLesson(const Variable& v) {
    return {"Variable `" + v.name + "` (" + v.type + ")",
            "`" + v.name + "` was declared on line " + std::to_string(v.line + 1) + " and is visible here.",
            "A variable can be used anywhere inside the { } block that declares it, after its declaration.", "",
            "A variable declared inside a loop or another function is not visible outside it."};
}

// =====================================================================
// Rule 1: container functions after a dot ("nums.pu")
// =====================================================================

std::vector<Suggestion> memberCompletions(const Context& ctx) {
    std::vector<Suggestion> out;
    auto var = ctx.variables.find(ctx.object);
    if (ctx.object.empty() || var == ctx.variables.end()) return out;
    const std::string type = baseType(var->second.type);
    for (const auto& m : kMembers) {
        std::string text = m.text;
        if (!listHas(m.types, type) || text.rfind(ctx.word, 0) != 0) continue;
        Suggestion s;
        s.insert = text.substr(ctx.word.size());
        s.full = ctx.object + "." + text;
        s.source = "member";
        s.score = 0.95;
        s.lesson = m.lesson;
        out.push_back(s);
    }
    return out;
}

// =====================================================================
// Rule 2: syntax ("for (", "while (", "#include <")
// =====================================================================

std::vector<Suggestion> syntaxCompletions(const Context& ctx) {
    std::vector<Suggestion> out;
    const std::string typed = trim(ctx.lineBefore);

    if (typed.rfind("#include", 0) == 0) {
        size_t lt = typed.find('<');
        std::string partial = lt == std::string::npos ? "" : typed.substr(lt + 1);
        for (const auto& h : kHeaders) {
            std::string name = h.name;
            if (name.rfind(partial, 0) != 0) continue;
            Suggestion s;
            s.full = "#include <" + name + ">";
            if (lt != std::string::npos) s.insert = name.substr(partial.size()) + ">";
            else s.insert = std::string(ctx.lineBefore.back() == ' ' ? "<" : " <") + name + ">";
            s.source = "syntax";
            s.score = 0.95;
            s.lesson = {"#include <" + name + ">", std::string("Gives you ") + h.gives + ".",
                        "#include copies the header's declarations into your file, so the compiler knows those names.",
                        "Compile time only", "Include what you use; don't rely on another header including it."};
            out.push_back(s);
        }
        return out;
    }

    if (typed == "for (" || typed == "for(") {
        const std::string vec = latestOfType(ctx, {"vector", "string", "deque"});
        const std::string open = typed;  // keep the user's spacing
        if (!vec.empty()) {
            if (auto s = lineSuggestion(ctx, open + "const auto& x : " + vec + ") {", "syntax", 0.95,
                    {"Loop over every element of " + vec, "for (const auto& x : " + vec + ") visits each element in order.",
                     "A range-based for loop needs no index: it runs once per element.", "O(n)",
                     "Use auto& (without const) to change elements; plain auto copies each one."}))
                out.push_back(*s);
            if (auto s = lineSuggestion(ctx, open + "size_t i = 0; i < " + vec + ".size(); ++i) {", "syntax", 0.94,
                    {"Loop with an index over " + vec, "Use an index when you need the position i.",
                     "size_t matches the type of size(), so there's no signed/unsigned warning.", "O(n)",
                     "i <= " + vec + ".size() is off by one and reads past the end."}))
                out.push_back(*s);
        } else if (auto s = lineSuggestion(ctx, open + "int i = 0; i < n; ++i) {", "syntax", 0.9,
                       {"Counting loop", "Start, condition, update: for (int i = 0; i < n; ++i).",
                        "The loop runs n times, with i = 0, 1, ..., n-1.", "O(n)", "i <= n runs n+1 times."})) {
            out.push_back(*s);
        }
    }

    if (typed == "while (" || typed == "while(") {
        const std::string c = latestOfType(ctx, {"stack", "queue", "priority_queue"});
        if (!c.empty())
            if (auto s = lineSuggestion(ctx, typed + "!" + c + ".empty()) {", "syntax", 0.95,
                    {"Process until empty", "Loop while " + c + " still has elements.",
                     "Each pass handles one element; the loop ends when the container is empty.", "O(n) iterations",
                     "Forgetting pop() inside the loop makes it run forever."}))
                out.push_back(*s);
    }
    return out;
}

// =====================================================================
// Rule 3: what usually comes next (shown when you pause on an empty line)
// =====================================================================

std::vector<Suggestion> nextSteps(const Context& ctx) {
    std::vector<Suggestion> out;
    const std::string& prev = ctx.previousLine;
    auto add = [&](const std::string& code, double score, const Lesson& lesson) {
        if (auto s = lineSuggestion(ctx, code, "next step", score, lesson)) out.push_back(*s);
    };
    const std::string vec = latestOfType(ctx, {"vector"});

    if (prev.empty()) {
        add("#include <iostream>", 0.9, {"Start your program", "Programs usually begin with #include lines.",
            "<iostream> gives you std::cout for output and std::cin for input.", "", ""});
    } else if (ctx.depth == 0 && (prev.rfind("#include", 0) == 0 || prev.rfind("using", 0) == 0)) {
        add("int main() {", 0.9, {"Write main()", "Every program starts running in int main().",
            "Statements can't be written at file level; they go inside a function such as main.", "",
            "main must return int."});
    } else if (prev.find("main") != std::string::npos && prev.back() == '{') {
        add("std::vector<int> " + (ctx.variables.count("nums") ? std::string("data") : std::string("nums")) + " = {5, 2, 8, 1, 9};", 0.9,
            {"Start with some data", "Create the values your program will work on.",
             "A std::vector holds a list of values that can grow.", "", "Needs #include <vector>."});
    } else if (prev.rfind("for", 0) == 0 && prev.back() == '{') {
        // "for (size_t i = 0; i < nums.size(); ++i) {"  -> std::cout << nums[i]
        // "for (const auto& x : nums) {"                  -> std::cout << x
        std::string container = nameBefore(prev, prev.find(".size()"));
        std::string element = container.empty() ? nameBefore(prev, prev.find(':'))
                                                : container + "[" + nameBefore(prev, prev.find('=')) + "]";
        if (!element.empty() && element.find("[]") == std::string::npos)
            add("std::cout << " + element + " << \" \";", 0.9,
                {"Use the current element", "Inside the loop, " + element + " is the current element.",
                 "Each pass of the loop handles one element.", "O(1) per pass", ""});
    } else if (!vec.empty() && prev.find("vector<") != std::string::npos) {
        add("std::sort(" + vec + ".begin(), " + vec + ".end());", 0.9,
            {"Next: sort " + vec + "?", "A common next step after creating a vector is sorting it.",
             "std::sort orders the elements from smallest to largest.", "O(n log n)", "Needs #include <algorithm>."});
        add("for (const auto& x : " + vec + ") std::cout << x << \" \";", 0.89,
            {"Next: print " + vec + "?", "Or loop over it to print every element.",
             "A range-based for loop visits each element once.", "O(n)", ""});
    } else if (!vec.empty() && (prev.find("map<") != std::string::npos)) {
        const std::string map = latestOfType(ctx, {"map", "unordered_map"});
        if (!map.empty())
            add("for (const auto& x : " + vec + ") " + map + "[x]++;", 0.9,
                {"Next: fill " + map + "?", "Count each value of " + vec + " in " + map + ".",
                 "A missing key starts at 0, so ++ makes it 1 the first time.", "O(n) average", ""});
    } else if (ctx.insideMain && prev.find("std::cout") != std::string::npos) {
        add("return 0;", 0.85, {"Finish main()", "return 0; tells the system the program ended successfully.",
            "main's return value is the program's exit code; 0 means success.", "", ""});
    }
    return out;
}

// =====================================================================
// Sample code for the next-word model
// =====================================================================

const std::string& sampleCorpus() {
    static const std::string corpus = R"(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

int main() {
    int n;
    std::cin >> n;
    std::vector<int> nums(n);
    for (int i = 0; i < n; ++i) {
        std::cin >> nums[i];
    }
    std::sort(nums.begin(), nums.end());
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += nums[i];
    }
    std::cout << "total: " << total << std::endl;
    std::string name;
    std::getline(std::cin, name);
    if (total > 100) {
        std::cout << "big" << std::endl;
    } else {
        std::cout << "small" << std::endl;
    }
    for (const auto& x : nums) {
        std::cout << x << " ";
    }
    std::cout << "\n";
    return 0;
}
)";
    return corpus;
}
