fn main() {
    let mut i = 0;
    while i < 10 {
        if i % 2 == 0 {
            println!("Even: {}", i);
        } else {
            println!("Odd: {}", i);
        }
        i += 1;
    }

    loop {
        if i >= 20 {
            break;
        }
        println!("Counting: {}", i);
        i += 1;
    }
}
