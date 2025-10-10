fn main() {
    let x = 10; // Outer x
    let y = {
        let x = 20; // Inner x, shadows outer
        x + 5       // Uses inner x, evaluates to 25
    };
    let z = x + y; // Uses outer x, z should be 10 + 25 = 35
}
