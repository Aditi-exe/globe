#include "render/Texture.h"

#include <glad/gl.h>
#include <stb_image.h>

#include <cstdio>
#include <vector>

namespace globe {

namespace {

// 8x8 magenta/black checker used when the real texture can't be loaded.
void uploadCheckerboard() {
    const int n = 8;
    std::vector<unsigned char> px(static_cast<size_t>(n) * n * 3);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            bool on = ((x + y) & 1) != 0;
            size_t i = (static_cast<size_t>(y) * n + x) * 3;
            px[i + 0] = on ? 255 : 0;
            px[i + 1] = 0;
            px[i + 2] = on ? 255 : 0;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, n, n, 0, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

} // namespace

Texture::Texture(const std::string& path) {
    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Equirectangular maps have row 0 at the top (+90 lat); the sphere's UVs
    // account for that (see EarthSphere), so do NOT flip here.
    stbi_set_flip_vertically_on_load(0);
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 3);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        std::fprintf(stderr,
                     "[Texture] could not load '%s' (%s) - using placeholder checker.\n",
                     path.c_str(), stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        uploadCheckerboard();
    }
}

Texture::~Texture() {
    if (id_) {
        glDeleteTextures(1, &id_);
    }
}

void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

} // namespace globe
