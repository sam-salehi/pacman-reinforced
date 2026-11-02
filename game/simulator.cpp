#include <cstring>
#include <stdexcept>

#include "simulator.hpp"

// start cells (column index includes the hidden tunnel column 0)
static const Position PACMAN_START(4, 13);
static const Position CHASER_START(9, 7);
static const Position AMBUSHER_START(10, 9);
static const Position FICKLE_START(9, 9);
static const Position STUPID_START(11, 9);

static const Direction ACTION_TO_DIR[4] = {UP, DOWN, LEFT, RIGHT};

void Body::advance(int step) {
    switch (dir) {
        case UP:    y -= step; break;
        case DOWN:  y += step; break;
        case LEFT:  x -= step; break;
        case RIGHT: x += step; break;
        default: break;
    }
}

Simulator::Simulator(uint32_t seed) { reset(seed); }

void Simulator::reset(uint32_t seed) {
    gs.reset(new GameState(seed));
    frames = 0;
    decisions = 0;

    pacman = Body();
    pacman.setCell(PACMAN_START);
    pacman.dir = IDLE;
    pacmanNextDir = RIGHT;
    gs->updatePacmanPos(pacman.cell());

    const GhostType types[4] = {CHASER, AMBUSHER, FICKLE, STUPID};
    const Position starts[4] = {CHASER_START, AMBUSHER_START, FICKLE_START, STUPID_START};
    for (int i = 0; i < 4; ++i) {
        ghosts[i] = GhostBody();
        ghosts[i].type = types[i];
        ghosts[i].setCell(starts[i]);
        ghosts[i].dir = IDLE;
        ghosts[i].moveState = SLEEP;
        gs->updateGhostPos(starts[i], types[i]);
    }
}

const GhostBody& Simulator::ghostBody(GhostType t) const {
    switch (t) {
        case CHASER: return ghosts[0];
        case AMBUSHER: return ghosts[1];
        case FICKLE: return ghosts[2];
        default: return ghosts[3];
    }
}

int Simulator::ghostStepSize(GhostState s) {
    switch (s) {
        case FRIGHTENED:
        case TRANSITION: return GHOST_FRIGHTENED_SPEED / GAME_FPS;
        case EATEN: return GHOST_EATEN_SPEED / GAME_FPS;
        default: return GHOST_CHASE_SPEED / GAME_FPS;
    }
}

void Simulator::movePacman() {
    // pacman can only turn at a cell centre
    if (pacman.contained() && pacmanNextDir != pacman.dir && gs->validPacmanMove(pacmanNextDir)) {
        pacman.dir = pacmanNextDir;
        gs->changePacmanDir(pacman.dir);
    }
    if (gs->validPacmanMove(pacman.dir)) pacman.advance(PACMAN_SPEED / GAME_FPS);
    if (pacman.contained()) gs->updatePacmanPos(pacman.cell());
}

void Simulator::updateGhost(GhostBody& g) {
    if (!g.contained()) return;
    gs->generateGhostMove(g.type);
    g.dir = gs->getGhostDir(g.type);
    g.moveState = gs->getGhostState(g.type);
}

void Simulator::moveGhost(GhostBody& g) {
    g.advance(ghostStepSize(g.moveState));
    if (g.contained()) gs->updateGhostPos(g.cell(), g.type);
}

void Simulator::checkGhostCollisions() {
    for (GhostBody& g : ghosts) {
        if (std::abs(pacman.x - g.x) < CELL_PX && std::abs(pacman.y - g.y) < CELL_PX) {
            gs->ghostCollided(g.type);
        }
    }
}

void Simulator::handleTeleport(Body& b, bool isPacman, GhostType type) {
    Position c = b.cell();
    if (!gs->jumpAvail(c)) return;
    Position dest = gs->jumpPortal(c);
    b.setCell(dest);
    // keep the logical position in sync immediately (previously it lagged one frame,
    // which made pacman freeze for a frame after every tunnel jump)
    if (isPacman) gs->updatePacmanPos(dest);
    else gs->updateGhostPos(dest, type);
}

