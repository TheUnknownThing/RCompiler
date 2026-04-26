fn main() {
    let mut x : i32 = getInt();
    x = x + x;
    x = x * 8;
    x = x * 15;
    x = x * 33;
    printInt(x);
    exit(0);
}