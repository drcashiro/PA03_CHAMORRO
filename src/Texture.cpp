#include "pa3/Texture.h"
#include <array>
#include <cstdint>
namespace pa3 {
Texture::Texture() {
    constexpr int size = 256;
    std::array<std::uint8_t, size*size*3> pixels{};
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const bool light = ((x/32 + y/32) % 2) == 0;
        const std::array<std::uint8_t,3> rgb = light
            ? std::array<std::uint8_t,3>{192,202,216} : std::array<std::uint8_t,3>{72,85,105};
        for (int c = 0; c < 3; ++c) pixels[static_cast<std::size_t>((y*size+x)*3+c)] = rgb[static_cast<std::size_t>(c)];
    }
    bind();
    GLint previous = 4; glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Hardware decodes sRGB texels to linear values before lighting.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
}
