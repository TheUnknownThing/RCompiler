struct Food {
    cnt: i32,
    ty: i32,
}
impl Food {
    fn better(self, other: Food) -> Food {
        if (self.cnt == other.cnt) {
            if (self.ty < other.ty) { self } else { other }
        } else if (self.cnt > other.cnt) {
            self
        } else {
            other
        }
    }
}

fn test( i : &mut i32) -> i32 {
    let mut j : i32 = *i - 1;
    loop {
        if (*i > 10) {
            break
        }
        j = j * *i;
        *i += 1;
    }
    j
}

fn main() {
    exit(0);
}
