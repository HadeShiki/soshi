pub const Vec2 = struct {
    x: f32 = 0,
    y: f32 = 0,

    pub fn add(self: Vec2, other: Vec2) Vec2 {
        return .{ .x = self.x + other.x, .y = self.y + other.y };
    }

    pub fn sub(self: Vec2, other: Vec2) Vec2 {
        return .{ .x = self.x - other.x, .y = self.y - other.y };
    }

    pub fn scale(self: Vec2, factor: f32) Vec2 {
        return .{ .x = self.x * factor, .y = self.y * factor };
    }

    pub fn dot(self: Vec2, other: Vec2) f32 {
        return self.x * other.x + self.y * other.y;
    }

    pub fn length(self: Vec2) f32 {
        return @sqrt(self.dot(self));
    }
};

pub const Rect = struct {
    x: f32 = 0,
    y: f32 = 0,
    width: f32 = 0,
    height: f32 = 0,

    pub fn right(self: Rect) f32 {
        return self.x + self.width;
    }

    pub fn bottom(self: Rect) f32 {
        return self.y + self.height;
    }
};
