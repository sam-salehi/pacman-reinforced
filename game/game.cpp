#include <algorithm>
#include <iostream>

#include "game.hpp"
#include "helpers.hpp"






GameState::GameState(uint32_t seed) : ghostAI(this), rng(seed) {
    maze_p = new Maze();
    pacman_p = new Pacman();
    ghosts.fickle_p = new FickleGhost();
    ghosts.chaser_p = new ChaserGhost();
    ghosts.ambusher_p = new AmbusherGhost();
    ghosts.stupid_p = new StupidGhost();
    ghostArray[0] = ghosts.fickle_p;
    ghostArray[1] = ghosts.chaser_p;
    ghostArray[2] = ghosts.ambusher_p;
    ghostArray[3] = ghosts.stupid_p;
    score = 0;
    eatenPelletCount = 0;   // was previously uninitialised (undefined behaviour in the win check)
    ghostsEatenCount = 0;
    gameOver = false;
    status = PROGRESS;
    frameCount = 0;
    stateStartFrame = 0;
    globalState = SCATTER;  // was previously uninitialised; game opens in scatter mode

    setInitialGhostStates();
}

void GameState::tick() { this->frameCount += 1; }

Ghost * GameState::getGhost(GhostType type) {
    switch (type) {
        case CHASER: return ghosts.chaser_p;
        case AMBUSHER: return ghosts.ambusher_p;
        case FICKLE: return ghosts.fickle_p;
        default: return ghosts.stupid_p;
    }
}
Position GameState::getGhostPos(GhostType type) { return getGhost(type)->getPos(); }
Cell GameState::getCell(Position pos) const { return maze_p->getCell(pos); }
unsigned int GameState::getEatenPelletCount() const { return eatenPelletCount; }
unsigned int GameState::getTotalPelletCount() const { return maze_p->getTotalPelletCount(); }
unsigned int GameState::getGhostsEatenCount() const { return ghostsEatenCount; }
bool GameState::isGameWon() const { return status == WON; }
std::mt19937& GameState::getRng() { return rng; }

int GameState::frightenedFramesLeft() const {
    long elapsed = (long)(frameCount - stateStartFrame);
    if (globalState == FRIGHTENED) return (int)((FRIGHTENED_DURATION_S + TRANSITION_DURATION_S) * GAME_FPS - elapsed);
    if (globalState == TRANSITION) return (int)(TRANSITION_DURATION_S * GAME_FPS - elapsed);
    return 0;
}


GameState::~GameState() {
    delete maze_p;
    delete pacman_p;
    delete ghosts.fickle_p;
    delete ghosts.chaser_p;
    delete ghosts.ambusher_p;
    delete ghosts.stupid_p;
}




Position GameState::getPacmanPos() const {return pacman_p->getPos();}
Direction GameState::getPacmanDir() const {return pacman_p->getDir();};



Direction GameState::getGhostDir(GhostType type) {
    switch (type) {
        case CHASER: return ghosts.chaser_p->getDir();
        case AMBUSHER: return ghosts.ambusher_p->getDir();
        case FICKLE: return ghosts.fickle_p->getDir();
        default: return ghosts.stupid_p->getDir();
    }
}
GhostState GameState::getGhostState(GhostType type) {
    switch (type) {
        case CHASER: return ghosts.chaser_p->getGhostState();
        case AMBUSHER: return ghosts.ambusher_p->getGhostState();
        case FICKLE: return ghosts.fickle_p->getGhostState();
        default: return ghosts.stupid_p->getGhostState();
    }
}


grid_t GameState::getGrid() const {return maze_p->getGrid();}
int GameState::getGridWidth()const {return maze_p->getGridWidth();}
int GameState::getGridHeight() const {return maze_p->getGridHeight();}

GhostState GameState::getGlobalState() const {return this->globalState;}

void GameState::setInitialGhostStates() {
    this->updateGhostState(CHASER,ESCAPE);
    this->updateGhostState(AMBUSHER,SLEEP);
    this->updateGhostState(FICKLE,SLEEP);
    this->updateGhostState(STUPID,SLEEP);
}

void GameState::generateGhostMove(GhostType type){
    if (this->hasTimeElapsed()) this->switchToNextState();
    switch (type) {
    case CHASER: ghostAI.moveChaser(this->ghosts.chaser_p,this->pacman_p); break;
    case AMBUSHER: ghostAI.moveAmbusher(this->ghosts.ambusher_p,this->pacman_p); break;
    case FICKLE: ghostAI.moveFickle(this->ghosts.fickle_p, this->pacman_p, this->ghosts.chaser_p->getPos());break;
    case STUPID: ghostAI.moveStupid(this->ghosts.stupid_p,this->pacman_p);break;    
    default:
        break;
    }
}

