# Test plan and validation results (Phase 1)

Date: 19 November 2026. Written for 0.1.0 ALPHA; updated for the 0.2.3 test build (20 November 2026).

## Honest summary
- All automated tests on Linux **pass**: 9 of 9 suites in 0.1.0 (41 test cases plus file round-trip tests), **12 of 12** in build 0.2.1 after `test_uji_standar`, `roundtrip_uji_standar` and `test_ui_history` were added, **16 of 16** in build 0.2.2 after `test_m2`, `test_m3`, `test_m4` and `test_m5`, and **19 of 19** in build 0.2.3 after `test_m6`, `test_m7` and `test_m8`.
- The solver is checked against **independent analytical or numerical solutions** (normal depth, RK4 for gradually varied profiles). **It has not been compared with HEC-RAS output.** For that reason De-RAS does **not** claim its results equal HEC-RAS results.
- The Windows programs (`De-RAS.exe`, `deras-steady.exe`, the installer) **compile but have not been run on Windows**. Wine in the build environment fails to run (it crashes before the program loads), so the Windows smoke tests have to be done on a real PC.

## Automated test suites (`ctest`)
| Test | Contents | Result |
|---|---|---|
| test_units | SI and US constants (g, Manning k, tolerances), conversions, typed units | pass |
| test_text | HEC-style 8-character number format, parsing, fixed columns, byte-identical output, plain format | pass |
| test_json_hash | SHA-256 (standard test vectors), JSON round trip and error messages | pass |
| test_xs | Rectangular and trapezoidal section properties, overbank subdivision and alpha, composite n, vertical walls, critical depth (analytical) and normal depth (reference) | pass |
| test_benchmark | B1 to B5 (see below) | pass |
| test_io | Fixture parsing, byte-identical round trip, edits that keep unknown lines, adding and removing cross sections, flow and plan, new project save and open, a file that is not a RAS project | pass |
| test_doctor | Good and bad cross sections, flow and boundary errors, ordering and structures, regimes, bilingual messages | pass |
| test_run | Run, save, reopen results; an invalid model leaves no results; safe cancel; an unsupported regime is refused | pass |
| test_uji_standar (0.2.1) | Standard test project: clean Model Doctor, steady run, depths compared with normal depth (S2 to S4) | pass |
| roundtrip_uji_standar (0.2.1) | Read and write the standard test project without changes (S1) | pass |
| test_ui_history (0.2.1) | Back/Forward history: 5 cases, 22 checks | pass |
| test_m2 (0.2.2) | 76 checks: uniform supercritical flow, S2 profile against RK4, hydraulic jump, confluence and split junctions, ineffective areas, levees and obstructions, files, HDF | pass |
| test_m3 (0.2.2) | 88 checks: GeoTIFF with several compressions, terrain sampling, cut lines, inundation map | pass |
| test_m4 (0.2.2) | 44 checks: weir flow, Yarnell, pressure flow, FHWA culverts, gates, files | pass |
| test_m5 (0.2.2) | 64 checks: dates and intervals, constant flow stays constant on a backwater profile, flood wave volume balance (difference 9e-6 %), peak celerity about 5/3 V, downstream stage hydrograph, junctions, lateral inflow, `.u##` files, Model Doctor | pass |
| test_m6 (0.2.3) | 93 checks: weir flow, storage areas, SA/2D connections, pumps, lateral structures, breach, 2D area BC lines, lateral flow into a 2D area, area blocks in `.g##` and `.u##`, Model Doctor | pass |
| test_m7 (0.2.3) | 59 checks: transport functions, equilibrium load, clear water with an erodible depth limit, rating curve, result files | pass |
| test_m8 (0.2.3) | 46 checks: uniform flow against hand calculation, pier, contraction and abutment scour against the HEC-18 formulas, calibration recovering a factor of 1.300 from synthetic data, water quality with a closed mass balance, SI to US to SI conversion and project copy, autosave recovery, `.docx` structure and CRC-32 | pass |
| roundtrip_fixture | `deras-roundtrip --edit-check` on the fixtures: 4 of 4 files identical, lossless regeneration | pass |

## Analytical benchmarks
| Case | Reference | Maximum difference |
|---|---|---|
| B1 uniform flow (SI) | normal depth 2.1627 m | 0.00000 m |
| B2 M1 backwater (SI) | fine RK4 integration | Δx 50 m: 0.00007 m; 100 m: 0.00022 m; 200 m: 0.00108 m; default tolerance: 0.00014 m |
| B3 M2 drawdown (SI) | RK4 (x > 250 m) | 0.00008 m |
| B4 M1 backwater (US) | RK4, yn = 7.346 ft | 0.00033 ft |
| B5 energy balance | energy equation residual at every step | 0.000015 m |

The differences shrink as the cross-section spacing is reduced, as expected of a second-order standard step method.

## UI tests (Linux, headless)
`deras-ui-snapshot` renders the splash screen, Home and every tab (ID and EN) to PNG and checks that opening and viewing a project **does not change any file**. Result: pass. Findings that have been fixed: a false "out of date" warning, the water fill in the longitudinal profile, and the water surface line in the cross-section plot.

