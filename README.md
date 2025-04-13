# North
Learn to independently develop a programming language north

## What is North?
    North is a programming language that is designed to be self-compiling.
    It is a self-compiling language, which means that it can compile itself.


## CMake Build

- CMakeLists.txt
    - Builds the compiler

- Config.cmake.in
    - Configuration file for CMake


##  Usage

```bash
north/
├── CMakeLists.txt
├── Config.cmake.in
├── include/
│   └── north/         
│       ├── lexer.h
│       └── parser.h
├── src/
│   ├── main.c
│   ├── lexer/
│   │   └── lexer.c
│   └── parser/
│       └── parser.c
└── test/
    ├── lexer_test.c
    └── parser_test.c
```
