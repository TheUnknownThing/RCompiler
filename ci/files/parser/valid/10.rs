fn main() {
    let x = { 5 }; // x is 5
    let y = {
        let z = 10;
        z + 5 // y is 15
    };
}
