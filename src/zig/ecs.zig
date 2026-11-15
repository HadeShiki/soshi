const animation = @import("animation.zig");
const math = @import("math.zig");

pub const capacity: u32 = 8192;
pub const handle_none: u64 = 0;
pub const parent_none: u32 = 0xffffffff;

const free_list_end: u32 = 0xffffffff;

pub const Slot = struct {
    alive: bool = false,
    generation: u32 = 1,
    next_free: u32 = free_list_end,
    position: math.Vec2 = .{},
    local_position: math.Vec2 = .{},
    velocity: math.Vec2 = .{},
    rotation: f32 = 0,
    world_rotation: f32 = 0,
    radius: f32 = 0,
    draw_size: math.Vec2 = .{},
    color: [4]f32 = .{ 1, 1, 1, 1 },
    parent: u32 = parent_none,
    texture: u32 = 0,
    animation: animation.State = .{},
};

pub fn handle_of(index: u32, generation: u32) u64 {
    return (@as(u64, generation) << 32) | @as(u64, index);
}

pub fn index_of(handle: u64) u32 {
    return @intCast(handle & 0xffffffff);
}

pub fn generation_of(handle: u64) u32 {
    return @intCast(handle >> 32);
}

pub const Table = struct {
    // Zeroed memory is the baseline; reset() builds the free list on cheap startup.
    slots: [capacity]Slot = undefined,
    free_head: u32 = 0,
    free_count: u32 = capacity,

    pub fn reset(self: *Table) void {
        for (&self.slots, 0..) |*slot, index| {
            const next: u32 = if (index + 1 < capacity) @intCast(index + 1) else free_list_end;
            slot.* = .{ .next_free = next };
        }
        self.free_head = 0;
        self.free_count = capacity;
    }

    pub fn count(self: *const Table) u32 {
        return capacity - self.free_count;
    }

    pub fn create(self: *Table, position: math.Vec2, velocity: math.Vec2, radius: f32, color: [4]f32) u64 {
        if (self.free_count == 0) return handle_none;

        const index = self.free_head;
        const slot = &self.slots[index];
        self.free_head = slot.next_free;
        self.free_count -= 1;

        // Generation survives reuse so old handles stay invalid; everything else resets.
        const generation = slot.generation;
        slot.* = .{
            .alive = true,
            .generation = generation,
            .position = position,
            .velocity = velocity,
            .radius = radius,
            .color = color,
        };

        return handle_of(index, generation);
    }

    pub fn destroy(self: *Table, handle: u64) bool {
        const slot = self.find(handle) orelse return false;
        const index = index_of(handle);

        for (self.for_each()) |*child| {
            if (child.alive and child.parent == index) child.parent = parent_none;
        }

        slot.alive = false;
        slot.generation +%= 1;
        slot.next_free = self.free_head;
        self.free_head = index;
        self.free_count += 1;

        return true;
    }

    pub fn find(self: *Table, handle: u64) ?*Slot {
        const index = index_of(handle);
        if (index >= capacity) return null;

        const slot = &self.slots[index];
        if (!slot.alive or slot.generation != generation_of(handle)) return null;

        return slot;
    }

    pub fn for_each(self: *Table) []Slot {
        return self.slots[0..];
    }
};
