fn main() {
    let x : i32 = 10; // Outer x
    let y : i32 = {
        let x : i32 = 20; // Inner x, shadows outer
        x + 5       // Uses inner x, evaluates to 25
    };
    let z : i32 = x + y; // Uses outer x, z should be 10 + 25 = 35
}
