#!/bin/bash

set -eux
FILES=$(find . -name "*.h" -o -name "*.cpp")
echo $FILES
clang-format-8 -style=file -i -fallback-style=none $FILES
