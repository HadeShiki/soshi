const ecs = @import("ecs.zig");
const math = @import("math.zig");

pub const Settings = struct {
    gravity: f32 = 0,
    restitution: f32 = 0.95,
    bounds: math.Rect = .{ .width = 800, .height = 600 },
};

const grid_limit: u32 = 48;
const max_cells: u32 = grid_limit * grid_limit;

pub const BroadPhase = struct {
    heads: [max_cells]i32 = undefined,
    next_in_cell: [ecs.capacity]i32 = undefined,
};

pub fn step(table: *ecs.Table, broad: *BroadPhase, settings: Settings, dt: f32) void {
    if (dt <= 0) return;

    const largest_radius = integrate(table, settings, dt);
    bounce_off_bounds(table, settings);

    if (largest_radius > 0) {
        resolve_contacts(table, broad, settings, largest_radius);
    }
}

fn integrate(table: *ecs.Table, settings: Settings, dt: f32) f32 {
    var largest_radius: f32 = 0;

    for (table.for_each()) |*slot| {
        if (!slot.alive or slot.parent != ecs.parent_none) continue;
        slot.velocity.y += settings.gravity * dt;
        slot.position = slot.position.add(slot.velocity.scale(dt));
        if (slot.radius > largest_radius) largest_radius = slot.radius;
    }

    return largest_radius;
}

fn bounce_off_bounds(table: *ecs.Table, settings: Settings) void {
    const bounds = settings.bounds;
    const bounce = settings.restitution;

    for (table.for_each()) |*slot| {
        if (!slot.alive or slot.parent != ecs.parent_none) continue;

        if (slot.position.x - slot.radius < bounds.x) {
            slot.position.x = bounds.x + slot.radius;
            slot.velocity.x = @abs(slot.velocity.x) * bounce;
        } else if (slot.position.x + slot.radius > bounds.right()) {
            slot.position.x = bounds.right() - slot.radius;
            slot.velocity.x = -@abs(slot.velocity.x) * bounce;
        }

        if (slot.position.y - slot.radius < bounds.y) {
            slot.position.y = bounds.y + slot.radius;
            slot.velocity.y = @abs(slot.velocity.y) * bounce;
        } else if (slot.position.y + slot.radius > bounds.bottom()) {
            slot.position.y = bounds.bottom() - slot.radius;
            slot.velocity.y = -@abs(slot.velocity.y) * bounce;
        }
    }
}

fn resolve_contacts(table: *ecs.Table, broad: *BroadPhase, settings: Settings, largest_radius: f32) void {
    if (settings.bounds.width <= 0 or settings.bounds.height <= 0) return;

    const cell_size = largest_radius * 2;
    const columns = extent_of(settings.bounds.width, cell_size);
    const rows = extent_of(settings.bounds.height, cell_size);

    @memset(broad.heads[0 .. columns * rows], -1);

    for (table.for_each(), 0..) |*slot, index| {
        if (!slot.alive or slot.parent != ecs.parent_none) continue;
        const cell = cell_of(slot.position, settings.bounds, cell_size, columns, rows);
        broad.next_in_cell[index] = broad.heads[cell];
        broad.heads[cell] = @intCast(index);
    }

    for (0..rows) |row| {
        for (0..columns) |column| {
            const cell: u32 = @intCast(row * columns + column);
            resolve_inside_cell(table, broad, cell, settings.restitution);
            resolve_with_neighbours(table, broad, cell, column, row, columns, rows, settings.restitution);
        }
    }
}

fn resolve_with_neighbours(
    table: *ecs.Table,
    broad: *BroadPhase,
    cell: u32,
    column: usize,
    row: usize,
    columns: u32,
    rows: u32,
    restitution: f32,
) void {
    const offsets = [_][2]i32{ .{ 1, 0 }, .{ -1, 1 }, .{ 0, 1 }, .{ 1, 1 } };

    for (offsets) |offset| {
        const neighbour_column = @as(i32, @intCast(column)) + offset[0];
        const neighbour_row = @as(i32, @intCast(row)) + offset[1];
        if (neighbour_column < 0 or neighbour_row < 0) continue;
        if (neighbour_column >= @as(i32, @intCast(columns))) continue;
        if (neighbour_row >= @as(i32, @intCast(rows))) continue;

        const neighbour: u32 = @intCast(neighbour_row * @as(i32, @intCast(columns)) + neighbour_column);
        resolve_across_cells(table, broad, cell, neighbour, restitution);
    }
}

fn resolve_inside_cell(table: *ecs.Table, broad: *BroadPhase, cell: u32, restitution: f32) void {
    var first = broad.heads[cell];
    while (first >= 0) : (first = broad.next_in_cell[@intCast(first)]) {
        var second = broad.next_in_cell[@intCast(first)];
        while (second >= 0) : (second = broad.next_in_cell[@intCast(second)]) {
            resolve_pair(&table.slots[@intCast(first)], &table.slots[@intCast(second)], restitution);
        }
    }
}

fn resolve_across_cells(table: *ecs.Table, broad: *BroadPhase, left: u32, right: u32, restitution: f32) void {
    var first = broad.heads[left];
    while (first >= 0) : (first = broad.next_in_cell[@intCast(first)]) {
        var second = broad.heads[right];
        while (second >= 0) : (second = broad.next_in_cell[@intCast(second)]) {
            resolve_pair(&table.slots[@intCast(first)], &table.slots[@intCast(second)], restitution);
        }
    }
}

fn resolve_pair(a: *ecs.Slot, b: *ecs.Slot, restitution: f32) void {
    const offset = b.position.sub(a.position);
    const distance = offset.length();
    const touching_distance = a.radius + b.radius;
    if (distance >= touching_distance or distance <= 0) return;

    const normal = offset.scale(1 / distance);
    const overlap = touching_distance - distance;
    a.position = a.position.sub(normal.scale(overlap * 0.5));
    b.position = b.position.add(normal.scale(overlap * 0.5));

    const closing_speed = b.velocity.sub(a.velocity).dot(normal);
    if (closing_speed >= 0) return;

    // Equal masses: the impulse is symmetric, so this stays allocation-free and branch-light.
    const impulse = -(1 + restitution) * closing_speed * 0.5;
    a.velocity = a.velocity.sub(normal.scale(impulse));
    b.velocity = b.velocity.add(normal.scale(impulse));
}

fn extent_of(span: f32, cell_size: f32) u32 {
    const raw = span / cell_size;
    if (raw <= 1) return 1;
    if (raw >= @as(f32, @floatFromInt(grid_limit))) return grid_limit;
    return @intFromFloat(@ceil(raw));
}

fn cell_of(position: math.Vec2, bounds: math.Rect, cell_size: f32, columns: u32, rows: u32) u32 {
    const column = axis_cell(position.x, bounds.x, cell_size, columns);
    const row = axis_cell(position.y, bounds.y, cell_size, rows);
    return row * columns + column;
}

fn axis_cell(value: f32, origin: f32, cell_size: f32, limit: u32) u32 {
    const relative = (value - origin) / cell_size;
    if (relative <= 0) return 0;

    const clamped = @min(relative, @as(f32, @floatFromInt(limit)));
    const raw: u32 = @intFromFloat(clamped);
    if (raw >= limit) return limit - 1;
    return raw;
}
