# De-RAS code changelog

A summary of the code changes per milestone, kept so that the version notes, the feature matrix, the README and the manual can be brought in line with each build.

## 0.2.3 · M6 to M8 (20 November 2026)

Status: the fourth test build. It contains M6 (storage areas, lateral structures, pumps, 2D areas), M7 (quasi-unsteady sediment) and M8 (autosave, Word and PDF reports, hydraulic design, n calibration, water quality, unit conversion), plus findings T3 to T7 from the documentation work. All new computation features are **Preview**: their results have not been compared with HEC-RAS. Three M8 items are **not** in this build (see "Not done").

### M6: storage areas, lateral structures, pumps, 2D areas (Preview)
- **Storage areas** (a level pond, with an elevation–volume curve or a fixed area), **SA/2D connections** (weirs between areas), **lateral structures** (a weir along a reach to a storage area, a 2D area or out of the system; Villemonte submergence reduction; optional **breach**), and **pump stations** (pump groups with on and off elevations). They are edited from the Geometric Data palette; breach and pump data are stored in `<name>.deras.json`.
- **2D area**: a square grid inside the area polygon with subgrid tables from the terrain (volume and conveyance follow the terrain inside each cell and along each cell face), solved with the implicit **diffusion wave** equation (Picard iteration and conjugate gradients). This is **not** the HEC-RAS polygon mesh. **BC lines** carry flow hydrographs, normal depth and rain; the initial area stage can be set.
- Everything is computed in **Unsteady Analysis**; exchange flows are implicit in the reach and area water levels. The volume error of each area is written in the messages (SOLV_2D_DONE).
- Results: hydrographs of storage areas, lateral structures, connections and pumps on the Hydrograph and Time series pages; **2D result maps** in RAS Mapper (depth per terrain pixel, velocity per cell, arrival time), stored in `<name>.<plan>.deras-2d.bin`.
- **HTab** is not needed: hydraulic tables are built automatically, and HTab lines in a file are kept.
- Breaklines and n regions for 2D areas are not scheduled (their chips were removed from the Geometry toolbar so it fits a 1366 px screen).
- 2D volume balance fix: cells drained by an outflow boundary no longer get a negative volume (the volume error used to reach 107 %; it is now below 1 % in the example project).

### M7: quasi-unsteady sediment (Preview)
- **Quasi-unsteady Data**: geometry file, reach, flow series (duration, flow, compute increment, downstream stage or normal depth).
- **Sediment Data**: transport functions **Meyer-Peter and Müller**, **Engelund-Hansen** and **Yang**; specific gravity, porosity, temperature; bed gradation (d50 and d90 by log interpolation); erodible depth; upstream load as equilibrium, clear water or a rating curve.
- **Sediment Analysis**: at every step steady hydraulics, then transport capacity, then the Exner equation with a supply limit; the step is split automatically when the bed changes by more than 5 % of the depth. It gives the bed profile over time, bed change, a table, the mass balance and CSV export. Results go to `<name>.deras-sedimen.json`.
- Limits: one reach, one grain size (d50), no sorting or armouring. HEC-RAS `.q##` and `.s##` files are not read or written (the data is in `.deras.json`, module `sediment`).

