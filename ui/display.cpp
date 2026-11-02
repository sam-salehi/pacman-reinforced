#include <iostream>
#include <string>
#include "display.hpp"
#include "ghosts/speeds.hpp"
#include "pacmanUI.hpp"

#ifndef ASSETS_DIR
#define ASSETS_DIR "ui/animations"
#endif


void Display::initWindow(bool limitFramerate) {
    this->videoMode.height = WINDOW_HEIGHT;
    this->videoMode.width = WINDOW_WIDTH;
    this->window = new sf::RenderWindow(this->videoMode,"Pacman");
    if (limitFramerate) this->window->setFramerateLimit(FRAMES);
}

void Display::initGameObjects() {
    this->pellet.setRadius(PELLET_RADIUS);
    sf::Color orange(254,138,24);
    this->pellet.setFillColor(orange);

    this->powerPellet.setRadius(POWER_PELLET_RADIUS);
    this->powerPellet.setFillColor(orange);

    this->empty.setSize(sf::Vector2f(PIXEL_SIZE,PIXEL_SIZE));
    this->empty.setFillColor(sf::Color::Black);

    if (!wallTexture.loadFromFile(std::string(ASSETS_DIR) + "/wall.png")) {
        std::cerr << "Failed to load Wall animation texture!" << std::endl;
    }
}

Display::Display(uint32_t seed, bool limitFramerate) : sim(seed) {
    this->initGameObjects();
    this->initWindow(limitFramerate);
}

Display::~Display() {
    delete this->window;
}

void Display::update() {
    this->pollEvents();
    if (!sim.gameOver()) sim.frame();
}

void Display::render() {
    this->window->clear();
    this->renderMaze();
    this->renderGhosts();
    this->renderPacman();
    this->window->display();
}

void Display::pollEvents() {
    while (this->window->pollEvent(this->ev)) {
        if (this->ev.type == sf::Event::Closed) {
            this->window->close();
        }
        if (this->ev.type == sf::Event::KeyPressed) {
            if (this->ev.key.code == sf::Keyboard::Escape) {
                this->window->close();
            } else if (this->ev.key.code == sf::Keyboard::Up) {
                this->setPacmanDir(UP);
            } else if (this->ev.key.code == sf::Keyboard::Down) {
                this->setPacmanDir(DOWN);
            } else if (this->ev.key.code == sf::Keyboard::Right) {
                this->setPacmanDir(RIGHT);
            } else if (this->ev.key.code == sf::Keyboard::Left) {
                this->setPacmanDir(LEFT);
            }
        }
    }
}


void Display::step(int i) {
    if (i < 0 || i > 3) throw std::invalid_argument("Given invalid direction for pacman.");
    sim.setPacmanNextDir((Direction)i);  // enum order UP, DOWN, LEFT, RIGHT matches action indices
}

void Display::setPacmanDir(Direction dir) {
    sim.setPacmanNextDir(dir);
}

bool Display::running() const {
    return this->window->isOpen();
}

void Display::close() {
    this->window->close();
}

void Display::renderPacman() {
    const Body& b = sim.pacmanBody();
    this->pacman.setSFPixel((float)b.x, (float)b.y);
    this->pacman.setFacing(b.dir);
    this->pacman.setPositionForRendering();
    this->pacman.setOrientationForRendering();
    this->window->draw(this->pacman.getSprite());
}

void Display::renderGhost(GhostUI& ghost) {
    const GhostBody& b = sim.ghostBody(ghost.getType());
    ghost.setSFPixel((float)b.x, (float)b.y);
    GameState& gs = sim.state();
    ghost.render(gs.getGhostState(ghost.getType()), gs.getGhostDir(ghost.getType()));
    this->window->draw(ghost.getSprite());
    this->window->draw(ghost.getFace());
}

void Display::renderGhosts() {
    renderGhost(chaser);
    renderGhost(ambusher);
    renderGhost(fickle);
    renderGhost(stupid);
}

void Display::renderMaze() {
    grid_t grid = sim.state().getGrid();
    sf::Sprite wallSprite;
    sf::Sprite doorSprite;
    wallSprite.setTexture(wallTexture);
    doorSprite.setTexture(wallTexture);
    doorSprite.setTextureRect(sf::IntRect(FRAME_SIZE * 2, FRAME_SIZE * 1 ,FRAME_SIZE, FRAME_SIZE));
    this->scaleSprite(doorSprite);
    // on assumption that grid is rectangular
    size_t grid_height = grid.size();
    size_t grid_width = grid[0].size();

    for (size_t y = 0; y < grid_height; ++y) {
        for (size_t x = 1; x < grid_width - 1; ++x) {
            switch (grid[y][x])
            {
            case WALL:
                this->selectWall(wallSprite,grid,x,y,grid_height,grid_width);
                wallSprite.setPosition((x-1) * PIXEL_SIZE, y * PIXEL_SIZE);
                this->window->draw(wallSprite);
                break;
            case PELLET:
                this->pellet.setPosition((x-1) * PIXEL_SIZE + PELLET_OFFSET, y * PIXEL_SIZE + PELLET_OFFSET);
                this->window->draw(this->pellet);
                break;
            case POWER_PELLET:
                this->powerPellet.setPosition((x-1) * PIXEL_SIZE + POWER_PELLET_OFFSET, y * PIXEL_SIZE + POWER_PELLET_OFFSET);
                this->window->draw(this->powerPellet);
                break;
            case DOOR:
                doorSprite.setPosition((x-1) * PIXEL_SIZE, y * PIXEL_SIZE);
                this->window->draw(doorSprite);
                break;
            default:
                this->empty.setPosition((x-1) * PIXEL_SIZE, y * PIXEL_SIZE);
                this->window->draw(this->empty);
                break;
            }
        }
    }
}

void Display::selectWall(sf::Sprite &sprite, grid_t &grid, size_t x, size_t y, size_t height, size_t width) {
    bool up = 0, down = 0, left = 0, right = 0; // use flagging for binary indexing

    if (y < height - 1 && WALL == grid[y+1][x]) down = 1;
    if (x > 0 && WALL == grid[y][x-1]) left = 1;
    if (x < width - 1 && WALL == grid[y][x+1]) right = 1;
    if (y > 0 && WALL == grid[y-1][x]) up = 1;

    int index = down + 2 * (left + 2 * (right + 2 * up));
    sprite.setTextureRect(sf::IntRect(FRAME_SIZE * index, 0, FRAME_SIZE, FRAME_SIZE));
    this->scaleSprite(sprite);
}


void Display::scaleSprite(sf::Sprite &sprite) {
    float scale = PIXEL_SIZE / FRAME_SIZE;
    sprite.setScale(scale, scale);
}

bool Display::pacmanContainedInCell() {
    return sim.decisionAvailable();
}

sf::Image Display::getScreenshot() {
    return this->window->capture();
}
