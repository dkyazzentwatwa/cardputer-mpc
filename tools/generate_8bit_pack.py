#!/usr/bin/env python3
"""Generate the bundled Cardputer MPC 8-bit sound pack."""

from __future__ import annotations

import json
import math
import random
import wave
from pathlib import Path

SAMPLE_RATE = 16_000
SOFT_LIMIT_BYTES = 160 * 1024
HARD_LIMIT_BYTES = 220 * 1024

ROOT = Path(__file__).resolve().parents[1]
SAMPLE_DIR = ROOT / "sdcard" / "cardputer-mpc" / "samples" / "8bit"
KIT_PATH = ROOT / "sdcard" / "cardputer-mpc" / "kits" / "8bit.json"

RNG = random.Random(0x8B17)


def clamp(value: float, low: float = -1.0, high: float = 1.0) -> float:
    return max(low, min(high, value))


def crush(value: float, steps: int = 15) -> float:
    return round(clamp(value) * steps) / steps


def decay_env(t: float, duration: float, curve: float = 2.0, attack: float = 0.003) -> float:
    if t < attack:
        return t / attack
    x = min(1.0, (t - attack) / max(0.001, duration - attack))
    return (1.0 - x) ** curve


def square(phase: float, duty: float = 0.5) -> float:
    return 1.0 if (phase % 1.0) < duty else -1.0


def tri(phase: float) -> float:
    p = phase % 1.0
    return 4.0 * abs(p - 0.5) - 1.0


def noise() -> float:
    return RNG.uniform(-1.0, 1.0)


def render(duration: float, synth) -> list[int]:
    frames = max(1, int(duration * SAMPLE_RATE))
    data: list[int] = []
    phase = 0.0
    for i in range(frames):
        t = i / SAMPLE_RATE
        value, freq = synth(t, duration, phase)
        phase += freq / SAMPLE_RATE
        unsigned = int((clamp(crush(value), -0.98, 0.98) * 127.0) + 128.0)
        data.append(max(0, min(255, unsigned)))
    return data


def write_wav(name: str, data: list[int]) -> None:
    path = SAMPLE_DIR / name
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(1)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(bytes(data))


