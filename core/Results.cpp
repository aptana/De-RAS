// De-RAS - core/Results.cpp
#include "Results.h"

namespace deras {

const char* originLabel(DataOrigin o, Language l) {
    const bool id = l == Language::ID;
    switch (o) {
        case DataOrigin::Input: return id ? "Data masukan" : "Input data";
        case DataOrigin::Computed: return id ? "Hasil hitungan De-RAS" : "Computed by De-RAS";
        case DataOrigin::Imported: return id ? "Hasil impor" : "Imported result";
        case DataOrigin::Example: return id ? "Data contoh" : "Example data";
    }
    return "";
}

const char* runStatusLabel(RunStatus s, Language l) {
    const bool id = l == Language::ID;
    switch (s) {
        case RunStatus::NotRun: return id ? "Belum dijalankan" : "Not run";
        case RunStatus::Succeeded: return id ? "Berhasil" : "Succeeded";
        case RunStatus::Failed: return id ? "Gagal" : "Failed";
        case RunStatus::Cancelled: return id ? "Dibatalkan" : "Cancelled";
    }
    return "";
}

}  // namespace deras
