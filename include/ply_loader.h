#ifndef PLY_LOADER_H
#define PLY_LOADER_H

#include "gaussian_splat.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Scene {
    std::vector<GaussianSplat> splats;
};

namespace plyDetail {

struct Property {
    std::string name;
    std::string type;
    std::size_t offset = 0;
    std::size_t size = 0;
};

inline std::size_t scalarSize(const std::string& type) {
    if (type == "char" || type == "uchar" || type == "int8" || type == "uint8") {
        return 1;
    }
    if (type == "short" || type == "ushort" || type == "int16" || type == "uint16") {
        return 2;
    }
    if (type == "int" || type == "uint" || type == "float" || type == "int32" || type == "uint32" ||
        type == "float32") {
        return 4;
    }
    if (type == "double" || type == "float64") {
        return 8;
    }
    throw std::runtime_error("Unsupported PLY scalar type: " + type);
}

template <typename T> inline T readScalar(const unsigned char* bytes) {
    T value;
    std::memcpy(&value, bytes, sizeof(T));
    return value;
}

inline float readAsFloat(const std::vector<unsigned char>& row, const Property& property) {
    const unsigned char* ptr = row.data() + property.offset;
    const std::string& type = property.type;

    if (type == "float" || type == "float32")
        return readScalar<float>(ptr);
    if (type == "double" || type == "float64")
        return static_cast<float>(readScalar<double>(ptr));
    if (type == "char" || type == "int8")
        return static_cast<float>(readScalar<std::int8_t>(ptr));
    if (type == "uchar" || type == "uint8")
        return static_cast<float>(readScalar<std::uint8_t>(ptr));
    if (type == "short" || type == "int16")
        return static_cast<float>(readScalar<std::int16_t>(ptr));
    if (type == "ushort" || type == "uint16")
        return static_cast<float>(readScalar<std::uint16_t>(ptr));
    if (type == "int" || type == "int32")
        return static_cast<float>(readScalar<std::int32_t>(ptr));
    if (type == "uint" || type == "uint32")
        return static_cast<float>(readScalar<std::uint32_t>(ptr));

    throw std::runtime_error("Unsupported PLY scalar type: " + type);
}

inline std::vector<std::string> splitWords(const std::string& line) {
    std::vector<std::string> words;
    std::string word;
    for (char c : line) {
        if (c == ' ' || c == '\t' || c == '\r') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word.push_back(c);
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

inline const Property& requireProperty(const std::unordered_map<std::string, std::size_t>& lookup,
                                       const std::vector<Property>& properties,
                                       const std::string& name) {
    auto it = lookup.find(name);
    if (it == lookup.end()) {
        throw std::runtime_error("Missing required vertex property: " + name);
    }
    return properties[it->second];
}

} // namespace plyDetail

inline Scene loadPly(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open PLY file: " + path);
    }

    std::string line;
    if (!std::getline(file, line) || line != "ply") {
        throw std::runtime_error("Not a PLY file: " + path);
    }

    bool sawFormat = false;
    bool inVertex = false;
    std::size_t vertexCount = 0;
    std::size_t rowStride = 0;
    std::vector<plyDetail::Property> properties;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line == "end_header") {
            break;
        }

        std::vector<std::string> words = plyDetail::splitWords(line);
        if (words.empty() || words[0] == "comment") {
            continue;
        }

        if (words[0] == "format") {
            if (words.size() < 3 || words[1] != "binary_little_endian") {
                throw std::runtime_error("Only binary_little_endian PLY files are supported");
            }
            sawFormat = true;
            continue;
        }

        if (words[0] == "element") {
            if (words.size() < 3) {
                throw std::runtime_error("Malformed PLY element line: " + line);
            }
            inVertex = words[1] == "vertex";
            if (inVertex) {
                vertexCount = static_cast<std::size_t>(std::stoull(words[2]));
            }
            continue;
        }