### M8: the remaining features (Preview unless stated)
- **Autosave and recovery** (Options → Autosave, default 5 minutes, 0 = off): a copy of unsaved changes is written to the application data folder (`recovery`), not to the project files. After a crash or power cut, De-RAS offers recovery when the project is opened. The copy is deleted after Save or Don't save.
- **Word (.docx) and PDF reports**: the Report… button in Results now offers HTML, Word and PDF (A4 landscape). The `.docx` is written by De-RAS itself (a zip of Office Open XML) and has never been opened in a real Microsoft Word.
- **Hydraulic Design** (Run menu and toolbar): **uniform flow** in a trapezoidal channel (flow from depth or normal depth, critical depth, Froude number, bed shear, section drawing); **HEC-18 scour**, 5th edition: piers (CSU, K1/K2/K3, upper limit), live-bed and clear-water contraction (Laursen), abutments (Froehlich); values can be taken from computed results (plan, profile, cross section).
- **Manning's n calibration**: one multiplying factor for n (channel only or all n) is found with a golden-section search so that the RMS of the difference between computed steady and observed water levels is smallest; before and after plots; an **Apply to geometry** button. Observations are stored in `.deras.json` (module `calibration`).
- **Water quality**: Water Quality Data and Water Quality Analysis. One constituent (mg/L), 1D implicit advection-dispersion with first-order decay, using the hydraulics of the chosen unsteady plan (first reach). The mass balance is shown, including a **hydraulic continuity correction** (the hydraulics stored at each output time do not close continuity exactly; in the example project the correction is 5.6 kg out of 114.8 t). Results go to `<name>.<plan>.deras-wq.json`. No temperature, DO or nutrients; `.w##` files are not read or written.
- **Project unit conversion SI ↔ US** (Options, and the Options menu): makes a **copy** of the project under a new name in the same folder, then converts the geometry, steady and unsteady flow, plan tolerances, and the De-RAS module data (gate openings, pumps, breach, sediment, water quality). The original project is not changed; old results are not copied. **Terrain and GIS layers are not converted.** Numbers in HEC-RAS files are rounded to the 8-character column width.

### Not done from the M8 plan
- **Vertical n**: read and kept in the file, not computed. Its button stays locked with the note "scheduled for M8 but not in build 0.2.3".
- **Multiple openings**: kept in the file, not computed.
- **HEC-RAS `.hdf` terrain, reprojection, online base maps**: not available.
- Still true from 0.2.2: `.hdf` files for unsteady results are not written; bridges, culverts and inline structures are not computed in unsteady runs.

### Example project
- The standard test project gets **g02/u02/p03** (a 100 m side weir on the right levee into the 2D area "Dataran Kanan"), **sediment data** in g01 (22 days of quasi-unsteady flow, sandy gravel with d50 2 mm, clear water), and **water quality data** (a 100 mg/L tracer for 3 hours upstream on the p02 flood). g01, f01, u01, p01 and p02 are unchanged apart from time stamps.
- **T5**: the example copy in `Documents\De-RAS Projects\Contoh` now records its version (`versi-contoh.txt`). If the copy is older than the application, opening the example project offers Open the old one / Update; the File → **Restore example project…** menu moves the old copy to `Contoh-lama-<date time>` (it is not deleted) and installs the new one.

### Findings from the documentation work
- **T3**: the Geometry canvas hint no longer says "Mapper follows in M3".
- **T4**: the coordinate system note in Options is updated (same as RAS Mapper → Set projection; reprojection does not exist yet).
- **T5**: see Example project above.
- **T6**: the new Model Doctor screenshots (`model-doctor_0.2.3.png` and the `_en` one) use the case of an empty downstream boundary, because the "Switch to Subcritical" quick fix no longer exists since 0.2.2.
- **T7**: English variants for the longitudinal profile, profile table, rating curve, 3D perspective and time series are in `screenshots/0.2.3/varian/`.

### Other fixes
- Area and volume units in the storage area sheet and the breach estimate used to show without ² or ³ (for example "Area (m)"); they now show "m²" and "m³".
- Numbers in `.deras.json` are written with the shortest digits that stay exact (2.65, not 2.6499999999999999).
- Remaining feature locks: vertical n (not in 0.2.3), breaklines and n regions (not scheduled).

### Help, feature matrix and messages
- 186 ⓘ help topics (163 in 0.2.2). New: 2D areas, storage areas, lateral structures, connections, BC lines, pumps, 2D maps, all sediment topics, `tools.autosave`, `design.uniform`, `design.scour`, `design.calibration`, `wq.data`, `wq.run`; `project.units` and `results.report` were rewritten. `docs/manual/help-topics.*.md` were rebuilt.
- Feature matrix in the application: the M6 to M8 rows are Preview with their limits; new rows for Word and PDF reports, hydraulic design, n calibration, unit conversion and autosave.
- New messages: SED_* (13 sediment messages), UNITS_SAME, WQ_NO_RESULT.

