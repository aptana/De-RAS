# HEC-RAS file format support (Phase 1)

Target: HEC-RAS 7.0.x text files (`Program Version=7.00` on new files). Lines ending in CRLF are kept as in the source file.

## Lossless principle
- A file that has not changed is written back **byte for byte identical** (tested by `deras-roundtrip` and the `roundtrip_unchanged_is_identical` test).
- Geometry is split into segments (header, reach, node). Only the cross-section blocks that changed are regenerated, key by key; lines in such a block that are not recognised are kept.
- Nodes other than cross sections (bridge, culvert, inline, lateral), junctions, storage areas, 2D areas and every unknown key are copied unchanged.

## .prj
Read and written: `Proj Title=`, `Current Plan=`, `SI Units` / `English Units`, `Geom File=`, `Flow File=`, `Plan File=`, `Unsteady File=` (kept), the description. The file list is rebuilt only when it changes.

## .g## (geometry)
| Key | Support |
|---|---|
| `Geom Title=`, `Program Version=` | read and write |
| `River Reach=` (16-character names), `Reach XY=` (16-character columns) | read and write |
| `Type RM Length L Ch R =` | read and write (type 1 = cross section; other types are kept) |
| `#Sta/Elev=` (10 columns of 8 characters) | read and write |
| `#Mann= n , f , 0` (triplets, 9 columns of 8 characters) | read and write |
| `Bank Sta=`, `Exp/Cntr=` | read and write |
| `BEGIN DESCRIPTION:` … `END DESCRIPTION:` | read and write |
| `#XS Ineff=`, `Levee=`, `#Block Obstruct=`, vertical n | read and kept; not computed (Model Doctor gives a warning) |

## .f## (steady flow)
`Flow Title=`, `Number of Profiles=`, `Profile Names=`, `River Rch & RM=` plus the flows (10 × 8 characters), `Boundary for River Rch & Prof#=`, `Up Type=` / `Dn Type=` (0 none, 1 known WS, 2 critical, 3 normal depth, 4 rating curve), `Dn Slope=`, `Dn Known WS=`, `Dn Rating Curve=`. Other lines that are not recognised are appended back unchanged.
**Not verified yet**: how the rating curve (`Dn Rating Curve=`) is stored. It needs a check by opening the file in HEC-RAS.

## .p## (plan)
Keys updated in place: `Plan Title=`, `Short Identifier=`, `Geom File=`, `Flow File=`, `Subcritical Flow` (regime), `Std Step Tol=`, `Critical Tol=`, `Max Error Tol=`, `Num of Std Step Trials=`, `Friction Slope Method=`. An unsteady plan (`Flow File=u##`) is read and kept, but refused when run through the Phase 1 solver.

## De-RAS files (ignored by HEC-RAS)
| File | Contents |
|---|---|
| `<name>.deras.json` | CRS, time zone, notes, target HEC-RAS version |
| `<name>.<plan>.deras-results.json` | Results per profile and cross section plus metadata (solver version, time, units, constants, tolerances, SHA-256 of the .g/.f/.p files) |
| `<name>.<plan>.deras-log.txt` | Run log |

## Not tested yet
- **K2**: a file saved by De-RAS has not been opened in a real HEC-RAS. The project owner needs to test this on a PC with HEC-RAS 6.x or 7.x.
- The example projects that ship with HEC-RAS have not been used as fixtures (the fixtures in `tests/data` are synthetic).
