fn main() {
    let mut x : i32 = 10;
    let mut y : i32 = 15;
    let mut p : &mut i32 = &mut x;
    let i : i32 = 5;
    if (i < 10) {
        p = &mut y;
    } else {
        p = &mut x;
    }
    *p = 20;
    printInt(x);
    printInt(y);
    exit(0);
}