### Packaging
- `release/De-RAS-0.2.3-setup-x64.exe` and `release/De-RAS-0.2.3-win64-portable.zip`. The 0.2.2 files were not deleted.
- Screenshots: `screenshots/0.2.3/` (0.2.3.png and 12 features, Indonesian UI, Dark Theme: `opsi`, `geometri-2d`, `peta-2d`, `data-quasi`, `data-sedimen`, `analisis-sedimen`, `desain-seragam`, `desain-gerusan`, `kalibrasi-n`, `data-kualitas-air`, `analisis-kualitas-air`, `model-doctor`) and `screenshots/0.2.3/varian/` (18 English images including the T7 set; 6 Light Theme images). The `kalibrasi-n` image uses illustrative "observed" water levels (computed + 0.25 m at three cross sections) entered in memory for the picture only.

### Tests
- Linux: `ctest` 19 of 19 pass. New tests: `test_m6` (93 checks: weir flow, storage areas, connections, pumps, lateral structures, breach, 2D BC lines, lateral flow into 2D, area blocks in `.g##` and `.u##`, Model Doctor), `test_m7` (59 checks: transport functions, equilibrium, clear water with an erodible limit, rating curve, result files), `test_m8` (46 checks: uniform flow against hand calculation, pier, contraction and abutment scour against the HEC-18 formulas, calibration recovering a factor of 1.300 from synthetic data, water quality with a closed mass balance, SI → US → SI conversion and project copy, autosave recovery, `.docx` structure and CRC-32). `test_uji_standar` gets S5 (p03), S6 (sediment) and S7 (water quality).
- Windows: the MinGW cross-build succeeds; **not run on Windows** (W1 to W4, K2 and V1 are still open).

### Notes for the documentation
- The manual needs new chapters: storage areas, lateral structures, pumps and 2D areas, 2D maps, sediment (3 views), Hydraulic Design (3 pages), n calibration, Water Quality (2 views), unit conversion, autosave, Word and PDF reports.
- The standard test project specification changed (g02/u02/p03, sediment, water quality) and has to go into `TESTING_EN.md`.
- `FEATURE_MATRIX_EN.md` is rebuilt from `featureRows()`; `screenshots/README.md` needs a 0.2.3 row.
- Vertical n, multiple openings and `.hdf` terrain / reprojection are not in 0.2.3: their schedule has to be decided by the project owner.

### Maintenance after release 0.2.3 (20 November 2026, no new build)
So that the code in the GitHub repository (which carries only the `_EN` documents) still builds. Application behaviour, computed results and installer contents are unchanged; the 0.2.3 installer is not replaced and the version stays 0.2.3.
- `tools/package/make_windows_dist.sh`: `FEATURE_MATRIX.md`, `TESTING.md` and `FILE_FORMATS.md` are taken from `docs/<NAME>.md`, or from `docs/<NAME>_EN.md` when the first does not exist; the names inside the package stay the same. When neither exists a warning is printed and the package is still built.
- `docs/manual/source/build_manuals.py`: a manual whose source is missing (e.g. `manual.id.md` in the repository) is skipped with a warning; the script fails only when no source exists at all.
- Code comments no longer refer to `BASE - PROMPT.md` or `CATATAN_VERSI.md`, which are not published: `ui/Help.cpp`, `ui/MainWindow.h`, `ui/Theme.h`, `ui/Theme.cpp`, `ui/Features.h`, `installer/De-RAS.nsi`. References to `docs/TESTING.md` in `tests/test_uji_standar.cpp` and `tools/testdata/make_uji_standar.cpp` now point to `docs/TESTING_EN.md`; the project owner's name in the test-project specification comment is replaced by "project owner".
- Tests: `ctest` 19/19 pass; the packaging script was run into a temporary folder (not `release/`).

## 0.2.2 · M2 to M5 (19–20 November 2026)

Status: the third test build. It contains M2 (complete steady flow), M3 (RAS Mapper), M4 (structures) and M5 (1D unsteady). M6 to M8 were not started and went into 0.2.3. All new features are **Preview**: their results have not been compared with HEC-RAS, and some file block formats are not verified.

