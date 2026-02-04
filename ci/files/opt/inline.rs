fn callee(a : i32) -> i32 {
    return a + 1;
}

fn main() {
    let x : i32 = 5;
    let y : i32 = callee(x);
    printInt(y);
    exit(0);
}