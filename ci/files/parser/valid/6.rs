fn main() {
    let a : i32 = (1,2).test();
    let b : i32 = (test).test(1 + 2,2);
    let c : i32 = test().test();
    let d : i32 = test.test.test.test();

    let t : (i32, bool) = (10, true);
    let x : i32 = t.0;
    let y : bool = t.1;
}
