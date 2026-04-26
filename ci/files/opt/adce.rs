fn main() {
    let input: i32 = getInt();

    let dead_a: i32 = input * 8;
    let dead_b: i32 = dead_a + 1024;
    let dead_c: i32 = dead_b / 2;
    let dead_d: i32 = dead_c - input;
    let dead_e: i32 = dead_d * dead_d;
    let _dead_sink: i32 = dead_e + 1;

    let live: i32 = input + 7;
    printInt(live);
    exit(0);
}
