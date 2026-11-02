"""Watch a trained agent play in the SFML window.

    python watch.py runs/dqn/best.pth [--games 3] [--fast]
"""
import argparse
import random

from ui import import_pacman_ui

pacman = import_pacman_ui()

import torch  # noqa: E402  (imported after the UI module so the right libstdc++ is loaded first)

from dqn import load_policy, masked_argmax  # noqa: E402


def main():
    p = argparse.ArgumentParser()
    p.add_argument("weights")
    p.add_argument("--games", type=int, default=3)
    p.add_argument("--fast", action="store_true", help="don't limit to 60 FPS")
    p.add_argument("--seed", type=int, default=None)
    args = p.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    net = load_policy(args.weights, device)
    rng = random.Random(args.seed)
    for g in range(args.games):
        display = pacman.Display(rng.getrandbits(32), not args.fast)
        while display.running() and not display.game_over():
            if display.pacman_contained():
                planes, scalars = display.observation()
                mask = torch.from_numpy(display.valid_actions()).bool()[None].to(device)
                with torch.no_grad():
                    q = net(torch.from_numpy(planes)[None].to(device), torch.from_numpy(scalars)[None].to(device))
                display.step(int(masked_argmax(q.float(), mask).item()))
            display.update()
            display.render()
        print(f"game {g + 1}: score {display.get_score()}  {'CLEARED' if display.game_won() else 'died'}"
              f"  pellets left {display.pellets_remaining()}")
        display.close()


if __name__ == "__main__":
    main()
