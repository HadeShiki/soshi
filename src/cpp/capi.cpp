#include "capi.h"

#include "capi_core.h"
#include "handles.h"
#include "sdl_context.h"

#include <array>
#include <cstdio>
#include <span>
#include <string_view>

namespace {

soshi::SdlContext context;
std::array<char, 256> error_message{};
bool world_ready = false;

// Entity snapshot staging: one buffer for every drawable, reused for the life of the process.
std::array<float, static_cast<std::size_t>(soshi::kFloatsPerEntity) * 8192> entity_primitives{};

int fail(const char* message) {
    std::snprintf(error_message.data(), error_message.size(), "%s", message);
    return -1;
}

int fail_with_sdl() {
    return fail(context.last_error());
}

int require_world() {
    return world_ready ? 0 : fail("call soshi.init() before touching the world");
}

int entity_result(std::int32_t status) {
    return status == 0 ? 0 : fail("entity handle is not alive");
}

bool usable(const float* data, int count) {
    return data != nullptr and count >= 0;
}

}  // namespace

int soshi_init(int width, int height, const char* title) {
    if (soshi_core_snapshot_stride() != static_cast<std::uint32_t>(soshi::kFloatsPerEntity)) {
        return fail("native snapshot stride does not match the renderer");
    }
    if (width <= 0 or height <= 0) {
        return fail("window size must be positive");
    }

    const std::string_view window_title = title != nullptr ? std::string_view{title} : std::string_view{"Soshi"};
    if (!context.open(width, height, window_title)) {
        return fail_with_sdl();
    }

    soshi_core_world_reset();
    soshi_core_world_set_bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    world_ready = true;
    return 0;
}

void soshi_quit(void) {
    context.close();
    world_ready = false;
}

int soshi_is_running(void) {
    return context.running() ? 1 : 0;
}

void soshi_pump_events(void) {
    context.pump_events();
}

void soshi_clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    context.clear(red, green, blue, alpha);
}

void soshi_present(void) {
    context.present();
}

void soshi_draw_rects(const float* xywh_rgba, int count) {
    if (!usable(xywh_rgba, count)) return;
    context.draw_rects(std::span<const float>{xywh_rgba, static_cast<std::size_t>(count) * soshi::kFloatsPerRect});
}

void soshi_draw_circles(const float* xy_radius_rgba, int count) {
    if (!usable(xy_radius_rgba, count)) return;
    context.draw_circles(
        std::span<const float>{xy_radius_rgba, static_cast<std::size_t>(count) * soshi::kFloatsPerCircle});
}

void soshi_draw_world(void) {
    const int capacity = soshi_world_capacity();
    const std::uint32_t drawn =
        soshi_core_world_snapshot(entity_primitives.data(), static_cast<std::uint32_t>(capacity));
    context.draw_entities(
        std::span<const float>{entity_primitives.data(), static_cast<std::size_t>(drawn) * soshi::kFloatsPerEntity});
}

void soshi_set_vsync(int enabled) {
    context.set_vsync(enabled != 0);
}

int soshi_display_size(float* out_wh) {
    if (out_wh == nullptr) return fail("missing output buffer");
    context.window_size(out_wh[0], out_wh[1]);
    return 0;
}

int soshi_key_pressed(int keycode) {
    return context.key_pressed(keycode) ? 1 : 0;
}

int soshi_key_released(int keycode) {
    return context.key_released(keycode) ? 1 : 0;
}

int soshi_mouse_position(float* out_xy) {
    if (out_xy == nullptr) return fail("missing output buffer");
    context.mouse_position(out_xy[0], out_xy[1]);
    return 0;
}

float soshi_clock_tick(float target_fps) {
    return context.tick(target_fps);
}

void soshi_world_set_bounds(float x, float y, float width, float height) {
    soshi_core_world_set_bounds(x, y, width, height);
}

void soshi_world_set_gravity(float gravity) {
    soshi_core_world_set_gravity(gravity);
}

void soshi_world_set_restitution(float value) {
    soshi_core_world_set_restitution(value);
}

void soshi_world_step(float dt) {
    if (require_world() != 0) return;
    soshi_core_world_step(dt);
}

int soshi_world_count(void) {
    return static_cast<int>(soshi_core_world_count());
}

int soshi_world_capacity(void) {
    return static_cast<int>(soshi_core_world_capacity());
}

