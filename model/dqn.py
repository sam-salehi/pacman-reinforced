"""Network, replay buffer and reward definition for the Pacman DQN agent.

Design (vs. the original CNN/LSTM agents):
  * Input is the structured game state (8 binary maze planes + 3 scalars), not a blurred
    screenshot. Ghost *heading* planes encode motion, so no recurrence/frame stack is needed.
  * Dueling Double DQN with n-step returns and invalid-action masking.
  * Rewards are derived from game events and scaled to a range that suits the Huber loss.
"""
import os
import sys

import torch
import torch.nn as nn
import torch.nn.functional as F

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build"))
import pacman_sim  # noqa: E402  (headless C++ simulator)

N_ACTIONS = 4
OBS_PLANES = pacman_sim.OBS_PLANES
OBS_SCALARS = pacman_sim.OBS_SCALARS
OBS_H, OBS_W = pacman_sim.OBS_H, pacman_sim.OBS_W

# ---- reward function (computed from per-decision game events) ----
REWARDS = dict(pellet=1.0, power_pellet=2.0, ghost=5.0, died=-25.0, won=50.0, step=-0.1)
REWARD_SCALE = 0.1  # keeps returns O(10) so the Huber loss works in its quadratic regime


def compute_rewards(events):
    """events: int array [N, 6] (pellets, power_pellets, ghosts, died, won, frames) -> float32 [N]."""
    e = torch.as_tensor(events, dtype=torch.float32)
    r = (REWARDS["pellet"] * e[:, 0] + REWARDS["power_pellet"] * e[:, 1] + REWARDS["ghost"] * e[:, 2]
         + REWARDS["died"] * e[:, 3] + REWARDS["won"] * e[:, 4] + REWARDS["step"])
    return r * REWARD_SCALE


class PacmanDQN(nn.Module):
    """Dueling convolutional Q-network over the full 21x21 maze."""

    def __init__(self, channels=64, hidden=512):
        super().__init__()
        c_in = OBS_PLANES + OBS_SCALARS  # scalars are also broadcast as constant planes
        self.conv = nn.Sequential(
            nn.Conv2d(c_in, channels, 3, padding=1), nn.ReLU(),
            nn.Conv2d(channels, channels, 3, padding=1), nn.ReLU(),
            nn.Conv2d(channels, channels, 3, padding=1), nn.ReLU(),
            nn.Conv2d(channels, channels, 3, stride=2, padding=1), nn.ReLU(),  # 21x21 -> 11x11
            nn.Flatten(),
        )
        flat = channels * ((OBS_H + 1) // 2) * ((OBS_W + 1) // 2)
        self.fc = nn.Sequential(nn.Linear(flat + OBS_SCALARS, hidden), nn.ReLU())
        self.value = nn.Sequential(nn.Linear(hidden, 256), nn.ReLU(), nn.Linear(256, 1))
        self.advantage = nn.Sequential(nn.Linear(hidden, 256), nn.ReLU(), nn.Linear(256, N_ACTIONS))

    def forward(self, planes, scalars):
        """planes: uint8/float [B, P, H, W]; scalars: float [B, S] -> Q-values [B, 4]."""
        x = planes.float()
        s = scalars.float()
        x = torch.cat([x, s[:, :, None, None].expand(-1, -1, x.shape[2], x.shape[3])], dim=1)
        h = self.fc(torch.cat([self.conv(x), s], dim=1))
        v, a = self.value(h), self.advantage(h)
        return v + a - a.mean(dim=1, keepdim=True)


def masked_argmax(q, mask):
    return q.masked_fill(~mask, float("-inf")).argmax(dim=1)


class ReplayBuffer:
    """Per-environment ring buffer stored on the GPU, sampling n-step transitions.

    Slot k % T of env e holds observation k and the action/reward/done that followed it,
    so the n-step bootstrap observation is simply slot (k + n) % T.
    """

    def __init__(self, capacity_per_env, num_envs, n_step, gamma, device):
        T, N = capacity_per_env, num_envs
        self.T, self.N, self.n, self.gamma, self.device = T, N, n_step, gamma, device
        self.planes = torch.zeros((T, N, OBS_PLANES, OBS_H, OBS_W), dtype=torch.uint8, device=device)
        self.scalars = torch.zeros((T, N, OBS_SCALARS), dtype=torch.float16, device=device)
        self.masks = torch.zeros((T, N, N_ACTIONS), dtype=torch.bool, device=device)
        self.actions = torch.zeros((T, N), dtype=torch.long, device=device)
        self.rewards = torch.zeros((T, N), dtype=torch.float32, device=device)
        self.dones = torch.zeros((T, N), dtype=torch.bool, device=device)
        self.g = 0  # number of completed transitions per env
        self.discounts = gamma ** torch.arange(n_step, device=device, dtype=torch.float32)

    def __len__(self):
        return min(self.g, self.T - 1) * self.N

    def store_obs(self, planes, scalars, masks):
        s = self.g % self.T
        self.planes[s] = planes
        self.scalars[s] = scalars.half()
        self.masks[s] = masks

    def store_step(self, actions, rewards, dones):
        s = self.g % self.T
        self.actions[s] = actions
        self.rewards[s] = rewards
        self.dones[s] = dones
        self.g += 1

    def sample(self, batch_size):
        lo = max(0, self.g - self.T + 1)
        hi = self.g - self.n  # inclusive upper bound so that obs k+n exists
        k = torch.randint(lo, hi + 1, (batch_size,), device=self.device)
        e = torch.randint(0, self.N, (batch_size,), device=self.device)
        slot = k % self.T
        win = (k[:, None] + torch.arange(self.n, device=self.device)[None]) % self.T  # [B, n]
        r = self.rewards[win, e[:, None]]
        d = self.dones[win, e[:, None]].float()
        alive = torch.cumprod(torch.cat([torch.ones_like(d[:, :1]), 1 - d[:, :-1]], dim=1), dim=1)
        ret = (r * alive * self.discounts).sum(1)
        terminal = (d * alive).sum(1) > 0  # an episode end occurred inside the window
        nslot = (k + self.n) % self.T
        return dict(
            planes=self.planes[slot, e], scalars=self.scalars[slot, e].float(),
            actions=self.actions[slot, e], returns=ret,
            next_planes=self.planes[nslot, e], next_scalars=self.scalars[nslot, e].float(),
            next_masks=self.masks[nslot, e],
            bootstrap=(~terminal).float() * (self.gamma ** self.n),
        )


def dqn_loss(online, target, batch):
    q = online(batch["planes"], batch["scalars"]).float().gather(1, batch["actions"][:, None]).squeeze(1)
    with torch.no_grad():
        # Double DQN: online net picks the action, target net evaluates it
        next_a = masked_argmax(online(batch["next_planes"], batch["next_scalars"]), batch["next_masks"])
        next_q = target(batch["next_planes"], batch["next_scalars"]).float().gather(1, next_a[:, None]).squeeze(1)
        y = batch["returns"] + batch["bootstrap"] * next_q
    return F.smooth_l1_loss(q, y), q.detach()


def load_policy(path, device):
    import numpy as np
    # allow numpy scalars in checkpoint metadata while still refusing arbitrary pickled code
    with torch.serialization.safe_globals([np._core.multiarray.scalar, np.dtype, np.dtypes.Float64DType]):
        ckpt = torch.load(path, map_location=device, weights_only=True)
    net = PacmanDQN(**ckpt.get("net_kwargs", {})).to(device)
    net.load_state_dict(ckpt["model"])
    net.eval()
    return net
