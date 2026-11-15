const ecs = @import("ecs.zig");
const math = @import("math.zig");
const scene = @import("scene.zig");
const world_module = @import("world.zig");

// Single engine world per process: the engine is designed as one game loop, one world.
var world: world_module.World = .{};

fn slot_of(handle: u64) ?*ecs.Slot {
    return world.table.find(handle);
}

fn parent_would_cycle(child_index: u32, parent_index: u32) bool {
    if (child_index == parent_index) return true;

    var current = parent_index;
    var depth: u32 = 0;
    while (depth < scene.max_depth) : (depth += 1) {
        const parent = world.table.slots[current].parent;
        if (parent == ecs.parent_none) return false;
        if (parent == child_index) return true;
        current = parent;
    }

    return true;
}

export fn soshi_core_world_reset() void {
    world.reset();
}

export fn soshi_core_world_set_bounds(x: f32, y: f32, width: f32, height: f32) void {
    world.settings.bounds = .{ .x = x, .y = y, .width = width, .height = height };
}

export fn soshi_core_world_set_gravity(gravity: f32) void {
    world.settings.gravity = gravity;
}

export fn soshi_core_world_set_restitution(value: f32) void {
    world.settings.restitution = value;
}

export fn soshi_core_world_step(dt: f32) void {
    world.step(dt);
}

export fn soshi_core_world_count() u32 {
    return world.table.count();
}

export fn soshi_core_world_capacity() u32 {
    return ecs.capacity;
}

export fn soshi_core_snapshot_stride() u32 {
    return world_module.floats_per_entity;
}

export fn soshi_core_world_snapshot(out: [*]f32, max_entities: u32) u32 {
    return world.snapshot_into(out, max_entities);
}

export fn soshi_core_entity_create(
    x: f32,
    y: f32,
    velocity_x: f32,
    velocity_y: f32,
    radius: f32,
    red: f32,
    green: f32,
    blue: f32,
    alpha: f32,
) u64 {
    const position = math.Vec2{ .x = x, .y = y };
    const velocity = math.Vec2{ .x = velocity_x, .y = velocity_y };
    return world.table.create(position, velocity, radius, .{ red, green, blue, alpha });
}

export fn soshi_core_entity_destroy(handle: u64) i32 {
    return if (world.table.destroy(handle)) 0 else -1;
}

export fn soshi_core_entity_position(handle: u64, out: [*]f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    out[0] = slot.position.x;
    out[1] = slot.position.y;
    return 0;
}

export fn soshi_core_entity_set_position(handle: u64, x: f32, y: f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.position = .{ .x = x, .y = y };
    slot.local_position = .{ .x = x, .y = y };
    return 0;
}

export fn soshi_core_entity_set_velocity(handle: u64, velocity_x: f32, velocity_y: f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.velocity = .{ .x = velocity_x, .y = velocity_y };
    return 0;
}

export fn soshi_core_entity_set_color(handle: u64, red: f32, green: f32, blue: f32, alpha: f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.color = .{ red, green, blue, alpha };
    return 0;
}

export fn soshi_core_entity_set_rotation(handle: u64, radians: f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.rotation = radians;
    return 0;
}

export fn soshi_core_entity_set_parent(handle: u64, parent_handle: u64) i32 {
    const slot = slot_of(handle) orelse return -1;

    if (parent_handle == ecs.handle_none) {
        slot.parent = ecs.parent_none;
        return 0;
    }

    const parent_slot = slot_of(parent_handle) orelse return -1;
    if (parent_would_cycle(ecs.index_of(handle), ecs.index_of(parent_handle))) return -1;

    slot.parent = ecs.index_of(parent_handle);
    slot.local_position = slot.position.sub(parent_slot.position);
    return 0;
}

export fn soshi_core_entity_set_sprite(
    handle: u64,
    texture_index: u32,
    frame_width: u32,
    frame_height: u32,
    frame_count: u32,
    columns: u32,
    fps: f32,
    loop: i32,
) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.texture = texture_index;
    slot.animation.configure(frame_width, frame_height, frame_count, columns, fps, loop != 0);
    return 0;
}

export fn soshi_core_entity_set_draw_size(handle: u64, width: f32, height: f32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.draw_size = .{ .x = width, .y = height };
    return 0;
}

export fn soshi_core_entity_set_frame(handle: u64, frame: u32) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.animation.playing = false;
    slot.animation.index = @floatFromInt(frame);
    return 0;
}

export fn soshi_core_entity_play_animation(handle: u64) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.animation.play();
    return 0;
}

export fn soshi_core_entity_stop_animation(handle: u64) i32 {
    const slot = slot_of(handle) orelse return -1;
    slot.animation.stop();
    return 0;
}
