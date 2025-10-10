// core_logic_and_functions.rs

fn add(a: i32, b: i32) -> i32 {
    a + b
}

fn main() {
    let x : i32 = 10;
    let y : i32 = {
        let temp : i32 = 20;
        temp * 2 // This block expression evaluates to 40
    };

    let z : i32 = add(x, y); // z should be 50
}
