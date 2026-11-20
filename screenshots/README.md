# Screenshots by build

Each folder holds screenshots of one De-RAS build, so you can see what the program looks like without installing it.

## How the folders are organized

- One folder per build number: `screenshots/<version>/`, for example `0.2.1/`. The first build of a minor version is `x.y.0`, and each later test build adds one (`x.y.1`, `x.y.2`, and so on).
- `<version>.png` is the main window on the Home view with the example project open. It uses the Indonesian interface and the dark theme.
- `<feature>_<version>.png` shows one feature that was built or changed in that build, for example `profil-memanjang_0.2.1.png`. Feature names are lowercase with hyphens and follow the Indonesian interface labels.
- `varian/` holds the same screens in English (`_en`) and in the light theme (`_terang`). Build 0.2.0 uses the suffix `_siang`, the name the light theme had then.
- A folder for an earlier build is not overwritten.

## Contents

| Folder | Build | What it shows |
|---|---|---|
| `0.1.0/` | 0.1.0 alpha, first stage (old interface) | Main window, results, splash screen |
| `0.2.0/` | 0.2.0 alpha, new single-window interface | Home, geometry, cross-section editor, steady flow, steady analysis, compute window, results, options, splash |
| `0.2.1/` | 0.2.1 alpha, follow-up to the 0.2.0 review | Top bar with the Home button, Model Doctor pop-up with quick fixes, dark and light themes in Options |
| `0.2.2/` | 0.2.2 alpha, milestones 2 to 5 | Bridge editor, unsteady flow, hydrograph, rating curve, 3D view, time series, RAS Mapper view, inundation map |
| `0.2.3/` | 0.2.3 alpha, milestones 6 to 8 | 2D flow areas, quasi-unsteady and sediment, water quality, uniform-flow design, scour, Manning's n calibration, autosave and unit conversion |

The "measured" water levels in the `kalibrasi-n` screenshot are illustrative. Screens that did not change since 0.2.2 (RAS Mapper, bridge editor, unsteady flow, hydrograph, results) stay in the `0.2.2/` folder.
