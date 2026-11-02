"""Record a trained agent playing to a GIF.

    python record.py runs/dqn/best.pth ../images/agent.gif [--require-clear] [--frame-skip 3]

The simulator is deterministic for a given seed, so with --require-clear the script first
plays seeds headlessly until it finds one the agent clears, then records that game in the
SFML window.
"""
import argparse

from ui import import_pacman_ui

pacman = import_pacman_ui()

import numpy as np  # noqa: E402
import torch  # noqa: E402
from PIL import Image  # noqa: E402

from dqn import load_policy, masked_argmax, pacman_sim  # noqa: E402

GAME_W, GAME_H = 570, 630  # rendered maze size in pixels (top-left of the window)


def greedy(net, planes, scalars, mask, device):
    with torch.no_grad():
        q = net(torch.from_numpy(planes)[None].to(device), torch.from_numpy(scalars)[None].to(device)).float()
    return int(masked_argmax(q, torch.from_numpy(mask).bool()[None].to(device)).item())


def play_headless(net, seed, device):
    s = pacman_sim.Simulator(seed)
    while not s.game_over() and s.decisions() < 3000:
        planes, scalars = s.observation()
        s.act(greedy(net, planes, scalars, s.valid_actions(), device))
    return s.game_won(), s.score()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("weights")
    p.add_argument("out")
    p.add_argument("--seed", type=int, default=None, help="game seed (default: search from 0)")
    p.add_argument("--require-clear", action="store_true", help="search for a seed the agent clears")
    p.add_argument("--frame-skip", type=int, default=3, help="keep every n-th frame (60 FPS game)")
    p.add_argument("--scale", type=float, default=0.67)
    args = p.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    net = load_policy(args.weights, device)

    seed = args.seed if args.seed is not None else 0
    if args.require_clear:
        while True:
            won, score = play_headless(net, seed, device)
            print(f"seed {seed}: score {score} {'CLEARED' if won else 'died'}")
            if won:
                break
            seed += 1

    display = pacman.Display(seed, False)
    frames, n = [], 0
    while display.running() and not display.game_over():
        if display.pacman_contained():
            planes, scalars = display.observation()
            display.step(greedy(net, planes, scalars, display.valid_actions(), device))
        display.update()
        display.render()
        if n % args.frame_skip == 0:
            frames.append(display.get_screenshot()[:GAME_H, :GAME_W, :3].copy())
        n += 1
    display.render()
    final = display.get_screenshot()[:GAME_H, :GAME_W, :3].copy()
    frames += [final] * 30  # hold the final frame for a moment
    result = f"score {display.get_score()}, {'CLEARED' if display.game_won() else 'died'}"
    display.close()

    size = (int(GAME_W * args.scale), int(GAME_H * args.scale))
    resized = [Image.fromarray(f).resize(size, Image.LANCZOS) for f in frames]
    # one shared palette built from frames spread over the whole game keeps colours stable
    sample = resized[:: max(1, len(resized) // 16)]
    montage = Image.new("RGB", (size[0], size[1] * len(sample)))
    for i, im in enumerate(sample):
        montage.paste(im, (0, i * size[1]))
    palette = montage.quantize(colors=96, method=Image.Quantize.MEDIANCUT)
    imgs = [im.quantize(palette=palette, dither=Image.Dither.NONE) for im in resized]
    imgs[0].save(args.out, save_all=True, append_images=imgs[1:], optimize=True, loop=0,
                 duration=int(1000 * args.frame_skip / 60))
    print(f"recorded seed {seed}: {result}; {len(frames)} frames -> {args.out}")


if __name__ == "__main__":
    main()
