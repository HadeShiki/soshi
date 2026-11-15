#pragma once

#include <cstdint>

extern "C" {

void soshi_core_world_reset(void);
void soshi_core_world_set_bounds(float x, float y, float width, float height);
void soshi_core_world_set_gravity(float gravity);
void soshi_core_world_set_restitution(float value);
void soshi_core_world_step(float dt);
std::uint32_t soshi_core_world_count(void);
std::uint32_t soshi_core_world_capacity(void);
std::uint32_t soshi_core_snapshot_stride(void);
std::uint32_t soshi_core_world_snapshot(float* out, std::uint32_t max_entities);

std::uint64_t soshi_core_entity_create(float x, float y, float velocity_x, float velocity_y, float radius,
                                       float red, float green, float blue, float alpha);
std::int32_t soshi_core_entity_destroy(std::uint64_t handle);
std::int32_t soshi_core_entity_position(std::uint64_t handle, float* out_xy);
std::int32_t soshi_core_entity_set_position(std::uint64_t handle, float x, float y);
std::int32_t soshi_core_entity_set_velocity(std::uint64_t handle, float velocity_x, float velocity_y);
std::int32_t soshi_core_entity_set_color(std::uint64_t handle, float red, float green, float blue, float alpha);
std::int32_t soshi_core_entity_set_rotation(std::uint64_t handle, float radians);
std::int32_t soshi_core_entity_set_parent(std::uint64_t handle, std::uint64_t parent_handle);
std::int32_t soshi_core_entity_set_sprite(std::uint64_t handle, std::uint32_t texture_index,
                                          std::uint32_t frame_width, std::uint32_t frame_height,
                                          std::uint32_t frame_count, std::uint32_t columns, float fps,
                                          std::int32_t loop);
std::int32_t soshi_core_entity_set_draw_size(std::uint64_t handle, float width, float height);
std::int32_t soshi_core_entity_set_frame(std::uint64_t handle, std::uint32_t frame);
std::int32_t soshi_core_entity_play_animation(std::uint64_t handle);
std::int32_t soshi_core_entity_stop_animation(std::uint64_t handle);
}
