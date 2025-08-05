// This is a valid line comment

/* This is a valid block comment */

/* Nested block comments are supported:
   /* inner comment */
   More content here
   // This is a valid line comment inside a block comment
*/

// /* line comment inside block comment */

//   - A comment  
//// - Also a comment

/*   - A comment */
/*** - A comment */

pub mod nested_comments {
    /* we can /* nest /* deeply */ nested */ comments */
    
    // empty line comment
    //
    let a = "/* aaaaaaaaaaaaa */";
    let b = '/';
    let c = "/*";
    let d = "*/";
    let e = "/* not a /* nested */ comment */";
    let f = "///";
    let g = "\\\" /* this is not a comment */

    \"";
    let h = "\\"; // this is a comment /////*"
    let i = 12/*this should not pass compilation*/13;
    let j = 12//; this should pass compilation
    ;

    /* /* */ this is a fucking nested comment */
    // empty block comment
    /**/
}

//! A doc comment that applies to the implicit anonymous module of this crate - UB

pub mod outer_module {

    //!  - Inner line doc - UB
    //!! - Still an inner line doc (but with a bang at the beginning) - UB

    /*!  - Inner block doc - UB */
    /*!! - Still an inner block doc (but with a bang at the beginning) - UB */

    ///  - Outer line doc (exactly 3 slashes) - UB

    /**  - Outer block doc (exactly) 2 asterisks - UB */

    pub mod inner_module {}

    pub mod degenerate_cases {
        // empty inner line doc - UB
        //!

        // empty inner block doc - UB
        /*!*/

        // empty outer line doc - UB
        ///

        pub mod dummy_item {}

    }

    /* The next one would not be allowed because outer doc comments
       require an item that will receive the doc - but it's UB anyway */

    /// Where is my item? - UB
}