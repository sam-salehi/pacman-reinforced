

#pragma once

#include "globals.hpp"
#include "ghost.hpp"
#include "pacman.hpp"
#include "maze.hpp"
#include "ghostAI.hpp"
#include "globals.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <vector>
#include <unordered_map>


enum GameStatus {
    LOST,
    WON,
    PROGRESS
};


// Pointer to created Ghosts fond on maze
struct Ghosts {
    FickleGhost * fickle_p;
    ChaserGhost * chaser_p;
    AmbusherGhost * ambusher_p;
    StupidGhost * stupid_p;
};



class GameState {
    Maze * maze_p;
    Pacman * pacman_p;
    Ghosts ghosts;
    std::array<Ghost *,4> ghostArray;
    GhostAI ghostAI;
    GhostState globalState;
    unsigned long frameCount;       // number of simulated frames since game start
    unsigned long stateStartFrame;  // frame at which globalState was entered
    unsigned int eatenPelletCount;
    unsigned int ghostsEatenCount;
    unsigned int score;
    bool gameOver;
    GameStatus status;
    std::mt19937 rng;               // seeded RNG (frightened ghost movement)

public:
    explicit GameState(uint32_t seed = std::random_device{}());
    ~GameState();
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // advance the game clock by one frame. Must be called once per simulated frame.
    void tick();

    Position getGhostPos(GhostType type);
    Ghost * getGhost(GhostType type);
    Cell getCell(Position pos) const;
    unsigned int getEatenPelletCount() const;
    unsigned int getTotalPelletCount() const;
    unsigned int getGhostsEatenCount() const;
    bool isGameWon() const;
    // frames left until ghosts stop being edible (0 if not frightened)
    int frightenedFramesLeft() const;
    std::mt19937& getRng();

    // getters used to get position of agents:
    Position getPacmanPos() const;
    Direction getPacmanDir() const;

    Direction getGhostDir(GhostType type);
    GhostState getGhostState(GhostType type);

    // maze_p accessors for communicating with GhostAI
    grid_t getGrid() const;
    int getGridWidth() const;
    int getGridHeight() const;

    // returns true if ghost is active in game
    bool isActive(Ghost * ghost) const;

    GhostState getGlobalState() const;



    // changes position of pacman to given pos.
    void updatePacmanPos(Position pos);

    // assesses wether pacman moving in given direction encounters a wall.
    bool validPacmanMove(Direction dir) const;

    // Changes direction of pacman to given direction
    void changePacmanDir(Direction dir);



    // set the directon of each ghost given the board and pacman positions.
    void generateGhostMove(GhostType type);

    // set position of given ghost
    void updateGhostPos(Position pos,GhostType type);

    // set state of type to given state
    void updateGhostState(GhostType type, GhostState state);

    // handle all plausible collisions.
    void handleCollisions();

    // returns true if jump possible
    bool jumpAvail(Position pos);
    // takes current pos and returns what it would become
    // requires jumpAvail to return true
    Position jumpPortal(Position pos);

    // returns valid positions that the ghost can move in.
    std::vector<Position> getValidPositions(Position ghostPos, Direction ghostDir, bool hasEscaped) const;
    // Return neighbors of position on grid which are not walls.
    std::vector<Position> getValidNeighbours(Position pos, bool hasEscaped) const;

    // gameOver getter
    bool isGameOver() const;

    unsigned int getScore() const;
    bool isGameLost() const;

    void ghostCollided(GhostType type); //added


private:

    // Iterators over all cells to find position of specifiedagentCell being one of: 
    // {GHOST_FICKLE,GHOST_CHASER, GHOST_AMBUSER, GHOST_STUPID}
    // To be used in constructor only
    // Position getAgentPositionBrute(Cell agentCell ,const std::vector<std::vector<Cell >> grid);    

    // check if there is a pellet at pacman's position
    // and remove it if there is so
    void handlePelletCollision();
    void handlePowerPelletCollision();
    // void handleGhostCollisions();s
    // void handleGhostCollision(Ghost * ghost, Position pacmanPosition);

    void handleGhostCollision(Ghost * ghost);


    void gameLost(); // signifies that ghosts have collided

    // handles case when all pellets have been eaten.
    void checkPelletStatus();
    

    // used to free Ghosts in GhostHouse if at approprite count.
    void freeGhostHouseGhosts();

    // handles pacman eating specified Ghost
    void eatGhost(Ghost * ghost);

    // changes globalState to newState and changes the state of all active prevStates.
    void updateGlobalState(GhostState newState);

    // void updateGlobalGameState(GhostState newState);

    // void updateGlobalStateToChase();
    // void updateGlobalStateToScatter();
    // void updateGlobalStateToFrightened();/
    // void updateGlobalStateToTransition();

    // sets ghost states for beginnig of the game.
    void setInitialGhostStates();

    // resets startTimer.
    void startStateTimer();
    // checks if it is time for new state
    bool hasTimeElapsed() const;
    // switches to next state and resets timer;
    void switchToNextState();

};




