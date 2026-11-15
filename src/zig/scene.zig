const ecs = @import("ecs.zig");
const math = @import("math.zig");

// Deeper chains are treated as broken or cyclic and stop resolving.
pub const max_depth: u32 = 8;

pub fn resolve(table: *ecs.Table) void {
    // Roots resolve inline: only attached entities need the chain walk, and most
    // scenes have none at all.
    for (table.for_each(), 0..) |*slot, index| {
        if (!slot.alive) continue;
        if (slot.parent == ecs.parent_none) {
            slot.world_rotation = slot.rotation;
            continue;
        }
        resolve_chain(table, @intCast(index), max_depth);
    }
}

fn resolve_chain(table: *ecs.Table, index: u32, depth: u32) void {
    const slot = &table.slots[index];
    if (!slot.alive) return;

    if (slot.parent == ecs.parent_none or depth == 0) {
        slot.world_rotation = slot.rotation;
        return;
    }

    resolve_chain(table, slot.parent, depth - 1);

    const parent = &table.slots[slot.parent];
    if (!parent.alive) {
        slot.parent = ecs.parent_none;
        slot.world_rotation = slot.rotation;
        return;
    }

    slot.position = parent.position.add(rotate(parent.world_rotation, slot.local_position));
    slot.world_rotation = parent.world_rotation + slot.rotation;
}

pub fn rotate(radians: f32, value: math.Vec2) math.Vec2 {
    if (radians == 0) return value;

    const cosine = @cos(radians);
    const sine = @sin(radians);
    return .{
        .x = value.x * cosine - value.y * sine,
        .y = value.x * sine + value.y * cosine,
    };
}
