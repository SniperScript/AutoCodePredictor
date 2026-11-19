#include "terminal.hpp"

#include <iostream>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#include <cstdio>
#else
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace term {
namespace {

int translate(int c) {
    switch (c) {
        case '\r': case '\n': return kEnter;
        case 8: case 127: return kBackspace;
        case '\t': return kTab;
        case 5: return kCtrlE;
        case 14: return kCtrlN;
        case 3: case 17: return kCtrlQ;  // Ctrl+C, Ctrl+Q
        case 19: return kCtrlS;
        default: return (c >= 32 && c < 127) ? c : kNone;
    }
}

#ifdef _WIN32
DWORD savedInputMode = 0;
bool saved = false;
#else
termios savedSettings{};
bool saved = false;

bool waitForInput(int ms) {
    pollfd p{STDIN_FILENO, POLLIN, 0};
    return poll(&p, 1, ms) > 0;
}
int readByte() {
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1 ? c : -1;
}
#endif

}  // namespace

bool isInteractive() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
}

bool outputIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}

void enableColors() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

void start() {
    enableColors();
#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    if (GetConsoleMode(in, &savedInputMode)) {
        saved = true;
        SetConsoleMode(in, savedInputMode & ~ENABLE_PROCESSED_INPUT);  // Ctrl+C arrives as a key
    }
#else
    if (tcgetattr(STDIN_FILENO, &savedSettings) == 0) {
        saved = true;
        termios raw = savedSettings;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);  // key by key, no echo, Ctrl+C as a key
        raw.c_iflag &= ~(IXON | ICRNL);                   // Ctrl+S/Ctrl+Q reach us; Enter arrives as '\r'
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
#endif
    std::cout << "\x1b[?25l" << std::flush;  // hide the cursor
}

void stop() {
    std::cout << "\x1b[0m\x1b[?25h" << std::flush;  // reset colours, show the cursor
#ifdef _WIN32
    if (saved) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), savedInputMode);
#else
    if (saved) tcsetattr(STDIN_FILENO, TCSAFLUSH, &savedSettings);
#endif
    saved = false;
}

int readKey(int timeoutMs) {
#ifdef _WIN32
    for (int waited = 0; !_kbhit(); waited += 10) {
        if (waited >= timeoutMs) return kNone;
        Sleep(10);
    }
    int c = _getch();
    if (c == 0 || c == 224) {  // arrow keys come as two codes
        int code = _getch();
        return code == 72 ? kUp : code == 80 ? kDown : kNone;
    }
    return c == 27 ? kEscape : translate(c);
#else
    if (!waitForInput(timeoutMs)) return kNone;
    int c = readByte();
    if (c != 27) return translate(c);
    // Escape on its own, or the start of a sequence such as ESC [ A (arrow up)
    if (!waitForInput(30)) return kEscape;
    int next = readByte();
    if (next != '[' && next != 'O') return kNone;
    int last = readByte();
    while (last != -1 && !(last >= 0x40 && last <= 0x7E) && waitForInput(30)) last = readByte();  // skip parameters
    return last == 'A' ? kUp : last == 'B' ? kDown : kNone;
#endif
}

void clearScreen() { std::cout << "\x1b[2J\x1b[H"; }

}  // namespace term
