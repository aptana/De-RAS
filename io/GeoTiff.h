// De-RAS - io/GeoTiff.h
// Single-band GeoTIFF reader and writer without external libraries, written
// from the public TIFF 6.0 and OGC GeoTIFF 1.1 specifications.
// Reader: classic TIFF (little or big endian), strips or tiles, no
// compression, LZW or Deflate, predictors 1/2/3, 8/16/32-bit integer and
// 32/64-bit float samples (first band only). GeoTIFF: ModelPixelScale +
// ModelTiepoint or ModelTransformation (no rotation), GeoKeyDirectory
// (EPSG code), GDAL_NODATA. BigTIFF is not supported.
// Writer: float32, uncompressed strips, GeoKeys copied from a source raster.
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "RasText.h"

namespace deras::io {

struct Raster {
    int nx = 0, ny = 0;          // columns, rows
    double x0 = 0, y0 = 0;       // top-left corner of the top-left cell
    double dx = 1, dy = 1;       // cell size (dy > 0, rows go towards -y)
    std::vector<float> z;        // row-major, NaN = no data
    int epsg = 0;                // projected or geographic CRS code (0 = unknown)
    std::string crsName;         // GTCitation / PCSCitation when present
    // GeoKey tags copied verbatim, so exports keep the CRS of the terrain.
    std::vector<uint16_t> geoKeys;
    std::vector<double> geoDoubles;
    std::string geoAscii;

    bool empty() const { return nx <= 0 || ny <= 0; }
    double xMax() const { return x0 + nx * dx; }
    double yMin() const { return y0 - ny * dy; }
    bool contains(double x, double y) const { return x >= x0 && x <= xMax() && y <= y0 && y >= yMin(); }
    float cell(int c, int r) const { return z[size_t(r) * nx + c]; }
    // Bilinear interpolation between cell centres; NaN outside or on no data.
    double sample(double x, double y) const;
    double cellX(int c) const { return x0 + (c + 0.5) * dx; }
    double cellY(int r) const { return y0 - (r + 0.5) * dy; }
};

bool readGeoTiff(const fs::path& path, Raster& out, std::string& error);
bool parseGeoTiff(const std::string& bytes, Raster& out, std::string& error);
bool writeGeoTiff(const fs::path& path, const Raster& r, std::string& error, float nodata = -9999.0f);
std::string buildGeoTiff(const Raster& r, float nodata = -9999.0f);

// RFC 1950/1951 decoder (zlib wrapper optional). Returns false on corrupt data.
bool inflateBytes(const uint8_t* in, size_t n, std::vector<uint8_t>& out, size_t expected);

}  // namespace deras::io
