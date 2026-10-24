# GT2+ patched disc support (private branch)

The port supports pez2k's **Gran Turismo 2 plus Complete beta 7** for US Simulation v1.2. GT2+ patches the
executable in place, so every address and every decompressed `GT2.OVL` member keeps its retail size and the normal
US Simulation address profile applies.

| Build | `SCUS_944.88` SHA-1 |
| --- | --- |
| Retail US Simulation v1.2 | `3030aa271c0a4022fc69ce09d76a6bc75e69a32a` |
| GT2+ Complete beta 7 | `9d733c40f91b7ff5c6b4e4141ce3ebf8f4f5bde1` |

The patched image is detected by `ExeProfile`. `gt2install inspect <disc>` prints the detected SHA-1 and mode.

## Install the patched image

The `assets/` directory is an override and is preferred over the disc. A data directory created from the retail
image silently masks every GT2+ data change. Extract into a **new** directory:

```sh
./gt2install extract "Gran Turismo 2 (Simulation Mode) (v1.2) - GT2+ Complete.bin" <new-data-directory>
```

Then point the application at the extracted `disc.raw2352`, or use the normal installer with the patched image.
`gt2install` writes `source.sha1` into the data directory; the application warns when the loaded disc's executable
does not match it, which catches a stale retail extraction.

## What comes from the disc

All data fixes are read from the patched image: car parameters, menu pages, textures, overlays (including the
Apricot Hill name, the N/A/Turbo message mapping and the Blocked wheel-shop list), the car string database and the
brake-controller stage byte. No source data is copied into this repository.

## Code changes in this branch

These are the only GT2+ executable/overlay patches that the reimplemented code has to reproduce:

- **Turbo section** (`0x8005F7C8`): GT2+ also accepts a real stock turbo at kit 0 (row id `> 0`), not only upgrade
  kits 1..4. See `career::HasTurboKit`.
- **Completion percentage** (`0x80019DCC`): Complete adds four counted events with the 206 and Opel/Vauxhall events,
  so the completion divisor subtracts 25 instead of 29. `MenuData` reads the patched immediate from the overlay.
- **Metric horsepower**: Complete removes the PS → BHP display conversion. `DisplayPower` uses raw PS for the
  Complete profile and keeps the retail conversion for retail discs.
- **Save/replay names**: GT2+ changes the card IDs to `BASCUS-94455GAME7C` and `BASCUS-94455REPL7C`. They are read
  from the executable at `0x80091C94` / `0x80091AB8`, so the port reads and writes the same memory-card files as an
  emulator running the patched image.
- **Unsorted `solodata`**: GT2+ appends new cars without re-sorting the table. The parser sorts a copy instead of
  rejecting the disc.

This GT2+ image does **not** contain the Light version's wheel-shop message patch: `0x47E0` still requests message
`0x1E`. The port therefore keeps `0x1E` for this image.

## Check

```sh
./gt2checks "Gran Turismo 2 (Simulation Mode) (v1.2) - GT2+ Complete.bin"
```

The disc-backed checks assert the save IDs, the completion divisor, the brake-controller row, the rotary labels,
the Apricot Hill course name and the turbo/metric-HP rules.
