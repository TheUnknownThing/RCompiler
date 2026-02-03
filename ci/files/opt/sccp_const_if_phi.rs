fn main() {
    // Constant condition; should enable SCCP to fold away the else path.
    let x: i32 = if (1 == 1) { 42 } else { 13 };
    let _y: i32 = x;
    exit(0);
}
