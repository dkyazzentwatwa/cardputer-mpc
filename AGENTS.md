# Repository Guidelines

## Project Scope

Cardputer MPC is an Arduino CLI firmware project for the M5Stack Cardputer. It turns the device into a small MPC-style groovebox with SD-loaded one-shot samples, keyboard pad triggering, 16-step sequencing, project save/load, and a compact 240x135 display UI.

Keep changes focused on real Cardputer behavior. Prefer small firmware-safe edits over broad rewrites, and preserve the current root-sketch layout unless a task explicitly asks for a restructure.

## Repository Layout

- `cardputer-mpc.ino`: main Arduino sketch, boot flow, input routing, SD setup, and loop.
- `src/`: firmware modules for audio, input mapping, samples, projects, sequencing, and UI.
- `sketch.yaml`: canonical Arduino CLI profile and pinned board/library dependencies.
- `sdcard/cardputer-mpc/`: SD-card runtime files copied to the microSD root.
- `intro-sound-kit/`: source kit/sample material and conversion notes.
- `README.md`: user-facing build, SD-card, controls, and hardware notes.

## Build And Flash

Use the checked-in Arduino CLI profile:

```sh
arduino-cli compile --profile cardputer /Users/cypher/Documents/GitHub/cardputer-mpc
```

For hardware upload, use the Cardputer touch-first flow:

```sh
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
arduino-cli board list
arduino-cli upload --profile cardputer -p /dev/cu.usbmodemYYYY /Users/cypher/Documents/GitHub/cardputer-mpc
```

Always re-check the live `/dev/cu.usbmodem*` port before flashing. The port may stay the same after the 1200-baud touch.

## Development Rules

- Keep Arduino CLI as the source of truth; do not introduce PlatformIO or app scaffolding.
- Preserve `m5stack:esp32:m5stack_cardputer` compatibility and the `cardputer` profile unless the user asks for another board.
- Be conservative with RAM. The standard Cardputer has no real PSRAM, and samples are preloaded into internal RAM.
- Keep SD paths rooted at `/cardputer-mpc/` on device and mirrored under `sdcard/cardputer-mpc/` in the repo.
- Keep sample handling limited to short mono PCM WAV files unless you also update docs and memory-budget checks.
- Avoid full-screen UI churn in tight loops. The display is small, so favor stable layouts and readable status text.
- When changing controls, update `README.md` and keep obvious Cardputer keyboard fallbacks where practical.
- Do not commit generated binaries, build folders, or local Arduino cache output.

## Testing Expectations

After firmware changes, run at least:

```sh
arduino-cli compile --profile cardputer /Users/cypher/Documents/GitHub/cardputer-mpc
```

For UI, input, audio, SD, or timing changes, prefer a real-device flash and smoke test. Check boot, fallback-without-SD behavior when relevant, pad triggering, play/stop, record toggle, step editing, kit loading, and save/load paths.

## Documentation

Update `README.md` when behavior visible to a user changes: controls, SD-card layout, hardware assumptions, build commands, sample limits, or boot/UI behavior. Keep this `AGENTS.md` operational and shorter than the README.
