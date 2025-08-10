// advanced_control_flow.rs

enum Message {
    Value(i32),
    Stop,
}

fn main() {
    let mut count = 0;
    let result = loop {
        count = count + 1;
        if count == 5 {
            break count * 2; // Test break with a return value
        }
    }; // result should be 10

    let mut msg = Message::Value(3);
    while let Message::Value(v) = msg {
        msg = Message::Stop; // Stop the loop on the next iteration
    }
}