### M2: complete steady flow (Preview)
- The **supercritical** and **mixed** regimes can be chosen in Steady Analysis. Supercritical computes from upstream to downstream. Mixed picks the regime at every cross section with specific force and writes the location of the hydraulic jump (message SOLV_JUMP).
- **Upstream boundary** (normal depth, known water surface, critical) in the Steady Flow boundary condition sheet, used for supercritical and mixed.
- **Junctions**: created, edited and deleted from the Geometric Data palette (a table of reaches in and out, with lengths). The computation uses an energy balance; at a split the upstream reach takes the highest water surface. Renaming or deleting a reach updates the junction.
- **Ineffective areas, obstructions and levees** can be edited in the Cross-Section Editor (chips above the plot) and are computed. The cross-section plot shows them.
- **Rating curve** in Results (water surface, energy grade and critical water surface against discharge at one cross section).
- **`.p##.hdf` results** in the HEC-RAS layout (Results/Steady/...). Written by De-RAS's own HDF5 writer, built from the format specification without an HDF5 library. The file is readable by `h5dump` but **has never been opened in HEC-RAS** (K3 is still open). If a `.p##.hdf` that De-RAS did not make already exists, it is not overwritten; De-RAS writes `<name>.<plan>.deras.hdf` and gives a warning.
- **Vertical n** moved to **M8**. A file that uses it is still read and kept intact.

### M3: RAS Mapper (Preview, Ctrl+6)
- Project projection from an EPSG code, the WKT of a `.prj` file, or from the terrain. No reprojection yet.
- Single-band **GeoTIFF** terrain (uncompressed, LZW, Deflate; strips or tiles). Read and written by De-RAS's own code because GDAL is not available for the Windows build. BigTIFF and rotated rasters are not supported. HEC-RAS `.hdf` terrain is not read directly, but its source GeoTIFF is loaded when present.
- Background pictures (PNG/JPG/BMP with a world file).
- Reach centrelines and **cross-section cut lines** (XS GIS Cut Line). A cross section without a cut line is drawn dashed.
- **Cut cross sections from terrain** and "Update from terrain" for the selected cross section.
- **Steady inundation map** per profile, depth or water surface export to GeoTIFF and the map image to PNG. No flow-connectivity check yet.
- **3D perspective** in Results.
- The terrain and background picture lists are stored in `<name>.deras.json`; HEC-RAS files are not changed.

### M4: structures (Preview, subcritical only)
- **Bridges**: the energy method with piers as obstructions, and Yarnell (K = 1.25), taking the one with the highest energy. Pressure flow with the sluice gate or orifice equation, plus weir flow over the deck with the Villemonte submergence reduction.
- Circular and box **culverts**: FHWA HDS-5 inlet control and full-flow outlet control, plus overtopping of the road. Other shapes are stored but not computed.
- **Inline structures**: a weir crest and gates (sluice, orifice, or weir flow over the sill).
- Editors in Geometric Data (palette Bridge / Culvert and Inline Structure, or double-click a structure on the canvas): deck, pier, barrel, crest and gate tables.
- **Gate openings** for steady runs are stored in `<name>.deras.json`, not yet in the flow file.
- The structure block format in `.g##` files (Bridge Culvert-, Deck Dist Width WeirC, Pier, Culvert=, IW Dist, IW Gate Name) was made from general knowledge of the HEC-RAS format and is **not verified** against real HEC-RAS files. A structure that cannot be read is kept intact and Model Doctor refuses to run it.
- Multiple openings stay in M8.