void GameState::updatePacmanPos(Position pos) {
    this->pacman_p->setPos(pos);
}

void GameState::updateGhostPos(Position pos, GhostType type) {
    switch (type) {
        case CHASER: this->ghosts.chaser_p->setPos(pos); break;
        case AMBUSHER: this->ghosts.ambusher_p->setPos(pos); break;
        case STUPID: this->ghosts.stupid_p->setPos(pos); break;
        default: this->ghosts.fickle_p->setPos(pos); break;
    }
}

void GameState::updateGhostState(GhostType type, GhostState state) {
    switch (type) {
        case CHASER: this->ghosts.chaser_p->setGhostState(state); break;
        case AMBUSHER: this->ghosts.ambusher_p->setGhostState(state); break;
        case STUPID: this->ghosts.stupid_p->setGhostState(state); break;
        default: this->ghosts.fickle_p->setGhostState(state); break;
    }
}

bool GameState::validPacmanMove(Direction dir) const {

    Position pos = pacman_p->getPos();

    switch (dir) {
        case UP:
            pos.y -= 1;
            break;
        case DOWN:
            pos.y += 1;
            break;
        case RIGHT:
            pos.x += 1;
            break;
        case LEFT:
            pos.x -= 1;
            break;
        default:
            break;
    }    

    if (pos.x < 0 || pos.x >= maze_p->getGridWidth()) return false;
    if (pos.y < 0 || pos.y >= maze_p->getGridHeight()) return false;
    
    Cell cell = maze_p->getCell(pos);
    return cell != WALL && cell != DOOR;
}
void GameState::handleCollisions() {
    this->handlePelletCollision();
    this->handlePowerPelletCollision();
    this->checkPelletStatus();
    // this->handleGhostCollisions();
}
 
void GameState::handlePelletCollision() {
    Position pacPos = this->pacman_p->getPos();
    if (maze_p->getCell(pacPos) == PELLET) {
        maze_p->setCell(pacPos,EMPTY);
        score += 10;
        eatenPelletCount += 1;
        this->freeGhostHouseGhosts();
    }
}

void GameState::handlePowerPelletCollision() {
    Position pacPos = this->pacman_p->getPos();
    if (maze_p->getCell(pacPos) == POWER_PELLET) {
        maze_p->setCell(pacPos,EMPTY);
        score += 20;
        eatenPelletCount += 1;
        this->freeGhostHouseGhosts();
        this->updateGlobalState(FRIGHTENED);
    }
}

void GameState::checkPelletStatus() {
    if (this->eatenPelletCount == this->maze_p->getTotalPelletCount()) {
        this->gameOver = true;
        this->status = WON;
    }
}

void GameState::freeGhostHouseGhosts() {
    if (eatenPelletCount > 0 && this->ghosts.ambusher_p->getGhostState() == SLEEP) {this->updateGhostState(AMBUSHER,ESCAPE);}
    else if (eatenPelletCount > 30 && this->ghosts.fickle_p->getGhostState() == SLEEP){this->updateGhostState(FICKLE,ESCAPE);}
    else if (eatenPelletCount > 60 && this->ghosts.stupid_p->getGhostState() == SLEEP){this->updateGhostState(STUPID,ESCAPE);}
}

void GameState::ghostCollided(GhostType type) {
    switch (type) {
        case CHASER: handleGhostCollision(this->ghosts.chaser_p); break;    
        case AMBUSHER: handleGhostCollision(this->ghosts.ambusher_p); break;    
        case FICKLE: handleGhostCollision(this->ghosts.fickle_p); break;    
        case STUPID: handleGhostCollision(this->ghosts.stupid_p); break;    
        default: break;
    }
}


void GameState::handleGhostCollision(Ghost * ghost) {
    switch (ghost->getGhostState()){
            case SCATTER:
            case ESCAPE:
            case SLEEP:
            case CHASE: this->gameLost(); break;
            case TRANSITION:
            case FRIGHTENED: this->eatGhost(ghost);break;
        default: // eaten or asleep
            break;
    }
}


void GameState::gameLost() {
    this->gameOver = true;
    this->status = LOST;
}


