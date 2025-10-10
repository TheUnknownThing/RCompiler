// This file should pass compilation

fn main() {
    i32();
    Main();
}

// strange names 1
fn i32() {
    let i32 = 42;
    println!("{}", i32);
}

// strange names 2
fn Main() {
    let main = "Hello, World!";
    println!("{}", main);

    let True = true;
    let superstaticmatch = "some keywords linked together";
}