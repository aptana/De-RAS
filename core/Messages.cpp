// De-RAS - core/Messages.cpp
#include "Messages.h"

#include <cstdio>
#include <map>

namespace deras {

namespace {
struct Pair { const char* id; const char* en; };

const std::map<std::string, Pair>& catalog() {
    static const std::map<std::string, Pair> c = {
        // ---------- unsteady (M5) ----------
        // ---------- storage areas, lateral structures, pumps, 2D (M6) ----------
        {"RUN_TERRAIN_UNREAD", {"Terrain '{0}' tidak bisa dibaca ({1}).", "Terrain '{0}' could not be read ({1})."}},
        {"RES_2D_FAILED", {"Hasil area 2D tidak bisa disimpan: {0}", "The 2D area results could not be saved: {0}"}},
        {"MD_AREA_POLYGON", {"Area 2D '{0}' butuh poligon dengan minimal 3 titik.", "2D area '{0}' needs a polygon with at least 3 points."}},
        {"MD_AREA_MANNING", {"Area 2D '{0}': nilai n Manning harus lebih dari 0.", "2D area '{0}': Manning's n must be greater than 0."}},
        {"MD_SA_VOLUME", {"Storage area '{0}' tidak punya data volume (tabel elevasi-volume, luas, atau poligon untuk terrain).",
                          "Storage area '{0}' has no volume data (elevation-volume table, area, or a polygon for the terrain)."}},
        {"MD_SA_TERRAIN", {"Storage area '{0}': volume dihitung dari terrain di dalam poligon saat simulasi.",
                           "Storage area '{0}': the volume is computed from the terrain inside the polygon when the simulation runs."}},
        {"MD_CONN_AREA", {"Koneksi '{0}': area hulu atau hilir tidak ditemukan.", "Connection '{0}': the upstream or downstream area was not found."}},
        {"MD_CONN_WEIR", {"Koneksi '{0}': puncak pelimpah butuh minimal 2 titik.", "Connection '{0}': the weir crest needs at least 2 points."}},
        {"MD_BCLINE_AREA", {"BC line '{0}': area 2D '{1}' tidak ditemukan.", "BC line '{0}': 2D area '{1}' was not found."}},
        {"MD_PUMP_DATA", {"Stasiun pompa '{0}': titik asal/tujuan tidak ditemukan atau belum ada grup pompa.",
                          "Pump station '{0}': the source/target was not found or there is no pump group."}},
        {"MD_LATERAL_TARGET", {"Struktur lateral RS {0}: area tujuan '{1}' tidak ditemukan.", "Lateral structure RS {0}: target area '{1}' was not found."}},
        {"MD_LATERAL_STEADY", {"Plan steady tidak menghitung struktur lateral, storage area, area 2D, dan pompa; jalankan plan unsteady untuk itu.",
                               "Steady plans do not compute lateral structures, storage areas, 2D areas and pumps; run an unsteady plan for them."}},
        {"SOLV_2D_BUILD_FAIL", {"Area 2D '{0}' tidak bisa dibuat gridnya ({1}). Area 2D butuh poligon, ukuran sel, dan terrain di RAS Mapper yang menutupi area itu.",
                                "The grid of 2D area '{0}' could not be built ({1}). A 2D area needs a polygon, a cell size and a RAS Mapper terrain that covers it."}},
        {"SOLV_SA_NO_VOLUME", {"Storage area '{0}' tidak punya data volume: isi tabel elevasi-volume, atau luas dan elevasi minimum, atau tambahkan terrain.",
                               "Storage area '{0}' has no volume data: enter an elevation-volume table, or an area and minimum elevation, or add a terrain."}},
        {"SOLV_AREA_BC_UNUSED", {"Kondisi batas area '{0}' (BC line {1}) tidak dipakai: area, garis, atau jenisnya tidak cocok.",
                                 "The boundary condition of area '{0}' (BC line {1}) is not used: the area, the line or its type does not match."}},
        {"SOLV_LAT_SKIPPED", {"Struktur lateral {0} RS {1} dilewati: puncak pelimpah butuh minimal 2 titik dan tujuan harus storage area atau area 2D yang ada.",
                              "Lateral structure {0} RS {1} was skipped: the weir crest needs at least 2 points and the target must be an existing storage area or 2D area."}},
        {"SOLV_LAT_NO_GEO", {"Struktur lateral {0} RS {1}: reach belum punya garis tengah atau cut line, jadi aliran ke area 2D masuk ke sel terendah area itu.",
                             "Lateral structure {0} RS {1}: the reach has no centreline or cut lines, so the flow into the 2D area enters its lowest cell."}},
        {"SOLV_CONN_SKIPPED", {"Koneksi '{0}' dilewati: butuh area hulu dan hilir yang ada dan puncak pelimpah minimal 2 titik.",
                               "Connection '{0}' was skipped: it needs existing upstream and downstream areas and a weir crest with at least 2 points."}},
        {"SOLV_PUMP_SKIPPED", {"Stasiun pompa '{0}' dilewati: titik asal atau tujuan tidak ditemukan, atau belum ada grup pompa.",
                               "Pump station '{0}' was skipped: the source or target was not found, or it has no pump group."}},
        {"SOLV_2D_UNSTABLE", {"Hitungan area 2D gagal pada {0}. Coba interval komputasi lebih kecil atau periksa kondisi batas.",
                              "The 2D area computation failed at {0}. Try a smaller computation interval or check the boundary conditions."}},
        {"SED_NO_REACH", {"Sedimen: reach '{0}' tidak ditemukan di file geometri, atau penampangnya kurang dari dua.",
                          "Sediment: reach '{0}' is not in the geometry file, or it has fewer than two cross sections."}},
        {"SED_ONE_REACH", {"Sedimen dihitung hanya untuk reach '{0}'; reach lain di geometri ini diabaikan (Pratinjau: satu reach).",
                           "Sediment is computed for reach '{0}' only; the other reaches of this geometry are ignored (Preview: one reach)."}},
        {"SED_NO_FLOW", {"Sedimen: data aliran quasi-unsteady masih kosong.", "Sediment: the quasi-unsteady flow data is empty."}},
        {"SED_NO_GRADATION", {"Sedimen: isi gradasi material dasar (diameter mm dan persen lolos).", "Sediment: enter the bed gradation (diameter mm and percent finer)."}},
        {"SED_BAD_PROPERTIES", {"Sedimen: berat jenis harus lebih dari 1 dan porositas antara 0 dan 0,9.", "Sediment: specific gravity must exceed 1 and porosity must be between 0 and 0.9."}},
        {"SED_YANG_GRAVEL", {"Fungsi Yang (1973) dibuat untuk pasir; d50 = {0} mm lebih besar dari 2 mm.", "The Yang (1973) function is for sand; d50 = {0} mm is larger than 2 mm."}},
        {"SED_NO_MOVABLE_BED", {"Sedimen: penampang RS {0} tidak punya titik dasar di antara stasiun tebing.", "Sediment: cross section RS {0} has no bed point between the bank stations."}},
        {"SED_BAD_RECORD", {"Sedimen: baris aliran {0} harus punya durasi dan debit lebih dari 0.", "Sediment: flow record {0} needs a duration and a flow greater than 0."}},
        {"SED_TOO_MANY_STEPS", {"Sedimen: {0} langkah hitung terlalu banyak (maks. 200.000). Perbesar interval hitung.", "Sediment: {0} computation steps are too many (max. 200,000). Use a longer computation increment."}},
        {"SED_HYDRAULICS_FAILED", {"Sedimen: hitungan muka air steady gagal pada jam {0}.", "Sediment: the steady water surface computation failed at hour {0}."}},
        {"SED_STEP_LARGE", {"Sedimen: pada {0} langkah perubahan dasar tetap lebih dari 5 % kedalaman air walau sudah dibagi 200 sub-langkah. Perkecil interval hitung.",
                            "Sediment: in {0} steps the bed still changed by more than 5 % of the water depth after 200 sub-steps. Use a shorter computation increment."}},
        {"SED_SUBSTEPS", {"Sedimen: {0} langkah dibagi menjadi sub-langkah agar perubahan dasar per sub-langkah tidak lebih dari 5 % kedalaman air (hidraulika dihitung ulang setiap sub-langkah).",
                          "Sediment: {0} steps were split into sub-steps so that the bed changes by at most 5 % of the water depth per sub-step (hydraulics recomputed every sub-step)."}},
        {"SED_DONE", {"Sedimen selesai: {0}, d50 {1} mm, {2} langkah selama {3} jam. Masuk {4} t, keluar {5} t, perubahan dasar {6} t.",
                      "Sediment finished: {0}, d50 {1} mm, {2} steps over {3} hours. In {4} t, out {5} t, bed change {6} t."}},
        {"UNITS_SAME", {"Proyek sudah memakai sistem satuan tujuan; tidak ada yang dikonversi.",
                        "The project already uses the target unit system; nothing was converted."}},
        {"WQ_NO_RESULT", {"Kualitas air: plan '{0}' belum punya hasil unsteady. Jalankan dulu Analisis Unsteady untuk plan itu.",
                          "Water quality: plan '{0}' has no unsteady results yet. Run the Unsteady Analysis for that plan first."}},
        {"SOLV_2D_DONE", {"Area 2D '{0}': {1} sel ukuran {2}, {3} sel pernah basah, selisih volume {4}.",
                          "2D area '{0}': {1} cells of size {2}, {3} cells were wet, volume error {4}."}},
        {"RES_HDF_UNSTEADY_LATER", {"Hasil unsteady disimpan di file hasil De-RAS (.deras-results.json); file .hdf untuk unsteady menyusul di versi berikutnya.",
                                    "Unsteady results are stored in the De-RAS results file (.deras-results.json); the .hdf file for unsteady follows in a later version."}},
        {"SOLV_U_INIT_FAIL", {"Kondisi awal (hitungan steady dengan debit awal) gagal, jadi hitungan unsteady tidak dimulai.",
                              "The initial conditions (steady computation with the initial flows) failed, so the unsteady computation did not start."}},
        {"SOLV_U_NOCONV", {"{0} langkah waktu tidak konvergen dalam {1} iterasi; selisih muka air terbesar {2}. Hasil langkah itu tetap dipakai.",
                           "{0} time steps did not converge within {1} iterations; largest water surface change {2}. Those steps were kept."}},
        {"SOLV_U_DRY", {"Kedalaman di {0} RS {1} turun hampir nol ({2}); kedalaman dibatasi minimum.",
                        "The depth at {0} RS {1} dropped to almost zero ({2}); it was held at a minimum."}},
        {"SOLV_U_UNSTABLE", {"Hitungan tidak stabil pada {0}: muka air tidak wajar di {1} RS {2}. Coba interval komputasi lebih kecil atau periksa kondisi batas.",
                             "The computation became unstable at {0}: unreasonable water surface at {1} RS {2}. Try a smaller computation interval or check the boundary conditions."}},
        {"SOLV_U_DONE", {"Unsteady selesai: {0} langkah waktu ({1}), {2} waktu keluaran.",
                         "Unsteady finished: {0} time steps ({1}), {2} output times."}},
        {"SOLV_U_FROUDE", {"Bilangan Froude di atas 1 terjadi di {0} RS {1} (maks. {2}). Skema implisit De-RAS dirancang untuk aliran subkritis.",
                           "A Froude number above 1 occurred at {0} RS {1} (max. {2}). The De-RAS implicit scheme is designed for subcritical flow."}},
        {"MD_U_NO_FILE", {"Plan tidak menunjuk file aliran unsteady yang ada.", "The plan does not reference an existing unsteady flow file."}},
        {"MD_U_TIME", {"Waktu simulasi tidak valid: mulai '{0}', selesai '{1}' (format 01JAN2024 dan 0000, selesai harus setelah mulai).",
                       "The simulation window is not valid: start '{0}', end '{1}' (format 01JAN2024 and 0000, the end must be after the start)."}},
        {"MD_U_INTERVAL", {"Interval '{0}' tidak dikenali atau tidak cocok (interval keluaran harus kelipatan interval komputasi).",
                           "Interval '{0}' is not understood or does not fit (the output interval must be a multiple of the computation interval)."}},
        {"MD_U_BC_MISSING", {"{0}: ujung {1} tidak punya kondisi batas.", "{0}: the {2} end has no boundary condition."}},
        {"MD_U_BC_TYPE", {"{0}: kondisi batas {1} tidak bisa dipakai di ujung {2}.", "{0}: a {3} boundary cannot be used at the {4} end."}},
        {"MD_U_BC_EMPTY", {"{0} RS {1}: hidrograf belum berisi data atau intervalnya tidak dikenali.", "{0} RS {1}: the hydrograph has no data or its interval is not understood."}},
        {"MD_U_BC_SHORT", {"{0} RS {1}: hidrograf hanya sampai jam {2}, simulasi sampai jam {3}; nilai terakhir dipakai sesudahnya.",
                           "{0} RS {1}: the hydrograph ends at hour {2} but the simulation runs to hour {3}; the last value is held."}},
        {"MD_U_BC_UNUSED", {"{0} RS {1}: kondisi batas ini tidak dipakai De-RAS (lokasi atau jenisnya belum didukung).",
                            "{0} RS {1}: De-RAS does not use this boundary condition (location or type not supported yet)."}},
        {"MD_U_STRUCTURE", {"{0} RS {1}: struktur belum dihitung pada aliran unsteady di versi ini.",
                            "{0} RS {1}: structures are not computed in unsteady flow in this version."}},
        {"MD_U_INIT", {"{0}: kondisi batas hulu berupa hidrograf muka air, jadi debit awal (Initial Flow) perlu diisi.",
                       "{0}: the upstream boundary is a stage hydrograph, so an initial flow is needed."}},
        {"MD_U_LENGTH", {"{0} RS {1}: jarak alur ke penampang hilir harus lebih dari 0 untuk hitungan unsteady.",
                         "{0} RS {1}: the channel length to the next section downstream must be greater than 0 for unsteady flow."}},
        {"MD_U_THETA", {"Theta {0} di luar rentang 0,6–1; nilai yang disarankan 1 (stabil) atau 0,6 (lebih akurat).",
                        "Theta {0} is outside 0.6–1; recommended 1 (stable) or 0.6 (more accurate)."}},
        // ---------- file I/O ----------
        {"FILE_OPEN_FAILED", {"File tidak bisa dibuka: {0}", "File could not be opened: {0}"}},
        {"FILE_WRITE_FAILED", {"File tidak bisa ditulis: {0}", "File could not be written: {0}"}},
        {"PRJ_NOT_RAS", {"File {0} bukan file proyek HEC-RAS (baris 'Proj Title=' tidak ditemukan).",
                         "{0} is not a HEC-RAS project file (no 'Proj Title=' line)."}},
        {"PRJ_MISSING_FILE", {"File {0} disebut di proyek tetapi tidak ada di folder.",
                              "File {0} is listed in the project but does not exist in the folder."}},
        {"PARSE_BAD_NUMBER", {"Angka tidak valid '{0}' di {1} baris {2}.",
                              "Invalid number '{0}' in {1} line {2}."}},
        {"PARSE_COUNT_MISMATCH", {"Jumlah data {0} tidak sesuai (diharapkan {1}, ditemukan {2}) di {3}.",
                                  "Data count for {0} does not match (expected {1}, found {2}) in {3}."}},
        {"GEOM_NODE_UNSUPPORTED", {"Node tipe {0} ({1}) tetap tersimpan di file, tetapi belum bisa dihitung De-RAS.",
                                   "Node type {0} ({1}) is preserved as-is but cannot be computed by De-RAS yet."}},
        {"GEOM_STRUCTURE_UNREAD", {"Data {0} di bagian '{1}' tidak bisa dibaca De-RAS. Struktur tetap tersimpan di file, tetapi tidak dihitung.",
                                   "The {0} data in block '{1}' could not be read by De-RAS. The structure stays in the file but is not computed."}},
        {"MD_STRUCTURE_UNREAD", {"{0} RS {1} tidak bisa dihitung karena datanya tidak terbaca. Hitungan berhenti di sini.",
                                 "{2} RS {1} cannot be computed because its data could not be read. The run stops here."}},
        {"MD_STRUCTURE_REGIME", {"{0} RS {1} hanya bisa dihitung dengan rezim subkritis di versi ini.",
                                 "{2} RS {1} can only be computed with the subcritical regime in this version."}},
        {"MD_STRUCTURE_PLACE", {"{0} RS {1} harus berada di antara dua penampang dalam reach yang sama.",
                                "{2} RS {1} must lie between two cross sections of the same reach."}},
        {"MD_STRUCTURE_DATA", {"{0} RS {1}: {3}", "{2} RS {1}: {4}"}},
        {"SOLV_BRIDGE", {"Jembatan RS {0}: {1}. Muka air hulu {2} (energi {3}), kehilangan energi {4}.",
                         "Bridge RS {0}: {5}. Upstream water surface {2} (energy {3}), energy loss {4}."}},
        {"SOLV_CULVERT", {"Gorong-gorong RS {0}: {1}. Muka air hulu {2} (energi {3}), debit lewat jalan {4}.",
                          "Culvert RS {0}: {5}. Upstream water surface {2} (energy {3}), flow over the road {4}."}},
        {"SOLV_INLINE", {"Struktur inline RS {0}: debit pelimpah {1}, debit pintu {2}. Muka air hulu {3} (energi {4}).",
                         "Inline structure RS {0}: weir flow {1}, gate flow {2}. Upstream water surface {3} (energy {4})."}},
        {"SOLV_STRUCTURE_FAIL", {"Struktur RS {0} tidak bisa mengalirkan debit {1} pada muka air hulu yang masuk akal.",
                                 "Structure RS {0} cannot pass the flow {1} at any reasonable upstream water surface."}},
        {"NON_ASCII_NAME", {"Nama '{0}' memakai huruf khusus (non-ASCII). HEC-RAS membaca file dengan pengodean Windows-1252, jadi huruf itu bisa berubah.",
                            "Name '{0}' contains non-ASCII characters; HEC-RAS uses Windows-1252 encoding."}},
        // ---------- Model Doctor ----------
        {"MD_NO_GEOMETRY", {"Plan tidak menunjuk file geometri yang ada.", "The plan does not reference an existing geometry file."}},
        {"MD_NO_FLOW", {"Plan tidak menunjuk file aliran steady yang ada.", "The plan does not reference an existing steady flow file."}},
        {"MD_NO_REACH", {"Geometri belum berisi reach.", "The geometry contains no reach."}},
        {"MD_REACH_FEW_XS", {"Reach memiliki kurang dari 2 penampang; perhitungan profil butuh minimal 2.",
                             "The reach has fewer than 2 cross sections; a profile needs at least 2."}},
        {"XS_TOO_FEW_POINTS", {"Penampang memiliki kurang dari 3 titik stasiun–elevasi.",
                               "Cross section has fewer than 3 station–elevation points."}},
        {"XS_STATION_ORDER", {"Stasiun tidak terus naik di titik ke-{0} ({1} sesudah {2}).",
                              "Stations are not increasing at point {0} ({1} after {2})."}},
        {"XS_BANKS_MISSING", {"Tebing kiri dan kanan (bank station) belum diisi.", "Left/right bank stations are missing."}},
        {"XS_BANKS_OUTSIDE", {"Tebing ({0}, {1}) berada di luar rentang stasiun penampang ({2} – {3}).",
                              "Bank stations ({0}, {1}) lie outside the station range ({2} – {3})."}},
        {"XS_BANKS_ORDER", {"Tebing kiri ({0}) harus lebih kecil dari tebing kanan ({1}).",
                            "Left bank ({0}) must be less than right bank ({1})."}},
        {"XS_NO_MANNING", {"Nilai Manning n belum diisi.", "Manning's n values are missing."}},
        {"XS_MANNING_RANGE", {"Manning n = {0} di luar rentang yang umum (0,01–0,25). Cek lagi nilainya.",
                              "Manning's n = {0} is outside the usual range 0.01–0.25; please check."}},
        {"XS_MANNING_NONPOS", {"Manning n harus lebih besar dari 0 (ditemukan {0}).",
                               "Manning's n must be greater than 0 (found {0})."}},
        {"XS_MANNING_START", {"Titik Manning n pertama ({0}) harus berada di atau sebelum stasiun pertama ({1}).",
                              "The first Manning's n breakpoint ({0}) must be at or before the first station ({1})."}},
        {"XS_LENGTH_NEG", {"Jarak ke penampang hilir tidak boleh negatif.", "Downstream reach lengths must not be negative."}},
        {"XS_LENGTH_ZERO", {"Jarak ke penampang hilir bernilai nol, padahal ini bukan penampang paling hilir.",
                            "Downstream reach length is zero on a cross section that is not the most downstream one."}},
        {"XS_COEF_RANGE", {"Koefisien kontraksi/ekspansi ({0}/{1}) di luar rentang yang umum (0–1).",
                           "Contraction/expansion coefficients ({0}/{1}) are outside the usual range (0–1)."}},
        {"XS_ORDER", {"Urutan penampang bukan dari hulu ke hilir (RS {0} sesudah RS {1}).",
                      "Cross sections are not ordered upstream to downstream (RS {0} after RS {1})."}},
        {"XS_SPACING", {"Jarak antarpenampang {0} {1} cukup jauh dibanding lebar aliran. Sebaiknya tambah penampang di antaranya.",
                        "Spacing {0} {1} between cross sections is large relative to the flow width; consider adding cross sections."}},
        {"XS_FEATURE_IGNORED", {"Data '{0}' pada penampang ini belum ikut dihitung (diabaikan).",
                                "Data '{0}' on this cross section is not supported by the solver yet and is ignored in the computation."}},
        {"FLOW_NO_PROFILES", {"Data aliran belum berisi profil.", "The flow data contains no profiles."}},
        {"FLOW_NO_UPSTREAM", {"Reach {0} tidak memiliki debit pada penampang paling hulu ({1}).",
                              "Reach {0} has no discharge at its most upstream cross section ({1})."}},
        {"FLOW_NONPOS", {"Debit profil '{0}' pada RS {1} harus > 0 (ditemukan {2}).",
                         "Discharge for profile '{0}' at RS {1} must be > 0 (found {2})."}},
        {"FLOW_UNKNOWN_RS", {"Lokasi debit RS {0} tidak ditemukan di geometri reach {1}.",
                             "Flow location RS {0} is not found in the geometry of reach {1}."}},
        {"BC_MISSING_DN", {"Kondisi batas hilir untuk profil '{0}' di reach {1} belum diisi.",
                           "The downstream boundary condition for profile '{0}' on reach {1} is missing."}},
        {"BC_BAD_SLOPE", {"Kemiringan normal depth harus > 0 (profil '{0}').", "Normal-depth slope must be > 0 (profile '{0}')."}},
        {"BC_BAD_RATING", {"Rating curve hilir butuh minimal 2 pasangan elevasi–debit yang naik (profil '{0}').",
                           "The downstream rating curve needs at least 2 increasing stage–flow pairs (profile '{0}')."}},
        {"BC_KNOWN_BELOW_BED", {"Muka air diketahui {0} lebih rendah dari dasar saluran {1} (profil '{2}').",
                                "Known water surface {0} is below the channel invert {1} (profile '{2}')."}},
        {"BC_MISSING_UP", {"Kondisi batas hulu untuk profil '{0}' di reach {1} belum diisi. Rezim superkritis dan campuran butuh batas hulu.",
                           "The upstream boundary condition for profile '{0}' on reach {1} is missing. Supercritical and mixed regimes need one."}},
        {"MD_JUNCTION_UNKNOWN", {"Junction '{0}' menyebut reach {1} yang tidak ada di geometri.",
                                 "Junction '{0}' refers to reach {1}, which is not in the geometry."}},
        {"MD_JUNCTION_LOOP", {"Jaringan sungai membentuk putaran (loop) atau ada reach yang terputus, jadi urutan hitungnya tidak bisa ditentukan.",
                              "The river network contains a loop or a disconnected reach, so the computation order cannot be determined."}},
        {"MD_FEATURE_PREVIEW", {"{1} dihitung dengan status Pratinjau: hasilnya belum dibandingkan dengan HEC-RAS.",
                                "{0} is computed with Preview status: the results have not been compared with HEC-RAS yet."}},
        {"PLAN_NONE", {"Tidak ada plan aktif. Buat atau pilih plan di Analisis Steady.", "No current plan. Create or pick a plan in Steady Analysis."}},
        {"MD_OK", {"Model lolos pemeriksaan: {0} peringatan.", "Model passed the checks: {0} warning(s)."}},
        // ---------- solver ----------
        {"SOLV_START", {"Mulai perhitungan steady subkritis: {0} profil, {1} penampang.",
                        "Starting subcritical steady computation: {0} profile(s), {1} cross section(s)."}},
        {"SOLV_START_REGIME", {"Mulai perhitungan steady {1}: {2} profil, {3} penampang.",
                               "Starting {0} steady computation: {2} profile(s), {3} cross section(s)."}},
        {"SOLV_SUPER_NO_SOLUTION", {"Tidak ada muka air superkritis yang memenuhi persamaan energi, jadi muka air diambil sama dengan kritis.",
                                    "No supercritical water surface satisfies the energy equation; water surface set to critical."}},
        {"SOLV_JUMP", {"Loncatan hidraulik antara RS {0} dan RS {1} (gaya spesifik subkritis lebih besar).",
                       "Hydraulic jump between RS {0} and RS {1} (subcritical specific force is larger)."}},
        {"SOLV_JUNCTION_DN", {"Muka air di ujung hilir reach ini dihitung dari junction '{0}' (persamaan energi).",
                              "Water surface at the downstream end of this reach computed from junction '{0}' (energy equation)."}},
        {"SOLV_JUNCTION_UP", {"Muka air superkritis di ujung hulu reach ini dihitung dari junction '{0}'.",
                              "Supercritical water surface at the upstream end of this reach computed from junction '{0}'."}},
        {"SOLV_UP_ABOVE_CRIT", {"Muka air batas hulu ({0}) di atas kritis ({1}). Untuk aliran superkritis dipakai kedalaman kritis.",
                                "Upstream boundary water surface ({0}) is above critical ({1}); critical depth is used for supercritical flow."}},
        {"SOLV_BC_UP_FAIL", {"Muka air batas hulu tidak bisa dihitung (profil '{0}').",
                             "The upstream boundary water surface could not be computed (profile '{0}')."}},
        {"SOLV_PROFILE_DONE", {"Profil '{0}' selesai.", "Profile '{0}' finished."}},
        {"SOLV_DONE", {"Perhitungan selesai tanpa kesalahan.", "Computation finished without errors."}},
        {"SOLV_CANCELLED", {"Perhitungan dibatalkan. Tidak ada hasil yang disimpan.",
                            "Computation cancelled by the user. No results were saved."}},
        {"SOLV_FAILED", {"Perhitungan gagal. Tidak ada hasil yang disimpan.", "Computation failed. No results were saved."}},
        {"SOLV_NO_CONVERGE_CRIT", {"Iterasi standard step tidak konvergen, jadi muka air diambil sama dengan kedalaman kritis.",
                                   "Standard-step iteration did not converge; water surface defaulted to critical depth."}},
        {"SOLV_NO_CONVERGE_MINERR", {"Iterasi belum konvergen setelah {0} percobaan. Dipakai muka air dengan selisih terkecil ({1} {2}).",
                                     "Iteration did not converge in {0} trials; the water surface with minimum error was used ({1} {2})."}},
        {"SOLV_CRITICAL_ASSUMED", {"Muka air hasil hitungan lebih rendah dari muka air kritis, jadi muka air diambil sama dengan kritis.",
                                   "The computed water surface is below critical depth; water surface set to critical."}},
        {"SOLV_WALLS_EXTENDED", {"Muka air ({0}) lebih tinggi dari ujung penampang, jadi ujungnya diperpanjang tegak ke atas.",
                                 "Water surface ({0}) exceeds the cross-section end points; vertical walls were extended."}},
        {"SOLV_ENERGY_LOSS_LARGE", {"Kehilangan energi antarpenampang besar ({0} {1}). Mungkin perlu penampang tambahan.",
                                    "Energy loss between cross sections is large ({0} {1}); additional cross sections may be needed."}},
        {"SOLV_CONV_RATIO", {"Rasio konveyansi hilir/hulu {0} di luar 0,7–1,4. Mungkin perlu penampang tambahan.",
                             "Conveyance ratio downstream/upstream {0} is outside 0.7–1.4; additional cross sections may be needed."}},
        {"SOLV_BC_NORMAL_FAIL", {"Normal depth di batas hilir tidak bisa dihitung (profil '{0}').",
                                 "Normal depth at the downstream boundary could not be computed (profile '{0}')."}},
        {"SOLV_BC_RATING_RANGE", {"Debit {0} di luar rentang rating curve hilir, jadi nilainya diekstrapolasi linear.",
                                  "Discharge {0} is outside the downstream rating curve range; extrapolated linearly."}},
        {"SOLV_BC_CRIT", {"Kondisi batas kedalaman kritis dipakai di hilir.", "Critical-depth boundary used downstream."}},
        {"SOLV_DN_BELOW_CRIT", {"Muka air batas hilir ({0}) lebih rendah dari kritis ({1}), jadi diambil sama dengan kritis.",
                                "Downstream boundary water surface ({0}) is below critical ({1}); set to critical."}},
        {"SOLV_NAN", {"Muncul nilai yang tidak valid (NaN/Inf), jadi perhitungan dihentikan.",
                      "An invalid numeric value (NaN/Inf) occurred; computation stopped."}},
        {"SOLV_MODEL_INVALID", {"Model belum lolos pemeriksaan, jadi perhitungan tidak dijalankan.",
                                "The model failed validation; the computation was not run."}},
        {"RES_HDF_WRITTEN", {"Hasil juga ditulis ke {0} (susunan file hasil HEC-RAS, status Pratinjau).",
                             "Results were also written to {0} (HEC-RAS results file layout, Preview status)."}},
        {"RES_HDF_KEPT", {"File {0} dibuat program lain, jadi tidak ditimpa. Hasil HDF De-RAS ditulis ke {1}.",
                          "{0} was written by another program, so it was not overwritten. The De-RAS HDF results went to {1}."}},
        {"RES_HDF_FAILED", {"File hasil HDF tidak bisa ditulis: {0}", "The HDF results file could not be written: {0}"}},
        {"RES_STALE", {"Hasil ini dihitung dari data yang sudah berubah. Hitung ulang plan ini.",
                       "These results were computed from inputs that have since changed; re-run this plan."}},
    };
    return c;
}

std::string substitute(std::string t, const std::vector<std::string>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string key = "{" + std::to_string(i) + "}";
        size_t p = 0;
        while ((p = t.find(key, p)) != std::string::npos) {
            t.replace(p, key.size(), args[i]);
            p += args[i].size();
        }
    }
    return t;
}
}  // namespace

