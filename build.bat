@echo off
REM Build on Windows with g++ 10 or newer (for example WinLibs). Run from this folder.
REM -static: the .exe runs on its own, without the compiler's DLLs.
cd /d "%~dp0"
echo Building autocode-predictor.exe ...
g++ -std=c++17 -Wall -Wextra -O2 -static src\main.cpp src\engine.cpp src\rules.cpp src\scope.cpp src\trie.cpp src\ngram.cpp src\editor.cpp src\terminal.cpp tests\tests.cpp -o autocode-predictor.exe
if errorlevel 1 (
    echo.
    echo Build FAILED. Check that g++ 10 or newer is installed:  g++ --version
    exit /b 1
)
echo Done. Run:  autocode-predictor.exe            (editor)
echo        or:  autocode-predictor.exe --demo     (showcase)
