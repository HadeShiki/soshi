#include "font.h"

#include "handles.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>

namespace soshi {
namespace {

constexpr int kInitialAtlas = 256;
constexpr int kMaxAtlas = 4096;
constexpr float kMaxPixelHeight = 512.0f;
constexpr int kFrameBytes = static_cast<int>(sizeof(std::uint32_t));

// Decodes one UTF-8 codepoint and advances the cursor. A broken sequence becomes
// U+FFFD instead of derailing the rest of the string.
std::uint32_t next_codepoint(const unsigned char*& cursor) {
    const unsigned char lead = *cursor++;

    if (lead < 0x80) return lead;

    int extra = 0;
    std::uint32_t value = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        value = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        value = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        value = lead & 0x07u;
    } else {
        return 0xFFFD;
    }

    for (int index = 0; index < extra; ++index) {
        if ((*cursor & 0xC0) != 0x80) return 0xFFFD;
        value = (value << 6) | (*cursor++ & 0x3Fu);
    }
    return value;
}

}  // namespace

struct FontBank::Font {
    ~Font() {
        if (atlas != nullptr) SDL_DestroyTexture(atlas);
        SDL_free(data);
    }

    unsigned char* data = nullptr;  // the whole font file; stb keeps pointers into it
    stbtt_fontinfo info{};
    float scale = 1.0f;
    float ascent = 0.0f;
    float line_height = 0.0f;

    SDL_Texture* atlas = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    std::vector<std::uint32_t> pixels;  // ARGB8888 staging buffer, mirrored to the atlas
    int pen_x = 0;
    int pen_y = 0;
    int row_height = 0;

    std::unordered_map<std::uint32_t, Glyph> glyphs;
};

FontBank::~FontBank() {
    for (Slot& slot : slots_) delete slot.font;
}

void FontBank::reset() {
    for (Slot& slot : slots_) {
        delete slot.font;
        slot.font = nullptr;
        ++slot.generation;
    }
    vertices_.clear();
}

FontBank::Font* FontBank::find(std::uint64_t handle) {
    const std::uint32_t index = handle_index(handle);
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kCapacity)) return nullptr;

    Slot& slot = slots_[index];
    if (slot.font == nullptr or slot.generation != handle_generation(handle)) return nullptr;
    return slot.font;
}

std::uint64_t FontBank::load(SDL_Renderer* renderer, const char* path, float pixel_height) {
    if (renderer == nullptr or path == nullptr) return 0;
    if (!(pixel_height >= 1.0f) or pixel_height > kMaxPixelHeight) {
        SDL_SetError("font size must be between 1 and %.0f pixels", kMaxPixelHeight);
        return 0;
    }

    std::size_t size = 0;
    void* file = SDL_LoadFile(path, &size);
    if (file == nullptr) return 0;

    if (size == 0 or size > 64u * 1024u * 1024u) {
        SDL_free(file);
        SDL_SetError("font file is empty or too large: %s", path);
        return 0;
    }

    // The font takes ownership of the file buffer: stb reads glyph outlines from it
    // for as long as the font is alive, so there is no point copying it.
    auto font = std::make_unique<Font>();
    font->data = static_cast<unsigned char*>(file);

    // Index 0 of a TTC, or the only font of a TTF/OTF.
    const int offset = stbtt_GetFontOffsetForIndex(font->data, 0);
    if (offset < 0 or !stbtt_InitFont(&font->info, font->data, offset)) {
        SDL_SetError("cannot parse font: %s", path);
        return 0;
    }

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);

    font->scale = stbtt_ScaleForPixelHeight(&font->info, pixel_height);
    font->ascent = static_cast<float>(ascent) * font->scale;
    // Never tighter than the glyph box itself, whatever the font's line gap says,
    // or stacked lines of tall fonts collide.
    font->line_height = std::max(static_cast<float>(ascent - descent + line_gap) * font->scale,
                                 static_cast<float>(ascent - descent) * font->scale);

    font->atlas_width = kInitialAtlas;
    font->atlas_height = kInitialAtlas;
    font->pixels.assign(static_cast<std::size_t>(kInitialAtlas) * kInitialAtlas, 0u);
    font->atlas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kInitialAtlas,
                                    kInitialAtlas);
    if (font->atlas == nullptr) return 0;

    SDL_SetTextureBlendMode(font->atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(font->atlas, SDL_SCALEMODE_NEAREST);

    for (std::uint32_t index = kFirstHandleIndex; index < static_cast<std::uint32_t>(kCapacity); ++index) {
        if (slots_[index].font != nullptr) continue;

        slots_[index].font = font.release();
        return handle_of(index, slots_[index].generation);
    }

    SDL_SetError("font pool is full (%d fonts)", kCapacity);
    return 0;
}

