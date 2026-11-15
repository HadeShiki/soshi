#include "sdl_context.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace soshi {
namespace {

constexpr float kRadiansPerTurn = 6.28318530718f;
constexpr float kMaxFrameTime = 0.25f;
constexpr int kMinCircleSegments = 12;
constexpr int kMaxCircleSegments = 64;
constexpr std::size_t kTexturesPerFrame = 32;

int segments_for(float radius) {
    const int wanted = static_cast<int>(radius * 0.7f);
    return std::clamp(wanted, kMinCircleSegments, kMaxCircleSegments);
}

bool list_contains(const std::array<std::uint32_t, kTrackedKeys>& values, std::uint32_t count,
                   std::uint32_t keycode) {
    return std::find(values.begin(), values.begin() + count, keycode) != values.begin() + count;
}

bool texture_seen(const std::array<std::uint32_t, kTexturesPerFrame>& values, std::size_t count,
                  std::uint32_t texture_index) {
    return std::find(values.begin(), values.begin() + count, texture_index) != values.begin() + count;
}

SDL_FPoint rotate_point(float local_x, float local_y, float cosine, float sine, float center_x, float center_y) {
    return SDL_FPoint{center_x + local_x * cosine - local_y * sine, center_y + local_x * sine + local_y * cosine};
}

}  // namespace

bool SdlContext::open(int width, int height, std::string_view title) {
    if (window_ != nullptr) {
        return true;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return false;
    }

    const std::string title_text{title};
    window_ = SDL_CreateWindow(title_text.c_str(), width, height, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        renderer_ = SDL_CreateRenderer(window_, "software");
    }
    if (renderer_ == nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    SDL_SetRenderVSync(renderer_, 1);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    for (int step = 0; step <= kCircleSteps; ++step) {
        const float turn = static_cast<float>(step) / kCircleSteps * kRadiansPerTurn;
        circle_steps_[step] = SDL_FPoint{std::cos(turn), std::sin(turn)};
    }

    vertices_.reserve(1u << 16);
    counter_frequency_ = SDL_GetPerformanceFrequency();
    last_counter_ = SDL_GetPerformanceCounter();
    running_ = true;
    return true;
}

void SdlContext::close() {
    audio_.shutdown();
    textures_.reset();

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();

    vertices_.clear();
    keys_down_count_ = 0;
    keys_released_count_ = 0;
    running_ = false;
}

bool SdlContext::running() const {
    return running_;
}

void SdlContext::pump_events() {
    keys_released_count_ = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                note_key_down(static_cast<std::uint32_t>(event.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                note_key_up(static_cast<std::uint32_t>(event.key.key));
                break;
            default:
                break;
        }
    }
}

void SdlContext::note_key_down(std::uint32_t keycode) {
    if (keys_down_count_ == kTrackedKeys or list_contains(keys_down_, keys_down_count_, keycode)) {
        return;
    }
    keys_down_[keys_down_count_++] = keycode;
}

void SdlContext::note_key_up(std::uint32_t keycode) {
    const auto begin = keys_down_.begin();
    const auto end = begin + keys_down_count_;
    const auto found = std::find(begin, end, keycode);
    if (found != end) {
        std::rotate(found, found + 1, end);
        --keys_down_count_;
    }

    if (keys_released_count_ == kTrackedKeys or list_contains(keys_released_, keys_released_count_, keycode)) {
        return;
    }
    keys_released_[keys_released_count_++] = keycode;
}

bool SdlContext::key_pressed(int keycode) const {
    const auto key = static_cast<std::uint32_t>(keycode);
    return list_contains(keys_down_, keys_down_count_, key);
}

bool SdlContext::key_released(int keycode) const {
    const auto key = static_cast<std::uint32_t>(keycode);
    return list_contains(keys_released_, keys_released_count_, key);
}

void SdlContext::mouse_position(float& x, float& y) const {
    SDL_GetMouseState(&x, &y);
}

void SdlContext::window_size(float& width, float& height) const {
    int pixel_width = 0;
    int pixel_height = 0;
    if (window_ != nullptr) {
        SDL_GetWindowSize(window_, &pixel_width, &pixel_height);
    }
    width = static_cast<float>(pixel_width);
    height = static_cast<float>(pixel_height);
}

void SdlContext::clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    if (renderer_ == nullptr) return;
    SDL_SetRenderDrawColor(renderer_, red, green, blue, alpha);
    SDL_RenderClear(renderer_);
}

