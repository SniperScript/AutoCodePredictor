// terminal.hpp - the only platform-specific part: reading single keys with a timeout
// and drawing with ANSI colours. Windows uses conio.h and the console API;
// Linux/macOS use termios.
#pragma once

namespace term {

enum Key {
    kNone = -1,  // no key before the timeout
    kEnter = 1000,
    kBackspace,
    kTab,
    kEscape,
    kUp,
    kDown,
    kCtrlE,
    kCtrlN,
    kCtrlQ,  // also Ctrl+C
    kCtrlS,
};

bool isInteractive();     // keyboard and screen are a real terminal (not a pipe or file)
bool outputIsTerminal();  // just the screen (colours are only used then)
void enableColors();      // Windows needs this before ANSI colour codes work
void start();             // read keys one at a time, no echo, hide the cursor
void stop();              // put the terminal back to normal
int readKey(int timeoutMs);  // a printable character (32..126), a Key, or kNone
void clearScreen();

}  // namespace term
