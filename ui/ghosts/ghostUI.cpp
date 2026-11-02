
#include <cmath>
#include <iostream>
#include <string>
#include <SFML/Graphics.hpp>
#include "ghostUI.hpp"
#include "speeds.hpp"

#ifndef ASSETS_DIR
#define ASSETS_DIR "ui/animations"
#endif


GhostUI::GhostUI(sf::Vector2f pos, sf::Color defaultColor, GhostType type) : AgentUI(pos){
    this->ghostType = type;
    this->animationSpeed = 0.1f;
    this->frame = 0;
    this->face = new sf::Sprite();
    this->defaultColor = defaultColor;
    this->dir = IDLE;
    this->state = SLEEP;

    if (!this->texture->loadFromFile(std::string(ASSETS_DIR) + "/GhostAnimation.png")) {
        std::cerr << "Failed to load Ghost animation texture!" << std::endl;
        return;
    }
    this->sprite->setTexture(*this->texture);
    this->face->setTexture(*this->texture);

}

sf::Sprite GhostUI::getFace() {
    return *this->face;
}

void GhostUI::render(GhostState state, Direction ghostDir) {
    // if direction is new, clip position?
    this->setPositionForRendering();
    this->setOrientationForRendering();
    this->setFacePositionForRendering();
    this->setFaceOrientationForRendering(state,ghostDir);
    this->setBodyColorForRendering(state);
}

void GhostUI::setOrientationForRendering() {
    if (this->animationClock.getElapsedTime().asSeconds() > this->animationSpeed) {
        this->sprite->setTextureRect(sf::IntRect(FRAME_SIZE * this->frame, 0, FRAME_SIZE, FRAME_SIZE));
        this->scaleSprite();
        this->nextFrame();
        this->animationClock.restart();
    }
}

void GhostUI::setBodyColorForRendering(GhostState state) {
    sf::Color frightenedBlue(0, 0, 255); // Pure blue
    sf::Color opaqueBlack = sf::Color::Black;
    opaqueBlack.a = 0;
    switch (state) {
        case TRANSITION: {
            if (this->sprite->getColor() == frightenedBlue) {
                this->sprite->setColor(sf::Color::White);
            } else {
                this->sprite->setColor(frightenedBlue);
            }
            break;
        }
        case FRIGHTENED: this->sprite->setColor(frightenedBlue); break;
        case EATEN: this->sprite->setColor(opaqueBlack); break;
        default:    this->sprite->setColor(this->defaultColor); break;
    }
}

int GhostUI::getRowIndex(Direction ghostDir) {
    switch (ghostDir) {
        case RIGHT: return 0;
        case UP: return 1;
        case LEFT: return 2;
        default: return 3;
    }
}

// must be called after rendering Sprite body
void GhostUI::setFacePositionForRendering() {
    this->face->setPosition(this->SFposition.x - PIXEL_SIZE,this->SFposition.y);
}
void GhostUI::setFaceOrientationForRendering(GhostState state, Direction ghostDir) {
    switch (state){
    case EATEN:this->face->setTextureRect(sf::IntRect(FRAME_SIZE * 4, FRAME_SIZE * 1, FRAME_SIZE, FRAME_SIZE));break;
    case TRANSITION:
    case FRIGHTENED: this->face->setTextureRect(sf::IntRect(FRAME_SIZE * this->getRowIndex(ghostDir), FRAME_SIZE * 2, FRAME_SIZE, FRAME_SIZE));break;
    default:
        this->face->setTextureRect(sf::IntRect(FRAME_SIZE * this->getRowIndex(ghostDir), FRAME_SIZE * 1, FRAME_SIZE, FRAME_SIZE));
    }
    float scale = PIXEL_SIZE / FRAME_SIZE;
    this->face->setScale(scale,scale);
}

void GhostUI::nextFrame() {
    this->frame = (this->frame + 1) % 6;
}


GhostType GhostUI::getType() const {
    return this->ghostType;
}


// Ghost Subclasses constructors

ChaserUI::ChaserUI() : GhostUI(sf::Vector2f(CHASER_START_X,CHASER_START_Y),sf::Color::Red,CHASER){}


FickleUI::FickleUI() : GhostUI(sf::Vector2f(FICKLE_START_X,FICKLE_START_Y),sf::Color::Cyan,FICKLE){
    this->sprite->setColor(sf::Color::Cyan);
}


sf::Color clydeColor(255, 165, 0);

StupidUI::StupidUI() : GhostUI(sf::Vector2f(STUPID_START_X,STUPID_START_Y),clydeColor,STUPID){}

AmbusherUI::AmbusherUI() : GhostUI(sf::Vector2f(AMBUSHER_START_X,AMBUSHER_START_Y),sf::Color::Magenta,AMBUSHER){    }
