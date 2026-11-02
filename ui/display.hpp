#pragma once

#include <SFML/Graphics.hpp>
#include "pacmanUI.hpp"
#include "globalsUI.hpp"
#include "../game/game.hpp"
#include "../game/simulator.hpp"
#include "ghosts/ghostUI.hpp"


// Renders a Simulator with SFML. All game logic lives in Simulator; this class only
// draws it and forwards keyboard input, so what you see is exactly what the agent trains on.
class Display {

private:
    sf::RenderWindow* window;
    sf::VideoMode videoMode;
    sf::Event ev;
    Simulator sim;

    // sprites
    PacmanUI pacman;
    ChaserUI chaser;
    AmbusherUI ambusher;
    FickleUI fickle;
    StupidUI stupid;

    sf::RectangleShape empty;
    sf::CircleShape pellet;
    sf::CircleShape powerPellet;
    sf::Texture wallTexture;

    void initWindow(bool limitFramerate);
    void initGameObjects();

    // check for and handle triggered events.
    void pollEvents();

    void renderPacman();
    void renderGhosts();
    void renderGhost(GhostUI& ghost);
    void renderMaze();

    void scaleSprite(sf::Sprite &sprite);
    void selectWall(sf::Sprite &sprite,grid_t& grid,size_t x, size_t y,size_t height, size_t width);

public:
    explicit Display(uint32_t seed = 0, bool limitFramerate = true);
    ~Display();

    // process window events and advance the game by one frame
    void update();
    // draw the current game state
    void render();

    // set pacman's next direction: 0=UP 1=DOWN 2=LEFT 3=RIGHT
    void step(int i);
    void setPacmanDir(Direction dir);

    bool pacmanContainedInCell();

    // returns whether the window is open
    bool running() const;
    void close();

    Simulator& simulator() { return sim; }

    sf::Image getScreenshot();
};
