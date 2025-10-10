// let_else_divergence.rs

enum MaybeInt {
    Some(i32),
    Nothing,
}

fn main() {
    let my_val = MaybeInt::Some(42);

    let MaybeInt::Some(x) = my_val else {
        // This block must diverge. `return` is a valid way to do so.
        return;
    };

    // This code is only reachable if the pattern matches.
    // The variable 'x' is now in scope.
    let _y = x + 1; // _y should be 43
}
