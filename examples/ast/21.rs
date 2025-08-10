// core_logic_and_functions.rs

fn add(a: i32, b: i32) -> i32 {
    a + b
}

fn main() {
    let x = 10;
    let y = {
        let temp = 20;
        temp * 2 // This block expression evaluates to 40
    };

    let z = add(x, y); // z should be 50

    // The return is implicit since the last expression's type is ()
    // but we can add an explicit one.
    return;
}
