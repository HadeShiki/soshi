const math = @import("math.zig");

pub const State = struct {
    frame_count: u32 = 1,
    frame_width: u32 = 0,
    frame_height: u32 = 0,
    columns: u32 = 1,
    index: f32 = 0,
    fps: f32 = 0,
    playing: bool = false,
    loop: bool = true,

    pub fn configure(self: *State, frame_width: u32, frame_height: u32, frame_count: u32, columns: u32, fps: f32, loop: bool) void {
        self.frame_width = frame_width;
        self.frame_height = frame_height;
        self.frame_count = @max(frame_count, 1);
        self.columns = if (columns == 0) self.frame_count else columns;
        self.fps = fps;
        self.loop = loop;
        self.index = 0;
        self.playing = fps > 0 and self.frame_count > 1;
    }

    pub fn play(self: *State) void {
        self.playing = self.frame_count > 1 and self.fps > 0;
    }

    pub fn stop(self: *State) void {
        self.playing = false;
        self.index = 0;
    }

    pub fn advance(self: *State, dt: f32) void {
        if (!self.playing or self.fps <= 0 or self.frame_count <= 1) return;

        self.index += dt * self.fps;
        const frames = @as(f32, @floatFromInt(self.frame_count));
        if (self.index < frames) return;

        if (self.loop) {
            self.index = @mod(self.index, frames);
        } else {
            self.index = frames - 1;
            self.playing = false;
        }
    }

    pub fn current_source_rect(self: State) math.Rect {
        if (self.frame_width == 0 or self.frame_height == 0) return .{};

        const last_frame = @as(f32, @floatFromInt(self.frame_count - 1));
        const frame: u32 = @intFromFloat(@min(self.index, last_frame));
        const column = frame % self.columns;
        const row = frame / self.columns;

        return .{
            .x = @floatFromInt(column * self.frame_width),
            .y = @floatFromInt(row * self.frame_height),
            .width = @floatFromInt(self.frame_width),
            .height = @floatFromInt(self.frame_height),
        };
    }
};
