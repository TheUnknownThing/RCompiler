#pragma once

namespace rc {
    enum class TokenType {
        EOF, // End of file

        // Strict keywords
        AS, // as
        BREAK, // break
        CONST, // const
        CONTINUE, // continue
        CRATE, // crate
        ELSE, // else
        ENUM, // enum
        EXTERN, // extern
        FALSE, // false
        FN, // fn
        FOR, // for
        IF, // if
        IMPL, // impl
        IN, // in
        LET, // let
        LOOP, // loop
        MATCH, // match
        MOD, // mod
        MOVE, // move
        MUT, // mut
        PUB, // pub
        REF, // ref
        RETURN, // return
        SELF, // self
        SELF_TYPE, // Self
        STATIC, // static
        STRUCT, // struct
        SUPER, // super
        TRAIT, // trait
        TRUE, // true
        TYPE, // type
        UNSAFE, // unsafe
        USE, // use
        WHERE, // where
        WHILE, // while
        ASYNC, // async
        AWAIT, // await
        DYN, // dyn

        // Reserved keywords
        ABSTRACT, // abstract
        BECOME, // become
        BOX, // box
        DO, // do
        FINAL, // final
        MACRO, // macro
        OVERRIDE, // override
        PRIV, // priv
        TYPEOF, // typeof
        UNSIZED, // unsized
        VIRTUAL, // virtual
        YIELD, // yield
        TRY, // try
        GEN, // gen

        // Weak keywords
        STATIC_LIFETIME, // 'static
        MACRO_RULES, // macro_rules
        RAW, // raw
        SAFE, // safe
        UNION, // union

        // identifiers
        NON_KEYWORD_IDENTIFIER,
        RAW_IDENTIFIER,
        RESERVED_RAW_IDENTIFIER,

        FLOAT_LITERAL,
        CHAR_LITERAL,
        INTEGER_LITERAL,
        STRING_LITERAL,
        RAW_STRING_LITERAL,
        BYTE_LITERAL,
        RAW_BYTE_LITERAL,
        C_STRING_LITERAL,
        RAW_C_STRING_LITERAL,

        ASSIGN, // =
        PLUS, // +
        MINUS, // -
        STAR, // *
        SLASH, // /
        PERCENT, // %
        AMPERSAND, // &
        PIPE, // |
        CARET, // ^

        L_PAREN, // (
        R_PAREN, // )
        L_BRACE, // {
        R_BRACE, // }
        L_BRACKET, // [
        R_BRACKET, // ]
        COMMA, // ,
        DOT, // .
        COLON, // :
        SEMICOLON, // ;
        QUESTION, // ?
        COLON_COLON, // ::

        UNKNOWN // unknown
    };   
}