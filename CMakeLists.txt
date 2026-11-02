cmake_minimum_required(VERSION 3.12)
project(PacmanGame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# pybind11: use the one installed in the active python environment (pip install pybind11)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
execute_process(COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
                OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
find_package(pybind11 CONFIG REQUIRED)
find_package(OpenMP)

# Pure game logic (no SFML)
set(GAME_SOURCES
    game/agent.cpp game/game.cpp game/ghost.cpp game/ghostAI.cpp
    game/helper.cpp game/maze.cpp game/pacman.cpp game/simulator.cpp)

# ---- Headless simulator used for training: `import pacman_sim` ----
pybind11_add_module(pacman_sim ${GAME_SOURCES} game/sim_bindings.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(pacman_sim PRIVATE OpenMP::OpenMP_CXX)
endif()
# Link the C++ runtime statically so the module imports cleanly in environments (e.g. conda)
# that ship an older libstdc++ than the compiler used here.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(pacman_sim PRIVATE -static-libstdc++ -static-libgcc)
endif()

# ---- SFML front-end: `import pacman` (watch / play) and the standalone game ----
find_package(SFML 2.5 COMPONENTS graphics window system QUIET HINTS /usr/lib/x86_64-linux-gnu/cmake/SFML)
if(SFML_FOUND)
    set(UI_SOURCES ui/display.cpp ui/pacmanUI.cpp ui/ghosts/agentUI.cpp ui/ghosts/ghostUI.cpp)
    set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui/animations")

    pybind11_add_module(pacman ${GAME_SOURCES} ${UI_SOURCES} ui/bindings.cpp)
    target_link_libraries(pacman PRIVATE sfml-graphics sfml-window sfml-system)
    target_compile_definitions(pacman PRIVATE ASSETS_DIR="${ASSETS_DIR}")

    add_executable(pacman_game ${GAME_SOURCES} ${UI_SOURCES} ui/main.cpp)
    target_link_libraries(pacman_game PRIVATE sfml-graphics sfml-window sfml-system)
    target_compile_definitions(pacman_game PRIVATE ASSETS_DIR="${ASSETS_DIR}")
else()
    message(WARNING "SFML not found: only the headless `pacman_sim` module will be built")
endif()
