# Jsnpg
A JSON Parser and Generator written in C.

Jsnpg can parse standard JSON text via a SAX / Event based interface 
or a Pull / Iterative interface.  If you are not familiar with these different
types of parsing see [Json Parsing](#json-parsing).

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
as a string with no whitespace, or pretty printed with a given
indentation.


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
    keyval("spec", array(boolean(true), 
                         boolean(false), 
                         null())
);

#include <jsnpg/undef_gen_macros.h>
```


>As macro expansion creates a single statement it would be advisable to keep the
>use of the object() and array() macros for small amounts of data.  
>Surround larger amounts of data with start_object() and end_object() 
>or start_array() and end_array()



