#!/bin/bash
FILES=$(find . -iname *.h -o -iname *.cpp)
echo $FILES | xargs clang-format-8 -style=file -i -fallback-style=none
