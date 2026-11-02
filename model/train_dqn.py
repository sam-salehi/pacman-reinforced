"""Train a Dueling Double DQN (n-step, action-masked) on the headless Pacman simulator.

    python train_dqn.py --steps 30_000_000 --run runs/dqn

Writes checkpoints (latest.pth / best.pth), a CSV log and a progress plot into the run dir.
"""
import argparse
import csv
import math
import os
import time

import numpy as np
import torch

from dqn import PacmanDQN, ReplayBuffer, compute_rewards, dqn_loss, masked_argmax, pacman_sim


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--run", default="runs/dqn")
    p.add_argument("--steps", type=int, default=30_000_000, help="total environment decisions")
    p.add_argument("--envs", type=int, default=64)
    p.add_argument("--buffer-per-env", type=int, default=8192)
    p.add_argument("--batch", type=int, default=256)
    p.add_argument("--updates-per-step", type=int, default=2, help="gradient steps per vector env step")
    p.add_argument("--lr", type=float, default=2.5e-4)
    p.add_argument("--gamma", type=float, default=0.99)
    p.add_argument("--n-step", type=int, default=3)
    p.add_argument("--target-update", type=int, default=2000, help="gradient steps between target syncs")
    p.add_argument("--eps-start", type=float, default=1.0)
    p.add_argument("--eps-end", type=float, default=0.01)
    p.add_argument("--eps-decay-steps", type=int, default=4_000_000)
    p.add_argument("--learning-starts", type=int, default=50_000)
    p.add_argument("--max-decisions", type=int, default=3000, help="episode time limit (decisions)")
    p.add_argument("--eval-every", type=int, default=500_000)
    p.add_argument("--eval-episodes", type=int, default=200)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--resume", default=None, help="checkpoint to initialise weights from")
    return p.parse_args()


def to_torch(planes, scalars, masks, device):
    return (torch.from_numpy(planes).to(device, non_blocking=True),
            torch.from_numpy(scalars).to(device, non_blocking=True),
            torch.from_numpy(masks).to(device, non_blocking=True).bool())


@torch.no_grad()
def act(net, planes, scalars, masks, eps):
    q = net(planes, scalars).float()
    greedy = masked_argmax(q, masks)
    if eps <= 0:
        return greedy
    rand = torch.multinomial(masks.float(), 1).squeeze(1)  # uniform over valid actions
    explore = torch.rand(greedy.shape[0], device=greedy.device) < eps
    return torch.where(explore, rand, greedy)


@torch.no_grad()
def evaluate(net, episodes, device, seed, max_decisions, num_envs=100):
    """Greedy evaluation on fresh games. Returns dict(win_rate, mean_score, median_score, mean_len)."""
    net.eval()
    env = pacman_sim.BatchSim(num_envs, seed, max_decisions)
    planes, scalars, masks = to_torch(*env.reset(), device)
    results = []
    while len(results) < episodes:
        a = act(net, planes, scalars, masks, eps=0.0)
        out = env.step(a.cpu().numpy())
        planes, scalars, masks = to_torch(out[0], out[1], out[2], device)
        done, score, length, won = out[4], out[6], out[7], out[8]
        for i in np.flatnonzero(done):
            results.append((score[i], length[i], won[i]))
    net.train()
    r = np.array(results[:episodes], dtype=np.float64)
    return dict(win_rate=float(r[:, 2].mean()), mean_score=float(r[:, 0].mean()),
                median_score=float(np.median(r[:, 0])), mean_len=float(r[:, 1].mean()))


def save(path, net, args, extra=None):
    torch.save(dict(model=net.state_dict(), net_kwargs={}, args=vars(args), **(extra or {})), path)


