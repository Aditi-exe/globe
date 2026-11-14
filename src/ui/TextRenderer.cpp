#include "ui/TextRenderer.h"

#include "render/Shader.h"

#include <glad/gl.h>
#include <stb_truetype.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace globe {

namespace {
constexpr int kFirstChar = 32;  // space
constexpr int kNumChars = 95;   // 32..126 inclusive
constexpr int kAtlasW = 512;
constexpr int kAtlasH = 512;

// Read a whole file as bytes; returns empty on failure.
std::vector<unsigned char> readBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return {};
    }
    std::streamsize n = f.tellg();
    if (n <= 0) {
        return {};
    }
    f.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(n));
    f.read(reinterpret_cast<char*>(bytes.data()), n);
    return bytes;
}
} // namespace

struct TextRenderer::Impl {
    bool ready = false;
    float pixelHeight = 18.0f;
    float baseline = 14.0f; // top-of-text -> baseline offset (~0.78 * height)
    unsigned int atlas = 0;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    std::unique_ptr<Shader> shader;
    stbtt_bakedchar cdata[kNumChars];

    ~Impl() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (atlas) glDeleteTextures(1, &atlas);
    }
};

TextRenderer::TextRenderer(const std::string& assetsDir, float pixelHeight)
    : impl_(std::make_unique<Impl>()) {
    impl_->pixelHeight = pixelHeight;
    impl_->baseline = pixelHeight * 0.78f;

    // Prefer the bundled JetBrains Mono (OFL, redistributable); ui.ttf is a
    // generic override slot; the Windows fonts are dev-only fallbacks (not
    // redistributable) for when the bundled asset isn't present.
    const std::string candidates[] = {
        assetsDir + "/fonts/JetBrainsMono-Regular.ttf",
        assetsDir + "/fonts/ui.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    std::vector<unsigned char> ttf;
    std::string used;
    for (const auto& path : candidates) {
        ttf = readBytes(path);
        if (!ttf.empty()) {
            used = path;
            break;
        }
    }
    if (ttf.empty()) {
        std::fprintf(stderr, "[ui] no usable font found; text disabled\n");
        return;
    }

    std::vector<unsigned char> bitmap(static_cast<std::size_t>(kAtlasW) * kAtlasH);
    int rows = stbtt_BakeFontBitmap(ttf.data(), 0, pixelHeight, bitmap.data(),
                                    kAtlasW, kAtlasH, kFirstChar, kNumChars,
                                    impl_->cdata);
    if (rows <= 0) {
        std::fprintf(stderr, "[ui] font bake failed for %s\n", used.c_str());
        return;
    }

    glGenTextures(1, &impl_->atlas);
    glBindTexture(GL_TEXTURE_2D, impl_->atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // single-channel rows aren't 4-aligned
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0, GL_RED,
                 GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    impl_->shader = std::make_unique<Shader>(assetsDir + "/shaders/ui_text.vert",
                                             assetsDir + "/shaders/ui_text.frag");

    glGenVertexArrays(1, &impl_->vao);
    glGenBuffers(1, &impl_->vbo);
    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    impl_->ready = true;
    std::fprintf(stderr, "[ui] font loaded: %s (%.0f px)\n", used.c_str(), pixelHeight);
}

TextRenderer::~TextRenderer() = default;

bool TextRenderer::ready() const { return impl_->ready; }
float TextRenderer::lineHeight() const { return impl_->pixelHeight; }

float TextRenderer::measure(const std::string& text) const {
    if (!impl_->ready) {
        return 0.0f;
    }
    float w = 0.0f;
    for (unsigned char c : text) {
        if (c < kFirstChar || c >= kFirstChar + kNumChars) {
            continue;
        }
        w += impl_->cdata[c - kFirstChar].xadvance;
    }
    return w;
}

void TextRenderer::draw(const glm::mat4& proj, float x, float y,
                        const std::string& text, const glm::vec4& color, float scale) const {
    if (!impl_->ready || text.empty()) {
        return;
    }

    // Lay the glyphs out in a local space anchored at (0,0) top-left, then scale
    // about that corner and translate to (x,y). This keeps scaling independent
    // of where on screen the text lands.
    float penX = 0.0f;
    float penY = impl_->baseline; // stb positions glyphs on the baseline
    std::vector<float> verts;
    verts.reserve(text.size() * 24); // 6 verts * 4 floats per glyph

    for (unsigned char c : text) {
        if (c < kFirstChar || c >= kFirstChar + kNumChars) {
            continue;
        }
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(impl_->cdata, kAtlasW, kAtlasH, c - kFirstChar,
                           &penX, &penY, &q, 1);
        const float x0 = x + q.x0 * scale;
        const float y0 = y + q.y0 * scale;
        const float x1 = x + q.x1 * scale;
        const float y1 = y + q.y1 * scale;
        const float quad[6][4] = {
            {x0, y0, q.s0, q.t0}, {x1, y0, q.s1, q.t0},
            {x1, y1, q.s1, q.t1}, {x0, y0, q.s0, q.t0},
            {x1, y1, q.s1, q.t1}, {x0, y1, q.s0, q.t1},
        };
        for (const auto& v : quad) {
            verts.insert(verts.end(), {v[0], v[1], v[2], v[3]});
        }
    }
    if (verts.empty()) {
        return;
    }

    impl_->shader->use();
    impl_->shader->setMat4("uProj", proj);
    impl_->shader->setVec4("uColor", color);
    impl_->shader->setInt("uAtlas", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->atlas);

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    glBindVertexArray(0);
}

} // namespace globe
