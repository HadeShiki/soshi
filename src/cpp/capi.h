#pragma once

#include <cstdint>

#if defined(_WIN32)
#define SOSHI_API extern "C" __declspec(dllexport)
#else
#define SOSHI_API extern "C" __attribute__((visibility("default")))
#endif

SOSHI_API int soshi_init(int width, int height, const char* title);
SOSHI_API void soshi_quit(void);
SOSHI_API int soshi_is_running(void);
SOSHI_API void soshi_pump_events(void);

SOSHI_API void soshi_clear(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha);
SOSHI_API void soshi_present(void);
SOSHI_API void soshi_draw_rects(const float* xywh_rgba, int count);
SOSHI_API void soshi_draw_circles(const float* xy_radius_rgba, int count);
SOSHI_API void soshi_draw_world(void);
SOSHI_API void soshi_set_vsync(int enabled);
SOSHI_API int soshi_display_size(float* out_wh);

SOSHI_API int soshi_key_pressed(int keycode);
SOSHI_API int soshi_key_released(int keycode);
SOSHI_API int soshi_mouse_position(float* out_xy);
SOSHI_API float soshi_clock_tick(float target_fps);

SOSHI_API void soshi_world_set_bounds(float x, float y, float width, float height);
SOSHI_API void soshi_world_set_gravity(float gravity);
SOSHI_API void soshi_world_set_restitution(float value);
SOSHI_API void soshi_world_step(float dt);
SOSHI_API int soshi_world_count(void);
SOSHI_API int soshi_world_capacity(void);

SOSHI_API std::uint64_t soshi_entity_create(float x, float y, float velocity_x, float velocity_y, float radius,
                                            float red, float green, float blue, float alpha);
SOSHI_API int soshi_entity_destroy(std::uint64_t handle);
SOSHI_API int soshi_entity_position(std::uint64_t handle, float* out_xy);
SOSHI_API int soshi_entity_set_position(std::uint64_t handle, float x, float y);
SOSHI_API int soshi_entity_set_velocity(std::uint64_t handle, float velocity_x, float velocity_y);
SOSHI_API int soshi_entity_set_color(std::uint64_t handle, float red, float green, float blue, float alpha);
SOSHI_API int soshi_entity_set_rotation(std::uint64_t handle, float radians);
SOSHI_API int soshi_entity_set_parent(std::uint64_t handle, std::uint64_t parent);
SOSHI_API int soshi_entity_set_sprite(std::uint64_t handle, std::uint64_t texture, int frame_width, int frame_height,
                                      int frame_count, int columns, float fps, int loop);
SOSHI_API int soshi_entity_set_draw_size(std::uint64_t handle, float width, float height);
SOSHI_API int soshi_entity_set_frame(std::uint64_t handle, int frame);
SOSHI_API int soshi_entity_play_animation(std::uint64_t handle);
SOSHI_API int soshi_entity_stop_animation(std::uint64_t handle);

SOSHI_API std::uint64_t soshi_texture_load(const char* path);
SOSHI_API void soshi_texture_destroy(std::uint64_t handle);
SOSHI_API int soshi_texture_size(std::uint64_t handle, float* out_wh);

SOSHI_API std::uint64_t soshi_sound_load(const char* path);
SOSHI_API void soshi_sound_destroy(std::uint64_t handle);
SOSHI_API std::uint32_t soshi_sound_frames(std::uint64_t handle);
SOSHI_API std::uint64_t soshi_sound_play(std::uint64_t handle, float volume, float pan, float pitch, int loops,
                                         float fade_in);

SOSHI_API int soshi_voice_stop(std::uint64_t voice, float fade_seconds);
SOSHI_API int soshi_voice_set_volume(std::uint64_t voice, float volume, float fade_seconds);
SOSHI_API int soshi_voice_set_pan(std::uint64_t voice, float pan);
SOSHI_API int soshi_voice_set_pitch(std::uint64_t voice, float pitch);
SOSHI_API int soshi_voice_playing(std::uint64_t voice);
SOSHI_API float soshi_voice_progress(std::uint64_t voice);
SOSHI_API void soshi_audio_stop_all(float fade_seconds);
SOSHI_API void soshi_audio_pause(int paused);
SOSHI_API int soshi_audio_voice_count(void);
SOSHI_API int soshi_audio_voice_capacity(void);
SOSHI_API int soshi_audio_sample_rate(void);

SOSHI_API void soshi_audio_set_volume(float volume);
SOSHI_API int soshi_audio_available(void);

SOSHI_API const char* soshi_last_error(void);