### M5: 1D unsteady flow (Preview)
- **Unsteady Flow Data** (Ctrl+4): boundary condition locations, an Add reach ends button, lateral inflow (single point or uniform), initial flows, and data per location: flow hydrograph, stage hydrograph, normal depth, rating curve. Other types are stored in the file but not computed.
- **Unsteady Analysis** (Run menu): the unsteady plan, start and end time, computation and output intervals, theta, water surface tolerance, maximum iterations, and Model Doctor.
- Engine: the four-point implicit **Preissmann** scheme on the Saint-Venant equations with Newton iterations. The initial state comes from a steady profile. Junctions with equal water surfaces and continuity. One flow path per reach; ineffective areas do not store water.
- Simplifications not handled yet: structures (Model Doctor refuses an unsteady run that has bridges, culverts or inline structures), the supercritical regime, storage areas, lateral structures and pumps.
- Results: a **Max WS** profile and a profile for every output time (at most 100), plus water level and flow time series at every cross section, stored in `<name>.<plan>.deras-results.json`. **`.hdf` files for unsteady results are not written yet.**
- New Results pages: **Hydrograph** (water level and flow against time) and **Time series** (a table).
- `.u##` files are read and written; blocks that did not change are written back exactly as they were. The format of the Rating Curve and lateral inflow lines is not verified with HEC-RAS.
- **Storage areas, lateral structures, pumps and HTab moved to M6** (they were scheduled for M5) because they have to be handled together with 2D areas.
- The project time zone is not used by the computation yet; plan dates and times are taken as they are.

### Example project and standard test project
- The standard test project `tests/data/uji-standar/` gets a **terrain** `Terrain/DerasUji1km_terrain.tif` (1 m, no EPSG), the unsteady flow file **u01 "Banjir uji 24 jam"** (initial flow 20; hourly hydrograph 20 → 150 m³/s at hour 6 → 20 m³/s at hour 18; downstream normal depth 0.001), and the plan **p02 "Plan unsteady banjir uji"** (01JAN2024 0000 to 02JAN2024 0000, 1MIN, output 30MIN). g01 and f01 are unchanged and the active plan stays p01. Result for p02: 1,440 steps, downstream peak flow 146.2 m³/s with a delay of about 0.5 hours.
- An example project copy that already exists in `Documents\De-RAS Projects\Contoh` is **not overwritten**, so it has no terrain, u01 or p02. Delete the folder and open the example project again to get the new version.

### Other fixes
- T1 from the documentation work: the example project row in the feature matrix now reads "standard test project Sungai Uji 1 km".
- The "results may be out of date" warning no longer appears just because the active plan changed; only changes to the geometry, flow and plan files that the results used are counted.
- The Compute Window names the step "1D unsteady simulation" for an unsteady plan.
- The Unsteady Analysis column can scroll so that fields are not cut off in a short window.

### Help, feature matrix and messages
- 163 ⓘ help topics. New topics for junctions, ineffective areas / obstructions / levees, the rating curve, RAS Mapper, the inundation map, 3D, bridges, culverts, inline structures, unsteady flow data, unsteady analysis, the hydrograph and time series. `docs/manual/help-topics.*.md` were rebuilt.
- The feature matrix in the application (Help → Feature matrix) is updated: all M2 to M5 features are Preview with their limits; storage / lateral / pumps / HTab are M6; vertical n and multiple openings are M8.

### Packaging
- `release/De-RAS-0.2.2-setup-x64.exe` and `release/De-RAS-0.2.2-win64-portable.zip`, replacing the 0.2.1 files.
- Screenshots: `screenshots/0.2.2/` (0.2.2.png and 23 features, Indonesian UI, Dark Theme, taken after the example project was computed) and `screenshots/0.2.2/varian/` (15 English images: the T2 set plus RAS Mapper, the inundation map, the bridge editor and the unsteady views; 5 Light Theme images).

### Tests
- Linux: `ctest` 16 of 16 pass. New tests: `test_m2` (76 checks: uniform supercritical flow, S2 against RK4, hydraulic jump, confluence and split junctions, ineffective areas / levees / obstructions, files, HDF), `test_m3` (88 checks: GeoTIFF with several compressions, sampling, cut lines, inundation map), `test_m4` (44 checks: weir flow, Yarnell, pressure flow, FHWA, gates, files), `test_m5` (64 checks: dates and intervals, constant flow stays constant on a backwater profile, flood wave volume balance with a difference of 9e-6 %, peak celerity about 5/3 V, downstream stage hydrograph, junctions, lateral inflow, `.u##` files, Model Doctor).
- Windows: the MinGW cross-build succeeds; **not run on Windows** (W1 to W4, K2 and V1 are still open).

