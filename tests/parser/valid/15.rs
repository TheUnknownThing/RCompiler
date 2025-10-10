fn main() {
    let pair = (0, 10);
    let result = match pair {
        (0, y) => y, // Matches, result is 10
        (x, 0) => x,
        (_, _) => -1,
    };
}
