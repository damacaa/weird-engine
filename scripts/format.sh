#!/usr/bin/env bash

# Change to the project root directory
cd "$(dirname "$0")/.." || exit 1

echo "Formatting project files..."

find . -type d \( -name "third-party" -o -name "build" -o -name "build-muos" -o -name "dist-muos" -o -name ".git" \) -prune -o \
       -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.glsl" -o -name "*.frag" -o -name "*.vert" \) \
       -exec clang-format -i {} +

echo "Formatting complete."
