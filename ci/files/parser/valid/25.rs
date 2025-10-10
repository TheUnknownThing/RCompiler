// borrowing_and_dereferencing.rs

// This function takes a mutable reference and modifies the value it points to.
fn increment(val: &mut i32) {
    *val = *val + 1;
}

fn main() {
    let mut num : i32 = 99;
    
    // Create a mutable borrow.
    let num_ref : &mut i32 = &mut num;

    // Pass the borrow to the function.
    increment(num_ref);
    
    // After the call, 'num' is 100.
    // The borrow has ended, so we can use 'num' again.
    let final_val : i32 = num;
}