### Notes for the documentation
- The standard test project specification changed (terrain, u01, p02): it has to go into `TESTING_EN.md` and the manual.
- Schedule change: storage areas, lateral structures, pumps and HTab to M6; vertical n to M8.
- The Preview status has to be the same in the application, the feature matrix and the manual for every M2 to M5 feature.
- The manual needs new chapters: RAS Mapper, structures, Unsteady Flow Data, Unsteady Analysis, Hydrograph, Time series.
- Gate openings are stored in `.deras.json`; so are the terrain and background picture lists.

## 0.2.1 · Follow-up to the 0.2.0 review, before M2 (19 November 2026)

Status: the second test build. It contains fixes R1 to R9 from the project owner's review of 0.2.0 (see the version notes). No M2 features yet. The computation engine is unchanged, except that the regime message now uses the regime name in the chosen language.

### Fixes from the review
- **R1** The File…Help menu is now vertically centred with the logo, name and version badge.
- **R2** A Home button (house icon) at the left end of the toolbar, in the PROJECT group. The same as Ctrl+1. Tooltip "Home (Ctrl+1)".
- **R3** Back/Forward fixed: Alt+← and Alt+→ work even when the cursor is in an input field, the mouse back/forward buttons work over the whole window (including over tables and plots), the history keeps no duplicate entries or views that can no longer be opened, and Back does nothing while an input sheet is open. There is a new automated test, `test_ui_history` (5 cases, 22 checks).
- **R4** The Model Doctor pop-up when Compute (F5) is pressed while errors remain: every error is shown with its content, location and how to fix it (at most 12 rows). There are quick-fix buttons where possible: **Switch to Subcritical** (the regime is not supported, recomputes at once), **Open Steady Analysis**, **Open Steady Flow**, **Open Geometric Data**, and **Open cross section** for messages that name an RS. Bottom buttons: **Show in the Messages drawer** and **Close**. Counts are written correctly: "1 error / 2 errors".
- **R5** The standard test project `tests/data/uji-standar/DerasUji1km.prj` (Sungai Uji 1 km). Details below.
- **R6** Theme names: **Dark Theme / Light Theme**. Dark Theme stays the default. Old settings ("malam" / "siang") are still read.
- **R7** English becomes the default language (splash screen, window, installer). The first-run language sheet puts English on top.
- **R8** The Indonesian text in the application, the Model Doctor messages and the ⓘ help was rewritten to read more naturally (about 80 sentences). Terms that changed: "galat" → **"kesalahan"**, "laci Pesan" → **"panel Pesan"**, "Toleransi galat maks." → **"Toleransi selisih maks."**, "Pra-proses" → **"Persiapan"**, "tidak dapat" → "tidak bisa".
- **R9** The Light Theme uses a light-blue palette (background #E6EFFA, card #F7FAFF, title text #01215F, primary #0151B7), also on the geometry canvas, plots and the Help Window.
- **R10** (optional, a more realistic Home river animation) **not done**, postponed.

### Standard test project (R5)
- River "Sungai Uji", reach "Utama", 1 km, 6 cross sections RS 1000 to RS 0 every 200 m. A trapezoidal channel with a 14 m bottom width, 1:1 bank slope, 3 m deep (20 m top width), and 30 m overbanks on the left and right that rise 1.5 m to the outer edge.
- Bed slope 0.001, channel n 0.035, overbank n 0.060. Flows Q20/Q60/Q150 = 20, 60, 150 m³/s. Upstream and downstream boundaries are normal depth 0.001. Plan "Plan steady subkritis" (short ID Uji1km).
- Deviation from the proposal: the overbank rises **1.5 m**, not 0.5 m, so that Q150 (normal depth 4.063 m) does not overtop the end points of the cross section.
- Regenerated with `deras-make-uji-standar` (a developer tool, not part of the installer).
- Test results (Linux):
  - S1 read–write round trip without changes: pass (`roundtrip_uji_standar`).
  - S2 Model Doctor: 0 errors, 0 warnings.
  - S3/S4 steady computation compared with normal depth computed separately (left overbank / channel / right overbank split): Q20 1.322 m (Fr 0.286), Q60 2.542 m (Fr 0.307), Q150 4.063 m (Fr 0.330). Maximum difference 0.0000 m.
- This project is now the **example project** on Home. It is copied to `Documents\De-RAS Projects\Contoh\DerasUji1km.prj` if it does not exist yet; an existing copy is not overwritten.

### Other fixes
- The Recent Projects list no longer contains the same project twice (paths are normalised; on Windows without distinguishing case).
- The "Flow regime not available yet" message now names the regime in the chosen language (Mixed / Supercritical).
- HTML report text: "Peringatan dan galat" → "Peringatan dan kesalahan".

### ⓘ help
- Updated: hub.example (standard test project), hub.new (a new project opens at once), tools.language (English by default), tools.theme (Dark / Light Theme), sim.doctor (pop-up and quick fixes), results.view and start ("not yet compared with HEC-RAS"), compute.steps (Persiapan), plan.maxerror ("maximum difference tolerance"). `docs/manual/help-topics.*.md` were rebuilt.

### Developer tools
- `deras-ui-snapshot` was rewritten for the screenshot-folder-per-version rule: `deras-ui-snapshot <folder> <version> <id|en> <dark|light> <project.prj|-> <shot,...>`. It uses temporary settings so it does not change the developer's recent projects list.

### Packaging
- `release/De-RAS-0.2.1-setup-x64.exe` and `release/De-RAS-0.2.1-win64-portable.zip`, replacing the 0.2.0 files.
- Screenshots: `screenshots/0.2.1/` (0.2.1.png and 10 features, Indonesian UI, Dark Theme) and `screenshots/0.2.1/varian/` (English and Light Theme).

### Tests
- Linux: `ctest` 12 of 12 pass (9 old + `test_uji_standar`, `roundtrip_uji_standar`, `test_ui_history`).
- Windows: the MinGW cross-build succeeds; **not run on Windows** (W1 to W4, K2 and V1 are still open).

### Notes for the documentation
- The manual and README need to follow the new terms: Dark / Light Theme (not Malam / Siang), English by default, "kesalahan" (not "galat"), "panel Pesan" (not "laci Pesan"), Persiapan (not Pra-proses), the Home button on the toolbar, the Model Doctor pop-up.
- The example project is now the standard test project `DerasUji1km`, not a prismatic channel.
- `TESTING_EN.md` can be filled in with the S1 to S4 results above.

## 0.2.0 · M1: the new single-window UI (19 November 2026)

Status: the first test build, for the project owner to review. The computation engine is unchanged from 0.1.0.

### Window and navigation
- One main window. The only separate windows are the **Compute Window** and the **Help Window**. All other input (new project, description, reach, distance table, profiles, boundary conditions, save confirmation) appears as a sheet inside the window.
- Header: the De-RAS logo, name, version badge, the menus **File / Edit / Run / View / Options / GIS Tools / Help**, an EXAMPLE badge for the example project, and a project name chip.
- A grouped, labelled toolbar: PROJECT, EDITOR, RUN, GIS, RESULTS & TABLES. When the window is narrow, locked buttons become icons only, then all buttons become icons.
- View bar: a back button, breadcrumb, title, status badge, ⓘ, and the main action of the view on the right. Navigation history with Alt+←/→ and the mouse back/forward buttons.
- The Messages drawer (Ctrl+M) holds the Log and Model Doctor; it is closed by default.
- A thin status bar: status text, run progress (click to open the Compute Window), an always up-to-date Model Doctor error and warning count, units, CRS, language and a Messages button.
- Shortcuts: Ctrl+1 Home, Ctrl+2 Geometric Data, Ctrl+3 Steady Flow, Ctrl+5 Steady Analysis, Ctrl+7 Results, Ctrl+, Options, F5 Compute, F7 Model Doctor, F1 Help, Ctrl+N/O/S. Ctrl+4 (Unsteady) and Ctrl+6 (Mapper) are locked.

### Views
- **Home**: the Project Control card, the project description, recent projects, the example project.
- **Geometric Data**: the editor palette, the schematic canvas (mouse wheel zoom, drag to pan), the Selected object card, Draw reach (click points, double-click or Enter to finish, Esc to cancel), Manage reaches (add, rename, which also updates the flow data, delete), Tables… for reach lengths and coefficients, a new geometry file.
- **Cross-Section Editor** (a sub-view of Geometric Data): choose reach and RS, PgUp/PgDn, add / copy / delete, CSV survey import, the station–elevation table, a plot with the computed water surface of up to 4 profiles and an out-of-date note, the Section data card (n in three parts or a table of n points).
- **Steady Flow**: a new flow file, the discharge table, profiles (at most 12 characters, no commas, no duplicates), a boundary condition sheet per reach × profile with copy to all profiles, CSV import and export.
- **Steady Analysis**: the plan (+ new plan), title, short ID, geometry, flow, regime, results file, computation options, and a live Model Doctor card (double-click opens the cross section).
- **Results**: cross section, longitudinal profile, general plot, detailed table, profile table, run messages; CSV export and HTML report. The Rating Curve, 3D, Hydrograph and Time Series pages are locked.
- **Options**: theme, font size, language; units; project settings (CRS, time zone, target HEC-RAS version 7.00, notes).
- **Compute Window** (replacing the Run Manager): the steps pre-process / simulation / write results (the map step is locked, M3), progress and time, the queue, a log, Cancel / Close / View results. Closing while a run is in progress asks: go back, cancel the run, or close and keep running.

### Theme and assets
- The **Malam** (dark) theme becomes the default; **Siang** (light) can be chosen in Options or the View menu.
- The fonts Inter (SIL OFL 1.1) and JetBrains Mono 2.304 (SIL OFL 1.1; earlier entries said Apache-2.0, corrected because the font file's own metadata states OFL 1.1) are embedded; their licenses go in the `licenses` folder of the Windows package.
- ⓘ is small and dim; the focus ring only appears during keyboard navigation.

### Locked features
Every M2 to M8 feature button shows as locked "Not available yet · Mx" with a milestone tooltip, from one list in `ui/Features.cpp`. Examples: supercritical / mixed, upstream boundary, junctions, ineffective areas / levees / obstructions, vertical n, Rating Curve, `.p##.hdf` (M2); Mapper, terrain, inundation map, 3D (M3); bridges, culverts, inline structures (M4); unsteady, lateral, storage areas, pumps, hydrograph (M5); 2D areas (M6); sediment, quasi-unsteady (M7); water quality, hydraulic design, unit conversion, Word / PDF reports (M8).

### Feature matrix in the application
- Steady subcritical is **Done** (analytical benchmarks pass), with a note that it has not been compared with HEC-RAS.
- The feature rows now name their planned milestone.

### ⓘ help
- 31 new topics for the 0.2.0 views (view.*, geometry.palette/selected/newfile/xsdata, flow.newfile, flow.col.bcupstream, plan.output, home.*, options, project.*, messages.drawer, compute.*, results.*).
- Old topics updated: "Phase 1/2/3" replaced by milestones M2 to M8; the ribbon topics (ribbon.*), Model Tree (dock.*) and sim.runmanager were removed; New Project is now one input sheet, not a 4-step wizard.
- The help status now supports "Preview".

### Packaging
- `tools/package/make_windows_dist.sh` copies the font licenses and only copies files (not folders) from `docs/manual`.
- Test build: `release/De-RAS-0.2.0-setup-x64.exe` and `release/De-RAS-0.2.0-win64-portable.zip` (overwriting the 0.1.0 build, following the file versioning rule).

### Tests
- Linux: clean compile, `ctest` 9 of 9 pass, screenshots of every view (ID/EN, Malam/Siang) checked, and the check that opening and viewing a project does not change project files passes.
- Windows: the MinGW cross-build succeeds; **not run on Windows** (tests W1 to W4, K2 and V1 wait for a PC or Remote Control).

### Notes for the documentation
- The manual needs to replace the terms ribbon / tab, Model Tree, project wizard and Run Manager with the new view names above. The help topics chapter (`docs/manual/help-topics.*.md`) was already rebuilt from the help source.

### Follow-up (19 November 2026)
- Feature matrix in the application: the row "Windows installer and portable ZIP" now carries the note "not tested on a real Windows machine (W1 to W4)". The test build in `release/` was rebuilt.
