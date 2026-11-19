#!/usr/bin/env bash
# Builds if needed, runs the tests and the showcase. Then try the editor.
set -e
cd "$(dirname "$0")"
[ -x ./autocode-predictor ] || make
./autocode-predictor --test
./autocode-predictor --demo
echo
echo "Now try the editor:  ./autocode-predictor"