def plot_progress(log_path, out_path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    rows = list(csv.DictReader(open(log_path)))
    if not rows:
        return
    steps = [int(r["steps"]) / 1e6 for r in rows]
    fig, ax = plt.subplots(1, 2, figsize=(11, 4))
    ax[0].plot(steps, [float(r["eval_mean_score"]) for r in rows], label="greedy eval")
    ax[0].plot(steps, [float(r["train_mean_score"]) for r in rows], label="training (ε-greedy)", alpha=0.6)
    ax[0].set_xlabel("environment steps (M)"); ax[0].set_ylabel("score"); ax[0].legend()
    ax[1].plot(steps, [100 * float(r["eval_win_rate"]) for r in rows])
    ax[1].set_xlabel("environment steps (M)"); ax[1].set_ylabel("level cleared (%)"); ax[1].set_ylim(0, 100)
    fig.tight_layout(); fig.savefig(out_path, dpi=120); plt.close(fig)


def main():
    args = parse_args()
    os.makedirs(args.run, exist_ok=True)
    torch.manual_seed(args.seed)
    np.random.seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else
                          "mps" if torch.backends.mps.is_available() else "cpu")
    torch.backends.cudnn.benchmark = True
    torch.backends.cuda.matmul.allow_tf32 = True
    torch.backends.cudnn.allow_tf32 = True

    online = PacmanDQN().to(device)
    if args.resume:
        online.load_state_dict(torch.load(args.resume, map_location=device, weights_only=True)["model"])
    target = PacmanDQN().to(device)
    target.load_state_dict(online.state_dict())
    target.eval()
    opt = torch.optim.Adam(online.parameters(), lr=args.lr, eps=1.5e-4)

    env = pacman_sim.BatchSim(args.envs, args.seed, args.max_decisions)
    buf = ReplayBuffer(args.buffer_per_env, args.envs, args.n_step, args.gamma, device)
    planes, scalars, masks = to_torch(*env.reset(), device)
    buf.store_obs(planes, scalars, masks)

    log_path = os.path.join(args.run, "log.csv")
    log_f = open(log_path, "w", newline="")
    log = csv.writer(log_f)
    log.writerow(["steps", "updates", "epsilon", "loss", "mean_q", "train_mean_score", "train_win_rate",
                  "eval_win_rate", "eval_mean_score", "eval_median_score", "eval_mean_len", "sps"])

    steps, updates, next_eval = 0, 0, args.eval_every
    best = (-1.0, -1.0)
    ep_scores, ep_wins, losses, qs = [], [], [], []
    t0, t_last, steps_last = time.time(), time.time(), 0

    while steps < args.steps:
        eps = max(args.eps_end, args.eps_start - (args.eps_start - args.eps_end) * steps / args.eps_decay_steps)
        a = act(online, planes, scalars, masks, eps)
        out = env.step(a.cpu().numpy())
        n_planes, n_scalars, n_masks = to_torch(out[0], out[1], out[2], device)
        events, done = out[3], out[4]
        rewards = compute_rewards(events).to(device)
        buf.store_step(a, rewards, torch.from_numpy(done).to(device))
        buf.store_obs(n_planes, n_scalars, n_masks)
        planes, scalars, masks = n_planes, n_scalars, n_masks
        steps += args.envs
        for i in np.flatnonzero(done):
            ep_scores.append(out[6][i]); ep_wins.append(out[8][i])

        if len(buf) >= args.learning_starts:
            for _ in range(args.updates_per_step):
                with torch.autocast(device.type, dtype=torch.bfloat16, enabled=device.type == "cuda"):
                    loss, q = dqn_loss(online, target, buf.sample(args.batch))
                opt.zero_grad(set_to_none=True)
                loss.backward()
                torch.nn.utils.clip_grad_norm_(online.parameters(), 10.0)
                opt.step()
                updates += 1
                if updates % args.target_update == 0:
                    target.load_state_dict(online.state_dict())
                if updates % 50 == 0:
                    losses.append(loss.item()); qs.append(q.mean().item())

        if steps >= next_eval or steps >= args.steps:
            next_eval += args.eval_every
            ev = evaluate(online, args.eval_episodes, device, seed=10_000 + steps, max_decisions=args.max_decisions)
            now = time.time()
            sps = (steps - steps_last) / (now - t_last)
            t_last, steps_last = now, steps
            tr_score = float(np.mean(ep_scores)) if ep_scores else 0.0
            tr_win = float(np.mean(ep_wins)) if ep_wins else 0.0
            row = [steps, updates, round(eps, 4), round(float(np.mean(losses)) if losses else math.nan, 5),
                   round(float(np.mean(qs)) if qs else math.nan, 3), round(tr_score, 1), round(tr_win, 4),
                   round(ev["win_rate"], 4), round(ev["mean_score"], 1), ev["median_score"],
                   round(ev["mean_len"], 1), round(sps)]
            log.writerow(row); log_f.flush()
            print(f"[{(now - t0) / 60:6.1f} min] steps {steps / 1e6:6.2f}M  eps {eps:.3f}  loss {row[3]}  Q {row[4]}"
                  f" | train score {tr_score:6.0f} win {100 * tr_win:5.1f}%"
                  f" | eval score {ev['mean_score']:6.0f} (median {ev['median_score']:.0f})"
                  f" win {100 * ev['win_rate']:5.1f}%  len {ev['mean_len']:.0f} | {sps:.0f} steps/s", flush=True)
            ep_scores, ep_wins, losses, qs = [], [], [], []
            save(os.path.join(args.run, "latest.pth"), online, args, dict(steps=steps, eval=ev))
            if (ev["win_rate"], ev["mean_score"]) > best:
                best = (ev["win_rate"], ev["mean_score"])
                save(os.path.join(args.run, "best.pth"), online, args, dict(steps=steps, eval=ev))
            plot_progress(log_path, os.path.join(args.run, "progress.png"))

    log_f.close()
    print(f"done. best eval win rate {100 * best[0]:.1f}% (mean score {best[1]:.0f})")


if __name__ == "__main__":
    main()
