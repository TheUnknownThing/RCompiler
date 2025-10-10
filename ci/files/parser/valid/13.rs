enum State { Go(i32), Stop }

fn main() {
    let mut state = State::Go(1);
    let result = loop {
        let State::Go(val) = state else {
            break 100; // Diverge by breaking loop
        };
        state = State::Stop;
    };
}
