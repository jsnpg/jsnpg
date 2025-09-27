/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * parse.c
 *   the main JSON parse loop
 */

/*
 * Use the parser to parse JSON values and pass the results to the generator 
 * 
 * Given the nested nature of JSON it would make sense to parse 
 * arrays and objects recursively however that can run into stack problems
 * with deeply nested input.
 *
 * This implementation flattens the parse into a loop with the nesting
 * levels being tracked in a bit stack (1/0 array/object)
 *
 * The result is fairly complex but the outline of the loop is:
 *
 * - At the top of the loop we are expecting a JSON value, or key:value pair
 *
 * - If we are in an object then parse the key:
 *
 * - We are now expecting a value, the type of which can be determined
 *   by the first character
 *
 * - If we have open array/object then we cannot immediately produce a value 
 *   unless the array/object is empty, so we check for empty straight away.
 *   If it is not empty we go back to the top as we now expect a value or key:value.
 *
 * - We now have a parsed value so need to check for end array/object
 *   and comma separator.  
 *   Need to handle multiple endings such as '... }]], ...'
 * 
 * - Once we have parsed a JSON value we have either finished, if at the
 *   top level, or we need to go round again
 */

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
static bool void_boolean(void *ctx, bool is_true) { return true; }
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

static void parse_generate(parser *p, generator *g)
{
        const boolean gen_boolean           = g->callbacks->boolean      ? g->callbacks->boolean      : void_boolean;
        const null gen_null                 = g->callbacks->null         ? g->callbacks->null         : void_null;
        const integer gen_integer           = g->callbacks->integer      ? g->callbacks->integer      : void_integer;
        const real gen_real                 = g->callbacks->real         ? g->callbacks->real         : void_real;
        const string gen_string             = g->callbacks->string       ? g->callbacks->string       : void_string;
        const key gen_key                   = g->callbacks->key          ? g->callbacks->key          : void_key;
        const start_array gen_start_array   = g->callbacks->start_array  ? g->callbacks->start_array  : void_start_array;
        const end_array gen_end_array       = g->callbacks->end_array    ? g->callbacks->end_array    : void_end_array;
        const start_object gen_start_object = g->callbacks->start_object ? g->callbacks->start_object : void_start_object;
        const end_object gen_end_object     = g->callbacks->end_object   ? g->callbacks->end_object   : void_end_object;

        void *gen_ctx = g->ctx;
        
        memory_input_stream *const mis = p->mis;
        const unsigned flags = p->flags;
        const bool opt_trailing_commas = flags & JSNPG_ALLOW_TRAILING_COMMAS;
 
        // Easier for us to think in terms of validating rather than allowing invalid
        const bool validate_utf8 = !(flags & JSNPG_ALLOW_INVALID_UTF8_IN);

        byte *bytes;
        size_t count;

        bool more_todo = true;

        // STACK_NONE   - at the base level, not in object or array
        // STACK_OBJECT - in an object
        // STACK_ARRY   - in an array
        int stack_type = STACK_NONE;

        byte b;

        do {
L_KEY:
                if(stack_type == STACK_OBJECT) {
                        if(!mis_next(mis, '"'))
                                throw_parse_error(p, JSNPG_ERROR_EXPECTED_KEY);

                        count = parse_string(p, &bytes, validate_utf8);
                        if(!mis_consume_next(mis, ':'))
                                throw_parse_error(p, JSNPG_ERROR_EXPECTED_KEY);

                        if(!gen_key(gen_ctx, bytes, count)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                }
L_VALUE:
                b = mis_peek(mis);
                switch(b) {
                case '[':
                        stack_type = parse_start_array(p);
                        if(!gen_start_array(gen_ctx)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        if(opt_trailing_commas && mis_consume_next(mis, ',')) {
                                if(!mis_consume_next(mis, ']'))
                                        throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
                        }
                        if(mis_next(mis, ']')) {
                                stack_type = parse_end_array(p);
                                if(!gen_end_array(gen_ctx))
                                        throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                                break;
                        }
                        goto L_VALUE;

                case '{':
                        stack_type = parse_start_object(p);
                        if(!gen_start_object(gen_ctx)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        if(opt_trailing_commas && mis_consume_next(mis, ',')) {
                                if(!mis_consume_next(mis, '}'))
                                        throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
                        }
                        if(mis_next(mis, '}')) {
                                stack_type = parse_end_object(p);
                                if(!gen_end_object(gen_ctx))
                                        throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                                break;
                        }
                        goto L_KEY;

                case '"':
                        count = parse_string(p, &bytes, validate_utf8);
                        if(!gen_string(gen_ctx, bytes, count)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        break;

                 case 't':
                        parse_true(p);
                        if(!gen_boolean(gen_ctx, true)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        break;

                case 'f':
                        parse_false(p);
                        if(!gen_boolean(gen_ctx, false)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        break;

                case 'n':
                        parse_null(p);
                        if(!gen_null(gen_ctx)) 
                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        break;

                default:
                        if(b == '-' || ('0' <= b && b <= '9')) {
                                double d;
                                long l;
                                if(JSNPG_REAL == parse_number(p, &d, &l)) {
                                        if(!gen_real(gen_ctx, d)) 
                                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                                } else {
                                        if(!gen_integer(gen_ctx, l)) 
                                                throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                                } 
                                break;
                        }
                        if(b == mis_consume_whitespace(mis))
                                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);

                        goto L_VALUE;

                }

                while(true) {
                        if(mis_consume_next(mis, ',')) {
                                // Optional comma only if followed by } or ]
                                if(!(opt_trailing_commas &&
                                                (mis_next(mis, '}') 
                                                 || mis_next(mis, ']'))))
                                        break;
                        }
                        if(mis_next(mis, '}') && stack_type == STACK_OBJECT) {
                                stack_type = parse_end_object(p);
                                if(!gen_end_object(gen_ctx))
                                        throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        } else if(mis_next(mis, ']') && stack_type == STACK_ARRAY) {
                                stack_type = parse_end_array(p);
                                if(!gen_end_array(gen_ctx))
                                        throw_parse_error(p, JSNPG_ERROR_TERMINATED);
                        } else if(stack_type == STACK_NONE) {
                                more_todo = false;
                                break;
                        } else {
                                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
                        }

                }

        } while(more_todo);

        mis_consume_whitespace(mis);

}

static parse_result parse(parser *p, generator *g)
{
        const bool multiple_values = p->flags & JSNPG_ALLOW_MULTIPLE_VALUES;
        const bool trailing_chars = p->flags & JSNPG_ALLOW_TRAILING_CHARS;

        parse_result val;

        if(0 == setjmp(p->env)) {
                while(true) {
                        parse_generate(p, g);

                        if(!mis_eof(p->mis)) {
                                if(multiple_values)
                                        continue;
                                if(!trailing_chars)
                                        throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
                        }
                        break;
                }
                val = make_parse_result(p, JSNPG_EOF);
        } else {
                val = make_pg_error_return(p, g);
        }

        return val;
}

parse_result jsnpg_parse_opt(parse_opts opts)
{
        generator *g;
        generator *new_g = NULL;
        parser *p;
        byte *bytes = opts.bytes;
        char *string = opts.string;
        size_t count = opts.count;

        if(1 != (opts.callbacks != NULL) + (opts.generator != NULL)) {
                return make_error_return(JSNPG_ERROR_OPT, 0);
        }
        
        if(opts.callbacks) {
                g = generator_new(0, opts.allow);
                new_g = g;
                if(!g) {
                        return make_error_return(JSNPG_ERROR_ALLOC, 0);
                }
                generator_set_callbacks(g, opts.callbacks, opts.ctx);
        } else {
                g = generator_reset(opts.generator, opts.allow);
        }

        if(!opts.writeable) {
                if(string) {
                        count = strlen(string);
                        string = (char *)copy_bytes(
                                        g->allocator, 
                                        (byte *)string, 
                                        1 + count);
                        if(!string) {
                                jsnpg_generator_free(new_g);
                                return make_error_return(JSNPG_ERROR_ALLOC, 0);
                        }
                }
                if(bytes) {
                        count = opts.count;
                        bytes = copy_bytes(
                                        g->allocator,
                                        bytes,
                                        count);
                        if(!bytes) {
                                jsnpg_generator_free(new_g);
                                return make_error_return(JSNPG_ERROR_ALLOC, 0);
                        }
                }
        } 


        
        p = jsnpg_parser_new_opt((parser_opts) {
                        .max_nesting = opts.max_nesting,
                        .allow = opts.allow,
                        .bytes = bytes,
                        .count = count,
                        .string = string,
                        .writeable = true,
                        .dom = opts.dom
                        });

        if(!p) {
                jsnpg_generator_free(new_g);
                return make_error_return(JSNPG_ERROR_ALLOC, 0);
        } else if(p->result.type == JSNPG_ERROR) {
                jsnpg_generator_free(new_g);
                return p->result;
        }
        
        parse_result result;
        if(opts.dom)
                result = dom_parse(p, g);
        else
                result = parse(p, g);

        jsnpg_parser_free(p);
        jsnpg_generator_free(new_g);

        return result;
}
