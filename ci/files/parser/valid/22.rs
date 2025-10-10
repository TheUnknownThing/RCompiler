// structs_and_methods.rs

struct Point {
    x: i32,
    y: i32,
}

impl Point {
    fn manhattan_distance(&self, other: &Point) -> i32 {
        (self.x - other.x) + (self.y - other.y)
    }
}

fn main() {
    let p1 : Point = Point { x: 3, y: 4 };
    let p2 : Point = Point { x: 10, y: 20 };

    let dist : i32 = p1.manhattan_distance(&p2); // dist should be 23
}
