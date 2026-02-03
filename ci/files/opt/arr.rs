fn main() {
    const arr: [[i32; 3]; 3] = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];
    const value: i32 = arr[1][0];
    let val : i32 = arr[2][0];
    printInt(value);
    printInt(val);
    exit(0);
}