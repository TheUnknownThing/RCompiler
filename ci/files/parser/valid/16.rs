fn main() {
    let mut x = 10;
    let r = &mut x;
    *r = 20; // x is now 20
}