void FontBank::destroy(std::uint64_t handle) {
    const std::uint32_t index = handle_index(handle);
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kCapacity)) return;

    Slot& slot = slots_[index];
    if (slot.font == nullptr or slot.generation != handle_generation(handle)) return;

    delete slot.font;
    slot.font = nullptr;
    ++slot.generation;
}

float FontBank::line_height(std::uint64_t handle) {
    Font* font = find(handle);
    return font != nullptr ? font->line_height : 0.0f;
}

float FontBank::ascent(std::uint64_t handle) {
    Font* font = find(handle);
    return font != nullptr ? font->ascent : 0.0f;
}

// The one place that knows how wide a codepoint is. Control characters take no
// space; the Glyph caches this so drawing never asks the font twice.
float FontBank::advance(const Font& font, std::uint32_t codepoint) {
    if (codepoint < 0x20) return 0.0f;

    int width = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&font.info, static_cast<int>(codepoint), &width, &bearing);
    return static_cast<float>(width) * font.scale;
}

// Extra space the font asks for between two codepoints (0 when it has no kern table).
float FontBank::kerning(const Font& font, std::uint32_t previous, std::uint32_t codepoint) {
    if (previous < 0x20 or codepoint < 0x20) return 0.0f;
    return static_cast<float>(stbtt_GetCodepointKernAdvance(&font.info, static_cast<int>(previous),
                                                            static_cast<int>(codepoint))) *
           font.scale;
}

bool FontBank::grow_atlas(SDL_Renderer* renderer, Font& font, int needed_width, int needed_height) {
    int size = std::max(font.atlas_width, kInitialAtlas);
    while (size < needed_width or size < needed_height) {
        size *= 2;
        if (size > kMaxAtlas) {
            SDL_SetError("font atlas is full at %d px", kMaxAtlas);
            return false;
        }
    }
    if (size == font.atlas_width) return true;

    // Same coordinates, bigger canvas: existing glyphs stay where they were packed.
    std::vector<std::uint32_t> grown(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 0u);
    for (int row = 0; row < font.atlas_height; ++row) {
        std::memcpy(&grown[static_cast<std::size_t>(row) * size],
                    &font.pixels[static_cast<std::size_t>(row) * font.atlas_width],
                    static_cast<std::size_t>(font.atlas_width) * sizeof(std::uint32_t));
    }

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size, size);
    if (texture == nullptr) return false;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    if (!SDL_UpdateTexture(texture, nullptr, grown.data(), size * kFrameBytes)) {
        SDL_DestroyTexture(texture);
        return false;
    }

    if (font.atlas != nullptr) SDL_DestroyTexture(font.atlas);
    font.atlas = texture;
    font.atlas_width = size;
    font.atlas_height = size;
    font.pixels = std::move(grown);
    return true;
}

// Shelf packing: left to right, wrapping to a new row when the atlas width runs out.
bool FontBank::reserve_region(SDL_Renderer* renderer, Font& font, int width, int height) {
    if (font.pen_x + width > font.atlas_width) {
        font.pen_x = 0;
        font.pen_y += font.row_height + 1;
        font.row_height = 0;
    }
    if (font.pen_y + height <= font.atlas_height) return true;
    return grow_atlas(renderer, font, font.pen_x + width, font.pen_y + height);
}

const FontBank::Glyph& FontBank::glyph(SDL_Renderer* renderer, Font& font, std::uint32_t codepoint) {
    const auto cached = font.glyphs.find(codepoint);
    if (cached != font.glyphs.end()) return cached->second;

    Glyph entry;
    entry.advance = advance(font, codepoint);

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    stbtt_GetCodepointBitmapBox(&font.info, static_cast<int>(codepoint), font.scale, font.scale, &x0, &y0, &x1, &y1);

    const int width = x1 - x0;
    const int height = y1 - y0;

    // Whitespace and control characters have no pixels: the entry stays "empty" and
    // only its advance is used.
    if (width > 0 and height > 0 and reserve_region(renderer, font, width, height)) {
        bitmap_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
        stbtt_MakeCodepointBitmap(&font.info, bitmap_.data(), width, height, width, font.scale, font.scale,
                                  static_cast<int>(codepoint));

        for (int row = 0; row < height; ++row) {
            const unsigned char* source = bitmap_.data() + static_cast<std::size_t>(row) * width;
            std::uint32_t* destination =
                &font.pixels[static_cast<std::size_t>(font.pen_y + row) * font.atlas_width + font.pen_x];
            for (int column = 0; column < width; ++column) {
                // White glyph, coverage in the alpha channel: the vertex color tints it.
                destination[column] = (static_cast<std::uint32_t>(source[column]) << 24) | 0x00FFFFFFu;
            }
        }

        const SDL_Rect region{font.pen_x, font.pen_y, width, height};
        const std::uint32_t* placed =
            &font.pixels[static_cast<std::size_t>(font.pen_y) * font.atlas_width + font.pen_x];

        // Uploading from the staging buffer with the atlas pitch means no second copy.
        if (SDL_UpdateTexture(font.atlas, &region, placed, font.atlas_width * kFrameBytes)) {
            entry.u0 = static_cast<float>(font.pen_x) / static_cast<float>(font.atlas_width);
            entry.v0 = static_cast<float>(font.pen_y) / static_cast<float>(font.atlas_height);
            entry.u1 = static_cast<float>(font.pen_x + width) / static_cast<float>(font.atlas_width);
            entry.v1 = static_cast<float>(font.pen_y + height) / static_cast<float>(font.atlas_height);
            entry.xoff = static_cast<float>(x0);
            entry.yoff = static_cast<float>(y0);
            entry.width = static_cast<float>(width);
            entry.height = static_cast<float>(height);
            entry.empty = false;

            font.pen_x += width + 1;
            font.row_height = std::max(font.row_height, height);
        }
    }

    return font.glyphs.emplace(codepoint, entry).first->second;
}

