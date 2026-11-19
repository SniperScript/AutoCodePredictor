@echo off
REM Builds if needed, runs the tests and the showcase. Then try the editor.
cd /d "%~dp0"
if not exist autocode-predictor.exe call build.bat
if not exist autocode-predictor.exe exit /b 1
autocode-predictor.exe --test
autocode-predictor.exe --demo
echo.
echo Now try the editor:  autocode-predictor.exe
