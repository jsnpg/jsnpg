cmake_minimum_required(VERSION 3.30)

project(jsnpg VERSION 1.0.0
        DESCRIPTION "A JSON SAX/Pull Parser and Generator"
        LANGUAGES C)

add_compile_options(
         -O3 
         -std=c23 
         -fstack-protector-strong 
         -Wall 
         -Wextra 
         -Wcast-align 
         -Wcast-qual
         -Wconversion 
         -Wdouble-promotion 
         -Wformat=2 
         -Winit-self
         -Wmissing-prototypes 
         -Wmissing-variable-declarations 
         -Wshadow 
         -Wstrict-overflow=2 
         -Wstrict-prototypes 
         -Wswitch-default 
         -Wundef 
         -Wunused-macros 
         -Wwrite-strings 
 )

 add_subdirectory(src)
 add_subdirectory(test)
