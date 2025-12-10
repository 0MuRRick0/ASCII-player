#version 430
layout(local_size_x = 8, local_size_y = 8) in;

layout(rgba8, binding = 0) readonly restrict uniform image2D inputImage;
layout(std430, binding = 1) writeonly restrict buffer OutputBuffer {
    uint data[];
};

uniform int outputWidth;
uniform int outputHeight;

const uint ASCII_CHARS[16] = uint[](
    0x20, 0x2E, 0x2C, 0x3A, 0x3B, 0x69, 0x31, 0x74,
    0x66, 0x4C, 0x43, 0x47, 0x30, 0x38, 0x40, 0x23
);

uint rgb_to_ansi(uint r, uint g, uint b) {
    if (r < 50u && g < 50u && b < 50u) return 0u;
    if (r > 200u && g > 200u && b > 200u) return 15u;
    if (r > 200u && g < 50u && b < 50u) return 9u;
    if (r < 50u && g > 200u && b < 50u) return 10u;
    if (r < 50u && g < 50u && b > 200u) return 12u;
    if (r > 200u && g > 200u && b < 50u) return 11u;
    if (r > 200u && g < 50u && b > 200u) return 13u;
    if (r < 50u && g > 200u && b > 200u) return 14u;
    if (r > 100u && g < 50u && b < 50u) return 1u;
    if (r < 50u && g > 100u && b < 50u) return 2u;
    if (r < 50u && g < 50u && b > 100u) return 4u;
    if (r > 100u && g > 100u && b < 50u) return 3u;
    if (r > 100u && g < 50u && b > 100u) return 5u;
    if (r < 50u && g > 100u && b > 100u) return 6u;
    if (r > 100u && g > 100u && b > 100u) return 7u;
    if (r < 100u && g < 100u && b < 100u) return 8u;

    uint ri = (r < 48u) ? 0u : (r < 115u) ? 1u : (r < 155u) ? 2u : (r < 195u) ? 3u : (r < 235u) ? 4u : 5u;
    uint gi = (g < 48u) ? 0u : (g < 115u) ? 1u : (g < 155u) ? 2u : (g < 195u) ? 3u : (g < 235u) ? 4u : 5u;
    uint bi = (b < 48u) ? 0u : (b < 115u) ? 1u : (b < 155u) ? 2u : (b < 195u) ? 3u : (b < 235u) ? 4u : 5u;
    return 16u + 36u*ri + 6u*gi + bi;
}

void main() {
    ivec2 outPos = ivec2(gl_GlobalInvocationID.xy);
    if (outPos.x >= outputWidth || outPos.y >= outputHeight) return;

    ivec2 imgSize = imageSize(inputImage);
    ivec2 srcPos = ivec2(
        (gl_GlobalInvocationID.x * imgSize.x) / outputWidth,
        (gl_GlobalInvocationID.y * imgSize.y) / outputHeight
    );
    srcPos = clamp(srcPos, ivec2(0), imgSize - 1);

    vec4 rgba = imageLoad(inputImage, srcPos);
    uint r = uint(rgba.r * 255.0);
    uint g = uint(rgba.g * 255.0);
    uint b = uint(rgba.b * 255.0);

    uint gray = (299u*r + 587u*g + 114u*b) / 1000u;
    gray = min(gray, 255u);
    uint asciiChar = ASCII_CHARS[(gray * 15u) / 255u];
    uint ansiColor = rgb_to_ansi(r, g, b);

    uint encoded = (ansiColor << 8) | asciiChar;
    data[outPos.y * outputWidth + outPos.x] = encoded;
}