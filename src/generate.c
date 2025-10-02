/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * generate.c
 *   provides a dual role
 *   1 - delivers parse events from parser to user provided callback
 *   2 - delivers user generated events to produce JSON output
 *
 *   during development/testing it will assert() that the events make
 *   a valid JSON document (for example,  "[2}" will be rejected)
 */
#include <assert.h>

// Validators for generator structure
// All used inside assert(...) so not required
// when NDEBUG is defined
#ifndef NDEBUG

// Can the generator accept a value?
// Anything but a key
static bool can_value(generator *g)
{
        if(g->stack.size && stack_peek(&g->stack) == STACK_OBJECT) {
                if(g->key_next) {
                        g->error = make_error(JSNPG_ERROR_EXPECTED_KEY);
                        return false;
                } else {
                        g->key_next = true;
                }
        }
        return true;

}

// Can the generator accept a key?
static bool can_key(generator *g)
{
        if(g->stack.size && !g->key_next) {
                g->error = make_error(JSNPG_ERROR_EXPECTED_VALUE);
                return false;
        }
        g->key_next = false;
        return true;
}

// Can the generator accept another level on the stack?
// Pushes type on the stack and sets 'key_next' to true if STACK_OBJECT was pushed
static bool can_push(generator *g, int type)
{
        if(!can_value(g))
                return false;
        if(g->stack.size) {
                if(-1 == stack_push(&g->stack, type)) {
                        g->error = make_error(JSNPG_ERROR_STACK_OVERFLOW);
                        return false;
                }
                g->key_next = type == STACK_OBJECT;
        }
        return true;
        
}

// Checks that the top of the stack is the expected type
// Checks that it is not in object after 'key:' 
// Pops the stack 
// And sets 'key_next' to true if new top of stack is STACK_OBJECT
static bool can_pop(generator *g, int type)
{
        int cur_type;
        if(g->stack.size) {
                cur_type = stack_peek(&g->stack);
                if(cur_type == -1) {
                        g->error = make_error(JSNPG_ERROR_STACK_UNDERFLOW);
                        return false;
                } else if(type != cur_type) {
                        g->error = make_error((type == STACK_OBJECT)
                                ? JSNPG_ERROR_NO_OBJECT
                                : JSNPG_ERROR_NO_ARRAY);
                        return false;
                } else if(type == STACK_OBJECT && !g->key_next) {
                        g->error = make_error(JSNPG_ERROR_EXPECTED_VALUE);
                        return false;
                }
                stack_pop(&g->stack);
                g->key_next = STACK_OBJECT == stack_peek(&g->stack);
        }
        return true;
}
#endif  // ifndef NDEBUG

// Public API
// Validate arguments

bool jsnpg_null(generator *g)
{
        if(!g) return false;

        assert(can_value(g));

        return (!g->callbacks->null) || g->callbacks->null(g->ctx);
}

bool jsnpg_boolean(generator *g, bool boolean)
{
        if(!g) return false;

        assert(can_value(g));

        return  (!g->callbacks->boolean) || g->callbacks->boolean(g->ctx, boolean);
}

bool jsnpg_integer(generator *g, long integer)
{
        if(!g) return false;

        assert(can_value(g));

        return  (!g->callbacks->integer) || g->callbacks->integer(g->ctx, integer);
}

bool jsnpg_real(generator *g, double real)
{
        if(!g) return false;

        assert(can_value(g));

        return  (!g->callbacks->real) || g->callbacks->real(g->ctx, real);
}

bool jsnpg_string(generator *g, const byte *bytes, size_t count)
{
        if(!g || !bytes) return false;

        assert(can_value(g));

        return (!g->callbacks->string) || g->callbacks->string(g->ctx, bytes, count);
}

bool jsnpg_key(generator *g, const byte *bytes, size_t count)
{
        if(!g || !bytes) return false;

        assert(can_key(g));

        return (!g->callbacks->key) || g->callbacks->key(g->ctx, bytes, count);
}

bool jsnpg_start_array(generator *g)
{
        if(!g) return false;

        assert(can_push(g, STACK_ARRAY));

        return  (!g->callbacks->start_array) ||g->callbacks->start_array(g->ctx);
}

bool jsnpg_end_array(generator *g)
{
        if(!g) return false;

        assert(can_pop(g, STACK_ARRAY));

        return  (!g->callbacks->end_array) ||g->callbacks->end_array(g->ctx);
}

bool jsnpg_start_object(generator *g)
{
        if(!g) return false;

        assert(can_push(g, STACK_OBJECT));

        return (!g->callbacks->start_object) ||g->callbacks->start_object(g->ctx);
}

bool jsnpg_end_object(generator *g)
{
        if(!g) return false;

        assert(can_pop(g, STACK_OBJECT));

        return  (!g->callbacks->end_object) ||g->callbacks->end_object(g->ctx);
}

// A generator created by the end user may have been used before so
// make sure it is fit for purpose
static generator *generator_reset(generator *g, unsigned flags)
{
        g->count = 0;
        g->validate_utf8 = !(flags & JSNPG_ALLOW_INVALID_UTF8_OUT);
        g->error = (error_info) {};
        return g;
}

// Create a new generator
// Generator is a top level object so creates its own allocator
// to manage all memory allocations performed by the generator
// (and to free them once the generator is freed)
static generator *generator_new(unsigned stack_size, unsigned flags)
{

#ifdef NDEBUG
        // generator stack is only used to validate generated output 
        // when debugging.
        // It is ignored in NDEBUG builds
        stack_size = 0;
#endif

        allocator *a = allocator_new();
        if(!a)
                return NULL;
        generator *g = allocator_alloc(a, sizeof(generator)
                        + (stack_size >> 3));
        if(!g) {
                allocator_free(a);
                return NULL;
        }

        g->allocator = a;
        g->key_next = false;
        g->validate_utf8 = !(flags & JSNPG_ALLOW_INVALID_UTF8_OUT);

        g->stack = (stack) {
                .ptr = 0,
                .size = stack_size,
                .stack = ((byte *)g) + sizeof(generator)
        };

        return g;
}

static generator *generator_set_callbacks(
                generator *g,
                callbacks *callback_fns, 
                void *ctx)
{
        g->callbacks = callback_fns;
        g->ctx = ctx;
        return g;
}

// External API
// Validate arguments

void jsnpg_generator_free(generator *g)
{
        if(!g)
                return;

        allocator_free(g->allocator);
}

// Creates generator configured for user specified options
// Checks that there is at most one output type (dom or callbacks)
// And outputs as JSON text if no output specified
// Indent is only used for JSON text and a max limited to 8
generator *jsnpg_generator_new_opt(generator_opts opts)
{
        unsigned flags = opts.allow;

        if(1 < (opts.dom == true) + (opts.callbacks != NULL))
                return NULL;
        
        unsigned indent = opts.indent <= 8 ? opts.indent : 8;
        unsigned stack_size = get_stack_size(opts.max_nesting);

        generator *g = generator_new(stack_size, flags);
        if(!g)
                return NULL;

        else if(opts.dom)
                return dom_generator(g);
        else if(opts.callbacks)
                return generator_set_callbacks(g, opts.callbacks, opts.ctx);
        else
                return json_generator(g, indent);
}

error_info jsnpg_result_error(generator *g)
{
        if(g)
                return g->error;
        else
                return (error_info){};
}
