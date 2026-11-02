#pragma once

#include <SFML/Graphics.hpp>
#include "../game/globals.hpp"
#include "globalsUI.hpp"
#include "ghosts/agentUI.hpp"


// Pacman sprite. Movement is simulated by Simulator; this class only animates it.
class PacmanUI : public AgentUI {
    private:
        Direction pacmanDir;

    public:
        PacmanUI();

        // direction pacman is facing (for choosing the animation row)
        void setFacing(Direction dir);

        // returns row index of image to be used for pacman animations
        int getRowIndex();

        // increases frame by one or resets it.
        void nextFrame();

        void setOrientationForRendering() override;
};
