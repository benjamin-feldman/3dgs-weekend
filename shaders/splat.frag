#version 330 core

in vec2 splatCoord;
in vec3 splatColor;
in float splatOpacity;

out vec4 FragColor;

const float SPLAT_EXTENT = 3.0;

void main() {
    float power = -0.5 * SPLAT_EXTENT * SPLAT_EXTENT * dot(splatCoord, splatCoord);
    if (power < -9.0) {
        discard;
    }

    float alpha = min(0.99, splatOpacity * exp(power));
    if (alpha < 0.001) {
        discard;
    }

    FragColor = vec4(splatColor, alpha);
}
