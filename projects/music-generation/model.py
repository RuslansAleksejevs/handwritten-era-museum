"""Modern next-event GRU and an interpolated Markov baseline.

Both model P(next pitch, next duration | prefix), including rests. The GRU
factorizes this as P(pitch | prefix) P(duration | prefix, next pitch).
"""
from collections import Counter, defaultdict
import numpy as np
import torch
from torch import nn
from corpus import PITCHES, DURATIONS, PROMPT, encode


class MelodyGRU(nn.Module):
    def __init__(self, hidden=128, layers=2, dropout=.2):
        super().__init__()
        self.config = dict(hidden=hidden, layers=layers, dropout=dropout)
        self.pitch = nn.Embedding(PITCHES, 32)
        self.duration = nn.Embedding(DURATIONS, 16)
        self.phase = nn.Embedding(32, 8)
        self.mode = nn.Embedding(2, 4)
        self.meter = nn.Embedding(33, 4)
        self.dropout = nn.Dropout(dropout)
        self.gru = nn.GRU(64, hidden, layers, batch_first=True,
                          dropout=dropout if layers > 1 else 0)
        self.pitch_head = nn.Linear(hidden, PITCHES)
        self.duration_head = nn.Sequential(nn.Linear(hidden + 32, 64), nn.GELU(),
                                           nn.Linear(64, DURATIONS))

    def forward(self, tokens, phase, mode, meter, hidden=None):
        shape = tokens.shape[:2]
        x = torch.cat((self.pitch(tokens[..., 0]), self.duration(tokens[..., 1]),
                       self.phase(phase), self.mode(mode).unsqueeze(1).expand(*shape, 4),
                       self.meter(meter).unsqueeze(1).expand(*shape, 4)), dim=-1)
        states, hidden = self.gru(self.dropout(x), hidden)
        return self.pitch_head(states), states, hidden

    def duration_logits(self, states, target_pitch):
        return self.duration_head(torch.cat((states, self.pitch(target_pitch)), dim=-1))


def batch(pieces):
    lengths = [len(p['events']) - 1 for p in pieces]
    x = torch.zeros(len(pieces), max(lengths), 2, dtype=torch.long)
    y = torch.zeros_like(x)
    phase = torch.zeros_like(x[..., 0])
    mask = torch.zeros_like(phase, dtype=torch.bool)
    for i, p in enumerate(pieces):
        tokens = torch.tensor(encode(p['events']), dtype=torch.long)
        n = lengths[i]
        x[i, :n], y[i, :n] = tokens[:-1], tokens[1:]
        # Position of the event being predicted is known from its prefix.
        phase[i, :n] = ((tokens[:-1, 1] + 1).cumsum(0) + p['pickup_ticks']) % p['bar_ticks']
        mask[i, PROMPT-1:n] = True
    return x, y, phase, torch.tensor([p['mode'] for p in pieces]), \
        torch.tensor([p['bar_ticks'] for p in pieces]), mask


def event_losses(model, packed):
    x, y, phase, mode, meter, mask = packed
    pitch, states, _ = model(x, phase, mode, meter)
    duration = model.duration_logits(states, y[..., 0])
    pl = nn.functional.cross_entropy(pitch[mask], y[..., 0][mask], reduction='none')
    dl = nn.functional.cross_entropy(duration[mask], y[..., 1][mask], reduction='none')
    return pl + dl, pitch, duration, mask


class Markov:
    """Mode-conditioned joint-event n-gram with recursive Dirichlet backoff."""
    def __init__(self, order=2, strength=5.):
        self.order, self.strength = order, strength
        self.counts = defaultdict(Counter)
        self.vocabulary = PITCHES * DURATIONS
        self.cache = {}

    def fit(self, pieces):
        self.counts.clear()
        self.cache.clear()
        for p in pieces:
            seq = [pitch*DURATIONS + duration for pitch, duration in encode(p['events'])]
            # Same scored targets as the GRU, with eight warm-up events.
            for i in range(PROMPT, len(seq)):
                for n in range(self.order + 1):
                    self.counts[(p['mode'], tuple(seq[i-n:i]))][seq[i]] += 1
        return self

    def probabilities(self, prefix, mode):
        key = (mode, tuple(prefix[-self.order:]) if self.order else ())
        if key in self.cache:
            return self.cache[key]
        base = np.full(self.vocabulary, .01, dtype=np.float64)
        for token, count in self.counts[(mode, ())].items():
            base[token] += count
        base /= base.sum()
        for n in range(1, min(self.order, len(prefix)) + 1):
            counts = self.counts[(mode, tuple(prefix[-n:]))]
            base *= self.strength
            for token, count in counts.items():
                base[token] += count
            base /= base.sum()
        self.cache[key] = base
        return base

    def score(self, piece):
        seq = [p*DURATIONS+d for p, d in encode(piece['events'])]
        return np.array([-np.log(self.probabilities(seq[:i], piece['mode'])[seq[i]])
                         for i in range(PROMPT, len(seq))])


def sample(probabilities, rng, temperature):
    if not 0 < temperature <= 2:
        raise ValueError('temperature must be in (0, 2]')
    logp = np.log(np.maximum(probabilities, 1e-300)) / temperature
    p = np.exp(logp - logp.max())
    return int(rng.choice(len(p), p=p/p.sum()))


@torch.no_grad()
def continue_melody(model, piece, seed, ticks_to_generate=192, temperature=.85):
    """Use the full prefix; carry GRU state. Stop on a musical event boundary."""
    if ticks_to_generate < 1 or len(piece['events']) < PROMPT:
        raise ValueError('invalid prompt or requested length')
    rng = np.random.default_rng(seed)
    result = encode(piece['events'][:PROMPT])
    cursor = piece['pickup_ticks'] + sum(d+1 for _, d in result)
    elapsed, hidden = 0, None
    if isinstance(model, MelodyGRU):
        model.eval()
        x = torch.tensor([result])
        phases = ((x[..., 1]+1).cumsum(1) + piece['pickup_ticks']) % piece['bar_ticks']
        logits, states, hidden = model(x, phases, torch.tensor([piece['mode']]),
                                       torch.tensor([piece['bar_ticks']]))
    while elapsed < ticks_to_generate:
        if isinstance(model, Markov):
            prefix = [p*DURATIONS+d for p, d in result]
            token = sample(model.probabilities(prefix, piece['mode']), rng, temperature)
            pitch, duration = divmod(token, DURATIONS)
        else:
            pitch = sample(torch.softmax(logits[0, -1], -1).numpy(), rng, temperature)
            durations = model.duration_logits(states[:, -1:], torch.tensor([[pitch]]))
            duration = sample(torch.softmax(durations[0, 0], -1).numpy(), rng, temperature)
        result.append([pitch, duration])
        elapsed += duration + 1
        cursor += duration + 1
        if not isinstance(model, Markov):
            logits, states, hidden = model(torch.tensor([[[pitch, duration]]]),
                torch.tensor([[cursor % piece['bar_ticks']]]), torch.tensor([piece['mode']]),
                torch.tensor([piece['bar_ticks']]), hidden)
    return result
