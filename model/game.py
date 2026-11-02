"""Play Pacman yourself with the arrow keys:  python game.py"""
from ui import import_pacman_ui

pacman = import_pacman_ui()

if __name__ == "__main__":
    import random
    display = pacman.Display(random.getrandbits(32))
    while display.running():
        display.update()
        display.render()
        if display.game_over():
            print("You won!" if display.game_won() else "Game over.", "Score:", display.get_score())
            break
