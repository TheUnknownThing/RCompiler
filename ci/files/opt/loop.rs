fn main() {
    let mut x : i32 = 1;
    let mut y : i32 = 0;
    let mut z : i32 = -1;
    let mut i : i32 = 0;
    while (i < 10) {
        let temp : i32 = x;
        x = y;
        y = z;
        z = temp;
        i = i + 1;
    }
    exit(0);
}