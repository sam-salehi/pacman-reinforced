#include <random>
#include "display.hpp"

// Standalone human-playable game. Build with CMake (target `pacman_game`).
int main() {
    Display display(std::random_device{}());
    while (display.running()) {
        display.update();
        display.render();
    }
    return 0;
}
