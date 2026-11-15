#pragma once

#include <SDL3/SDL.h>

#include "audio_bank.h"
#include "texture_bank.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace soshi {

inline constexpr int kFloatsPerRect = 8;
inline constexpr int kFloatsPerCircle = 7;
inline constexpr int kFloatsPerEntity = 16;
inline constexpr int kCircleSteps = 64;
inline constexpr int kTrackedKeys = 64;

class SdlContext {
public:
    bool open(int width, int height, std::string_view title);
    void close();

    bool running() const;

    void pump_events();
    bool key_pressed(int keycode) const;
    bool key_released(int keycode) const;
    void mouse_position(float& x, float& y) const;
    void window_size(float& width, float& height) const;

    void clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
    void present();
    void set_vsync(bool enabled);

    void draw_rects(std::span<const float> xywh_rgba);
    void draw_circles(std::span<const float> xy_radius_rgba);
    void draw_entities(std::span<const float> entities);

    std::uint64_t load_texture(const char* path);
    void destroy_texture(std::uint64_t handle);
    bool texture_size(std::uint64_t handle, float& width, float& height);

    std::uint64_t load_sound(const char* path);
    void destroy_sound(std::uint64_t handle);
    bool play_sound(std::uint64_t handle);
    void set_volume(float volume);
    bool audio_available();

    float tick(float target_fps);

    const char* last_error() const;

private:
    void note_key_down(std::uint32_t keycode);
    void note_key_up(std::uint32_t keycode);
    void add_vertex(float x, float y, const float* rgba, float u, float v);
    void add_triangle(float ax, float ay, float bx, float by, float cx, float cy, const float* rgba);
    void add_circle(float center_x, float center_y, float radius, const float* rgba);
    void draw_sprites_for(std::span<const float> entities, std::uint32_t texture_index);
    void flush_geometry(SDL_Texture* texture = nullptr);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::vector<SDL_Vertex> vertices_;
    std::array<SDL_FPoint, kCircleSteps + 1> circle_steps_{};
    std::array<std::uint32_t, kTrackedKeys> keys_down_{};
    std::uint32_t keys_down_count_ = 0;
    std::array<std::uint32_t, kTrackedKeys> keys_released_{};
    std::uint32_t keys_released_count_ = 0;
    std::uint64_t counter_frequency_ = 1;
    std::uint64_t last_counter_ = 0;
    bool running_ = false;

    TextureBank textures_;
    AudioBank audio_;
};

}  // namespace soshi
