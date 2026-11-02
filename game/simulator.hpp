#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "globals.hpp"
#include "game.hpp"

// Headless, deterministic, frame-stepped Pacman simulation.
//
// This class owns all game dynamics (movement, collisions, teleports, ghost AI via GameState).
// It has no dependency on SFML, so it can run thousands of games per second for training.
// The SFML Display only *renders* a Simulator; it no longer contains game logic, which
// guarantees that an agent trained headless sees exactly the same game when it is watched.

// A moving body in simulation pixel coordinates. Column index = x / CELL_PX.
struct Body {
    int x = 0;
    int y = 0;
    Direction dir = IDLE;

    bool contained() const { return x % CELL_PX == 0 && y % CELL_PX == 0; }
    // nearest cell (round half up, as std::round did on the original float positions)
    Position cell() const { return Position((x + CELL_PX / 2) / CELL_PX, (y + CELL_PX / 2) / CELL_PX); }
    void setCell(Position p) { x = p.x * CELL_PX; y = p.y * CELL_PX; }
    void advance(int step);
};

struct GhostBody : Body {
    GhostType type = CHASER;
    GhostState moveState = SLEEP;  // state that determines the speed; refreshed at cell centres
};

// What happened during one call of Simulator::act / frame.
struct StepEvents {
    int frames = 0;
    int pellets = 0;        // small pellets eaten
    int powerPellets = 0;   // power pellets eaten
    int ghosts = 0;         // frightened ghosts eaten
    bool died = false;
    bool won = false;
};

// Observation layout, written by Simulator::writeObservation
#define OBS_PLANES 8
#define OBS_SCALARS 3
#define OBS_H MAZE_HEIGHT
#define OBS_W MAZE_WIDTH
enum ObsPlane {
    PLANE_WALL = 0,           // walls and ghost-house door
    PLANE_PELLET,
    PLANE_POWER_PELLET,
    PLANE_PACMAN,
    PLANE_DANGER_GHOST,       // cell occupied by a ghost that kills pacman
    PLANE_DANGER_GHOST_NEXT,  // cell that ghost is moving into (gives motion direction)
    PLANE_EDIBLE_GHOST,       // frightened ghost
    PLANE_EDIBLE_GHOST_NEXT,
};
// scalars: [frightened time left (0..1), fraction of pellets remaining, 1 if global mode is CHASE]

class Simulator {
    std::unique_ptr<GameState> gs;
    Body pacman;
    Direction pacmanNextDir;
    std::array<GhostBody, 4> ghosts;  // update order: chaser, ambusher, fickle, stupid
    unsigned long frames;
    unsigned int decisions;

    void movePacman();
    void updateGhost(GhostBody& g);
    void moveGhost(GhostBody& g);
    void checkGhostCollisions();
    void handleTeleport(Body& b, bool isPacman, GhostType type);
    static int ghostStepSize(GhostState s);

public:
    explicit Simulator(uint32_t seed = 0);

    void reset(uint32_t seed);

    // advance the game by exactly one frame
    StepEvents frame();

    // Set pacman's desired direction (0=UP,1=DOWN,2=LEFT,3=RIGHT) and simulate until pacman
    // reaches the next cell centre (the next point at which a decision matters) or the game ends.
    StepEvents act(int action);

    // set the direction pacman turns into at the next cell centre where that is possible
    void setPacmanNextDir(Direction d) { pacmanNextDir = d; }

    // pacman is at a cell centre, i.e. can change direction
    bool decisionAvailable() const { return pacman.contained(); }
    // true if moving in `action` from pacman's current cell is not blocked by a wall/door
    bool validAction(int action) const;
    void validActions(uint8_t* out4) const;

    void writeObservation(uint8_t* planes /*OBS_PLANES*H*W*/, float* scalars /*OBS_SCALARS*/) const;

    // accessors (used by the renderer and python)
    GameState& state() { return *gs; }
    const GameState& state() const { return *gs; }
    const Body& pacmanBody() const { return pacman; }
    const GhostBody& ghostBody(GhostType t) const;
    bool gameOver() const { return gs->isGameOver(); }
    bool gameWon() const { return gs->isGameWon(); }
    unsigned int score() const { return gs->getScore(); }
    unsigned long frameCount() const { return frames; }
    unsigned int decisionCount() const { return decisions; }
    unsigned int pelletsRemaining() const { return gs->getTotalPelletCount() - gs->getEatenPelletCount(); }
};