std::uint64_t soshi_entity_create(float x, float y, float velocity_x, float velocity_y, float radius, float red,
                                  float green, float blue, float alpha) {
    if (require_world() != 0) return 0;
    return soshi_core_entity_create(x, y, velocity_x, velocity_y, radius, red, green, blue, alpha);
}

int soshi_entity_destroy(std::uint64_t handle) {
    if (require_world() != 0) return -1;
    if (soshi_core_entity_destroy(handle) != 0) return fail("entity handle is not alive");
    return 0;
}

int soshi_entity_position(std::uint64_t handle, float* out_xy) {
    if (out_xy == nullptr) return fail("missing output buffer");
    return entity_result(soshi_core_entity_position(handle, out_xy));
}

int soshi_entity_set_position(std::uint64_t handle, float x, float y) {
    return entity_result(soshi_core_entity_set_position(handle, x, y));
}

int soshi_entity_set_velocity(std::uint64_t handle, float velocity_x, float velocity_y) {
    return entity_result(soshi_core_entity_set_velocity(handle, velocity_x, velocity_y));
}

int soshi_entity_set_color(std::uint64_t handle, float red, float green, float blue, float alpha) {
    return entity_result(soshi_core_entity_set_color(handle, red, green, blue, alpha));
}

int soshi_entity_set_rotation(std::uint64_t handle, float radians) {
    return entity_result(soshi_core_entity_set_rotation(handle, radians));
}

int soshi_entity_set_parent(std::uint64_t handle, std::uint64_t parent) {
    if (soshi_core_entity_set_parent(handle, parent) != 0) {
        return fail("parent handle is not alive or would create a cycle");
    }
    return 0;
}

int soshi_entity_set_sprite(std::uint64_t handle, std::uint64_t texture, int frame_width, int frame_height,
                            int frame_count, int columns, float fps, int loop) {
    if (frame_width < 0 or frame_height < 0 or frame_count < 0 or columns < 0) {
        return fail("sprite frame sizes must not be negative");
    }
    return entity_result(soshi_core_entity_set_sprite(
        handle, soshi::handle_index(texture), static_cast<std::uint32_t>(frame_width),
        static_cast<std::uint32_t>(frame_height), static_cast<std::uint32_t>(frame_count),
        static_cast<std::uint32_t>(columns), fps, loop));
}

int soshi_entity_set_draw_size(std::uint64_t handle, float width, float height) {
    return entity_result(soshi_core_entity_set_draw_size(handle, width, height));
}

int soshi_entity_set_frame(std::uint64_t handle, int frame) {
    if (frame < 0) return fail("frame index must not be negative");
    return entity_result(soshi_core_entity_set_frame(handle, static_cast<std::uint32_t>(frame)));
}

int soshi_entity_play_animation(std::uint64_t handle) {
    return entity_result(soshi_core_entity_play_animation(handle));
}

int soshi_entity_stop_animation(std::uint64_t handle) {
    return entity_result(soshi_core_entity_stop_animation(handle));
}

std::uint64_t soshi_texture_load(const char* path) {
    if (path == nullptr) {
        fail("missing texture path");
        return 0;
    }
    const std::uint64_t handle = context.load_texture(path);
    if (handle == 0) fail_with_sdl();
    return handle;
}

void soshi_texture_destroy(std::uint64_t handle) {
    context.destroy_texture(handle);
}

int soshi_texture_size(std::uint64_t handle, float* out_wh) {
    if (out_wh == nullptr) return fail("missing output buffer");
    if (!context.texture_size(handle, out_wh[0], out_wh[1])) return fail("texture handle is not alive");
    return 0;
}

std::uint64_t soshi_sound_load(const char* path) {
    if (path == nullptr) {
        fail("missing sound path");
        return 0;
    }
    const std::uint64_t handle = context.load_sound(path);
    if (handle == 0) fail_with_sdl();
    return handle;
}

void soshi_sound_destroy(std::uint64_t handle) {
    context.destroy_sound(handle);
}

int soshi_sound_play(std::uint64_t handle) {
    if (!context.play_sound(handle)) return fail("sound handle is not alive or audio is unavailable");
    return 0;
}

void soshi_audio_set_volume(float volume) {
    context.set_volume(volume);
}

int soshi_audio_available(void) {
    return context.audio_available() ? 1 : 0;
}

const char* soshi_last_error(void) {
    return error_message.data();
}
