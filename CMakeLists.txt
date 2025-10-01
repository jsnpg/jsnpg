cmake_minimum_required(VERSION 3.30)

project(jsnpg VERSION 0.1.0
        DESCRIPTION "A JSON SAX/Pull Parser and Generator"
        LANGUAGES C)

add_compile_options(
         -O3 
         -std=c23 
         -Wall 
         -Wextra 
 )

 add_subdirectory(src)
 add_subdirectory(test)
