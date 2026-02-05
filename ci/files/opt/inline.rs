fn callee(a : i32) -> i32 {
    return a + 1;
}

fn test(a : i32) -> i32 {
    if (a > 0) {
        return a;
    } else {
        return callee(a);
    }
}

fn main() {
    let x : i32 = 5;
    let y : i32 = test(x);
    printInt(y);
    exit(0);
}