#version 330 core

layout(location = 0) in vec2 quadCorner;
layout(location = 1) in vec3 splatCentroid;
layout(location = 2) in float opacity;
layout(location = 3) in vec3 scale;
layout(location = 4) in vec4 rotation;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPosition;
uniform vec2 viewportSize;
uniform samplerBuffer sphericalHarmonicsTexture;

out vec2 splatCoord;
out vec3 splatColor;
out float splatOpacity;

const int SH_FLOAT_COUNT = 48;
const float SPLAT_EXTENT = 3.0;

const float SH_C0 = 0.28209479177387814;
const float SH_C1 = 0.4886025119029199;
const float SH_C2_0 = 1.0925484305920792;
const float SH_C2_1 = -1.0925484305920792;
const float SH_C2_2 = 0.31539156525252005;
const float SH_C2_3 = -1.0925484305920792;
const float SH_C2_4 = 0.5462742152960396;
const float SH_C3_0 = -0.5900435899266435;
const float SH_C3_1 = 2.890611442640554;
const float SH_C3_2 = -0.4570457994644658;
const float SH_C3_3 = 0.3731763325901154;
const float SH_C3_4 = -0.4570457994644658;
const float SH_C3_5 = 1.445305721320277;
const float SH_C3_6 = -0.5900435899266435;

float sphericalHarmonicFloat(int index) {
    return texelFetch(sphericalHarmonicsTexture, gl_InstanceID * SH_FLOAT_COUNT + index).r;
}

vec3 sphericalHarmonicCoefficient(int coefficient) {
    int baseIndex = coefficient * 3;
    return vec3(sphericalHarmonicFloat(baseIndex), sphericalHarmonicFloat(baseIndex + 1),
        sphericalHarmonicFloat(baseIndex + 2));
}

vec3 sphericalHarmonicsToRgb(vec3 direction) {
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;

    vec3 rgb = SH_C0 * sphericalHarmonicCoefficient(0);

    rgb += -SH_C1 * y * sphericalHarmonicCoefficient(1);
    rgb += SH_C1 * z * sphericalHarmonicCoefficient(2);
    rgb += -SH_C1 * x * sphericalHarmonicCoefficient(3);

    rgb += SH_C2_0 * x * y * sphericalHarmonicCoefficient(4);
    rgb += SH_C2_1 * y * z * sphericalHarmonicCoefficient(5);
    rgb += SH_C2_2 * (2.0 * z * z - x * x - y * y) * sphericalHarmonicCoefficient(6);
    rgb += SH_C2_3 * x * z * sphericalHarmonicCoefficient(7);
    rgb += SH_C2_4 * (x * x - y * y) * sphericalHarmonicCoefficient(8);

    rgb += SH_C3_0 * y * (3.0 * x * x - y * y) * sphericalHarmonicCoefficient(9);
    rgb += SH_C3_1 * x * y * z * sphericalHarmonicCoefficient(10);
    rgb += SH_C3_2 * y * (4.0 * z * z - x * x - y * y) * sphericalHarmonicCoefficient(11);
    rgb += SH_C3_3 * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y) *
            sphericalHarmonicCoefficient(12);
    rgb += SH_C3_4 * x * (4.0 * z * z - x * x - y * y) * sphericalHarmonicCoefficient(13);
    rgb += SH_C3_5 * z * (x * x - y * y) * sphericalHarmonicCoefficient(14);
    rgb += SH_C3_6 * x * (x * x - 3.0 * y * y) * sphericalHarmonicCoefficient(15);

    return clamp(rgb + vec3(0.5), 0.0, 1.0);
}

mat3 quaternionToMat3(vec4 quaternion) {
    vec4 q = normalize(quaternion);
    float w = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;

    return mat3(
        1.0 - 2.0 * y * y - 2.0 * z * z,
        2.0 * x * y + 2.0 * w * z,
        2.0 * x * z - 2.0 * w * y,
        2.0 * x * y - 2.0 * w * z,
        1.0 - 2.0 * x * x - 2.0 * z * z,
        2.0 * y * z + 2.0 * w * x,
        2.0 * x * z + 2.0 * w * y,
        2.0 * y * z - 2.0 * w * x,
        1.0 - 2.0 * x * x - 2.0 * y * y);
}

mat3 buildCovariance3D() {
    vec3 sigma = exp(scale);
    mat3 rotationMatrix = quaternionToMat3(rotation);
    mat3 scaleMatrix = mat3(
        sigma.x, 0.0, 0.0,
        0.0, sigma.y, 0.0,
        0.0, 0.0, sigma.z);
    mat3 rs = rotationMatrix * scaleMatrix;
    mat3 covariance = rs * transpose(rs);

    // Match the CPU-side preview flip applied to centroids in main.cpp.
    mat3 previewFlipY = mat3(
        1.0, 0.0, 0.0,
        0.0, -1.0, 0.0,
        0.0, 0.0, 1.0);
    return previewFlipY * covariance * previewFlipY;
}

mat3 projectCovarianceToScreen(vec3 centerCamera, mat3 covariance3D) {
    float focalX = projection[0][0] * viewportSize.x * 0.5;
    float focalY = projection[1][1] * viewportSize.y * 0.5;
    float z = centerCamera.z;

    mat3 jacobian = mat3(
        -focalX / z, 0.0, 0.0,
        0.0, -focalY / z, 0.0,
        focalX * centerCamera.x / (z * z), focalY * centerCamera.y / (z * z), 0.0);

    mat3 viewLinear = mat3(view);
    mat3 transform = jacobian * viewLinear;
    return transform * covariance3D * transpose(transform);
}

void main() {
    vec4 centerClip = projection * view * vec4(splatCentroid, 1.0);
    vec3 centerCamera = vec3(view * vec4(splatCentroid, 1.0));

    if (centerClip.w <= 0.0 || centerCamera.z >= -0.001 || viewportSize.x <= 0.0 ||
        viewportSize.y <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        splatCoord = quadCorner;
        splatColor = vec3(0.0);
        splatOpacity = 0.0;
        return;
    }

    mat3 covariance2D = projectCovarianceToScreen(centerCamera, buildCovariance3D());

    float a = covariance2D[0][0] + 0.3;
    float b = covariance2D[1][0];
    float d = covariance2D[1][1] + 0.3;

    float determinant = a * d - b * b;
    float mid = 0.5 * (a + d);
    float radius = sqrt(max(mid * mid - determinant, 0.0));
    float lambda1 = max(mid + radius, 0.01);
    float lambda2 = max(mid - radius, 0.01);

    vec2 axisDirection1 =
        abs(b) > 0.00001 ? normalize(vec2(b, lambda1 - a))
                         : (a >= d ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    vec2 axisDirection2 = vec2(-axisDirection1.y, axisDirection1.x);

    vec2 axis1 = SPLAT_EXTENT * sqrt(lambda1) * axisDirection1;
    vec2 axis2 = SPLAT_EXTENT * sqrt(lambda2) * axisDirection2;

    vec3 ndc = centerClip.xyz / centerClip.w;
    vec2 pixelOffset = quadCorner.x * axis1 + quadCorner.y * axis2;
    vec2 ndcOffset = pixelOffset / (viewportSize * 0.5);

    gl_Position = vec4(ndc.xy + ndcOffset, ndc.z, 1.0);
    splatCoord = quadCorner;
    splatColor = sphericalHarmonicsToRgb(normalize(splatCentroid - cameraPosition));
    splatOpacity = 1.0 / (1.0 + exp(-opacity));
}