def kick(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 42.0 + 118.0 * math.exp(-t * 22.0)
    env = decay_env(t, d, 2.4, 0.001)
    click = 0.55 * math.exp(-t * 230.0) * square(phase * 4.0, 0.12)
    return 0.92 * math.sin(2 * math.pi * phase) * env + click, freq


def snare(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 2.1, 0.001)
    tone = 0.35 * square(phase, 0.42) * decay_env(t, d, 3.0)
    body = 0.75 * noise() * env
    return body + tone, 175.0


def closed_hat(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 1.7, 0.001)
    metallic = 0.45 * square(phase, 0.25) + 0.55 * noise()
    return metallic * env, 5_200.0


def open_hat(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 1.25, 0.001)
    metallic = 0.35 * square(phase, 0.18) + 0.65 * noise()
    return metallic * env, 4_700.0


def clap(t: float, d: float, phase: float) -> tuple[float, float]:
    bursts = sum(math.exp(-((t - x) * 95.0) ** 2) for x in (0.006, 0.026, 0.047))
    tail = 0.55 * decay_env(max(0.0, t - 0.045), d, 2.2)
    return noise() * min(1.0, bursts + tail), 1_800.0


def tom(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 82.0 + 105.0 * math.exp(-t * 10.0)
    env = decay_env(t, d, 2.0, 0.002)
    return (0.70 * math.sin(2 * math.pi * phase) + 0.25 * square(phase, 0.5)) * env, freq


def rim(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 2.8, 0.001)
    return (0.65 * square(phase, 0.18) + 0.35 * math.sin(2 * math.pi * phase * 2.0)) * env, 1_150.0


def shaker(t: float, d: float, phase: float) -> tuple[float, float]:
    gate = 1.0 if int(t * 85.0) % 2 == 0 else 0.45
    return noise() * decay_env(t, d, 0.9, 0.001) * gate, 3_200.0


def perc(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 2.0, 0.001)
    return (0.55 * tri(phase) + 0.35 * noise()) * env, 680.0 + 180.0 * math.exp(-t * 18.0)


def fx_down(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 1_200.0 - 950.0 * (t / d)
    env = decay_env(t, d, 0.7, 0.002)
    return (0.75 * square(phase, 0.33) + 0.20 * noise()) * env, max(80.0, freq)


def fx_up(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 180.0 + 1_350.0 * (t / d)
    env = decay_env(t, d, 0.6, 0.002)
    return (0.65 * tri(phase) + 0.25 * square(phase * 0.5, 0.2)) * env, freq


def bass(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 55.0 if t < d * 0.55 else 82.41
    env = decay_env(t, d, 1.2, 0.004)
    return (0.70 * square(phase, 0.42) + 0.25 * tri(phase)) * env, freq


def lead(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 440.0 if t < d * 0.5 else 659.25
    vibrato = 1.0 + 0.018 * math.sin(2 * math.pi * 7.0 * t)
    env = decay_env(t, d, 0.9, 0.006)
    return (0.65 * square(phase, 0.28) + 0.25 * tri(phase)) * env, freq * vibrato


def chord(t: float, d: float, phase: float) -> tuple[float, float]:
    env = decay_env(t, d, 0.8, 0.006)
    root = square(phase, 0.38)
    third = square(phase * 1.25, 0.42)
    fifth = square(phase * 1.5, 0.34)
    return (0.34 * root + 0.28 * third + 0.28 * fifth) * env, 261.63


def arp(t: float, d: float, phase: float) -> tuple[float, float]:
    notes = (261.63, 329.63, 392.0, 523.25, 392.0, 329.63)
    note = notes[min(len(notes) - 1, int((t / d) * len(notes)))]
    gate = 0.88 if int(t * 36.0) % 2 == 0 else 0.25
    return square(phase, 0.30) * decay_env(t, d, 0.65, 0.003) * gate, note


def blip(t: float, d: float, phase: float) -> tuple[float, float]:
    freq = 1_050.0 if t < d * 0.42 else 1_570.0
    return square(phase, 0.22) * decay_env(t, d, 1.5, 0.001), freq


SOUNDS = [
    ("kick.wav", "KICK", "q", 0.260, 236, kick),
    ("snare.wav", "SN", "w", 0.220, 220, snare),
    ("hat_closed.wav", "CHH", "e", 0.070, 178, closed_hat),
    ("hat_open.wav", "OHH", "r", 0.190, 178, open_hat),
    ("clap.wav", "CLAP", "a", 0.170, 210, clap),
    ("tom.wav", "TOM", "s", 0.230, 208, tom),
    ("rim.wav", "RIM", "d", 0.080, 200, rim),
    ("shaker.wav", "SHAK", "f", 0.150, 174, shaker),
    ("perc.wav", "PERC", "z", 0.120, 200, perc),
    ("fx_down.wav", "FX1", "x", 0.300, 190, fx_down),
    ("fx_up.wav", "FX2", "c", 0.310, 188, fx_up),
    ("bass.wav", "BASS", "v", 0.300, 224, bass),
    ("lead.wav", "LEAD", "1", 0.280, 198, lead),
    ("chord.wav", "CHORD", "2", 0.330, 186, chord),
    ("arp.wav", "ARP", "3", 0.400, 190, arp),
    ("blip.wav", "BLIP", "4", 0.090, 192, blip),
]


def main() -> None:
    SAMPLE_DIR.mkdir(parents=True, exist_ok=True)
    KIT_PATH.parent.mkdir(parents=True, exist_ok=True)

    total_frames = 0
    pads = []
    for filename, label, key, duration, volume, synth in SOUNDS:
        data = render(duration, synth)
        write_wav(filename, data)
        total_frames += len(data)
        pads.append(
            {
                "key": key,
                "label": label,
                "sample": f"samples/8bit/{filename}",
                "volume": volume,
            }
        )

    loaded_bytes = total_frames * 2
    if loaded_bytes > HARD_LIMIT_BYTES:
        raise SystemExit(
            f"8-bit pack would use {loaded_bytes} bytes after load; hard limit is {HARD_LIMIT_BYTES}"
        )

    kit = {
        "version": 1,
        "name": "8bit",
        "pads": pads,
    }
    KIT_PATH.write_text(json.dumps(kit, indent=2) + "\n")

    status = "OK"
    if loaded_bytes > SOFT_LIMIT_BYTES:
        status = "WARN"
    print(
        f"{status}: generated {len(SOUNDS)} sounds, {total_frames / SAMPLE_RATE:.2f}s, "
        f"{loaded_bytes // 1024}K loaded RAM estimate"
    )


if __name__ == "__main__":
    main()
