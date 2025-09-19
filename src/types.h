/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * types.h
 *   structs and typedefs used throughout the project
 *   aliases types from the public api so we dont have to prefix
 *   everything with jsnpg_
 */

#pragma once

#include <stdint.h>
#include <setjmp.h>

// Alias jsnpg types so we don't have to prefix our code
// The only jsnpgs left in code should be 
//  - the names of extern functions
//  - the members of the jsnpg_type and jsnpg_error_code enums

typedef jsnpg_type                     json_type;
typedef jsnpg_error_code               error_code;
typedef jsnpg_result                   parse_result;
typedef jsnpg_error_info               error_info;
typedef jsnpg_callbacks                callbacks;
typedef struct jsnpg_parser            parser;
typedef struct jsnpg_generator         generator;
typedef struct jsnpg_dom               dom;
typedef jsnpg_parser_opts              parser_opts;
typedef jsnpg_parse_opts               parse_opts;
typedef jsnpg_generator_opts           generator_opts;


typedef unsigned char                   byte;

typedef struct allocator                allocator;
typedef struct memory_input_stream      memory_input_stream;
typedef struct memory_output_stream     memory_output_stream;
typedef struct json_output_stream       json_output_stream;
typedef struct stack                    stack;
typedef struct dom_info                 dom_info;

#define STACK_OBJECT 0
#define STACK_ARRAY  1
#define STACK_NONE   2

struct stack {
       unsigned ptr;
       unsigned size;
       byte     *stack;
};

struct dom_info {
        dom     *hdr;
        size_t  offset;
};

// For pull parser to keep track of where it is up to
typedef enum {
        STATE_START,
        STATE_OBJECT,
        STATE_KEY,
        STATE_KEY_VALUE,
        STATE_OBJECT_COMMA,
        STATE_ARRAY,
        STATE_ARRAY_VALUE,
        STATE_ARRAY_COMMA,
        STATE_DONE,
        STATE_EOF
} parse_state;

// Types exposed by library via opaque pointer

struct jsnpg_parser {
        unsigned                        flags;
        allocator                       *allocator;
        memory_input_stream             *mis;
        parse_state                     state;
        dom_info                        dom_info;
        parse_result                    result;
        jmp_buf                         env;
        stack                           stack;
};

struct jsnpg_generator {
        allocator                       *allocator;
        callbacks                       *callbacks;
        void                            *ctx;
        bool                            validate_utf8;
        bool                            key_next;
        error_info                      error;
        size_t                          count;
        stack                           stack;
};

struct jsnpg_dom {
        allocator                       *allocator;
        dom                             *next;
        dom                             *current;
        size_t                          count;
        size_t                          size;
};
