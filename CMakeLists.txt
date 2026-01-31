cmake_minimum_required(VERSION 3.10.0)
project(sunflower VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

add_subdirectory(test)

add_library(sunflower
    header.h
    malloc.h malloc.c
    token.h token.c
    stmt.h stmt.c
    expr.h expr.c
    parser.h parser.c
    const.h const.c
    object.h object.c
    mod.h mod.c
    mut.h mut.c
    ht.h ht.c
    fun.h fun.c
    ast.h ast.c
    arith.h arith.c
    bytecode.h bytecode.c
    sunflower.h sunflower.c)

