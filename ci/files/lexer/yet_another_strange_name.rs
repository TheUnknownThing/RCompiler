// Test file for keyword handling in Rust compiler
fn main() {
    let as = "invalid";
    let break = "invalid";
    let const = "invalid";
    let continue = "invalid";
    let crate = "invalid";
    let else = "invalid";
    let enum = "invalid";
    let extern = "invalid";
    let false = "invalid";
    let fn = "invalid";
    let for = "invalid";
    let if = "invalid";
    let impl = "invalid";
    let in = "invalid";
    let let = "invalid";
    let loop = "invalid";
    let match = "invalid";
    let mod = "invalid";
    let move = "invalid";
    let mut = "invalid";
    let pub = "invalid";
    let ref = "invalid";
    let return = "invalid";
    let self = "invalid";
    let Self = "invalid";
    let static = "invalid";
    let struct = "invalid";
    let super = "invalid";
    let trait = "invalid";
    let true = "invalid";
    let type = "invalid";
    let unsafe = "invalid";
    let use = "invalid";
    let where = "invalid";
    let while = "invalid";
    let async = "invalid";
    let await = "invalid";
    let dyn = "invalid";

    let abstract = "invalid";
    let become = "invalid";
    let box = "invalid";
    let do = "invalid";
    let final = "invalid";
    let macro = "invalid";
    let override = "invalid";
    let priv = "invalid";
    let typeof = "invalid";
    let unsized = "invalid";
    let virtual = "invalid";
    let yield = "invalid";
    let try = "invalid";
    let gen = "invalid";

    let r#as = "invalid";
    let r#break = "invalid"; 
    let r#const = "invalid";
    let r#abstract = "still invalid";
    let r#become = "still invalid";

    test_union();
    test_raw();
}


fn test_union() {
    let union = "this should work";
    union MyUnion { x: i32 }; // This should fail
}

fn test_raw() {
    let raw = "this should work";
    let ptr = raw const i32; // This should fail
}
