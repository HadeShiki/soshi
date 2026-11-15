const ecs = @import("ecs.zig");
const physics = @import("physics.zig");
const scene = @import("scene.zig");

pub const floats_per_entity: u32 = 16;

pub const World = struct {
    table: ecs.Table = .{},
    broad: physics.BroadPhase = .{},
    settings: physics.Settings = .{},

    pub fn reset(self: *World) void {
        self.table.reset();
    }

    pub fn step(self: *World, dt: f32) void {
        physics.step(&self.table, &self.broad, self.settings, dt);
        scene.resolve(&self.table);

        for (self.table.for_each()) |*slot| {
            if (!slot.alive) continue;
            slot.animation.advance(dt);
        }
    }

    pub fn snapshot_into(self: *World, out: [*]f32, max_entities: u32) u32 {
        var written: u32 = 0;

        for (self.table.for_each()) |slot| {
            if (!slot.alive or written >= max_entities) continue;

            const source = slot.animation.current_source_rect();
            const base: usize = @as(usize, written) * floats_per_entity;

            out[base + 0] = slot.position.x;
            out[base + 1] = slot.position.y;
            out[base + 2] = slot.radius;
            out[base + 3] = slot.color[0];
            out[base + 4] = slot.color[1];
            out[base + 5] = slot.color[2];
            out[base + 6] = slot.color[3];
            out[base + 7] = slot.world_rotation;
            out[base + 8] = @floatFromInt(slot.texture);
            out[base + 9] = source.x;
            out[base + 10] = source.y;
            out[base + 11] = source.width;
            out[base + 12] = source.height;
            out[base + 13] = slot.draw_size.x;
            out[base + 14] = slot.draw_size.y;
            out[base + 15] = if (slot.texture == 0) 0 else 1;

            written += 1;
        }

        return written;
    }
};