void GameState::eatGhost(Ghost * ghost) {
    ghost->setGhostState(EATEN);
    this->score += 200;
    this->ghostsEatenCount += 1;
}

bool GameState::jumpAvail(Position pos) {   
    return (pos.y == MIDLE_ROW_INDEX && pos.x == 0) || (pos.y == MIDLE_ROW_INDEX && pos.x == maze_p->getGridWidth() - 1);
}
Position GameState::jumpPortal(Position pos) {
    if (pos.y == MIDLE_ROW_INDEX && pos.x == 0) {
        pos.x = maze_p->getGridWidth() - 2;
    } else if (pos.y == MIDLE_ROW_INDEX && pos.x == maze_p->getGridWidth() - 1) {
        pos.x = 1;
    }
    return pos;
}

void GameState::changePacmanDir(Direction dir) {
    this->pacman_p->setDir(dir);
}

// takes position and way agent is facing and returns Position behind agent
Position getReversePosition(Position pos, Direction dir) {
    switch (dir) {
        case UP: pos.y += 1; break;
        case DOWN: pos.y -= 1; break;
        case RIGHT: pos.x -= 1; break;
        case LEFT: pos.x += 1; break;
        default: break;
    }
    return pos;
}

std::vector<Position> GameState::getValidPositions(Position ghostPos, Direction ghostDir, bool hasEscaped) const {

    // get Empty neighbouring cells
    std::vector<Position> validNeighbours = this->getValidNeighbours(ghostPos, hasEscaped);
    // remove the one behind the ghost
    Position pos = getReversePosition(ghostPos, ghostDir);
    auto index = std::find(validNeighbours.begin(), validNeighbours.end(), pos);
    if (index != validNeighbours.end() && validNeighbours.size() > 1) {
        validNeighbours.erase(index); 
    }
    return validNeighbours;
}

std::vector<Position> GameState::getValidNeighbours(Position pos, bool hasEscaped) const {
    if (hasEscaped) return this->maze_p->getValidEscapedNeighbours(pos);
    return this->maze_p->getValidNeighbours(pos);
}

void GameState::updateGlobalState(GhostState newState) {

    static const std::unordered_map<GhostState, std::array<GhostState,2>> canSwitch = { // ghosts which are in the list of states mapped to each state key can switch to the state key.
        {CHASE, {SCATTER,TRANSITION}},
        {SCATTER, {CHASE}},
        {TRANSITION, {FRIGHTENED}},
        {FRIGHTENED,{SCATTER,CHASE}}
    };
    std::array<GhostState,2> plausibleStates = canSwitch.at(newState);
    for (Ghost * ghost: this->ghostArray) {
        if (std::find(plausibleStates.begin(), plausibleStates.end(), ghost->getGhostState()) != plausibleStates.end()){
            if (newState == TRANSITION && ghost->getGhostState() != FRIGHTENED) continue; // this case leaks through for some reason.
            ghost->setGhostState(newState);
        }
    }
    this->globalState = newState;
    this->startStateTimer();
}


bool GameState::isActive(Ghost * ghost) const {
    GhostState state =  ghost->getGhostState();
    return (state != SLEEP && state != ESCAPE && state != EATEN);
}

void GameState::startStateTimer() {
    this->stateStartFrame = this->frameCount;
}

bool GameState::hasTimeElapsed() const {
    // durations are counted in simulated frames, not wall-clock seconds, so the game
    // behaves identically whether it is rendered at 60 FPS or simulated headless at 100k FPS.
    static const std::unordered_map<int, int> globalStateDurations = {
        {CHASE, CHASE_DURATION_S * GAME_FPS},
        {SCATTER, SCATTER_DURATION_S * GAME_FPS},
        {FRIGHTENED, FRIGHTENED_DURATION_S * GAME_FPS},
        {TRANSITION, TRANSITION_DURATION_S * GAME_FPS},
    };
    return (long)(frameCount - stateStartFrame) >= globalStateDurations.at(this->globalState);
}

void GameState::switchToNextState() {
    if (this->globalState == CHASE) {
        this->updateGlobalState(SCATTER);
    } else if (this->globalState == SCATTER) {
        this->updateGlobalState(CHASE);
    } else if (this->globalState == FRIGHTENED) {
        this->updateGlobalState(TRANSITION);
    } else {
        this->updateGlobalState(CHASE);
    }
}


bool GameState::isGameOver() const {return this->gameOver;}
unsigned int GameState::getScore() const {return this->score;}

bool GameState::isGameLost() const {
    return this->status == LOST;
}
