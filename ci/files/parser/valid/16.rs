fn main() {
    let mut x : i32 = 10;
    let r : &mut i32 = &mut x;
    *r = 20; // x is now 20
}