void SdlContext::present() {
    if (renderer_ == nullptr) return;
    SDL_RenderPresent(renderer_);
}

void SdlContext::set_vsync(bool enabled) {
    if (renderer_ == nullptr) return;
    SDL_SetRenderVSync(renderer_, enabled ? 1 : 0);
}

void SdlContext::add_vertex(float x, float y, const float* rgba, float u, float v) {
    const SDL_FColor color{rgba[0], rgba[1], rgba[2], rgba[3]};
    vertices_.push_back(SDL_Vertex{SDL_FPoint{x, y}, color, SDL_FPoint{u, v}});
}

void SdlContext::add_triangle(float ax, float ay, float bx, float by, float cx, float cy, const float* rgba) {
    add_vertex(ax, ay, rgba, 0, 0);
    add_vertex(bx, by, rgba, 0, 0);
    add_vertex(cx, cy, rgba, 0, 0);
}

void SdlContext::add_circle(float center_x, float center_y, float radius, const float* rgba) {
    if (radius <= 0) return;

    const int segments = segments_for(radius);
    for (int segment = 0; segment < segments; ++segment) {
        const SDL_FPoint from = circle_steps_[segment * kCircleSteps / segments];
        const SDL_FPoint to = circle_steps_[(segment + 1) * kCircleSteps / segments];
        add_triangle(center_x, center_y, center_x + from.x * radius, center_y + from.y * radius,
                     center_x + to.x * radius, center_y + to.y * radius, rgba);
    }
}

void SdlContext::flush_geometry(SDL_Texture* texture) {
    if (renderer_ == nullptr or vertices_.empty()) {
        vertices_.clear();
        return;
    }
    SDL_RenderGeometry(renderer_, texture, vertices_.data(), static_cast<int>(vertices_.size()), nullptr, 0);
    vertices_.clear();
}

void SdlContext::draw_rects(std::span<const float> xywh_rgba) {
    vertices_.clear();

    const std::size_t count = xywh_rgba.size() / kFloatsPerRect;
    for (std::size_t index = 0; index < count; ++index) {
        const float* item = xywh_rgba.data() + index * kFloatsPerRect;
        const float left = item[0];
        const float top = item[1];
        const float right = left + item[2];
        const float bottom = top + item[3];
        const float* rgba = item + 4;

        add_triangle(left, top, right, top, right, bottom, rgba);
        add_triangle(left, top, right, bottom, left, bottom, rgba);
    }

    flush_geometry();
}

void SdlContext::draw_circles(std::span<const float> xy_radius_rgba) {
    vertices_.clear();

    const std::size_t count = xy_radius_rgba.size() / kFloatsPerCircle;
    for (std::size_t index = 0; index < count; ++index) {
        const float* item = xy_radius_rgba.data() + index * kFloatsPerCircle;
        add_circle(item[0], item[1], item[2], item + 3);
    }

    flush_geometry();
}

void SdlContext::draw_entities(std::span<const float> entities) {
    if (renderer_ == nullptr) return;

    const std::size_t count = entities.size() / kFloatsPerEntity;

    vertices_.clear();
    for (std::size_t index = 0; index < count; ++index) {
        const float* entity = entities.data() + index * kFloatsPerEntity;
        if (entity[15] != 0.0f) continue;
        add_circle(entity[0], entity[1], entity[2], entity + 3);
    }
    flush_geometry();

    std::array<std::uint32_t, kTexturesPerFrame> drawn{};
    std::size_t drawn_count = 0;

    for (std::size_t index = 0; index < count; ++index) {
        const float* entity = entities.data() + index * kFloatsPerEntity;
        if (entity[15] == 0.0f) continue;

        const auto texture_index = static_cast<std::uint32_t>(entity[8]);
        if (texture_seen(drawn, drawn_count, texture_index)) continue;
        if (drawn_count == drawn.size()) break;

        drawn[drawn_count++] = texture_index;
        draw_sprites_for(entities, texture_index);
    }
}

