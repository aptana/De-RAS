# Roadmap

Draft. Scope and order will change as the design firms up.

## Milestone 0: Foundations

- Decide technology stack and project file format
- Define units, datum and CRS handling (SI units, explicit datum and CRS on every table and map)
- Set up repository, CI and test harness

## Milestone 1: 1D steady flow (MVP)

- Cross-section and reach geometry model
- Manning roughness for channel and overbanks
- Standard step solver, subcritical and supercritical
- Boundary conditions: known water surface, critical depth, normal depth
- Multiple flow profiles per run (for example Q10, Q20, Q50, Q100)
- Pre-run validation with plain-language messages
- Tables and plots: profile, cross section, summary

## Milestone 2: Mapping

- Terrain import (GeoTIFF)
- Inundation depth, water surface elevation and velocity rasters
- Export to GeoTIFF, GeoJSON and Shapefile

## Milestone 3: Workflow tools

- Guided project wizard
- Calibration helper (compare modeled and observed levels, log each trial)
- Sensitivity testing for roughness
- Report export

## Milestone 4: Extended hydraulics

- Bridges, culverts, weirs
- 1D unsteady flow
- 2D flow areas

## Open questions

- Technology stack and packaging
- Project file format
- Compatibility level with existing HEC-RAS project files
- License
