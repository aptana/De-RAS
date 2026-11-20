# De-RAS feature matrix

Version 0.2.3 (ALPHA, test build; M6 to M8), 20 November 2026. This list matches what the application shows (**Help › Feature matrix**, source `ui/Dialogs.cpp`). Locked feature buttons take their milestone from `ui/Features.cpp`.

Status: **Done** means built and covered by automated tests. **Preview** means usable, but validation is not complete. **Partial** means only part of it works. **Not available yet** means it appears in the UI locked, with the label "Not available yet · Mx".

"Done" does not mean the results equal HEC-RAS results. As of 0.2.3, **no feature has been compared with HEC-RAS** (see `TESTING_EN.md`). All new computation and map features from M2 to M8 are **Preview**. The exceptions that the application labels Done are the upstream boundaries, the rating curve, the 3D perspective, HTab parameters (not needed, because the hydraulic tables are built automatically) and autosave (not tested on a real Windows machine). Not built: vertical n (Partial: read and kept, not computed), multiple openings, 2D area breaklines and n regions (not scheduled), HEC-RAS `.hdf` terrain, reprojection and online base maps. `.hdf` files for unsteady results are not written either. Build 0.2.3 **has not been run on Windows**; tests W1 to W4, K2 and V1 are still open.

| Area | Feature | Status | Notes |
|---|---|---|---|
| Application | Single-window UI, Dark and Light themes | Done | New in 0.2.0 (M1); theme names changed in 0.2.1 |
| Application | Indonesian / English | Done | |
| Application | Offline bilingual ⓘ help, Help window | Done | |
| Application | Windows installer and portable ZIP | Done | Cross-compiled; not tested on a real Windows machine (W1 to W4) |
| Project | Create, open and save a HEC-RAS project (.prj) | Done | Lossless for files that were not changed |
| Project | Duplicate project (Save As) | Done | |
| Project | Example project (the 1 km standard test project, Sungai Uji) | Done | Synthetic data, labelled Example |
| Geometry | Reach schematic, drawing the reach centreline | Done | A schematic, not a GIS map |
| Geometry | 1D cross-section editor, CSV survey import | Done | |
| Geometry | Ineffective areas, levees, obstructions | Preview | Edited and computed; not compared with HEC-RAS |
| Geometry | Vertical n | Partial | Read and kept in the file; not computed (not in 0.2.3) |
| Geometry | Junctions (create, edit, delete, compute) | Preview | Energy equation; not compared with HEC-RAS |
| Geometry | Bridges (energy, Yarnell, pressure flow, deck weir) | Preview | Subcritical only; the `.g##` block format is not verified; not compared with HEC-RAS |
| Geometry | Circular and box culverts (FHWA inlet and outlet control) | Preview | Other shapes are kept but not computed; the `.g##` format is not verified |
| Geometry | Inline structures: weir crest and gates | Preview | Gate openings are stored in `.deras.json`; the `.g##` format is not verified |
| Geometry | Multiple openings | Not available yet | Kept in the file; not computed (not in 0.2.3) |
| Geometry | Lateral structures (weir, breach), storage areas, SA/2D connections | Preview | Computed in unsteady runs; breach data and pumps are stored in `.deras.json`; not compared with HEC-RAS |
| Geometry | 2D areas (square grid, terrain subgrid), BC lines, pump stations | Preview | Diffusion wave; not the HEC-RAS polygon mesh; not compared |
| Geometry | 2D area breaklines and n regions | Not available yet | Not scheduled in M1 to M8 |
| Geometry | HTab parameters | Done | Not needed: hydraulic tables are built automatically; HTab lines in a file are kept |
| Flow | Steady flow data (.f##), CSV | Done | |
| Flow | Downstream boundary: normal depth, known WS, critical | Done | |
| Flow | Rating-curve boundary (downstream) | Partial | The `.f##` format is not verified against HEC-RAS |
| Flow | Upstream boundary: normal depth, known WS, critical | Done | For supercritical and mixed regimes |
| Flow | Unsteady flow (.u##): hydrographs, normal depth, rating curve, lateral inflow | Preview | Unknown lines are kept intact; the Rating Curve and lateral formats are not verified |
| Flow | Area boundaries: hydrographs, normal depth on BC lines, precipitation; initial area stages | Preview | The area line format in `.u##` is not verified |
| Flow | Quasi-unsteady flow data (flow series, downstream boundary) | Preview | Stored in `.deras.json`; HEC-RAS `.q##` files are not read or written |
| Flow | Sediment data (transport function, gradation, inflow load) | Preview | Stored in `.deras.json`; HEC-RAS `.s##` files are not read or written |
| Simulation | Steady subcritical (standard step) | Done | Analytical benchmarks pass; agreement with HEC-RAS not compared |
| Simulation | Steady supercritical and mixed (hydraulic jump) | Preview | Analytical benchmarks pass; not compared with HEC-RAS |
| Simulation | Model Doctor (live data checks) | Done | |
| Simulation | Compute window: separate process, queue, cancel | Done | |
| Simulation | 1D unsteady (implicit Preissmann) | Preview | Subcritical; bridges, culverts and inline structures are not computed; not compared with HEC-RAS; no `.hdf` output |
| Simulation | 1D–2D unsteady: storage areas, 2D areas, lateral weirs, pumps | Preview | Volume error is reported per area; not compared with HEC-RAS |
| Simulation | Quasi-unsteady sediment (MPM, Engelund-Hansen, Yang; Exner) | Preview | One reach, one grain size (d50), no armouring; not compared with HEC-RAS |
| Simulation | Water quality: one constituent, advection-dispersion, first-order decay | Preview | Uses the unsteady results; no temperature, DO or nutrients; `.w##` files are not read or written; not compared |
| Results | Cross section, profile, general profile | Done | |
| Results | Detailed and profile tables, message summary | Done | |
| Results | CSV export and HTML report | Done | |
| Results | Word (.docx) and PDF report | Preview | Made without Microsoft Word; not opened in a real Word |
| Results | Rating curve | Done | |
| Results | `.p##.hdf` results (HEC-RAS layout) | Preview | Readable with h5dump; not opened in HEC-RAS (K3) |
| Results | Hydrograph, time series | Preview | Cross sections, storage areas, lateral structures, connections, pumps |
| Maps | 2D area result maps: depth, velocity, arrival time | Preview | Depth per terrain pixel; velocity per cell |
| Maps | RAS Mapper: projection, GeoTIFF terrain, background pictures | Preview | No GDAL; cannot reproject yet |
| Maps | Cut cross sections from terrain (GIS cut lines) | Preview | Not compared with HEC-RAS RAS Mapper |
| Maps | Steady inundation map, GeoTIFF and PNG export | Preview | No flow-connectivity check |
| Maps | HEC-RAS `.hdf` terrain, reprojection, online base maps | Not available yet | Not in 0.2.3 |
| Results | 3D perspective | Done | |
| Other | Hydraulic design: uniform flow, pier, contraction and abutment scour (HEC-18) | Preview | Not compared with the HEC-RAS hydraulic design functions |
| Other | Manning's n calibration (one factor, steady plan) | Preview | A single factor for the channel or for all n |
| Other | Project unit conversion SI ↔ US (project copy) | Preview | Terrain and GIS layers are not converted |
| Other | Autosave and crash recovery | Done | Not tested on a real Windows machine |

History: the 0.1.0 matrix (Phase 1) was replaced by this one. Changes are recorded in the project's version notes (not published) and in `CHANGELOG_EN.md`.
