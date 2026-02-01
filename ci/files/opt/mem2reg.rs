fn main() {
    let mut loop_count : i32 = 0;
    let mut result : i32 = 0;
    while (loop_count < 100000) {
        loop_count += 1;
        while (result < loop_count) {
            result += 1;
        }
        result = 0;
    } 
    printInt(loop_count);
    exit(0);
}