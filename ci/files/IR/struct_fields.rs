struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let p: Point = Point { x: 4, y: 5 };
    let sum: i32 = p.x + p.y;
    exit(0);
}
