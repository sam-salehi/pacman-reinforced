
#pragma once

#include <iostream>

enum Direction {UP,DOWN,LEFT,RIGHT,IDLE}; // Represents Direction in which agent is moving in

struct Position { // Represents Position where agent is located correspondig to gameState.
    int x;
    int y;
    Position():x(-1),y(-1){}
    Position(int x_val, int y_val) : x(x_val), y(y_val) {}
    bool operator==(const Position& other) const {
    return (x == other.x && y == other.y);
    }
};

enum Cell {
    PACMAN_CELL,
    GHOST_FICKLE,
    GHOST_CHASER,
    GHOST_AMBUSHER,
    GHOST_STUPID,
    WALL,
    EMPTY,
    PELLET,
    POWER_PELLET,
    DOOR
};

enum GhostType {
    CHASER,
    STUPID,
    FICKLE,
    AMBUSHER
};

enum GhostState {
    CHASE, //
    SCATTER, //
    EATEN,
    FRIGHTENED,
    TRANSITION, // for turning from FRIGHTENED to CHASE
    ESCAPE,//
    SLEEP
};

// index of middle of the screen
// used for handling teleportation
#define MIDLE_ROW_INDEX 9

#define MAZE_HEIGHT 21
#define MAZE_WIDTH 21

// ---- Simulation constants ----
// The game is simulated in discrete frames. All timings are expressed in frames so
// that the simulation is deterministic and independent of wall-clock time / render speed.
#define GAME_FPS 60
#define CELL_PX 30                      // size of one maze cell in simulation pixels

// speeds in pixels per second (must divide evenly into CELL_PX * GAME_FPS / step)
#define PACMAN_SPEED 300
#define GHOST_CHASE_SPEED 180
#define GHOST_FRIGHTENED_SPEED 120
#define GHOST_EATEN_SPEED 300

// global ghost-mode durations in seconds
#define CHASE_DURATION_S 20
#define SCATTER_DURATION_S 7
#define FRIGHTENED_DURATION_S 4
#define TRANSITION_DURATION_S 2