StepEvents Simulator::frame() {
    StepEvents ev;
    if (gs->isGameOver()) return ev;

    const unsigned int eatenBefore = gs->getEatenPelletCount();
    const unsigned int ghostsBefore = gs->getGhostsEatenCount();
    const unsigned int scoreBefore = gs->getScore();

    gs->tick();
    frames += 1;
    ev.frames = 1;

    // identical ordering to the original Display::update
    movePacman();
    for (GhostBody& g : ghosts) updateGhost(g);
    for (GhostBody& g : ghosts) moveGhost(g);
    gs->handleCollisions();
    checkGhostCollisions();
    handleTeleport(pacman, true, CHASER);
    for (GhostBody& g : ghosts) handleTeleport(g, false, g.type);

    const int dEaten = (int)(gs->getEatenPelletCount() - eatenBefore);
    ev.ghosts = (int)(gs->getGhostsEatenCount() - ghostsBefore);
    const int pelletScore = (int)(gs->getScore() - scoreBefore) - 200 * ev.ghosts;  // 10 per pellet, 20 per power pellet
    ev.powerPellets = (pelletScore - 10 * dEaten) / 10;
    ev.pellets = dEaten - ev.powerPellets;
    if (gs->isGameOver()) {
        ev.won = gs->isGameWon();
        ev.died = !ev.won;
    }
    return ev;
}

StepEvents Simulator::act(int action) {
    if (action < 0 || action > 3) throw std::invalid_argument("action must be in [0, 3]");
    pacmanNextDir = ACTION_TO_DIR[action];
    decisions += 1;

    StepEvents total;
    const int maxFrames = 10 * CELL_PX;  // safety bound; a cell normally takes CELL_PX/5 = 6 frames
    do {
        StepEvents e = frame();
        total.frames += e.frames;
        total.pellets += e.pellets;
        total.powerPellets += e.powerPellets;
        total.ghosts += e.ghosts;
        total.died = total.died || e.died;
        total.won = total.won || e.won;
    } while (!gs->isGameOver() && !pacman.contained() && total.frames < maxFrames);
    return total;
}

bool Simulator::validAction(int action) const {
    if (action < 0 || action > 3) return false;
    return gs->validPacmanMove(ACTION_TO_DIR[action]);
}

void Simulator::validActions(uint8_t* out4) const {
    for (int a = 0; a < 4; ++a) out4[a] = validAction(a) ? 1 : 0;
}

static inline bool inBounds(Position p) {
    return p.x >= 0 && p.x < OBS_W && p.y >= 0 && p.y < OBS_H;
}

static inline Position stepFrom(Position p, Direction d) {
    switch (d) {
        case UP: p.y -= 1; break;
        case DOWN: p.y += 1; break;
        case LEFT: p.x -= 1; break;
        case RIGHT: p.x += 1; break;
        default: break;
    }
    return p;
}

void Simulator::writeObservation(uint8_t* planes, float* scalars) const {
    const int HW = OBS_H * OBS_W;
    std::memset(planes, 0, OBS_PLANES * HW);
    auto at = [&](int plane, Position p) -> uint8_t& { return planes[plane * HW + p.y * OBS_W + p.x]; };

    for (int y = 0; y < OBS_H; ++y) {
        for (int x = 0; x < OBS_W; ++x) {
            Position p(x, y);
            switch (gs->getCell(p)) {
                case WALL:
                case DOOR: at(PLANE_WALL, p) = 1; break;
                case PELLET: at(PLANE_PELLET, p) = 1; break;
                case POWER_PELLET: at(PLANE_POWER_PELLET, p) = 1; break;
                default: break;
            }
        }
    }

    Position pc = pacman.cell();
    if (inBounds(pc)) at(PLANE_PACMAN, pc) = 1;

    for (const GhostBody& g : ghosts) {
        GhostState s = gs->getGhost(g.type)->getGhostState();
        if (s == EATEN) continue;  // harmless eyes returning home
        const bool edible = (s == FRIGHTENED || s == TRANSITION);
        const int curPlane = edible ? PLANE_EDIBLE_GHOST : PLANE_DANGER_GHOST;
        const int nextPlane = edible ? PLANE_EDIBLE_GHOST_NEXT : PLANE_DANGER_GHOST_NEXT;
        Position c = g.cell();
        if (inBounds(c)) at(curPlane, c) = 1;
        if (g.dir != IDLE) {
            Position n = stepFrom(c, g.dir);
            if (inBounds(n)) at(nextPlane, n) = 1;
        }
    }

    const float maxFright = (float)((FRIGHTENED_DURATION_S + TRANSITION_DURATION_S) * GAME_FPS);
    scalars[0] = (float)gs->frightenedFramesLeft() / maxFright;
    scalars[1] = (float)pelletsRemaining() / (float)gs->getTotalPelletCount();
    scalars[2] = gs->getGlobalState() == CHASE ? 1.f : 0.f;
}
