#pragma once

#include <stdio.h>
#include <stddef.h>

// Allow some non-strict JSON behaviour
// 'Or' values together where multiple are to be allowed
// Pass in to .allow option when parsing/generating

// Allow C style block and line comments
#define JSNPG_ALLOW_COMMENTS                    0x01

// Allow commas before end of arrays and objects
#define JSNPG_ALLOW_TRAILING_COMMAS             0x02

// Allow trailing characters in input after successful parse
#define JSNPG_ALLOW_TRAILING_CHARS              0x04

// Allow multiple JSON values in the input
// If this is set then ALLOW_TRAILING_CHARS is ignored
#define JSNPG_ALLOW_MULTIPLE_VALUES             0x08 

// Allow invalid UTF8 sequences in the input
#define JSNPG_ALLOW_INVALID_UTF8_IN             0x10

// Allow invalid utf8 sequences in the output
// It is up to a generator whether or not it validates utf8 sequences by default.
// The only supplied generator that does is the default generator for creating 
// JSON output, and it respects this setting
#define JSNPG_ALLOW_INVALID_UTF8_OUT            0x20


typedef enum {
        JSNPG_NONE,
        JSNPG_PULL,
        JSNPG_NULL,
        JSNPG_FALSE,
        JSNPG_TRUE,
        JSNPG_INTEGER,
        JSNPG_REAL,
        JSNPG_STRING,
        JSNPG_KEY,
        JSNPG_START_ARRAY,
        JSNPG_END_ARRAY,
        JSNPG_START_OBJECT,
        JSNPG_END_OBJECT,
        JSNPG_ERROR,
        JSNPG_EOF
} jsnpg_type;

typedef enum {
        JSNPG_ERROR_NONE,
        JSNPG_ERROR_OPT,
        JSNPG_ERROR_ALLOC,
        JSNPG_ERROR_NUMBER,
        JSNPG_ERROR_UTF8,
        JSNPG_ERROR_SURROGATE,
        JSNPG_ERROR_STACK_UNDERFLOW,
        JSNPG_ERROR_STACK_OVERFLOW,
        JSNPG_ERROR_EXPECTED_VALUE,
        JSNPG_ERROR_EXPECTED_KEY,
        JSNPG_ERROR_NO_OBJECT,
        JSNPG_ERROR_NO_ARRAY,
        JSNPG_ERROR_ESCAPE,
        JSNPG_ERROR_UNEXPECTED,
        JSNPG_ERROR_INVALID,
        JSNPG_ERROR_TERMINATED,
        JSNPG_ERROR_EOF
} jsnpg_error_code;

typedef struct {
        const unsigned char *bytes;
        size_t count;
} jsnpg_string_info;

typedef union {
        long integer;
        double real;
} jsnpg_number_info;

typedef struct {
        jsnpg_error_code code;
        const char *text;
} jsnpg_error_info;

typedef struct {
        jsnpg_type type;
        size_t position;
        union {
                jsnpg_number_info number;
                jsnpg_string_info string;
                jsnpg_error_info error;
        };
} jsnpg_result;

typedef struct {
        bool (*boolean)(void *ctx, bool is_true);
        bool (*null)(void *ctx);
        bool (*integer)(void *ctx, long integer);
        bool (*real)(void *ctx, double real);
        bool (*string)(void *ctx, const unsigned char *bytes, size_t length);
        bool (*key)(void *ctx, const unsigned char *bytes , size_t length);
        bool (*start_array)(void *ctx);
        bool (*end_array)(void *ctx);
        bool (*start_object)(void *ctx);
        bool (*end_object)(void *ctx);
} jsnpg_callbacks;

typedef struct jsnpg_parser            jsnpg_parser;
typedef struct jsnpg_generator         jsnpg_generator;
typedef struct jsnpg_dom               jsnpg_dom;


void jsnpg_set_allocators(
                void *(*malloc)(size_t), 
                void *(*realloc)(void *, size_t),
                void (*free)(void *));

typedef struct {
        // required to track array/object nesting
        // will force a minimum of 1024
        unsigned max_nesting;

        // mask of JSNPG_ALLOW_... values above
        unsigned allow;

        // Input options, specify one type only
        //
        // All input is JSON bytes except for the 'dom' option
        // which is an in-memeory representation of parsed JSON
        // created by jsnpg_generator_new(.dom = true, ...)
        unsigned char *bytes;           // input bytes, must set count
        size_t count;
        char *string;                   // NULL terminated C string
        jsnpg_dom *dom;

} jsnpg_parser_opts;

// ------------------------------------
// Pull Parsing
// ------------------------------------

// Create a parser for pull parsing
// Not needed for callback / generator parsing as jsnpg_parse
// creates one internally
jsnpg_parser *jsnpg_parser_new_opt(jsnpg_parser_opts);
#define jsnpg_parser_new(...)   jsnpg_parser_new_opt(     \
                (jsnpg_parser_opts){ __VA_ARGS__ })     


// Pull parser, get next parse event and result
jsnpg_type jsnpg_parse_next(jsnpg_parser *);
jsnpg_result jsnpg_parse_result(jsnpg_parser *);

