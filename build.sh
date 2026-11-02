#!/usr/bin/env bash
# Build the C++ game modules (headless `pacman_sim` for training, SFML `pacman` for playing/watching).
# Requires: cmake, a C++17 compiler, `pip install pybind11`, and (optional, for the UI) SFML 2.5+.
# Conda's compiler wrappers/flags are bypassed so the system compiler and SFML are used.
set -euo pipefail
cd "$(dirname "$0")"
env -u CXXFLAGS -u CFLAGS -u CPPFLAGS -u LDFLAGS CC="${SYS_CC:-/usr/bin/gcc}" CXX="${SYS_CXX:-/usr/bin/g++}" \
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
env -u CXXFLAGS -u CFLAGS -u CPPFLAGS -u LDFLAGS cmake --build build -j"$(nproc 2>/dev/null || echo 4)"
