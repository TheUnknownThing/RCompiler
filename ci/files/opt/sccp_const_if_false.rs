fn main() {
    // Constant false; ensures the 'else' path is selected.
    let x: i32 = if (1 == 0) { 111 } else { 222 };
    let _y: i32 = x;
    exit(0);
}
