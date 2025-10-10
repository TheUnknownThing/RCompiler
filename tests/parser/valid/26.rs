// match_with_guards.rs

enum Action {
    Number(i32),
    Text(bool),
}

fn main() {
    let action = Action::Number(10);
    
    let result_code = match action {
        Action::Text(is_true) => 1,
        Action::Number(n) if n > 100 => 2,
        Action::Number(n) if n > 5 => 3, // This arm should be taken
        Action::Number(_) => 4,
    };
    // result_code should be 3
}
