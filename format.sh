#!/bin/bash
cd "$(dirname "$0")"
find . -maxdepth 1 \( -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i -style=WebKit {} \;
