# Help per feature (ⓘ topics)

<!-- Dibuat otomatis dari tools/help/help_source.py; jangan diedit manual. / Generated from tools/help/help_source.py; do not edit by hand. -->

## Getting started with De-RAS

`start`

_Basic workflow: project → geometry → flow → plan → run → results._

**Purpose.** De-RAS computes 1D steady water surface profiles (subcritical, supercritical or mixed regime) and uses HEC-RAS project files (.prj, .g##, .f##, .p##) as its native format. Projects saved by De-RAS can still be opened in HEC-RAS, and vice versa for supported features.

**Steps:**

1. On Home, create a new project (Ctrl+N), open a .prj file (Ctrl+O), or open the example project.
2. Open Geometric Data (Ctrl+2); double-click a cross section to open the Cross Section Editor.
3. Enter discharges and downstream boundaries in Steady Flow (Ctrl+3).
4. Check the plan in Steady Analysis (Ctrl+5), then press F7 (Model Doctor) and F5 (Compute).
5. The Compute window shows progress; when it finishes click View results.
6. In Results, review cross sections, the longitudinal profile and tables; export CSV or an HTML report.

**Data needed.** Cross sections (station–elevation), Manning's n, reach lengths, discharge per profile, downstream boundary.

**Result.** Water surface, energy grade, velocity, area, top width and Froude number per cross section. Equivalence with HEC-RAS has not been validated.

**Status:** Available

## New project

`hub.new`

_Creates a new HEC-RAS project from a single form (Ctrl+N)._

**Purpose.** Creates .prj, .g01, .f01 and .p01 files in the folder you choose.

**Steps:**

1. Click New project on Home or File → New project.
2. Section 1: enter a title, file name and folder.
3. Section 2: choose units, coordinate system and time zone.
4. Section 3: enter river and reach names, then choose an empty or prismatic-channel template.
5. Click Create project.

**Data needed.** A file name without spaces; a writable folder.

**Result.** The new project opens right away: the prismatic template on Home, the empty template in Geometric Data. De-RAS never overwrites an existing project.

**Status:** Available

## Open project

`hub.open`

_Opens a HEC-RAS .prj file with the geometry, flow and plan files it lists._

**Purpose.** De-RAS reads every file listed in the .prj. Parts it does not support yet (e.g. bridges) are kept unchanged when saving.

**Steps:**

1. Click Open project.
2. Choose the .prj file.
3. Check the Messages panel for read warnings.

**Data needed.** The .prj file and the files it references in the same folder.

**Result.** The project opens; unchanged files are written back byte-for-byte identical.

**Status:** Available

## Duplicate project

`hub.duplicate`

_Copies all project files to a new name or folder (Save As)._

**Purpose.** Useful for creating scenarios without changing the original project.

**Steps:**

1. Click Duplicate.
2. Choose a new .prj name.
3. The copy opens automatically.

**Data needed.** The source project (open or chosen).

**Result.** A complete copy with the new file names.

**Status:** Available

## Example project

`hub.example`

_Opens the De-RAS standard test project (Sungai Uji 1 km), labelled EXAMPLE._

**Purpose.** A 1 km river with 6 cross sections every 200 m, a 20 m wide main channel, and flows of 20, 60 and 150 m³/s. Made-up data for learning and for testing every analysis feature, not field data.

**Steps:**

1. Click Example project on Home.
2. Press F5 to compute.
3. Click View results in the Compute window.

**Result.** Project DerasUji1km in Documents\De-RAS Projects\Contoh. An existing copy is not overwritten.

**Status:** Available

## Recent projects

`hub.recent`

_Projects opened or saved recently. Double-click to open._

**Purpose.** A ⚠ mark means the file was not found (moved or deleted).

**Status:** Available

## New project: Name and location

`wizard.page1`

_Project title, file name and folder._

**Status:** Available

## New project: Units, coordinates, time zone

`wizard.page2`

_Project unit system and georeference information._

**Status:** Available

## New project: River and reach

`wizard.page3`

_Name of the first river and reach._

**Status:** Available

## New project: Template

`wizard.page4`

_Empty project or a ready-to-run prismatic channel._

**Status:** Available

## Project title

`wizard.title`

_Title shown in HEC-RAS and De-RAS (Proj Title line in the .prj)._

**Status:** Available

## Project file name

`wizard.basename`

_Base name of all project files, e.g. River → River.prj, River.g01. Letters, digits, _ and - only._

**Status:** Available

## Project folder

`wizard.folder`

_Folder where all project files are stored. Created if missing._

**Status:** Available

## Unit system

`wizard.units`

_SI (m, m³/s) or US Customary (ft, cfs). Sets the Manning constant (1.0 or 1.486) and gravity._

**Purpose.** Choose to match your survey data. Changing units after data entry requires conversion (not available yet).

**Status:** Available

## Coordinate system

`wizard.crs`

_EPSG code or WKT. Stored in the De-RAS sidecar file and used by RAS Mapper to check terrains._

**Status:** Available

## Time zone

`wizard.timezone`

_Project time zone (default WIB, UTC+07:00). Stored in the project. Unsteady Analysis uses the plan dates and times as entered, without time zone conversion._

**Status:** Partial

## River and reach names

`wizard.river`

_Maximum 16 characters each (HEC-RAS format limit)._

**Status:** Available

## Save project

`project.save`

_Saves every changed file in HEC-RAS format (Ctrl+S)._

**Purpose.** Only changed files are rewritten, atomically (temporary file then replace), so the old file is not damaged if saving fails. Unknown parts of files are preserved.

**Status:** Available

## Close project

`project.close`

_Closes the project and returns to the start page. You are asked if there are unsaved changes._

**Status:** Available

## Project folder

`project.folder`

_Opens the project folder in Windows Explorer._

**Status:** Available

## Cross Section Editor

`geometry.editor`

_Edit reaches and 1D cross sections: station–elevation, Manning's n, lengths, banks and coefficients._

**Purpose.** All edits go straight into the .g## model and are saved with Ctrl+S. The cross-section plot updates immediately.

**Steps:**

1. From Geometric Data, double-click a cross section on the canvas or click Cross section in the palette.
2. Choose the reach and RS in the top row; PgUp/PgDn move to the previous/next section.
3. Edit the station–elevation table; paste from Excel with Ctrl+V.
4. Enter downstream lengths, banks, Manning's n and coefficients in the Section data card.
5. Add, copy or delete sections with the buttons in the top row; save with Ctrl+S.

**Data needed.** Cross-section survey data; Manning's n values; reach lengths (left, channel, right).

**Result.** A .g## file that HEC-RAS can open.

**Status:** Available

## Reach

`geometry.reach`

_A river segment holding ordered cross sections. Add or rename reaches; river/reach names max. 16 characters._

**Purpose.** Renaming a reach also updates the references in the flow data.

**Status:** Available

## Cross-section list

`geometry.xslist`

_Cross sections of this reach, ordered upstream (largest RS) to downstream._

**Purpose.** Bridges, culverts and inline structures are shown and edited from the palette (double-click the structure mark on the canvas). Other structures are kept in the file but not computed yet.

**Status:** Available

## Import survey CSV

`geometry.import`

_Imports cross sections from CSV: RS, station, elevation per row._

**Purpose.** Each new RS becomes a new cross section in the selected reach; Manning's n, lengths and banks must be completed afterwards.

**Steps:**

1. Prepare a CSV with columns RS,Station,Elevation (dot or comma decimals).
2. Click Import survey CSV and choose the file.
3. Check the summary, then complete the cross-section data.

**Status:** Available

## River Station (RS)

`geometry.rs`

_Cross-section identifier. A larger number is further upstream._

**Status:** Available

## Description

`geometry.description`

_Free notes for this cross section (stored in the .g## file)._

**Status:** Available

## Downstream reach lengths

`geometry.lengths`

_Distance from this section to the next one downstream, for the left overbank, main channel and right overbank._

**Purpose.** Used to compute friction loss with a discharge-weighted length. The most downstream section may be 0.

**Status:** Available

## Bank stations

`geometry.banks`

_Left and right bank stations separating the overbanks from the main channel._

**Purpose.** Must lie between the first and last station–elevation points. Model Doctor checks them.

**Status:** Available

## Contraction and expansion coefficients

`geometry.coefficients`

_Loss coefficients for changes in velocity head. Typical: 0.1 and 0.3._

**Purpose.** Loss = C × |Δ(αV²/2g)|. Contraction when the downstream velocity head is larger, expansion when smaller.

**Status:** Available

## Station–elevation table

`geometry.staelev`

_Cross-section points from left to right (looking downstream). Stations must not decrease._

**Purpose.** Red cells hold invalid values that are not stored; the previous value is kept. Pasting from Excel is supported.

**Status:** Available

## Manning's n table

`geometry.manning`

_Manning's n breakpoints: from this station to the right the given n applies._

**Purpose.** Overbanks are subdivided at each n breakpoint; the main channel uses a composite n when it has more than one value.

**Status:** Available

## Cross-section plot

`geometry.plot`

_Cross-section view with bank positions and n breakpoints. Hover to read coordinates._

**Status:** Available

## Station column

`geometry.col.station`

_Horizontal distance from the left reference, in project length units._

**Status:** Available

## Elevation column

`geometry.col.elevation`

_Ground elevation at that station, in project length units._

**Status:** Available

## n Station column

`geometry.col.nstation`

_Station where the n value starts to apply._

**Status:** Available

## n value column

`geometry.col.nvalue`

_Manning's roughness coefficient (dimensionless), e.g. 0.03 for a clean natural channel._

**Status:** Available

## Structures

`geometry.structures`

_Bridges, culverts and inline structures (weirs with gates) are edited from the Geometric Data palette and computed in the subcritical regime. Lateral structures, multiple openings and pumps are not available yet._

**Purpose.** A structure sits between two cross sections by its river station. The upstream and downstream sections bound the computation; the length from the upstream to the downstream section stays on the upstream section.

**Result.** Preview status: results are not compared with HEC-RAS yet and the structure block format in the .g## file is not verified. Structures that cannot be read are kept unchanged and Model Doctor refuses the run.

**Status:** Preview

## Bridge

`geometry.bridge`

_A bridge with deck, piers and flow over the deck._

**Purpose.** Low flow (water below the deck): energy method through sections inside the bridge with piers as obstructions, and the Yarnell equation (K = 1.25); the highest energy answer is used. High flow: sluice gate equation (Cd from Y3/Z) or orifice (C = 0.8) when the downstream low chord is submerged, plus weir flow over the deck with Villemonte submergence.

**Steps:**

1. Open Geometric Data, click Bridge / Culvert in the palette.
2. Pick the reach, click Add bridge.
3. Enter the river station (between two sections), distance to the upstream face, deck width and weir coefficient.
4. Fill the deck table (station, high chord, low chord) and the piers.
5. Click Add, then save the project.

**Data needed.** A Type RM = 3 block with Bridge Culvert-, Deck Dist Width WeirC and Pier lines in the .g## file.

**Result.** Preview status: not compared with HEC-RAS yet; the block format in the .g## file is not verified. Subcritical regime only.

**Status:** Preview

## Culvert

`geometry.culvert`

_Circular or box culverts under a road embankment._

**Purpose.** The upstream energy is the larger of inlet control (FHWA HDS-5 form 1 equations, charts 1, 2, 8 and scales 1–3, with the unsubmerged/submerged transition between 3.5 and 4.0) and outlet control (entrance, friction and exit losses with full flow above max(TW, (dc + D)/2)). Flow over the road is computed as weir flow.

**Steps:**

1. Click Bridge / Culvert in the palette, then Add culvert.
2. Fill the road table (high chord = low chord for an embankment).
3. Enter one row per group of identical barrels: shape, span, rise, length, n, Ke, Kx, chart, scale, inverts and barrel stations.
4. Click Add.

**Data needed.** Culvert= lines in a Type RM = 2 block of the .g## file (one line per barrel).

**Result.** Preview status: not compared with HEC-RAS yet; the block format in the .g## file is not verified. Subcritical regime only.

**Status:** Preview

## Inline structure

`geometry.inline`

_A weir or spillway across the river, with or without gates._

**Purpose.** Flow is split between the weir crest (Q = C L H^1.5, Villemonte submergence; gate openings excluded) and the gates: sluice gate, orifice when the tailwater ratio exceeds 0.67, or weir flow over the sill when the water does not reach the gate lip. The upstream energy is found by bisection.

**Steps:**

1. Click Inline Structure in the palette, then Add inline structure.
2. Fill the crest table (station, elevation).
3. Enter the gates: name, width, height, sill elevation, coefficient, opening centre stations and the opening for steady runs.
4. Click Add, then save the project.

**Data needed.** IW Dist, #Inline Weir SE= and IW Gate Name lines in a Type RM = 5 block of the .g## file. Gate openings are stored in <name>.deras.json because De-RAS does not write them to the flow file yet.

**Result.** Preview status: not compared with HEC-RAS yet; the block format in the .g## file is not verified. Subcritical regime only.

**Status:** Preview

## Deck/road columns

`geometry.col.deck`

_Station, high chord (top of deck or road) and low chord (underside of the deck). For a road embankment without an opening the low chord equals the high chord._

**Status:** Preview

## Pier columns

`geometry.col.pier`

_Centre station, width, bottom and top elevation of the pier. Piers reduce the opening area and are used in the Yarnell equation._

**Status:** Preview

## Culvert columns

`geometry.col.culvert`

_One row per group of identical barrels. The number of barrels equals the number of barrel stations. Chart and scale follow the FHWA HDS-5 table._

**Status:** Preview

## Crest columns

`geometry.col.weir`

_Station and crest elevation of the weir from left to right._

**Status:** Preview

## Gate columns

`geometry.col.gate`

_Width and height of each opening, sill elevation, sluice/orifice coefficient, centre station of each opening and the opening for steady runs (empty = fully open)._

**Status:** Preview

## Special areas

`geometry.areas`

_Ineffective areas, levees and obstructions are edited and computed per cross section. Storage areas and 2D areas are not available yet._

**Purpose.** Open the Cross-Section Editor, then the Ineffective, Obstruction or Levee button on the Section data card.

**Status:** Partial

## Ineffective flow areas

`geometry.ineffective`

_Parts of the section that hold water but do not convey it, for example behind a road embankment or beside a bridge._

**Purpose.** While the water surface is at or below its elevation, water inside the station range is left out of area, wetted perimeter and conveyance. Once the water surface is above it, the area conveys flow again. Permanent areas never convey flow.

**Steps:**

1. Open the Cross-Section Editor and pick a section.
2. Click Ineffective on the Section data card.
3. Enter start station, end station, elevation and Permanent (0 or 1) per row.
4. Click Apply, then save the project.

**Data needed.** The #XS Ineff= and Permanent Ineff= lines of the .g## file.

**Result.** Preview status: results have not been compared with HEC-RAS yet.

**Status:** Preview

## Blocked obstructions

`geometry.obstruction`

_Solid parts of the section, for example buildings or fill on the overbank._

**Purpose.** Inside the station range the ground is raised to the given elevation. The area below disappears and its vertical sides add wetted perimeter.

**Steps:**

1. Open the Cross-Section Editor and pick a section.
2. Click Obstruction on the Section data card.
3. Enter start station, end station and elevation per row.
4. Click Apply.

**Data needed.** The #Block Obstruct= line of the .g## file.

**Result.** Preview status: results have not been compared with HEC-RAS yet.

**Status:** Preview

## Levees

`geometry.levee`

_An embankment left or right of the channel that holds water back until its crest is overtopped._

**Purpose.** Water beyond a levee (left of the left levee, right of the right levee) does not flow until the water surface is above the levee crest. When the crest is above the ground, the ground is raised there.

**Steps:**

1. Open the Cross-Section Editor and pick a section.
2. Click Levee on the Section data card.
3. Tick the left and/or right levee and enter its station and crest elevation.
4. Click Apply.

**Data needed.** The Levee= line of the .g## file.

**Result.** Preview status: results have not been compared with HEC-RAS yet.

**Status:** Preview

## Start and end station columns

`geometry.col.blockstation`

_Station range of the area, left to right looking downstream. Must lie inside the section._

**Status:** Available

## Elevation column

`geometry.col.blockelev`

_For ineffective areas: above this water surface the area conveys flow. For obstructions: the top of the solid part._

**Status:** Available

## Permanent column

`geometry.col.permanent`

_1 means the area never conveys flow, whatever the water surface. 0 means it conveys flow once the water surface is above its elevation._

**Status:** Available

## Junctions

`geometry.junction`

_Where reaches join or split._

**Purpose.** The downstream ends of incoming reaches connect to the upstream ends of outgoing reaches. For subcritical flow the water surface at the end of an incoming reach comes from the reach below it through the energy equation over the junction length; at a split the highest water surface is used. Supercritical flow works the other way round.

**Steps:**

1. In Geometric Data, click Junction in the palette.
2. Click Add…, enter a name, tick the incoming and outgoing reaches.
3. Enter lengths for the reaches on the side with more than one reach.
4. Click Apply, then Close.

**Data needed.** The Junct Name= block of the .g## file. Each reach's flow is entered in Steady Flow.

**Result.** Reach ends at a junction need no boundary condition. Preview status: not compared with HEC-RAS yet.

**Status:** Preview

## Steady flow data

`flow.editor`

_Profiles, discharge per location and downstream boundaries (.f## file)._

**Purpose.** Each profile is one discharge scenario (e.g. Q2, Q25, Q100) computed separately.

**Steps:**

1. Set the number of profiles and their names.
2. Enter discharges at each flow-change location (at least the most upstream section).
3. Choose the downstream boundary type per reach and profile, then enter its value.

**Data needed.** Peak discharge per profile; downstream energy slope, known water surface, or rating curve.

**Status:** Available

## Number of profiles

`flow.profiles`

_Number of discharge scenarios computed in one run._

**Status:** Available

## Profile names

`flow.profilenames`

_Name of each profile, max. 12 characters, no commas._

**Status:** Available

## Discharge table

`flow.table`

_Discharge per profile at flow-change locations. A discharge applies downstream until the next location._

**Status:** Available

## Import/export flows CSV

`flow.csv`

_Format: River,Reach,RS,<profile 1>,<profile 2>,… one row per location._

**Status:** Available

## Boundary conditions

`flow.boundary`

_The condition at the downstream end of a reach (start of the subcritical computation) and at its upstream end (start of the supercritical computation)._

**Purpose.** Normal depth: enter the energy slope (e.g. 0.001). Known WS: a water surface elevation. Critical: no value. Rating curve (downstream only): stage:flow pairs. Reach ends at a junction need no boundary.

**Status:** Available

## Flow location

`flow.col.location`

_River, reach and RS where the discharge is set._

**Status:** Available

## Discharge

`flow.col.discharge`

_Discharge of this profile at that location, in m³/s (SI) or cfs (US)._

**Status:** Available

## Reach / profile

`flow.col.bcreach`

_Reach and profile the boundary applies to._

**Status:** Available

## Boundary type

`flow.col.bctype`

_Normal depth, Known W.S., Critical depth or Rating curve._

**Status:** Available

## Boundary value

`flow.col.bcvalue`

_Energy slope (normal depth) or water surface elevation (known WS)._

**Status:** Available

## Rating curve

`flow.col.bcrating`

_Stage:flow pairs separated by semicolons, e.g. 101.2:50; 102.0:120._

**Purpose.** The .f## storage format of rating curves has not been verified against HEC-RAS.

**Status:** Partial

## Unsteady flow

`flow.unsteady`

_A .u## file holds time-varying boundary conditions and initial flows. Edited in Unsteady Flow Data (Ctrl+4)._

**Purpose.** Preview status: results are not compared with HEC-RAS yet and some line formats (Rating Curve, lateral inflows) are not verified. Lines De-RAS does not recognise are kept unchanged.

**Status:** Preview

## Plan

`plan.editor`

_A plan combines one geometry and one flow file with computation settings._

**Purpose.** Plans are set up in Steady Analysis (Ctrl+5). A run uses the files on disk; De-RAS saves automatically before running.

**Steps:**

1. Choose a plan or create a new one with the + button.
2. Choose the geometry and flow files.
3. Check tolerances and the friction slope method.
4. Press F7 then F5.

**Status:** Available

## Plan title

`plan.title`

_Plan name shown in the plan list._

**Status:** Available

## Short ID

`plan.shortid`

_Short plan identifier (Short Identifier in HEC-RAS)._

**Status:** Available

## Plan files

`plan.files`

_Geometry (.g##) and steady flow (.f##) files used by the plan._

**Status:** Available

## Flow regime

`plan.regime`

_Subcritical is computed from downstream to upstream, supercritical from upstream to downstream, mixed uses both._

**Purpose.** Mixed: De-RAS computes the subcritical profile and then the supercritical one. At each section the profile with the larger specific force (momentum) is used, which locates hydraulic jumps. Subcritical needs a downstream boundary, supercritical an upstream one, mixed both. When no water surface can be found, De-RAS uses critical depth and warns.

**Result.** Supercritical and mixed have Preview status: not compared with HEC-RAS yet.

**Status:** Preview

## Tolerances

`plan.tolerance`

_Allowed difference between computed and assumed water surface for standard-step and critical-depth iterations._

**Purpose.** Default 0.003 m (SI) or 0.01 ft (US).

**Status:** Available

## Maximum trials

`plan.trials`

_Maximum number of iterations per cross section (default 20)._

**Status:** Available

## Maximum error tolerance

`plan.maxerror`

_If iterations do not converge, the result with the smallest error is accepted when below this value; otherwise critical depth is used._

**Status:** Available

## Friction slope method

`plan.friction`

_How the friction slope between two sections is averaged._

**Purpose.** Average conveyance (default), arithmetic, geometric or harmonic mean, per the equations in the Hydraulic Reference Manual.

**Status:** Available

## Model Doctor

`sim.doctor`

_Checks data before running (F7): sections, banks, n, lengths, discharges, boundaries and plan._

**Purpose.** Errors (⛔) must be fixed before running. Warnings (⚠) may be ignored if you are sure. If you press F5 while errors remain, a pop-up lists each error, where it is and how to fix it, sometimes with a quick-fix button (for example Switch to Subcritical).

**Status:** Available

## Run plan

`sim.run`

_Computes the current plan in a separate process (F5) and opens the Compute window. The project is saved first._

**Purpose.** Results are written to <name>.<plan>.deras-results.json with metadata (solver version, time, SHA-256 of input files) for reproducibility.

**Status:** Available

## Other engines

`sim.other`

_Unsteady 1D/2D, quasi-unsteady sediment and water quality._

**Purpose.** 1D unsteady is available as Preview since 0.2.2. Since 0.2.3: 2D areas, storage areas, lateral structures and pumps (M6), quasi-unsteady sediment (M7), water quality (M8). All are Preview; see Help → Feature matrix.

**Status:** Preview

## Simulation results

`results.view`

_Table, longitudinal profile, cross-section plot and run messages for the current plan._

**Purpose.** The origin line names the solver, version and run time. If input files change after the run, a stale-results warning appears. Equivalence with HEC-RAS has not been validated.

**Status:** Available

## Result cross-section plot

`results.xsplot`

_Cross section with water surface, energy grade and critical WS of the selected profile._

**Status:** Available

## Export CSV

`results.export`

_Saves the results table of all profiles to CSV (UTF-8, dot decimals)._

**Status:** Available

## Run report (HTML, Word, PDF)

`results.report`

_A bilingual run report with metadata, messages and the results table, as HTML, Word (.docx) or PDF._

**Purpose.** Click Report… in Results and choose the format. Word and PDF contain the title, run data, messages and the results table of all profiles (for unsteady: the Max WS profile). The Word file is written by De-RAS itself, without Microsoft Word, and can be edited in Word or LibreOffice. The PDF uses A4 landscape.

**Status:** Available

## Reach

`results.col.reach`

_River / reach of the section._

**Status:** Available

## RS

`results.col.rs`

_River Station of the section._

**Status:** Available

## Q total

`results.col.q`

_Total discharge through the section._

**Status:** Available

## Min Ch El

`results.col.minchel`

_Lowest main-channel elevation._

**Status:** Available

## W.S. Elev

`results.col.ws`

_Computed water surface elevation._

**Status:** Available

## Crit W.S.

`results.col.critws`

_Water surface at critical depth (minimum specific energy)._

**Status:** Available

## E.G. Elev

`results.col.eg`

_Water surface + αV²/2g._

**Status:** Available

## E.G. Slope

`results.col.egslope`

_Friction slope at the section, (Q/K)²._

**Status:** Available

## Vel Chnl

`results.col.velch`

_Mean velocity in the main channel._

**Status:** Available

## Flow Area

`results.col.area`

_Total flow area (overbanks + channel)._

**Status:** Available

## Top Width

`results.col.topwidth`

_Width of the water surface._

**Status:** Available

## Froude # Chl

`results.col.froude`

_Main-channel Froude number; < 1 is subcritical._

**Status:** Available

## Flow distribution

`results.col.qparts`

_Discharge in the left overbank, channel and right overbank by conveyance._

**Status:** Available

## Energy losses

`results.col.losses`

_Friction and contraction/expansion losses to the downstream section._

**Status:** Available

## Trials

`results.col.trials`

_Number of standard-step iterations at this section._

**Status:** Available

## Notes

`results.col.notes`

_Computation warnings, e.g. WS set to critical or conveyance ratio outside 0.7–1.4._

**Status:** Available

## River schematic

`map.schematic`

_Geometric Data canvas: reach centrelines with cross-section ticks. Click to select, double-click to open the Cross Section Editor._

**Purpose.** This is not a georeferenced GIS map. When the file has no reach coordinates, a straight line scaled by the section spacing is drawn.

**Status:** Partial

## GIS map

`map.gis`

_The map is in RAS Mapper (Ctrl+6)._

**Status:** Preview

## Appearance

`tools.settings`

_Theme, font size and language in the Options view. Stored for this Windows user._

**Status:** Available

## Language

`tools.language`

_English (default) or Indonesian. The window is rebuilt without closing the project._

**Status:** Available

## Theme

`tools.theme`

_Dark Theme (default) or Light Theme (light blue), both using the De-RAS logo colours. The window is rebuilt without closing the project._

**Status:** Available

## Font size

`tools.fontsize`

_Base interface font size (11–20 px)._

**Status:** Available

## Feature matrix

`help.features`

_List of features with status Done, Preview, Partial or Not available, with the planned milestone._

**Status:** Available

## About De-RAS

`help.about`

_Version, third-party notices and the statement that De-RAS is not affiliated with HEC._

**Status:** Available

## Geometric Data

`view.geometry`

_River schematic, editor palette, and reach and cross-section data (Ctrl+2)._

**Purpose.** The canvas shows reach centrelines and cross sections from the .g## file. The palette on the left opens editors; features not built yet are shown locked with their planned milestone.

**Steps:**

1. Choose the geometry file in the top row.
2. Click an object on the canvas to see its summary in the Selected object card.
3. Double-click a cross section to open the Cross Section Editor.
4. Use Draw reach to draw a new reach, or Tables… to edit lengths and coefficients of all sections.
5. Save with Ctrl+S.

**Status:** Available

## Cross Section Editor

`view.xs`

_Sub-view of Geometric Data for editing one cross section: station–elevation, banks, n, lengths and coefficients._

**Purpose.** The plot shows the ground and, when results exist, the water surface for up to 4 profiles. If data changed after the run, the plot notes that the results are stale.

**Status:** Available

## Steady Flow

`view.steady`

_Profiles, discharge per location and boundary conditions (.f## file, Ctrl+3)._

**Purpose.** Each profile is one discharge scenario computed separately. Upstream and downstream boundaries are set per reach and profile.

**Status:** Available

## Steady Analysis

`view.analysis`

_Plan: geometry and flow selection, regime, computation options and Model Doctor (Ctrl+5)._

**Purpose.** Model Doctor refreshes automatically half a second after data changes. Double-click a message to open the cross section it refers to. F5 computes this plan.

**Status:** Available

## Editor palette

`geometry.palette`

_Buttons that open the geometry editors. A lock marks features not available yet._

**Purpose.** Reaches, Junctions, Cross sections, Bridges / Culverts, Inline structures, Lateral structures, Storage areas, 2D areas, SA/2D connections, 2D BC lines and Pump stations work; apart from reaches and cross sections they have Preview status. Breaklines and n regions for 2D areas are not available in this version. Hover a locked button to see why.

**Status:** Available

## Selected object

`geometry.selected`

_Summary of the reach, cross section or structure selected on the canvas._

**Purpose.** For a cross section: point count, banks, n, lengths and coefficients, with a button to the Cross Section Editor. For a reach: section count, channel length, coordinates, and Manage reaches…

**Status:** Available

## New geometry file

`geometry.newfile`

_Creates the next .g## file in this project, empty or as a copy of the current file._

**Status:** Available

## Section data

`geometry.xsdata`

_RS, description, downstream lengths, banks, Manning's n and coefficients of this section._

**Purpose.** When n varies within the section, use n breakpoint table…. Ineffective areas, levees and obstructions are edited with their own buttons. Vertical n is still locked (M8); its data in the file is preserved.

**Status:** Available

## New flow file

`flow.newfile`

_Creates the next .f## file, empty or as a copy of the current file._

**Status:** Available

## Upstream boundary

`flow.col.bcupstream`

_Boundary at the most upstream section of the reach: normal depth, known water surface or critical depth._

**Purpose.** Only used by the supercritical and mixed regimes. Reach ends at a junction need no boundary.

**Status:** Available

## Results file

`plan.output`

_Where the run results are stored._

**Purpose.** De-RAS writes results to <name>.<plan>.deras-results.json (used by De-RAS itself) and to <name>.<plan>.hdf with the HEC-RAS results layout (Results/Steady/Output/...). A .hdf written by another program is never overwritten; De-RAS then writes <name>.<plan>.deras.hdf.

**Result.** The .hdf layout has Preview status: not opened in HEC-RAS yet (test K3).

**Status:** Preview

## Project Control

`home.control`

_Summary of the open project: files, units, current plan, geometry and flow files._

**Purpose.** Click a row to open the related view. The project description can be edited from this card.

**Status:** Available

## Project description

`home.description`

_Free notes about the project, stored in the .prj file as in HEC-RAS._

**Status:** Available

## Options

`options`

_Appearance (theme, font size, language), units and project settings (Ctrl+,)._

**Purpose.** Appearance is stored for this Windows user. Project settings are stored in the De-RAS sidecar file and travel with the project.

**Status:** Available

## Project settings

`project.settings`

_Coordinate system, time zone, target HEC-RAS version and project notes._

**Purpose.** Stored in the De-RAS sidecar file (not in the HEC-RAS files), so the HEC-RAS files are not changed.

**Status:** Available

## Project units

`project.units`

_SI or US Customary, set when the project is created and stored in the .prj file._

**Purpose.** Options → Convert project units SI ↔ US… (Preview, since 0.2.3) copies the project into the same folder under a new name, then converts the copy: geometry (stations, elevations, lengths, structures), steady and unsteady flow, plan tolerances, and the De-RAS module data (gate openings, pumps, breaches, sediment, water quality). Manning's n, contraction/expansion coefficients and concentrations do not change. Weir coefficients are multiplied (or divided) by √0.3048. The original project is not changed.

**Steps:**

1. Save the project.
2. Open Options and click Convert project units SI ↔ US….
3. Accept or change the name of the copy.
4. Choose Open the copy, then compute every plan again.

**Data needed.** Terrains, pictures and GIS layers are not converted; their elevation units stay as they were, so maps and 2D areas of the copy use a terrain in other units until it is replaced. Result files are not copied. Numbers in the HEC-RAS files are rounded to the 8-character column width.

**Status:** Preview

## Coordinate system

`project.crs`

_The project's EPSG code or WKT. Used by RAS Mapper._

**Purpose.** This is the same value as RAS Mapper → Set projection…; changing either one changes both. RAS Mapper uses it to check that terrains are in the same coordinate system. Reprojection is not available yet.

**Status:** Available

## Time zone

`project.timezone`

_Project time zone, e.g. UTC+07:00 (WIB). Unsteady Analysis uses the plan dates and times as entered, without time zone conversion._

**Status:** Partial

## Target HEC-RAS version

`project.target`

_The HEC-RAS file format version De-RAS writes. Currently 7.00 only._

**Status:** Available

## Project notes

`project.notes`

_De-RAS internal notes for this project (not written to the HEC-RAS files)._

**Status:** Available

## Messages drawer

`messages.drawer`

_Activity log and the Model Doctor findings (Ctrl+M)._

**Purpose.** The error and warning counts in the status bar are always current. Double-click a finding to open its location.

**Status:** Available

## Compute window

`compute.window`

_A separate window showing the steps, progress, queue and log of a run (F5)._

**Purpose.** The main window stays usable during a run. Closing the window during a run asks whether to cancel the run or keep it running in the background.

**Status:** Available

## Compute steps

`compute.steps`

_Geometry preprocessing, steady simulation, writing results. The inundation map is made afterwards in RAS Mapper._

**Status:** Available

## Run queue

`compute.queue`

_Plans waiting to be computed; they run one at a time._

**Status:** Available

## Run log

`compute.log`

_Engine messages during the run. Cancel stops the engine safely; previous results are unchanged when a run is cancelled or fails._

**Status:** Available

## Results: cross section

`results.xs`

_Cross section with water surface, energy grade and critical WS for the chosen profile._

**Status:** Available

## Results: profile

`results.profile`

_Channel invert, water surface and energy grade along the reach._

**Status:** Available

## Results: rating curve

`results.rating`

_Water surface, energy grade and critical WS against flow at one cross section, from all profiles of the plan._

**Purpose.** More profiles give a smoother curve. Between profiles the line is straight, not computed.

**Status:** Available

## Results: general plot

`results.general`

_One result variable (e.g. velocity, Froude) against distance along the reach._

**Status:** Available

## Results: detailed table

`results.detail`

_All result quantities at one cross section for the chosen profile: left overbank, channel and right overbank._

**Status:** Available

## Results: profile table

`results.table`

_Results table for all sections and profiles; can be exported to CSV._

**Status:** Available

## Results: run messages

`results.messages`

_Warnings and computation notes from the last run, per cross section._

**Status:** Available

## RAS Mapper

`view.mapper`

_The project map: terrain, background pictures, reach centrelines, cross-section cut lines and inundation maps (Ctrl+6)._

**Purpose.** Drag to pan, mouse wheel to zoom, Home or Fit to show everything. Click a cut line to select its cross section. A dashed cut line means the section has no cut line in the file; De-RAS places it across the centreline using the reach lengths.

**Result.** Preview status: not compared with HEC-RAS RAS Mapper yet.

**Status:** Preview

## Map layers

`mapper.layers`

_Projection, terrains, background pictures, geometry and inundation map, from bottom to top._

**Purpose.** The terrain and picture lists are stored in the De-RAS project file (.deras.json). HEC-RAS files are not changed.

**Status:** Available

## Projection (CRS)

`mapper.crs`

_The project's map coordinate system: an EPSG code or the WKT text of an ESRI .prj file._

**Purpose.** De-RAS does not reproject data yet. Terrains, pictures, centrelines and cut lines must use the same coordinate system. De-RAS warns when a terrain's EPSG differs from the project projection. The projection of HEC-RAS RAS Mapper (.rasmap) is shown when present.

**Steps:**

1. Open RAS Mapper (Ctrl+6).
2. Click Set projection….
3. Type an EPSG code, or click From .prj file… or From terrain.
4. Click Apply, then save the project.

**Status:** Preview

## Terrain

`mapper.terrain`

_An elevation model (DEM) as a single-band GeoTIFF._

**Purpose.** De-RAS reads GeoTIFFs without compression, LZW or Deflate, as strips or tiles, with integer or floating-point values. BigTIFF and rotated rasters are not supported yet. Files outside the project folder are copied to the project's Terrain folder. HEC-RAS RAS Mapper terrains (.hdf) are not read directly, but their source GeoTIFFs in the same folder are loaded. Where terrains overlap, the first one in the list is used.

**Steps:**

1. Click Add GeoTIFF… on the Terrain card.
2. Choose the .tif file.
3. Save the project so the terrain is recorded.

**Data needed.** Terrain list in .deras.json; GeoTIFF files in the Terrain folder.

**Status:** Preview

## Background pictures

`mapper.picture`

_Aerial photos or maps (PNG, JPG, BMP) with a world file._

**Purpose.** A world file holds six numbers that place the picture on the map (e.g. photo.pgw for photo.png, photo.jgw for photo.jpg). Rotated pictures are not supported yet. Pictures are not copied; only their location is recorded.

**Status:** Preview

## Geometry on the map

`mapper.geometry`

_Reach centrelines (blue, arrow pointing downstream) and cross-section cut lines (yellow)._

**Purpose.** Cut lines from the file (XS GIS Cut Line) are solid. Sections without one are dashed, across the centreline and spaced by the main-channel lengths.

**Status:** Available

## Cut cross sections from the terrain

`mapper.cut`

_Creates a new cross section from a line drawn on the terrain, or updates the selected one._

**Purpose.** Station-elevation points are sampled every sample spacing (default: the terrain cell size), then points on a straight line are dropped until at most 500 remain. Bank stations, Manning's n and coefficients are copied from the neighbouring section and moved to the nearest points. The upstream section's reach lengths are split by the position of the line. A line drawn right to left is reversed automatically.

**Steps:**

1. Click Cut cross section.
2. Click the line points from the left to the right bank (looking downstream), crossing the reach centreline.
3. Double-click or press Enter to finish.
4. Check the proposed RS, click Create section.
5. Check banks and n in the Cross-Section Editor, then save the geometry.

**Data needed.** The XS GIS Cut Line= line of the .g## file.

**Result.** Preview status: not compared with HEC-RAS RAS Mapper yet.

**Status:** Preview

## Inundation map

`mapper.inundation`

_Depth and water surface of one steady profile over the terrain._

**Purpose.** Between two consecutive cut lines the water surface is interpolated linearly by the distances to both lines. Depth = water surface - terrain, positive values only. The grid follows the first terrain. There is no flow connectivity check yet, so separate depressions inside the cut lines are flooded too.

**Steps:**

1. Run the plan (F5).
2. In RAS Mapper pick a profile on the Inundation map card.
3. Click Compute map.

**Result.** Preview status: not compared with HEC-RAS RAS Mapper yet.

**Status:** Preview

## Map export

`mapper.export`

_Depth or water surface as GeoTIFF, or the map picture as PNG._

**Purpose.** GeoTIFFs are float32, uncompressed, no data -9999, with the terrain's coordinate system, so they open in QGIS or ArcGIS.

**Status:** Available

## Results: 3D perspective

`results.3d`

_The cross sections of one reach and the water surface of one profile in a three-dimensional view._

**Purpose.** Sections are placed by the main-channel lengths, centred on the channel. Drag to rotate, mouse wheel to change the vertical exaggeration.

**Status:** Available

## Unsteady Flow Data

`view.unsteady`

_Time-varying boundary conditions and initial flows (.u## file, Ctrl+4)._

**Purpose.** Left: the list of boundary locations. Right: the data of the selected location (type, interval, value table and plot). Below the list: initial flows per reach.

**Steps:**

1. Pick the unsteady flow file above the list, or create one with the + button.
2. Click Add reach ends. Upstream ends get a flow hydrograph, downstream ends normal depth.
3. Select each location and enter its data.
4. Enter the initial flow of every reach.
5. Save the project, then open Unsteady Analysis.

**Data needed.** The .u## file (Boundary Location, Interval, Flow Hydrograph, Stage Hydrograph, Friction Slope, Initial Flow Loc lines).

**Result.** Preview status: results are not compared with HEC-RAS yet. Subcritical only, one flow path per reach. Lateral structures, storage areas, 2D areas, connections and pumps are computed since 0.2.3 (also Preview); bridges, culverts and inline structures are not computed in unsteady runs yet.

**Status:** Preview

## Boundary condition locations

`unsteady.boundaries`

_Every upstream and downstream reach end not connected to a junction needs one boundary condition._

**Purpose.** Add reach ends adds the missing locations. Lateral inflow… adds an inflow inside a reach. Delete removes the selected location. Model Doctor reports an error when a reach end has no boundary condition or an unsuitable type (e.g. normal depth upstream).

**Status:** Preview

## Initial flows

`unsteady.initial`

_The flow in every reach at the start of the simulation._

**Purpose.** De-RAS computes a steady (subcritical backwater) profile with these flows and the downstream boundary at the first hour, and uses it as the initial state. Empty means the first value of the upstream hydrograph is used.

**Status:** Preview

## Boundary condition data

`unsteady.data`

_Type, interval and values of the boundary condition at the selected location._

**Purpose.** The table and plot follow the type: hydrograph (one value per interval), normal depth (friction slope) or rating curve (stage–flow pairs).

**Status:** Preview

## Boundary condition types

`unsteady.types`

_Flow hydrograph, stage hydrograph, lateral inflow, uniform lateral inflow, normal depth and rating curve._

**Purpose.** Upstream: flow or stage hydrograph. Downstream: stage hydrograph, normal depth, rating curve or flow hydrograph. Inside a reach: lateral inflow (one point) or uniform lateral inflow (spread by length between two RS). Other types (e.g. gate openings, IB stage/flow) stay in the file but are not computed yet.

**Status:** Preview

## Intervals

`unsteady.interval`

_The time between two data values, computation steps or outputs: 1MIN to 1DAY._

**Purpose.** The hydrograph data interval need not match the computation interval; values in between are interpolated linearly. The output interval should be a multiple of the computation interval. At most 100 output times are stored.

**Status:** Preview

## Hydrograph

`unsteady.hydrograph`

_A series of flow or stage values, one per data interval, starting at the simulation start time._

**Purpose.** Type or paste values into the table; the plot alongside updates. If the simulation is longer than the hydrograph, the last value is held and Model Doctor warns.

**Status:** Preview

## Normal depth

`unsteady.normal`

_The downstream flow from Manning's equation with the given friction slope._

**Purpose.** At every step the downstream flow is computed from the current water surface: Q = K·√S. The slope must be greater than 0.

**Status:** Preview

## Rating curve

`unsteady.rating`

_A stage–flow relation at the downstream end._

**Purpose.** Enter increasing stage and flow pairs. Linear interpolation is used between points. The Rating Curve line format of the .u## file is not verified against HEC-RAS yet.

**Status:** Preview

## Lateral inflow

`unsteady.lateral`

_Inflow inside a reach, at one cross section or spread between two cross sections._

**Steps:**

1. Click Lateral inflow….
2. Pick the reach, the type and the inflow RS (and the end RS for uniform inflow).
3. Click Add, then fill its hydrograph.

**Status:** Preview

## Unsteady Flow Analysis

`view.run-unsteady`

_Unsteady plan: geometry and flow files, simulation window, intervals, options and Model Doctor._

**Purpose.** Method: the four-point Preissmann implicit scheme on the Saint-Venant equations, Newton iterations per step. The initial state comes from a steady profile. Junctions use equal stage and continuity. Model Doctor refuses the run when the geometry contains bridges, culverts or inline structures.

**Steps:**

1. Pick an unsteady plan, or click + to create one.
2. Pick the geometry file and the unsteady flow file.
3. Enter the start and end dates and times (e.g. 01JAN2024, 0000).
4. Choose the computation and output intervals.
5. Press F7 for Model Doctor, then F5 to compute.

**Data needed.** Simulation Date, Computation Interval, Output Interval, UNET Theta, UNET ZTol and UNET MaxIter lines in the .p## file.

**Result.** Results go to <name>.<plan>.deras-results.json: a Max WS profile, a profile for every output time, and stage and flow time series at every section. No unsteady .hdf results file is written yet. Preview status: results are not compared with HEC-RAS yet. Subcritical only, one flow path per reach. Lateral structures, storage areas, 2D areas, connections and pumps are computed since 0.2.3 (also Preview); bridges, culverts and inline structures are not computed in unsteady runs yet.

**Status:** Preview

## Simulation window

`unsteady.time`

_Start and end date and time of the simulation._

**Purpose.** Dates are written DDMMMYYYY (e.g. 01JAN2024), times HHMM (e.g. 0000 or 2400). The end must be after the start. Hydrographs start at the start time.

**Status:** Preview

## Theta and tolerances

`unsteady.theta`

_Implicit weighting (0.5–1.0), water surface tolerance and maximum iterations per step._

**Purpose.** Theta 1.0 is the most stable; values near 0.6 are more accurate but can oscillate. Newton iterations stop when the water surface change is below the tolerance. When the maximum is reached the step is still accepted and logged as a warning.

**Status:** Preview

## Results: hydrographs

`results.hydrograph`

_Stage and flow against time at one cross section, from an unsteady plan result._

**Purpose.** Pick the reach and section. Top plot: stage with the channel invert line. Bottom plot: flow. Preview status: results are not compared with HEC-RAS yet. Subcritical only, one flow path per reach. Lateral structures, storage areas, 2D areas, connections and pumps are computed since 0.2.3 (also Preview); bridges, culverts and inline structures are not computed in unsteady runs yet.

**Status:** Preview

## Results: time series

`results.timeseries`

_A table of date, hour, stage and flow at one cross section for every output time._

**Purpose.** The table can be copied into a spreadsheet. Preview status: results are not compared with HEC-RAS yet. Subcritical only, one flow path per reach. Lateral structures, storage areas, 2D areas, connections and pumps are computed since 0.2.3 (also Preview); bridges, culverts and inline structures are not computed in unsteady runs yet.

**Status:** Preview

## Lateral structure

`geometry.lateral`

_A weir or levee along a reach that spills water into a storage area, a 2D area or out of the system._

**Purpose.** Flow uses the weir equation Q = C·L·H^1.5 per crest piece, with Villemonte submergence when the tailwater rises. Flow reverses when the area is higher than the river. Default coefficient 1.1 (SI) or 2.0 (US). Lateral structures are computed in unsteady plans only; steady plans skip them with a warning.

**Steps:**

1. In Geometric Data, select the reach, then click Lateral Structure in the palette.
2. Click Add, enter the RS (between two cross sections), the side (left or right looking downstream) and the distance from the upstream section.
3. Pick the destination: out of the system, a storage area or a 2D area.
4. Fill the crest station–elevation table and the weir coefficient.
5. If needed, fill the breach data and click Froehlich estimate for a suggested width and formation time.

**Data needed.** Lateral Weir lines in the .g## file (Type 6). Breach data is stored in the project's .deras.json, because HEC-RAS keeps it in the plan file.

**Result.** The lateral flow hydrograph appears in Results (pick "Lateral structure: …" in the reach list). Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## Storage area

`geometry.storage`

_A pond or floodplain that stores water at one level water surface._

**Purpose.** Volume comes from an elevation–volume table, from a flat area above a minimum elevation, or from the terrain inside the polygon. The water surfaces of all connected storage areas are solved together with Newton iterations at every step.

**Steps:**

1. Click Storage Area in the palette, then draw the polygon on the canvas (click the points, double-click to finish), or click Add in the list.
2. Give it a name and choose how the volume is computed.
3. Fill the elevation–volume table, or the area and minimum elevation, or choose Terrain.
4. Connect it with a lateral structure, an SA/2D connection or a pump.

**Data needed.** Storage Area block in the .g## file.

**Result.** The storage area stage appears in Results. Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## 2D flow area

`geometry.area2d`

_A floodplain region where flow is computed in 2D on a grid of square cells._

**Purpose.** Method: diffusion-wave equations, fully implicit, on a square grid inside the polygon. Every cell keeps its terrain samples (subgrid), so the cell volume and the face conveyance follow the terrain inside the cell. This differs from the HEC-RAS polygon mesh; results are not compared yet. A 2D area needs a terrain in RAS Mapper.

**Steps:**

1. Make sure a terrain is added in RAS Mapper.
2. Click 2D Area in the palette, then draw the polygon on the canvas.
3. Enter the name, the cell size (empty = about 1/60 of the longest side) and Manning's n.
4. Add a BC line on the area edge where water must enter or leave.

**Data needed.** Storage Area block with Storage Area Is2D=-1 in the .g## file; the cell size goes on the Point Generation Data line.

**Result.** Maximum depth, depth per time, maximum velocity and arrival time appear in RAS Mapper → 2D area results. The volume error is written in the run messages. Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## SA/2D connection

`geometry.connection`

_A weir between two storage areas or 2D areas._

**Purpose.** Flow uses the same weir equation as lateral structures, in both directions. Breach data can be entered as for lateral structures.

**Steps:**

1. Click Connection in the palette, then draw its line on the canvas.
2. Pick the upstream and downstream areas.
3. Fill the crest table, width and coefficient.

**Data needed.** Connection block in the .g## file.

**Result.** Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## 2D area BC line

`geometry.bcline`

_A line on the edge of a 2D area where a boundary condition is applied._

**Purpose.** The cells crossed by the line receive its boundary condition: a flow hydrograph (shared by conveyance), a stage hydrograph or normal depth (outflow with the given friction slope).

**Steps:**

1. Click BC Line in the palette, then draw the line near the edge of the 2D area.
2. Pick the 2D area and give it a name.
3. In Unsteady Flow Data, click Area/BC line… to set its boundary condition.

**Data needed.** BC Line block in the .g## file.

**Result.** Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## Pump station

`geometry.pump`

_Pumps water from a reach, a storage area or a 2D area cell to another location or out of the system._

**Purpose.** Each pump group starts when the inlet water surface rises above its on elevation and stops below its off elevation. Flow = number of pumps × capacity.

**Steps:**

1. Click Pump Station in the palette, then Add.
2. Pick the source and destination (reach cross section, storage area, point in a 2D area, or out of the system).
3. Fill the pump groups: count, capacity, on and off elevations.

**Data needed.** Stored in the project's .deras.json. Pump Station blocks in .g## files written by HEC-RAS are kept but not read yet.

**Result.** Pump flow appears in Results. Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## Elevation–volume table

`geometry.col.volume`

_Storage volume of the storage area at each elevation, elevations increasing._

**Purpose.** Linear interpolation is used between points. Above the last point, the volume grows with the area of the last two points.

**Status:** Available

## X–Y coordinates

`geometry.col.xy`

_Polygon or line points in map coordinates, in order._

**Purpose.** Polygons need not be closed; the last point joins the first automatically.

**Status:** Available

## Area boundary conditions

`unsteady.area`

_Boundary conditions for storage areas and 2D areas._

**Purpose.** Storage area: inflow hydrograph, stage hydrograph or precipitation. 2D area: flow or stage hydrograph, or normal depth, through a BC line; precipitation over the whole area. Precipitation is entered as depth per interval (mm or in). The initial water surface of an area goes in the initial conditions table; empty means dry (2D area) or the minimum elevation (storage area).

**Steps:**

1. Click Area/BC line….
2. Pick the area, the BC line (for a 2D area) and the type.
3. Click Add, then fill its hydrograph.

**Data needed.** Boundary Location in the .u## file with the area name in the 6th field and the BC line in the 8th.

**Result.** Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## 2D area results

`mapper.results2d`

_Depth, velocity and arrival time maps from the 2D areas of an unsteady plan._

**Purpose.** Pick the area and the variable: maximum depth, depth at a time (move the slider), maximum velocity, or arrival time (hours from the start until a depth of 0.03 m / 0.1 ft). Depth is the cell water surface minus the terrain at each pixel. Velocity and arrival time have one value per cell. Maps export to GeoTIFF like inundation maps.

**Data needed.** Results are stored in <name>.<plan>.deras-2d.bin next to the JSON results file.

**Result.** Preview status: computed by Unsteady Flow Analysis; results are not compared with HEC-RAS yet.

**Status:** Preview

## Quasi-unsteady Flow Data

`sediment.quasi`

_A sequence of steady flows for the sediment computation: duration, flow, computation increment and downstream stage._

**Purpose.** Each record is a flow held for its duration. The hydraulics (water surface, velocity, energy slope) are recomputed with the steady solver at every computation increment, and the channel bed is updated in between. An empty downstream stage means normal depth with the given slope.

**Steps:**

1. Pick the geometry file and the reach.
2. Fill the table: duration (h), flow, computation increment (h) and, if needed, the downstream stage.
3. Check the hydrograph plot on the right.

**Data needed.** Stored in the project's .deras.json (sediment module). HEC-RAS .q## files are not read or written yet.

**Result.** Preview status: results are not compared with HEC-RAS yet. One reach, one grain size (d50), no grain sorting or armouring.

**Status:** Preview

## Sediment Data

`sediment.data`

_Transport function, grain properties, bed gradation, erodible depth and upstream inflow load._

**Purpose.** The movable bed is the cross-section points between the bank stations; they move up or down by the same amount. Erosion is limited to the erodible depth below the initial bed. Default porosity 0.4 and specific gravity 2.65.

**Data needed.** Stored in the project's .deras.json (sediment module). HEC-RAS .s## files are not read or written yet.

**Result.** Preview status: results are not compared with HEC-RAS yet. One reach, one grain size (d50), no grain sorting or armouring.

**Status:** Preview

## Sediment transport function

`sediment.function`

_The transport capacity equation of the main channel._

**Purpose.** Meyer-Peter Müller (1948): bed load for coarse sand and gravel, qs* = 8 (θ' − 0.047)^1.5 with the grain roughness correction (n'/n)^1.5, n' = d90^(1/6)/26. Engelund-Hansen (1967): total load for sand, qs = 0.05 V² √(d/(g(s−1))) θ^1.5. Yang (1973): total sand load from the unit stream power VS; a warning is given for d50 above 2 mm. Fall velocity uses Soulsby (1997).

**Status:** Preview

## Bed gradation

`sediment.gradation`

_Grain diameter (mm) and percent finer from a sieve analysis._

**Purpose.** d50 and d90 are interpolated logarithmically from the curve. The computation uses d50 as the representative size; d90 is used for the Meyer-Peter Müller grain roughness correction.

**Status:** Preview

## Sediment inflow

`sediment.inflow`

_The sediment load entering at the most upstream section._

**Purpose.** Equilibrium load: equal to the transport capacity at the upstream section, so the bed there does not change. Clear water: no load (for example below a dam), the upstream bed erodes. Rating curve: load from a flow–load table in tons per day.

**Status:** Preview

## Sediment Analysis

`sediment.run`

_Runs the quasi-unsteady sediment computation and shows the channel bed change._

**Purpose.** At every computation increment: steady hydraulics → transport capacity at every section → the Exner equation (1 − p) B L dz/dt = Qs,in − Qs,out. When the bed would change by more than 5 % of the water depth, the step is split automatically and the hydraulics are recomputed. The mass balance (in − out = bed change) is shown below the table.

**Steps:**

1. Fill the Quasi-unsteady Data and Sediment Data.
2. Set the output interval (hours).
3. Click Compute sediment.
4. Move the time slider to see the bed profile and table at that time; Export CSV for all times.

**Data needed.** Results go to <name>.deras-sedimen.json in the project folder.

**Result.** Preview status: results are not compared with HEC-RAS yet. One reach, one grain size (d50), no grain sorting or armouring.

**Status:** Preview

## Sediment results

`sediment.results`

_Initial bed and bed at the selected time, bed change over time, and a table per cross section._

**Purpose.** The load in the table is the load leaving the control volume of each section, in tons per day.

**Result.** Preview status: results are not compared with HEC-RAS yet. One reach, one grain size (d50), no grain sorting or armouring.

**Status:** Preview

## Autosave

`tools.autosave`

_Saves a recovery copy of unsaved changes every few minutes (0 = off, default 5 minutes)._

**Purpose.** The recovery copy goes to the application data folder (recovery), not to the project files, so the HEC-RAS files do not change until you save. If De-RAS closes without saving (power cut, crash), it offers to recover those changes the next time the project opens. Recovered changes show as unsaved. The copy is deleted after the project is saved or when you choose Don't save on closing.

**Status:** Available

## Uniform flow

`design.uniform`

_Flow from depth, or normal depth from flow, in a trapezoidal channel (Manning's equation)._

**Purpose.** Q = (k/n) A R^(2/3) S^(1/2), k = 1 (SI) or 1.486 (US). The normal depth is found by bisection. Critical depth from Q²T/(gA³) = 1. Bed shear τ = γRS. Side slope z = 0 means a rectangular channel.

**Steps:**

1. Enter the bottom width, side slope, n and slope.
2. Choose Flow from depth or Normal depth from flow.
3. Enter the depth or flow; the results and the cross-section drawing update at once.

**Result.** Preview status: equations and results are not compared with HEC-RAS yet. Design decisions remain the engineer's responsibility.

**Status:** Preview

## Bridge scour (HEC-18)

`design.scour`

_Pier, contraction and abutment scour after FHWA HEC-18, 5th edition (2012)._

**Purpose.** Pier (CSU equation): ys/a = 2.0 K1 K2 K3 (y1/a)^0.35 Fr1^0.43; K1 from the nose shape (square 1.1; round and circular 1.0; sharp 0.9; K1 = 1 when the attack angle is above 5°), K2 = (cos θ + L/a sin θ)^0.65 with L/a at most 12, K3 from the bed condition. For round-nose piers aligned with the flow, the upper limit is 2.4a for Fr ≤ 0.8 and 3.0a above. Live-bed contraction (modified Laursen 1960): y2/y1 = (Q2/Q1)^(6/7) (W1/W2)^k1, k1 0.59/0.64/0.69 from V*/ω. Clear-water contraction (Laursen 1963): y2 = [Ku Q² / (Dm^(2/3) W²)]^(3/7), Dm = 1.25 D50. Scour = y2 − y0. Abutment (Froehlich 1989): ys/ya = 2.27 K1 K2 (L'/ya)^0.43 Fr^0.61 + 1.

**Steps:**

1. Choose the plan, profile and cross section under Take values from computed results, then click Fill from results (or enter values by hand).
2. Complete the pier, contraction and abutment data.
3. Read the scour below each card and the total at the pier at the bottom.

**Data needed.** Fill from results uses y1 = maximum main-channel depth, V1 = main-channel velocity, Q1 = channel flow, Q2 = total flow, S1 = energy slope, Ve = left overbank flow/area. These are starting values; check them against the actual bridge.

**Result.** Preview status: equations and results are not compared with HEC-RAS yet. Design decisions remain the engineer's responsibility.

**Status:** Preview

## Manning's n calibration

`design.calibration`

_Finds one factor on Manning's n so that the steady water surfaces come closest to the observed ones._

**Purpose.** De-RAS runs the steady computation repeatedly (golden-section search on the log of the factor within the given range) and minimises the RMS water-surface difference at the sections with observations. The factor applies to the main-channel n only or to every n. One factor for the whole geometry; n by location is not calibrated yet.

**Steps:**

1. Choose the steady plan and the profile that matches the observed event.
2. Enter the observed water surfaces in the last column.
3. Click Calibrate and check the plot and the RMS.
4. Click Apply to geometry if the result makes sense, save, then compute again.

**Data needed.** Observations are stored in .deras.json (calibration module). Apply to geometry multiplies the n values in the plan's geometry file; close without saving to undo.

**Result.** Preview status: equations and results are not compared with HEC-RAS yet. Design decisions remain the engineer's responsibility.

**Status:** Preview

## Water Quality Data

`wq.data`

_Constituent, initial concentration, dispersion coefficient, decay rate and upstream inflow concentration._

**Purpose.** Water quality uses the hydraulics (water surface and flow at every output time) of the stored unsteady results of the chosen plan, in that plan's first reach. One constituent in mg/L with first-order decay (rate per day, 0 = conservative). Temperature, DO and nutrients are not available yet.

**Steps:**

1. Choose an unsteady plan that has been computed.
2. Enter the constituent name, initial concentration, dispersion and decay rate.
3. Fill the table of hours and upstream concentration.

**Data needed.** Stored in .deras.json (water_quality module). HEC-RAS .w## files are not read or written yet.

**Result.** Preview status: equations and results are not compared with HEC-RAS yet. Design decisions remain the engineer's responsibility.

**Status:** Preview

## Water Quality Analysis

`wq.run`

_Computes the concentration along the reach over time._

**Purpose.** 1D advection-dispersion with an implicit finite volume scheme (upwind advection, central dispersion); steps are split so that the Courant number is about 1. Mass balance: change in storage = in − out − decayed + hydraulic continuity correction. The correction appears because the stored hydraulics at output times do not close continuity exactly; it is shown so its size is visible (usually far below 1 % of the mass in).

**Steps:**

1. Fill the Water Quality Data.
2. Click Compute water quality.
3. Move the time slider; the right plot shows the concentration upstream, in the middle and downstream; Export CSV for all times.

**Data needed.** Results go to <name>.<plan>.deras-wq.json. Example: the DerasUji1km project uses a 100 mg/L tracer for 3 hours on the p02 flood.

**Status:** Preview
