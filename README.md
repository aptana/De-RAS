# De-RAS

De-RAS is a desktop application for river and floodplain hydraulics. It computes water surface profiles, flood extents and flow velocities with its own C++ engine, and it works directly on HEC-RAS project files (`.prj`, `.g##`, `.f##`, `.u##`, `.p##`), so no separate HEC-RAS installation is needed to open, edit and compute a project. The interface is in English and Indonesian.

De-RAS is not affiliated with, endorsed by or sponsored by HEC or the US Army Corps of Engineers. HEC-RAS is their product and name.

**Status: 0.2.3 alpha, a test build.** Read [What has been checked](#what-has-been-checked) before relying on any number it produces.

## What it does

| Area | Features | Label |
|---|---|---|
| Steady 1D flow | Standard step method, subcritical, supercritical and mixed regimes, junctions, ineffective areas, levees, obstructions, rating-curve boundary | Done / Preview |
| Structures | Bridges, culverts, inline weirs and gates (steady, subcritical only) | Preview |
| Unsteady 1D flow | Implicit four-point (Preissmann) scheme, hydrographs, lateral inflow, time series | Preview |
| Storage areas, lateral structures, pumps | Weir flow with Villemonte submergence, optional breach, pump groups with on/off levels | Preview |
| 2D flow areas | Square grid with terrain subgrid tables, diffusion wave equation, implicit solver; depth, velocity and arrival-time maps | Preview |
| Sediment | Quasi-unsteady flow, Exner equation, Meyer-Peter and Müller, Engelund-Hansen and Yang transport functions (one reach, one grain size) | Preview |
| Water quality | One constituent in mg/L, 1D advection-dispersion with first-order decay | Preview |
| Hydraulic design | Uniform flow in a trapezoidal channel, HEC-18 pier, contraction and abutment scour, Manning's n calibration | Preview |
| RAS Mapper view | Projection, GeoTIFF terrain, background pictures, cutting cross sections from terrain, inundation maps, GeoTIFF and PNG export | Preview |
| Model Doctor | Checks geometry and flow data before a run and says where each problem is and how to fix it | Done |
| Reports and export | CSV, HTML, Word (.docx) and PDF run reports | Done / Preview |
| Other | Autosave and recovery, SI to US project conversion (makes a copy), 186 built-in help topics | Done / Preview |

The labels have fixed meanings. **Done** means built, unit tested and documented. **Preview** means usable but not fully tested, and every plot, table and map from it is marked "Preview, not validated". The full list is in [`docs/FEATURE_MATRIX_EN.md`](docs/FEATURE_MATRIX_EN.md).

Not built yet: vertical n variation, multiple bridge openings, HEC-RAS `.hdf` terrain, map reprojection, online base maps, `.hdf` files for unsteady results, bridge and inline structures in unsteady runs, polygon 2D meshes, the Shallow Water equations, animation, and the `.q##`, `.s##` and `.w##` files. Sediment, water quality, pumps, breach data and calibration observations are stored in `<project>.deras.json` instead.

## What has been checked

- No result has been compared with HEC-RAS yet. Do not describe De-RAS output as equal to HEC-RAS output.
- The Windows build is cross-compiled from Linux with MinGW-w64 and has never been run on Windows.
- Project files written by De-RAS have not been opened in HEC-RAS. Blocks for structures, storage areas, 2D areas and lateral inflow, and the `.p##.hdf` result files, are written from the public format description and are unverified.
- The Word report has only been opened in LibreOffice.
- 19 of 19 automated tests pass on Linux (`ctest`). The test plan, including the Windows and HEC-RAS comparison tests that are still open, is in [`docs/TESTING_EN.md`](docs/TESTING_EN.md).

## Installing

Windows 10 or 11, 64-bit. Builds come as an installer (`De-RAS-<version>-setup-x64.exe`, needs administrator rights) and a portable ZIP (`De-RAS-<version>-win64-portable.zip`, no installation). The installer touches nothing except De-RAS and asks before removing an older De-RAS version.

## Building from source

```
sudo apt install cmake ninja-build g++ qt6-base-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build && ctest --test-dir build
```

The Windows package is built by cross-compiling with MinGW-w64; see [`docs/BUILD_EN.md`](docs/BUILD_EN.md) and `third_party/qt/BUILD-QT-WINDOWS.md`. HDF5 output and GeoTIFF reading and writing are De-RAS's own code, without external libraries.

## Repository layout

| Path | Contents |
|---|---|
| `core/`, `io/`, `engines/`, `app/`, `ui/` | C++20 / Qt 6 source: model and results, file I/O, solvers, application logic, user interface |
| `tests/` | Automated tests; the synthetic standard test project (a 1 km river) is in `tests/data/uji-standar/` |
| `tools/` | Round-trip checker, help generator, Windows packaging scripts |
| `resources/`, `installer/`, `third_party/` | Fonts and help content, NSIS installer scripts, third-party build notes |
| `branding/` | Logo and icons |
| `docs/` | Architecture, file formats, feature matrix, testing, build notes, changelog and the user manual, in English |
| `screenshots/` | Screenshots by build number |

The documents under `docs/` that are meant for readers of this repository carry the suffix `_EN` and are written in English: `ARCHITECTURE_EN.md`, `FILE_FORMATS_EN.md`, `BUILD_EN.md`, `FEATURE_MATRIX_EN.md`, `TESTING_EN.md` and `CHANGELOG_EN.md`. The English user manual is `docs/manual/De-RAS_User_Manual_EN.docx`, with its Markdown source in `docs/manual/source/manual.en.md`.

## Example project

**Sungai Uji 1 km** is a synthetic project, not field data. It has a trapezoidal channel with six cross sections, steady and unsteady plans, a terrain, a side weir into a 2D area, sediment data and a water quality case. The same project is used to test every analysis in every build. Open it from the Home view with **Example project**.

## License

De-RAS is released under the [Apache License 2.0](LICENSE). See [`NOTICE`](NOTICE) for the attribution notice. Third-party components and fonts are listed in [`THIRD_PARTY_NOTICES.txt`](THIRD_PARTY_NOTICES.txt).