// Example, pull parsing from string that has trailing commas in it
//
// p = jsnpg_parser_new( .allow = JSNPG_ALLOW_TRAILING_COMMAS      
//                       .string = "{\"k1\": [12.5, true,],}");
// 
// The comments below indicate what JSON items are parsed
// The actual type of item is returned from jsnpg_parse_next
// Values are recovered from jsnpg_parse_result(p)
//
// Note: true and false have their own jsnpg_type variables
//       there is no jsnpg_type boolean
//
// jsnpg_parse_next(p); // type: begin_object
// jsnpg_parse_next(p); // type: key, value: "k1"
// jsnpg_parse_next(p); // type: begin_array
// jsnpg_parse_next(p); // type: real, value: 12.5
// jsnpg_parse_next(p); // type: true
// jsnpg_parse_next(p); // type: end_array
// jsnpg_parse_next(p); // type: end_object
// jsnpg_parse_next(p); // type: EOF
// jsnpg_parser_free(p);
//

// Free the parser returned from jsnpg_parser_new
void jsnpg_parser_free(jsnpg_parser *);

// ------------------------------------
// Callback and Generator Parsing
// ------------------------------------

typedef struct {
        // Options for parser created internally by json_parse
        // The parser will be freed before returning
        // See parser_opts above for desriptions
        unsigned max_nesting;
        unsigned allow;      
        unsigned char *bytes;
        size_t count;
        char *string;
        jsnpg_dom *dom;

        // Optional callbacks and callback ctx for SAX style parsing
        // This is a common use case so providing the options here
        // saves the caller having to create and free a generator themselves
        jsnpg_callbacks *callbacks;
        void *ctx;

        // Optional generator
        // Ignored if callbacks/ctx are specified
        jsnpg_generator *generator;

} jsnpg_parse_opts;

jsnpg_result jsnpg_parse_opt(jsnpg_parse_opts);
#define jsnpg_parse(...)  jsnpg_parse_opt(              \
                (jsnpg_parse_opts){ __VA_ARGS__ })         

// Example, parse a byte buffer and call callbacks with context
//          allow comments and multiple values in input
//
// jsnpg_parse( .allow = JSNPG_ALLOW_MULTIPLE_VALUES | JSNPG_ALLOW_COMMENTS
//               .bytes = my_bytes, 
//               .count = my_byte_count, 
//               .callbacks = my_callbacks,
//               .ctx = my_context);


// ------------------------------------
// Generating output
// ------------------------------------

typedef struct {
        // Pretty printing is ignored when writing to DOM or callbacks
        // Pretty printing indent, 0 = stringify
        unsigned indent;    
        
        // Currently the only value that effects generators is 
        // JSNPG_ALLOW_INVALID_UTF8_OUT
        unsigned allow;

        // Output options, specify max one type
        // If none are specified then the output will be buffered
        // and results will be available via jsnpg_result_string
        // or jsnpg_result_bytes
        
        // Options 'dom' build an in-memory representation of the parse
        // results which is available via jsnpg_result_dom
        //
        // Note: not much can be done with dom at the moment apart from
        //       providing it as an input to parse
        bool dom;

        jsnpg_callbacks *callbacks;
        void *ctx;

        // The structure of generated JSON is validated via the C assert
        // mechanism so is active during development and testing but
        // will be removed in an NDEBUG build.
        // While validating the correct nesting of arrays and objects
        // jsnpg needs to know the maximum nesting level to support
        // It will default to 1024 which is more than enough for most use cases
        // The setting has no effect in producion builds
        unsigned max_nesting;

} jsnpg_generator_opts;

jsnpg_generator *jsnpg_generator_new_opt(jsnpg_generator_opts);
#define jsnpg_generator_new(...)  jsnpg_generator_new_opt(    \
                (jsnpg_generator_opts){ __VA_ARGS__ })           


// The lifetime of results is that of their generator.
// A string or dom returned from these functions should not be used
// once their generator has been freed

jsnpg_error_info jsnpg_result_error(jsnpg_generator *);
jsnpg_dom *jsnpg_result_dom(jsnpg_generator *);
char *jsnpg_result_string(jsnpg_generator *);
size_t jsnpg_result_bytes(jsnpg_generator *, unsigned char **);

void jsnpg_generator_free(jsnpg_generator *);

// Write JSON items to a generator
//
// Functions return true if successful, false on error
// The only errors returned will be out of memory or invalid utf8 output
// both of which are quite unlikely, so you can probably get away with
// creating output then checking for errors at the end.
//
// error information can be retrieved from jsnpg_result_error.
// An error code of JSNPG_ERROR_NONE indicates no errors.
//
// Macros to make building JSON more concise can be found in
// def_gen_macros.h

bool jsnpg_null(jsnpg_generator *);
bool jsnpg_boolean(jsnpg_generator *, bool);
bool jsnpg_integer(jsnpg_generator *, long);
bool jsnpg_real(jsnpg_generator *, double);
bool jsnpg_string(jsnpg_generator *, const unsigned char *, size_t);
bool jsnpg_key(jsnpg_generator *, const unsigned char *, size_t);
bool jsnpg_start_array(jsnpg_generator *);
bool jsnpg_end_array(jsnpg_generator *);
bool jsnpg_start_object(jsnpg_generator *);
bool jsnpg_end_object(jsnpg_generator *);