void SdlContext::draw_sprites_for(std::span<const float> entities, std::uint32_t texture_index) {
    SDL_Texture* texture = textures_.raw(texture_index);
    if (texture == nullptr) return;

    float texture_width = 0;
    float texture_height = 0;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height)) return;
    if (texture_width <= 0 or texture_height <= 0) return;

    const std::size_t count = entities.size() / kFloatsPerEntity;
    vertices_.clear();

    for (std::size_t index = 0; index < count; ++index) {
        const float* entity = entities.data() + index * kFloatsPerEntity;
        if (entity[15] == 0.0f or static_cast<std::uint32_t>(entity[8]) != texture_index) continue;

        const float width = entity[13] > 0 ? entity[13] : entity[2] * 2;
        const float height = entity[14] > 0 ? entity[14] : entity[2] * 2;
        if (width <= 0 or height <= 0) continue;

        float source_x = entity[9];
        float source_y = entity[10];
        float source_width = entity[11];
        float source_height = entity[12];
        if (source_width <= 0 or source_height <= 0) {
            source_x = 0;
            source_y = 0;
            source_width = texture_width;
            source_height = texture_height;
        }

        const float u0 = source_x / texture_width;
        const float v0 = source_y / texture_height;
        const float u1 = (source_x + source_width) / texture_width;
        const float v1 = (source_y + source_height) / texture_height;

        const float half_width = width * 0.5f;
        const float half_height = height * 0.5f;
        const float cosine = std::cos(entity[7]);
        const float sine = std::sin(entity[7]);
        const float* rgba = entity + 3;

        const SDL_FPoint top_left = rotate_point(-half_width, -half_height, cosine, sine, entity[0], entity[1]);
        const SDL_FPoint top_right = rotate_point(half_width, -half_height, cosine, sine, entity[0], entity[1]);
        const SDL_FPoint bottom_right = rotate_point(half_width, half_height, cosine, sine, entity[0], entity[1]);
        const SDL_FPoint bottom_left = rotate_point(-half_width, half_height, cosine, sine, entity[0], entity[1]);

        add_vertex(top_left.x, top_left.y, rgba, u0, v0);
        add_vertex(top_right.x, top_right.y, rgba, u1, v0);
        add_vertex(bottom_right.x, bottom_right.y, rgba, u1, v1);
        add_vertex(top_left.x, top_left.y, rgba, u0, v0);
        add_vertex(bottom_right.x, bottom_right.y, rgba, u1, v1);
        add_vertex(bottom_left.x, bottom_left.y, rgba, u0, v1);
    }

    flush_geometry(texture);
}

std::uint64_t SdlContext::load_texture(const char* path) {
    return textures_.load(renderer_, path);
}

void SdlContext::destroy_texture(std::uint64_t handle) {
    textures_.destroy(handle);
}

bool SdlContext::texture_size(std::uint64_t handle, float& width, float& height) {
    return textures_.size_of(handle, width, height);
}

std::uint64_t SdlContext::load_sound(const char* path) {
    return audio_.load(path);
}

void SdlContext::destroy_sound(std::uint64_t handle) {
    audio_.destroy(handle);
}

bool SdlContext::play_sound(std::uint64_t handle) {
    return audio_.play(handle);
}

void SdlContext::set_volume(float volume) {
    audio_.set_volume(volume);
}

bool SdlContext::audio_available() {
    return audio_.available();
}

float SdlContext::tick(float target_fps) {
    const std::uint64_t now = SDL_GetPerformanceCounter();
    float elapsed = static_cast<float>(now - last_counter_) / static_cast<float>(counter_frequency_);
    last_counter_ = now;

    if (elapsed > kMaxFrameTime) {
        elapsed = kMaxFrameTime;
    }
    if (target_fps <= 0) {
        return elapsed;
    }

    const float frame_time = 1.0f / target_fps;
    if (elapsed < frame_time) {
        const auto remaining_ns = static_cast<std::uint64_t>((frame_time - elapsed) * 1e9f);
        SDL_DelayPrecise(remaining_ns);
        elapsed = frame_time;
    }
    return elapsed;
}

const char* SdlContext::last_error() const {
    return SDL_GetError();
}

}  // namespace soshi
