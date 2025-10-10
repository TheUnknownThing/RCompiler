fn main() {
    let mut i : i32 = 0;
    while (i < 10) {
        if (i % 2 == 0) {
            printInt(i);
        } else {
            continue;
        }
        i += 1;
    }

    loop {
        if (i >= 20) {
            break;
        }
        i += 1;
    }
}
