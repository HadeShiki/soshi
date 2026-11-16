#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <vector>

namespace soshi {

// TTF/OTF fonts rasterized into a growing texture atlas. One slot per (file, pixel
// size); a glyph is rasterized the first time it is drawn, so a font costs almost
// nothing until used and the atlas only holds the glyphs a game actually shows.
//
// Text is drawn glyph by glyph with the font's own advances and kerning. It is not a
// full shaper: scripts that need reordering or mark stacking (Thai combining marks,
// Arabic, Indic) render each glyph in sequence.
class FontBank {
public:
    static constexpr int kCapacity = 16;

    FontBank() = default;
    ~FontBank();
    FontBank(const FontBank&) = delete;
    FontBank& operator=(const FontBank&) = delete;

    void reset();

    std::uint64_t load(SDL_Renderer* renderer, const char* path, float pixel_height);
    void destroy(std::uint64_t handle);

    bool measure(std::uint64_t handle, const char* text, float& width, float& height);
    bool draw(SDL_Renderer* renderer, std::uint64_t handle, const char* text, float x, float y, const float* rgba);
    float line_height(std::uint64_t handle);
    float ascent(std::uint64_t handle);

private:
    struct Glyph {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float xoff = 0.0f;  // quad offset from the pen position
        float yoff = 0.0f;  // ...and from the baseline (negative is up)
        float width = 0.0f;
        float height = 0.0f;
        float advance = 0.0f;
        bool empty = true;  // no pixels (space, control): advance only
    };

    struct Font;

    struct Slot {
        Font* font = nullptr;  // owned; Font is only complete inside font.cpp
        std::uint32_t generation = 1;
    };

    Font* find(std::uint64_t handle);
    float advance(const Font& font, std::uint32_t codepoint);
    float kerning(const Font& font, std::uint32_t previous, std::uint32_t codepoint);
    bool grow_atlas(SDL_Renderer* renderer, Font& font, int needed_width, int needed_height);
    bool reserve_region(SDL_Renderer* renderer, Font& font, int width, int height);
    const Glyph& glyph(SDL_Renderer* renderer, Font& font, std::uint32_t codepoint);

    std::array<Slot, kCapacity> slots_{};
    std::vector<SDL_Vertex> vertices_;
    std::vector<unsigned char> bitmap_;  // one glyph of coverage, reused for every glyph
};

}  // namespace soshi
