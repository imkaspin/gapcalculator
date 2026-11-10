# Gap Calculator (Geode mod for Geometry Dash 2.2081)

Select two objects in the editor, press the **Gap** button, and see the gap between them plus the minimum and recommended TPS.

## Folder layout

```
GapCalc/
├── .github/workflows/multi-platform.yml   <- GitHub Actions cloud build (optional)
├── resources/                             <- empty, for future sprites/sounds
├── src/main.cpp                           <- ALL the mod code (button, maths, popup)
├── .gitignore
├── about.md                               <- description shown in the Geode mod list
├── changelog.md                           <- shown in the Geode mod list
├── CMakeLists.txt                         <- build config (don't edit)
├── logo.png                               <- mod icon (replace with your own 336x336)
├── mod.json                               <- mod id, version, settings, GD version
└── README.md                              <- this file
```

## Option A: build on your PC (Windows)

1. Install the Geode CLI and SDK: https://docs.geode-sdk.org/getting-started/
   (you need Visual Studio 2022 Build Tools with "Desktop development with C++", CMake, and the Geode CLI).
2. Make sure the `GEODE_SDK` environment variable points to your SDK folder (the installer sets this).
3. Open a terminal inside this `GapCalc` folder and check your SDK version:
   ```
   geode sdk version
   ```
4. Open `mod.json` and set the first line `"geode": "4.6.0"` to the version printed above.
5. Build:
   ```
   geode build
   ```
6. The finished file is `build/kaspin.gapcalc.geode`. Copy it into
   `Geometry Dash/geode/mods/` (or just double-click it, or drag it onto the game window).
7. Launch GD, open the editor, select 2 objects, and press the **Gap** button.

## Option B: build in the cloud (no compiler needed)

1. Create a GitHub repo and push this whole folder (the `.github` folder must be included).
2. Open the repo's **Actions** tab, run **Build Binaries**, and wait for it to finish.
3. Download **Build Output**, unzip it, and take `kaspin.gapcalc.geode`.
4. Put it in `Geometry Dash/geode/mods/`.

## Using it

- Select exactly **2 objects** in the editor, then press **Gap** (top right).
- Press **Settings** in the popup (or open the mod's settings in Geode) to change:
  - **Speed portal** (0.5x to 4x), or **Custom speed** in units/sec (useful for vertical gaps)
  - **Player size** (normal 30u / mini 18u)
  - **Gap axis** (Auto / Horizontal / Vertical)
  - **Safety ticks inside gap** (used for the recommended TPS)

## How the numbers work

- Gap = edge-to-edge distance between the two objects on the chosen axis.
- Free window = gap - player hitbox.
- Minimum TPS = speed / free window (at least one physics tick lands inside the gap).
- Recommended TPS = speed x safety ticks / free window.
- Gap smaller than the hitbox: the player can't fit, so the popup shows the max TPS to skip across it in one tick.

## Common problems

| Problem | Fix |
|---|---|
| `Unable to find Geode SDK` | Set the `GEODE_SDK` environment variable and reopen your terminal. |
| Loader says the mod needs a different Geode version | Fix the `"geode"` line in `mod.json` (step 4). |
| Button overlaps other editor UI | In `src/main.cpp`, change `BTN_X_FROM_RIGHT` and `BTN_Y_FROM_TOP` at the top. |
| Compile error about a Geode function | Update the SDK (`geode sdk update`), then rebuild. |
| Wrong numbers for ship/UFO/etc. | Set **Custom speed** in the settings to the speed along your gap axis. |
