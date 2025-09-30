# Jsnpg
A JSON Parser and Generator written in C.

Jsnpg can parse standard JSON text via a SAX / Event based interface 
or a Pull / Iterative interface. 

What is considered "standard" JSON, and which variants Jsnpg supports, is 
discussed in [JSON Variants](#json-variants).

## SAX Parsing

To perform a SAX parse jsnpg needs 4 things:
1. the bytes that make up the JSON to be parsed
2. the number of bytes
3. details of which functions to call for each item in the JSON
4. a user provided object that is provided as context to each function call


```C
#include <jsnpg/jsnpg.h>

// Provide a handler function for each callback  
// that you wish to receive.  
// The prototypes for each function can be found 
// in jsnpg.h  
// Each function must return true to continue parsing   

bool handle_start_object(void *ctx)   
{  ...  }  


bool handle_key(void *ctx, 
                const unsigned char *bytes, 
                size_t count)  
{  ...  }  


bool handle_integer(void *ctx, long integer)  
{  ...  }


...  

// The JSON data and its length
unsigned char *json_data = ...;  
size_t json_length = ...;  

// The required callbacks and user context
jsnpg_callbacks handlers = {   
                    .start_object = handle_start_object, 
                    .key = handle_key,
                    .integer = handle_integer,
                    ...  
};  
my_context ctx = ...;  


jsnpg_result result = jsnpg_parse(
                                .bytes = json_data,  
                                .count = json_length,  
                                .callbacks = &handlers,  
                                .ctx = &ctx);  

// result.type       == JSNPG_EOF    on success
// result.type       == JSNPG_ERROR  on failure
// result.position   == position where parsing stopped

// On failure
// result.error.code == one of the jsnpg_error_code 
//                      values in jsnpg.h
// result.error.text == a brief text description of the error

```

## Generating JSON
The example below illustrates how to generate the following JSON
as a string with no whitespace.  Pretty printing can be enabled
by providing a non-zero indent.


```JSON
{ 
  "length": 123,
  "width": 34.65, 
  "name": "Type 3",
  "spec": [true, false, null]
}
```


```C
#include <jsnpg/jsnpg.h>

jsnpg_generator *gen = jsnpg_generator_new();

// jsnpg_generator_new( .indent = 4 ) for pretty print

jsnpg_start_object(gen);
jsnpg_key(gen, "length", 6);
jsnpg_integer(gen, 123);
jsnpg_key(gen, "width", 5);
jsnpg_real(gen, 34.65);
jsnpg_key(gen, "name", 4);
jsnpg_string(gen, "Type 3", 6);
jsnpg_key(gen, "spec", 4);
jsnpg_start_array(gen);
jsnpg_boolean(gen, true);
jsnpg_boolean(gen, false);
jsnpg_null(gen);
jsnpg_end_array(gen);
jsnpg_end_object(gen);
```


Running out of memory or attempting to output an invalid UTF-8
sequence can result these functions failing.  Checking the result
of each call would be tedious but you can test at the end.

```C
// check for errors
if(jsnpg_result_error(gen).code != JSNPG_ERROR_NONE) {
    ...
}
```

You can retrieve the resultant JSON as a null-terminated C string
or as a pointer to bytes.


```C

// get result as C string
char *json_string = jsnpg_result_string(gen);


// get result length and JSON bytes 
unsigned char *json_bytes;
size_t length = jsnpg_result_bytes(gen, &json_bytes);
```

The pointers returned from these functions are freed when the
generator is freed so make sure that you are finished with them 
before calling jsnpg_generator_free.

```C
// do something with result

jsnpg_generator_free(gen);
```

# Optional Macros
In the section on [Generating JSON](#generating-json) we gave an example of
using a generator to produce the following JSON.


```json
{ 
  "length": 123,
  "width": 34.65, 
  "name": "Type 3",
  "spec": [true, false, null]
}
```

The header files <jsnpg/def_gen_macros.h> and <jsnpg/undef_gen_macros.h> can
be used to define macros that make the production of JSON a little easier.


>It should be noted that using the macro files is optional,
>they are provided purely as a convenience.  They can be ignored,
>or modified to use different macro names to avoid name
>clashes or to conform to coding guidelines.

```
#include <jsnpg/jsnpg.h>

jsnpg_generator *my_gen = jsnpg_generator_new();

// Tell the macros the name of the variable holding the 
// generator (defaults to 'gen') 

#define JSNPG_GEN      my_gen

#include <jsnpg/def_gen_macros.h>

object(
    keyval("length", integer(123)),
    keyval("width", real(34.65)),
    keyval("name", string("Type 3")),
    keyval("spec", array(boolean(true), boolean(false), null())
);

#include <jsnpg/undef_gen_macros.h>
```


>As macro expansion creates a single statement it would be advisable to keep the
>use of the object() and array() macros for small amounts of data.  
>Surround larger amounts of data with start_object() and end_object() 
>or start_array() and end_array()

# Parse Options
This section describes the various options that can be specified
when using jsnpg_parse.

One option out of bytes, string or dom must be specified to provide the data
to be parsed.

One option out of callbacks or generator must be specified to provide handlers
for the SAX events.


## .allow (unsigned)
Allow is used to enable one or more of the provided [JSON Variants](#json-variants).

Set .allow to an OR mask of the JSNPG_ALLOW_... variables defined in jsnpg.h.

## .max_nesting (unsigned)
Max nesting limits the number of nested objects and/or arrays that the parser
can support.  This defaults to 1024 (and cannot be set lower) which should be
sufficient for all but the most pathalogical input.  The memory requirement is
1 bit per nesting level.

## .writeable (bool)
The parser makes a writeable copy of the input JSON with additonal padding
at the end.  For larger inputs memory and time can be saved if the caller
provides a writeable buffer with the required padding.  Setting .writeable
to true informs the parser that the input JSON satisfies this requirement.

JSNPG_WRITEABLE_PADDING (from jsnpg.h) bytes of padding are required.

## .bytes (unsigned char *)
A pointer to the JSON data to be parsed.

## .count (size_t)
The number of bytes in the supplied JSON data.

## .string (char *)
A null terminated string of JSON data.  An alternative to supplying
.bytes and .count.

## .dom (jsnpg_dom *)
The dom option allows the parser to replay a previous parse that has been
stored in memory.  [Generator Options](#generator-options) shows how to
generate a dom object.

Th dom option was provided as the JSON benchmark used for performance
analysis required that a parse to and from a DOM could be performed. Jsnpg does
not provide facilities to query the in memory representation.P

## .callbacks (jsnpg_callbacks *)
The SAX callback functions that the parser should call.

## .ctx (void *)
The context to be passed to each callback function.

## .generator (jsnpg_generator *)
Rather than providing callbacks and context a generator created with 
jsnpg_generator_new can be used.

# Generator Options
This section describes the options available when craeting generators with
jsnpg_generator_new.

If no options are specified to indicate what to generate, a JSON output
generator will be created.

Generator output can be retrived via jsnpg_result_string, jsnpg_result_bytes or
jsnpg_result_dom.  Errors can be retreievd via jsnpg_result_error.

## .allow (unsigned)
Enables one or more of the provided [JSON Variants](#json-variants).

JSNPG_ALLOW_INVALID_UTF8_OUT is the only setting that applies to generators,
and that is for disabling UTF-8 validation when generating JSON.

## .indent (unsigned)
The number of spaces to indent each level of output when generating
JSON.  A setting of 0 disables pretty printing.

## .max-nesting (unsigned)
Allows the generator to keep track of, and validate, object/array nesting
when generating JSON.  It is only used during debug builds (via the C assert
mechanism) and has no effect on release builds.

## .callbacks (jsnpg_callbacks *)
The SAX callback functions to generate output.

## .ctx (void *)
The context to pass to each callback function.

## .dom (bool)
If set to true the generator will create an in memory representation of the
JSON input.  This object can be retrieved via json_result_dom.

# JSON Variants
By default jsnpg conforms to the JSON standard, with the following implementation details:

* Input must be valid UTF-8
* Numbers are treated as 64 bit int if they have no decimal point or exponent indicator and can be stored in a uint64_t without loss of information
* All other numbers are treated as double.
* Numbers larger than can be stored in a double will be rejected, and the parse will fail.
* Numbers smaller than can be stored in a double will be converted to 0.0 or -0.0

Standard behaviour can be relaxed by passing a bit wise OR of the following boolean masks
in the `allow` option of the parser or generator.

|           *Mask*             |                     *Description                    |
|------------------------------|-----------------------------------------------------|
| JSNPG_ALLOW_COMMENTS         | Allow C style block and line comments in whitespace |
| JSNPG_ALLOW_TRAILING_COMMAS  | Allow comma at end of objects and arrays            |
| JSNPG_ALLOW_TRAILING_CHARS   | Allow any text in input after successful parse      |
| JSNPG_ALLOW_MULTIPLE_VALUES  | Allow multiple JSON values in input                 | 
| JSNPG_ALLOW_INVALID_UTF8_IN  | Allow invalid UTF-8 in input                        |
| JSNPG_ALLOW_INVALID_UTF8_OUT | Allow invalid UTF-8 in output (generator only)      |












