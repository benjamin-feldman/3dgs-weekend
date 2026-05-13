#ifndef GAUSSIAN_SPLAT_H
#define GAUSSIAN_SPLAT_H

#include "glm/ext/vector_float3.hpp"

#include <array>

constexpr int SH_COUNT = 16;
constexpr int SH_CHANNEL_COUNT = 3;
constexpr int SH_FLOAT_COUNT = SH_COUNT * SH_CHANNEL_COUNT;
constexpr int SH_REST_FLOAT_COUNT = SH_FLOAT_COUNT - SH_CHANNEL_COUNT;
constexpr int SH_PACKED_VEC4_COUNT = SH_FLOAT_COUNT / 4;

static_assert(SH_FLOAT_COUNT % 4 == 0,
              "Spherical harmonic coefficients must pack into vec4 attributes");

struct GaussianSplat {
    glm::vec3 centroid = glm::vec3(0.0f);
    float opacity = 0.0f;
    std::array<float, SH_FLOAT_COUNT> sphericalHarmonics = {};
    std::array<float, 3> scale = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {1.0f, 0.0f, 0.0f, 0.0f};
};

#endif