std::string catalogText(const std::string& code, Language lang) {
    auto it = catalog().find(code);
    if (it == catalog().end()) return code;
    return lang == Language::ID ? it->second.id : it->second.en;
}

std::string Message::body(Language lang) const { return substitute(catalogText(code, lang), args); }

std::string Message::text(Language lang) const {
    std::string t = body(lang);
    if (!location.empty()) t = "[" + location + "] " + t;
    return t;
}

std::string fixHint(const std::string& code, Language lang) {
    static const std::map<std::string, Pair> h = {
        {"MD_NO_GEOMETRY", {"Buka Analisis Steady (Ctrl+5) dan pilih file geometri untuk plan ini.", "Open Steady Analysis (Ctrl+5) and pick a geometry file for this plan."}},
        {"MD_U_NO_FILE", {"Buka Analisis Unsteady dan pilih file aliran unsteady untuk plan ini.", "Open Unsteady Analysis and pick an unsteady flow file for this plan."}},
        {"MD_U_TIME", {"Isi tanggal dan jam mulai/selesai di Analisis Unsteady.", "Enter the start and end date and time in Unsteady Analysis."}},
        {"MD_U_INTERVAL", {"Pilih interval komputasi dan keluaran di Analisis Unsteady.", "Pick the computation and output intervals in Unsteady Analysis."}},
        {"MD_U_BC_MISSING", {"Buka Data Aliran Unsteady dan isi kondisi batas untuk ujung reach ini.", "Open Unsteady Flow Data and set a boundary condition for this reach end."}},
        {"MD_U_BC_TYPE", {"Hulu: hidrograf debit atau muka air. Hilir: hidrograf muka air, normal depth, atau rating curve.", "Upstream: flow or stage hydrograph. Downstream: stage hydrograph, normal depth or rating curve."}},
        {"MD_U_BC_EMPTY", {"Isi tabel hidrograf di Data Aliran Unsteady.", "Fill the hydrograph table in Unsteady Flow Data."}},
        {"MD_U_STRUCTURE", {"Jalankan plan steady untuk model dengan struktur, atau tunggu versi berikutnya.", "Run a steady plan for models with structures, or wait for a later version."}},
        {"MD_U_INIT", {"Isi Debit awal untuk reach ini di Data Aliran Unsteady.", "Enter the Initial flow for this reach in Unsteady Flow Data."}},
        {"MD_U_LENGTH", {"Isi jarak ke hilir (alur) di Editor Penampang.", "Enter the downstream (channel) length in the Cross-Section Editor."}},
        {"MD_NO_FLOW", {"Buka Analisis Steady (Ctrl+5) dan pilih file aliran steady untuk plan ini.", "Open Steady Analysis (Ctrl+5) and pick a steady flow file for this plan."}},
        {"MD_NO_REACH", {"Gambar reach di Data Geometri (tombol Gambar reach) atau tambahkan lewat Kelola reach.", "Draw a reach in Geometric Data (Draw reach) or add one with Manage reaches."}},
        {"MD_REACH_FEW_XS", {"Tambahkan penampang di Editor Penampang sampai reach punya minimal 2.", "Add cross sections in the Cross Section Editor until the reach has at least 2."}},
        {"XS_TOO_FEW_POINTS", {"Buka penampang ini dan isi minimal 3 titik stasiun–elevasi.", "Open this cross section and enter at least 3 station–elevation points."}},
        {"XS_STATION_ORDER", {"Urutkan stasiun dari kiri ke kanan (menghadap hilir) di tabel stasiun–elevasi.", "Sort the stations left to right (looking downstream) in the station–elevation table."}},
        {"XS_BANKS_MISSING", {"Isi tebing kiri dan kanan di kartu Data penampang.", "Enter the left and right bank stations in the Section data card."}},
        {"XS_BANKS_OUTSIDE", {"Pindahkan tebing ke dalam rentang stasiun penampang.", "Move the bank stations inside the station range of the section."}},
        {"XS_BANKS_ORDER", {"Tukar nilai tebing kiri dan kanan.", "Swap the left and right bank stations."}},
        {"XS_NO_MANNING", {"Isi n Manning bantaran kiri, alur, dan bantaran kanan di kartu Data penampang.", "Enter Manning's n for the left overbank, channel and right overbank in the Section data card."}},
        {"XS_MANNING_RANGE", {"Cek lagi nilai n. Nilai di luar 0,01–0,25 jarang dipakai.", "Check the n value again. Values outside 0.01–0.25 are rarely used."}},
        {"XS_MANNING_NONPOS", {"Ganti n dengan angka lebih dari 0, misalnya 0,035.", "Change n to a number above 0, for example 0.035."}},
        {"XS_MANNING_START", {"Mulai titik n pertama di stasiun pertama penampang.", "Start the first n breakpoint at the first station of the section."}},
        {"XS_LENGTH_NEG", {"Isi jarak ke penampang hilir dengan angka 0 atau lebih.", "Enter downstream lengths of 0 or more."}},
        {"XS_LENGTH_ZERO", {"Isi jarak alur ke penampang hilir berikutnya (Data Geometri → Tabel…).", "Enter the channel length to the next downstream section (Geometric Data → Tables…)."}},
        {"XS_COEF_RANGE", {"Nilai yang umum: kontraksi 0,1 dan ekspansi 0,3.", "Typical values: contraction 0.1 and expansion 0.3."}},
        {"XS_ORDER", {"Cek RS penampang: RS hulu harus lebih besar dari RS hilir.", "Check the cross-section RS: the upstream RS must be larger than the downstream one."}},
        {"XS_FEATURE_IGNORED", {"Hasil dihitung tanpa data ini. Data tetap tersimpan di file.", "Results are computed without this data. The data stays in the file."}},
        {"GEOM_NODE_UNSUPPORTED", {"Struktur ini tetap tersimpan di file, tetapi De-RAS belum menghitungnya (bukaan ganda / multiple opening belum tersedia).", "The structure stays in the file, but De-RAS does not compute it yet (multiple openings are not available)."}},
        {"GEOM_STRUCTURE_UNREAD", {"Periksa struktur di HEC-RAS, atau buat ulang di Data Geometri → Jembatan/Gorong-gorong atau Struktur Inline.", "Check the structure in HEC-RAS, or create it again in Geometric Data → Bridge/Culvert or Inline Structure."}},
        {"MD_STRUCTURE_UNREAD", {"Buat ulang struktur di Data Geometri, atau hapus node-nya.", "Create the structure again in Geometric Data, or delete its node."}},
        {"MD_STRUCTURE_REGIME", {"Ganti rezim plan ke Subkritis di Analisis Steady.", "Change the plan regime to Subcritical in Steady Analysis."}},
        {"MD_STRUCTURE_PLACE", {"Tambahkan penampang di hulu dan hilir struktur.", "Add cross sections upstream and downstream of the structure."}},
        {"MD_STRUCTURE_DATA", {"Perbaiki data struktur di Data Geometri.", "Fix the structure data in Geometric Data."}},
        {"SOLV_STRUCTURE_FAIL", {"Periksa ukuran bukaan, elevasi dek atau pelimpah, dan debit.", "Check the opening size, deck or weir elevations, and the flow."}},
        {"FLOW_NO_PROFILES", {"Buka Aliran Steady (Ctrl+3) dan tambahkan minimal satu profil.", "Open Steady Flow (Ctrl+3) and add at least one profile."}},
        {"FLOW_NO_UPSTREAM", {"Di Aliran Steady, tambahkan lokasi debit di penampang paling hulu reach ini.", "In Steady Flow, add a flow location at the most upstream section of this reach."}},
        {"FLOW_NONPOS", {"Isi debit lebih dari 0 di tabel debit Aliran Steady.", "Enter a discharge above 0 in the Steady Flow table."}},
        {"FLOW_UNKNOWN_RS", {"Hapus lokasi debit ini atau ganti RS-nya dengan RS yang ada di geometri.", "Remove this flow location or change it to an RS that exists in the geometry."}},
        {"BC_MISSING_DN", {"Di Aliran Steady, klik Atur kondisi batas… lalu pilih tipe batas hilir.", "In Steady Flow, click Set boundary conditions… and choose a downstream type."}},
        {"BC_BAD_SLOPE", {"Isi kemiringan energi lebih dari 0, misalnya 0,001.", "Enter an energy slope above 0, for example 0.001."}},
        {"BC_BAD_RATING", {"Isi minimal 2 pasangan elevasi:debit dengan debit yang terus naik.", "Enter at least 2 stage:flow pairs with increasing flow."}},
        {"BC_KNOWN_BELOW_BED", {"Naikkan elevasi muka air batas hilir di atas dasar saluran.", "Raise the downstream water surface above the channel invert."}},
        {"BC_MISSING_UP", {"Di Aliran Steady, klik Atur kondisi batas… lalu pilih tipe batas hulu (misalnya kedalaman kritis atau normal depth).", "In Steady Flow, click Set boundary conditions… and choose an upstream type (for example critical depth or normal depth)."}},
        {"MD_JUNCTION_UNKNOWN", {"Periksa nama sungai dan reach di junction, atau tambahkan reach yang hilang.", "Check the river and reach names of the junction, or add the missing reach."}},
        {"MD_JUNCTION_LOOP", {"Periksa junction: setiap reach harus mengalir ke satu arah tanpa membentuk putaran.", "Check the junctions: every reach must flow one way without forming a loop."}},
        {"PLAN_NONE", {"Buka Analisis Steady (Ctrl+5) dan buat plan dengan tombol +.", "Open Steady Analysis (Ctrl+5) and create a plan with the + button."}},
        {"NON_ASCII_NAME", {"Pakai huruf tanpa aksen supaya HEC-RAS membacanya dengan benar.", "Use letters without accents so HEC-RAS reads the name correctly."}},
    };
    auto it = h.find(code);
    if (it == h.end()) return {};
    return lang == Language::ID ? it->second.id : it->second.en;
}

Message makeMsg(Severity s, std::string code, std::vector<std::string> args, std::string location) {
    Message m;
    m.severity = s;
    m.code = std::move(code);
    m.args = std::move(args);
    m.location = std::move(location);
    return m;
}

const char* severityLabel(Severity s, Language lang) {
    switch (s) {
        case Severity::Info: return lang == Language::ID ? "Info" : "Info";
        case Severity::Warning: return lang == Language::ID ? "Peringatan" : "Warning";
        case Severity::Error: return lang == Language::ID ? "Kesalahan" : "Error";
    }
    return "";
}

bool hasErrors(const MessageList& msgs) {
    for (const auto& m : msgs)
        if (m.severity == Severity::Error) return true;
    return false;
}

std::string fmt(double x, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, x);
    return buf;
}

}  // namespace deras