        if (inVertex && words[0] == "property") {
            if (words.size() < 3 || words[1] == "list") {
                throw std::runtime_error("Only scalar vertex properties are supported");
            }

            plyDetail::Property property;
            property.name = words[2];
            property.type = words[1];
            property.offset = rowStride;
            property.size = plyDetail::scalarSize(property.type);
            rowStride += property.size;
            properties.push_back(property);
        }
    }

    if (!sawFormat) {
        throw std::runtime_error("PLY file is missing a format line");
    }
    if (vertexCount == 0) {
        throw std::runtime_error("PLY file has no vertex records");
    }

    std::unordered_map<std::string, std::size_t> lookup;
    for (std::size_t i = 0; i < properties.size(); ++i) {
        lookup[properties[i].name] = i;
    }

    const plyDetail::Property& x = plyDetail::requireProperty(lookup, properties, "x");
    const plyDetail::Property& y = plyDetail::requireProperty(lookup, properties, "y");
    const plyDetail::Property& z = plyDetail::requireProperty(lookup, properties, "z");
    const plyDetail::Property& dcR = plyDetail::requireProperty(lookup, properties, "f_dc_0");
    const plyDetail::Property& dcG = plyDetail::requireProperty(lookup, properties, "f_dc_1");
    const plyDetail::Property& dcB = plyDetail::requireProperty(lookup, properties, "f_dc_2");
    const plyDetail::Property& opacity = plyDetail::requireProperty(lookup, properties, "opacity");
    const plyDetail::Property& scale0 = plyDetail::requireProperty(lookup, properties, "scale_0");
    const plyDetail::Property& scale1 = plyDetail::requireProperty(lookup, properties, "scale_1");
    const plyDetail::Property& scale2 = plyDetail::requireProperty(lookup, properties, "scale_2");
    const plyDetail::Property& rot0 = plyDetail::requireProperty(lookup, properties, "rot_0");
    const plyDetail::Property& rot1 = plyDetail::requireProperty(lookup, properties, "rot_1");
    const plyDetail::Property& rot2 = plyDetail::requireProperty(lookup, properties, "rot_2");
    const plyDetail::Property& rot3 = plyDetail::requireProperty(lookup, properties, "rot_3");

    std::vector<const plyDetail::Property*> shRest;
    for (int i = 0; i < SH_REST_FLOAT_COUNT; ++i) {
        auto it = lookup.find("f_rest_" + std::to_string(i));
        if (it == lookup.end()) {
            break;
        }
        shRest.push_back(&properties[it->second]);
    }
    if (lookup.find("f_rest_" + std::to_string(SH_REST_FLOAT_COUNT)) != lookup.end()) {
        throw std::runtime_error(
            "PLY has more spherical harmonic coefficients than this tutorial loads");
    }

    if (shRest.size() % SH_CHANNEL_COUNT != 0) {
        throw std::runtime_error(
            "PLY f_rest_* properties must contain the same count per RGB channel");
    }

    const std::size_t restCountPerChannel = shRest.size() / SH_CHANNEL_COUNT;

    Scene result;
    result.splats.reserve(vertexCount);

    std::vector<unsigned char> row(rowStride);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        file.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()));
        if (!file) {
            throw std::runtime_error("PLY ended before all vertex records were read");
        }

        GaussianSplat splat;
        splat.centroid = {
            plyDetail::readAsFloat(row, x),
            plyDetail::readAsFloat(row, y),
            plyDetail::readAsFloat(row, z),
        };
        splat.sphericalHarmonics[0] = plyDetail::readAsFloat(row, dcR);
        splat.sphericalHarmonics[1] = plyDetail::readAsFloat(row, dcG);
        splat.sphericalHarmonics[2] = plyDetail::readAsFloat(row, dcB);
        for (std::size_t channel = 0; channel < SH_CHANNEL_COUNT; ++channel) {
            for (std::size_t coeff = 0; coeff < restCountPerChannel; ++coeff) {
                std::size_t plyRestIndex = channel * restCountPerChannel + coeff;
                std::size_t splatCoeff = coeff + 1;
                std::size_t splatIndex = splatCoeff * SH_CHANNEL_COUNT + channel;
                splat.sphericalHarmonics[splatIndex] =
                    plyDetail::readAsFloat(row, *shRest[plyRestIndex]);
            }
        }
        splat.opacity = plyDetail::readAsFloat(row, opacity);
        splat.scale[0] = plyDetail::readAsFloat(row, scale0);
        splat.scale[1] = plyDetail::readAsFloat(row, scale1);
        splat.scale[2] = plyDetail::readAsFloat(row, scale2);
        splat.rotation[0] = plyDetail::readAsFloat(row, rot0);
        splat.rotation[1] = plyDetail::readAsFloat(row, rot1);
        splat.rotation[2] = plyDetail::readAsFloat(row, rot2);
        splat.rotation[3] = plyDetail::readAsFloat(row, rot3);
        result.splats.push_back(splat);
    }

    return result;
}

#endif