## Standard test project (since build 0.2.1)
Requested by the project owner after the 0.2.0 review: one fixed project that is used to test **every analysis in every build**. A 1 km river, cross sections every 200 m (RS 1000 to 0), 20 m wide. The full specification is in the project's version notes (item R5). Files: `tests/data/uji-standar/DerasUji1km.prj` with `.g01`, `.f01`, `.p01`: a trapezoidal channel with a 14 m bottom width (20 m top width, 3 m deep), 30 m overbanks rising 1.5 m, bed slope 0.001, n 0.035/0.060, flows 20/60/150 m³/s, normal depth upstream and downstream. The project is also the example project in the application. Since 0.2.2 it also has the terrain `Terrain/DerasUji1km_terrain.tif` (1 m resolution, no EPSG), the unsteady flow file **u01 "Banjir uji 24 jam"** (initial flow 20 m³/s; an hourly hydrograph rising from 20 to 150 m³/s at hour 6 and falling back to 20 m³/s at hour 18; downstream normal depth 0.001), and the plan **p02 "Plan unsteady banjir uji"** (01JAN2024 0000 to 02JAN2024 0000, computation interval 1MIN, output 30MIN). g01, f01 and the active plan p01 are unchanged apart from time stamps. Since 0.2.3 it also has **g02/u02/p03** (a 100 m side weir on the right levee into the 2D area "Dataran Kanan", the unsteady plan "Plan unsteady banjir + dataran 2D", short ID `Dataran2D`), **sediment data** in g01 (22 days of quasi-unsteady flow, sandy gravel with d50 2 mm, clear water), and **water quality data** (a 100 mg/L tracer for 3 hours upstream on the p02 flood).

| ID | Test on the standard test project | Status |
|---|---|---|
| S1 | Open and save without changes: all files identical byte for byte | Passed in 0.2.1 (`roundtrip_uji_standar`) |
| S2 | Model Doctor: no errors | Passed in 0.2.1: 0 errors, 0 warnings |
| S3 | Steady subcritical run of 3 profiles completes | Passed in 0.2.1. Depths: Q20 1.322 m (Fr 0.286), Q60 2.542 m (Fr 0.307), Q150 4.063 m (Fr 0.330; overtops onto the overbanks) |
| S4 | Result depths compared with normal depth computed separately (left overbank / channel / right overbank split), all profiles | Passed in 0.2.1: maximum difference 0.0000 m |
| S5 | Project opened in HEC-RAS and the results compared (part of K2/V1). Note: in the code, `test_uji_standar` labels the M6 to M8 tests S5 to S7; in this document those tests are numbered S9 to S11. | Needs a PC with HEC-RAS |
| S6 | Supercritical and mixed regimes on p01 (M2) | Tested in `test_m2` with analytical cases; no figures specific to this project yet |
| S7 | Terrain: cut cross sections from terrain and the Q150 inundation map (M3) | Runs in 0.2.2: p01 Q150 flooded area 6.30 ha (from the inundation map screenshot); not compared with RAS Mapper |
| S8 | Unsteady run p02 (M5) | Runs in 0.2.2: 1,440 steps, downstream peak flow 146.2 m³/s, about 0.5 hours after the upstream peak |
| S9 | Plan p03: the side weir overtops into the 2D area and the area volume balance closes (M6; label S5 in the code) | Runs in 0.2.3: Model Doctor with no errors, area volume error below 1 % (before the dry-cell fix it could be 107 %). Not compared with HEC-RAS |
| S10 | Clear-water sediment over 22 days: the bed erodes upstream and the mass balance closes (M7; label S6 in the code) | Runs in 0.2.3; detailed figures are in the `ctest` output. Not compared with HEC-RAS |
| S11 | Water quality: a 100 mg/L tracer pulse for 3 hours on the p02 flood, mass balance (M8; label S7 in the code) | Runs in 0.2.3: hydraulic continuity correction 5.6 kg out of 114.8 t of inflow mass. Not compared with HEC-RAS |

Every later milestone adds rows here without changing the base geometry of the project. The example project in the application records its version in `versi-contoh.txt`; **File › Restore example project…** installs the new version and moves the old copy to `Contoh-lama-<date time>`.

## Not tested yet (needs the project owner or Remote Control)
| ID | Test | How |
|---|---|---|
| W1 | The installer on Windows 10/11: install, shortcuts, run, uninstall | Run `De-RAS-0.2.3-setup-x64.exe` (test build 0.2.3) |
| W2 | Detecting an older De-RAS and asking before removing it | Install twice |
| W3 | Splash screen, first-run language choice (English by default), changing language and theme, the Home button, Back/Alt+←, running with F5, the Model Doctor pop-up, cancelling, the HTML report on Windows | Use the example project (Sungai Uji 1 km) |
| W4 | A static splash screen when "Show animations in Windows" is off | Windows settings → Accessibility → Visual effects |
| K2 | A file saved by De-RAS opens in HEC-RAS 6.x/7.x without error messages | Open the example project in HEC-RAS |
| K3 | A `.p##.hdf` results file made by De-RAS opens in HEC-RAS / RAS Mapper | Compute p01 in De-RAS, then open the results in HEC-RAS |
| K4 | The `.g##` structure blocks (bridges, culverts, inline structures) and the `.u##` Rating Curve and lateral inflow lines made by De-RAS are read correctly by HEC-RAS | Save a project with structures in De-RAS, open it in HEC-RAS, compare the content |
| W5 | Autosave and recovery: change a project, kill De-RAS from Task Manager, reopen the project, and check that the recovery offer appears | Options → Autosave (default 5 minutes) |
| W6 | The Word (.docx) and PDF reports open correctly in Microsoft Word and a PDF reader | Results → Report… |
| K5 | 2D area and storage area results compared with HEC-RAS on the same case; De-RAS uses a square diffusion-wave grid, not the HEC-RAS polygon mesh | Compare maximum depth and arrival time |
| V1 | Compare De-RAS and HEC-RAS water surfaces on the same project | Run both and compare the tables (target: difference ≤ 0.003 m) |
| V2 | A HEC-RAS example project as a round-trip fixture | Needs an example file from a HEC-RAS installation |
