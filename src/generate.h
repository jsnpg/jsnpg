/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 */

#pragma once

static generator *generator_new(unsigned, unsigned);
static generator *generator_set_callbacks(generator *, callbacks *callbacks, void *ctx);
static generator *generator_reset(generator *, unsigned);

typedef bool (*boolean)(void *, bool);
typedef bool (*null)(void *); 
typedef bool (*integer)(void *, long);
typedef bool (*real)(void *, double);
typedef bool (*string)(void *, const unsigned char *, size_t);
typedef bool (*key)(void *, const unsigned char *, size_t);
typedef bool (*start_array)(void *);
typedef bool (*end_array)(void *);
typedef bool (*start_object)(void *);
typedef bool (*end_object)(void *);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static bool void_boolean(void *ctx, bool boolean) { return true; }
static bool void_null(void *ctx) { return true; }
static bool void_integer(void *ctx, long integer) { return true; }
static bool void_real(void *ctx, double real) { return true; }
static bool void_string(void *ctx, const unsigned char *bytes, size_t length) { return true; }
static bool void_key(void *ctx, const unsigned char *bytes , size_t length) { return true; }
static bool void_start_array(void *ctx) { return true; }
static bool void_end_array(void *ctx) { return true; }
static bool void_start_object(void *ctx) { return true; }
static bool void_end_object(void *ctx) { return true; }
#pragma GCC diagnostic pop

// Used by main parser and DOM parser
#define DECLARE_CALLBACKS(G, P)                                                                                               \
        const boolean P##boolean           = G->callbacks->boolean      ? G->callbacks->boolean      : void_boolean;          \
        const null P##null                 = G->callbacks->null         ? G->callbacks->null         : void_null;             \
        const integer P##integer           = G->callbacks->integer      ? G->callbacks->integer      : void_integer;          \
        const real P##real                 = G->callbacks->real         ? G->callbacks->real         : void_real;             \
        const string P##string             = G->callbacks->string       ? G->callbacks->string       : void_string;           \
        const key P##key                   = G->callbacks->key          ? G->callbacks->key          : void_key;              \
        const start_array P##start_array   = G->callbacks->start_array  ? G->callbacks->start_array  : void_start_array;      \
        const end_array P##end_array       = G->callbacks->end_array    ? G->callbacks->end_array    : void_end_array;        \
        const start_object P##start_object = G->callbacks->start_object ? G->callbacks->start_object : void_start_object;     \
        const end_object P##end_object     = G->callbacks->end_object   ? G->callbacks->end_object   : void_end_object;

