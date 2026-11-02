"""Headless greedy evaluation of a trained agent.

    python evaluate.py runs/dqn/best.pth --episodes 1000
"""
import argparse

import numpy as np
import torch

from train_dqn import evaluate
from dqn import load_policy


def main():
    p = argparse.ArgumentParser()
    p.add_argument("weights")
    p.add_argument("--episodes", type=int, default=1000)
    p.add_argument("--seed", type=int, default=12345)
    p.add_argument("--max-decisions", type=int, default=3000)
    args = p.parse_args()
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    net = load_policy(args.weights, device)
    r = evaluate(net, args.episodes, device, args.seed, args.max_decisions)
    print(f"{args.episodes} games | level cleared {100 * r['win_rate']:.1f}% | mean score {r['mean_score']:.0f}"
          f" | median score {r['median_score']:.0f} | mean decisions {r['mean_len']:.0f}")


if __name__ == "__main__":
    main()
