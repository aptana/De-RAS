// De-RAS - io/HdfWriter.h
// Minimal HDF5 file writer without external dependencies, written from the
// public "HDF5 File Format Specification" (version 3.0): superblock version
// 2, version-2 object headers, compact link storage for groups, contiguous
// datasets and compact attributes. Enough for result files such as
// .p##.hdf; readable by the HDF5 library 1.8 and later (h5dump, h5py,
// HDFView). Files are built in memory and written in one go.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "RasText.h"

namespace deras::io::hdf {

enum class Type { Float32, Float64, Int32, String };

struct Attribute {
    std::string name;
    Type type = Type::String;
    std::vector<uint64_t> dims;  // empty = scalar
    std::vector<double> num;
    std::vector<std::string> str;
    size_t strLen = 0;  // fixed string length (0 = longest value)
};

struct Dataset {
    std::string name;
    Type type = Type::Float32;
    std::vector<uint64_t> dims;
    std::vector<double> num;          // row-major
    std::vector<std::string> str;     // for Type::String
    size_t strLen = 0;
    std::vector<Attribute> attrs;
};

struct Group {
    std::string name;
    std::vector<std::unique_ptr<Group>> groups;
    std::vector<Dataset> datasets;
    std::vector<Attribute> attrs;

    // Child group by path ("A/B/C"), created when missing.
    Group& group(const std::string& path);
    Dataset& addFloat32(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values);
    Dataset& addFloat64(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values);
    Dataset& addInt32(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values);
    Dataset& addStrings(const std::string& name, std::vector<std::string> values, size_t fixedLen = 0);
    void attr(const std::string& name, const std::string& value);
    void attr(const std::string& name, double value, Type t = Type::Float64);
};

// Serialise the tree rooted at `root` (its name is ignored) to bytes.
std::string build(const Group& root);
bool write(const fs::path& path, const Group& root, std::string& error);

uint32_t lookup3(const void* data, size_t length, uint32_t initval = 0);  // Jenkins hashlittle (HDF5 checksum)

}  // namespace deras::io::hdf
