// De-RAS - core/Hash.h
// SHA-256 (FIPS 180-4) used to fingerprint run inputs for reproducibility.
#pragma once

#include <cstdint>
#include <string>

namespace deras {

std::string sha256Hex(const std::string& data);

}  // namespace deras