bool FontBank::measure(std::uint64_t handle, const char* text, float& width, float& height) {
    Font* font = find(handle);
    if (font == nullptr or text == nullptr) return false;

    float widest = 0.0f;
    float pen_x = 0.0f;
    int lines = 1;
    std::uint32_t previous = 0;

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != 0) {
        const std::uint32_t codepoint = next_codepoint(cursor);

        if (codepoint == '\n') {
            widest = std::max(widest, pen_x);
            pen_x = 0.0f;
            previous = 0;
            ++lines;
            continue;
        }

        pen_x += advance(*font, codepoint) + kerning(*font, previous, codepoint);
        previous = codepoint;
    }

    width = std::max(widest, pen_x);
    height = static_cast<float>(lines) * font->line_height;
    return true;
}

bool FontBank::draw(SDL_Renderer* renderer, std::uint64_t handle, const char* text, float x, float y,
                    const float* rgba) {
    Font* font = find(handle);
    if (font == nullptr or text == nullptr or renderer == nullptr) return false;

    vertices_.clear();
    float pen_x = x;
    float baseline = y + font->ascent;
    std::uint32_t previous = 0;

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != 0) {
        const std::uint32_t codepoint = next_codepoint(cursor);

        if (codepoint == '\n') {
            pen_x = x;
            baseline += font->line_height;
            previous = 0;
            continue;
        }

        if (codepoint < 0x20) {
            previous = 0;  // stray control characters (\r, \t, ...) are dropped, not drawn
            continue;
        }

        // Kerning shifts this glyph relative to the previous advance, before it is placed.
        pen_x += kerning(*font, previous, codepoint);

        const Glyph& entry = glyph(renderer, *font, codepoint);
        if (!entry.empty) {
            const float left = pen_x + entry.xoff;
            const float top = baseline + entry.yoff;
            const float right = left + entry.width;
            const float bottom = top + entry.height;

            const SDL_FColor color{rgba[0], rgba[1], rgba[2], rgba[3]};
            const SDL_FPoint top_left{left, top};
            const SDL_FPoint top_right{right, top};
            const SDL_FPoint bottom_right{right, bottom};
            const SDL_FPoint bottom_left{left, bottom};

            vertices_.push_back(SDL_Vertex{top_left, color, SDL_FPoint{entry.u0, entry.v0}});
            vertices_.push_back(SDL_Vertex{top_right, color, SDL_FPoint{entry.u1, entry.v0}});
            vertices_.push_back(SDL_Vertex{bottom_right, color, SDL_FPoint{entry.u1, entry.v1}});
            vertices_.push_back(SDL_Vertex{top_left, color, SDL_FPoint{entry.u0, entry.v0}});
            vertices_.push_back(SDL_Vertex{bottom_right, color, SDL_FPoint{entry.u1, entry.v1}});
            vertices_.push_back(SDL_Vertex{bottom_left, color, SDL_FPoint{entry.u0, entry.v1}});
        }

        pen_x += entry.advance;
        previous = codepoint;
    }

    if (!vertices_.empty()) {
        // One geometry call per string, with the atlas as the texture. Glyphs added
        // above may have grown the atlas; the final texture is the one used here.
        SDL_RenderGeometry(renderer, font->atlas, vertices_.data(), static_cast<int>(vertices_.size()), nullptr, 0);
    }
    return true;
}

}  // namespace soshi
