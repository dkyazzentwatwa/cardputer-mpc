# Cardputer MPC

Tiny MPC-style groovebox firmware for the M5Stack Cardputer. It loads short drum samples from SD, maps them to the keyboard, sequences 16-step patterns, and saves/loads project JSON files back to SD.

On boot, Cardputer MPC shows a 4-second `Cypher Tune` intro with a scan-grid frame, animated waveform strip, sweep line, and loading bar before the main pad screen.

## Hardware Target

- M5Stack Cardputer K132 / `m5stack:esp32:m5stack_cardputer`
- ESP32-S3FN8, 240 MHz, 8 MB flash, no real PSRAM on the standard unit
- 240x135 display, keyboard, microSD over SPI, built-in mono I2S speaker
- SD pins: SCK 40, MISO 39, MOSI 14, CS 12

## Build

```sh
arduino-cli compile --profile cardputer /Users/cypher/Documents/GitHub/cardputer-mpc
```

Flash with the usual Cardputer touch-first workflow:

```sh
arduino-cli board list
stty -f /dev/cu.usbmodemXXXX 1200
arduino-cli board list
arduino-cli upload --profile cardputer -p /dev/cu.usbmodemYYYY /Users/cypher/Documents/GitHub/cardputer-mpc
```

## SD Card

Copy `sdcard/cardputer-mpc` to the root of the microSD card:

```text
/cardputer-mpc/
  samples/
  kits/starter.json
  projects/demo-groove.json
```

Samples must be PCM WAV, mono, 8-bit or 16-bit, at 16000 Hz or 22050 Hz. The loader preloads samples into internal RAM and rejects oversized files instead of trying risky live SD streaming.

## Controls

- Pads: `q w e r`, `a s d f`, `z x c v`, `1 2 3 4`
- Space: play/stop
- Enter: start/toggle record overdub. Recording captures pad hits into the 16-step pattern while playback runs; it does not record live audio.
- Tab: change view
- `WAV` view: live waveform-style visualizer for currently triggered sounds
- `5` / `6`: previous/next selected step
- `[` / `]`: previous/next selected step fallback
- `,` / `.`: previous/next selected pad
- Shift + `,` / `.`: selected pad volume down/up
- Selected pad key in Steps view: toggle that pad on the selected step
- `-` / `=`: master volume down/up
- Shift + `-` / `=`: BPM down/up by 5
- Shift + `s`: save `/cardputer-mpc/projects/cardputer-jam.json`
- `l`: load `/cardputer-mpc/projects/demo-groove.json`
- `k`: reload starter kit
- `n`: new blank pattern
- Backspace or `` ` ``: panic/stop voices

## Practical Limits

Default sample budget is intentionally conservative: about 160 KB soft budget and 220 KB hard budget. At 16-bit mono, 22050 Hz is about 44 KB per second, so short one-shots matter. If the SD kit is missing or too large, the firmware creates a tiny fallback kit in RAM so the sequencer and controls can still be tested.
