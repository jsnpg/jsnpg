/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * parser.c
 *   provides parsing functionality shared by the sax/event style parser
 *   and the pull parser
 */

#include <setjmp.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

static inline bool parser_in_object(parser *p)
{
        return stack_peek(&p->stack) == STACK_OBJECT;
}

static inline bool parser_in_array(parser *p)
{
        return stack_peek(&p->stack) == STACK_ARRAY;
}

static size_t parse_position(parser *p)
{
        return p->mis->start ? mis_tell(p->mis) : 0;
}

static parse_result make_parse_result(parser *p, json_type type, ...)
{
        va_list ap;
        parse_result result;
        va_start(ap, type);
        
        result.type = type;
        result.position = parse_position(p);

        switch(type) {
        case JSNPG_STRING:
        case JSNPG_KEY:
                result.string.bytes = va_arg(ap, byte *);
                result.string.count = va_arg(ap, size_t);
                break;
        case JSNPG_REAL:
                result.number.real = va_arg(ap, double);
                break;
        case JSNPG_INTEGER:
                result.number.integer = va_arg(ap, long);
                break;
        case JSNPG_ERROR:
                result.error.code = va_arg(ap, error_code);
                result.error.text = error_text(result.error.code);
                break;
        default:
                // nothing to do
        }
        va_end(ap);

        return result;
}

[[noreturn]]
static void throw_parse_error_at(parser *p, error_code code, size_t at)
{
        p->result = make_error_return(code, at);
        longjmp(p->env, 1);
}

[[noreturn]]
static void throw_parse_error(parser *p, error_code code)
{
        throw_parse_error_at(p, code, parse_position(p));
}

static inline int parse_start_object(parser *p)
{
        assert(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static inline int parse_end_object(parser *p)
{
        assert(mis_peek(p->mis) == '}');
        assert(stack_peek(&p->stack) == STACK_OBJECT);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSNPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline int parse_start_array(parser *p)
{
        assert(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static inline int  parse_end_array(parser *p)
{
        assert(mis_peek(p->mis) == ']');
        assert(stack_peek(&p->stack) == STACK_ARRAY);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSNPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline void parse_true(parser *p)
{
        memory_input_stream *const mis = p->mis;

        assert(mis_peek(mis) == 't');

        mis_take(mis);
        if(!mis_consume(mis, 'r')
                        || !mis_consume(mis, 'u')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
}

static inline void parse_false(parser *p)
{
        memory_input_stream *const mis = p->mis;

        assert(mis_peek(mis) == 'f');

        mis_take(mis);
        if(!mis_consume(mis, 'a')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 's')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
}

static inline void parse_null(parser *p)
{
        memory_input_stream *const mis = p->mis;

        assert(mis_peek(mis) == 'n');

        mis_take(mis); // 'n'
        if(!mis_consume(mis, 'u')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 'l'))
                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
}

static unsigned parse_hex4(parser *p)
{
        memory_input_stream *const mis = p->mis;

        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                byte c = mis_peek(mis);
                if(!(byte_map[c] & BYTE_HEX_DIGIT))
                        throw_parse_error(p, JSNPG_ERROR_ESCAPE);

                codepoint <<= 4;
                codepoint += byte_map_extra[c];

                mis_take(mis);
        }
        return codepoint;
}

static unsigned parse_escape(parser *p)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        memory_input_stream *const mis = p->mis;

        mis_take(mis); // '\\'

        const byte e = mis_peek(mis);

        if(escape[e]) {
                mis_take(mis);
                return (unsigned)escape[e];
        }

        if(e == 'u') {
                mis_take(mis);
                unsigned codepoint = parse_hex4(p);
                if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        // Got surrogate but high (first one) must be 0xD800-0xDBFF
                        if(codepoint <= 0xDBFF) {
                                // high surrogate must be followed by low
                                if(!(mis_consume(mis, '\\')
                                                && mis_consume(mis, 'u')))
                                        throw_parse_error(p, JSNPG_ERROR_SURROGATE);

                                const unsigned codepoint2 = parse_hex4(p);

                                if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
                                        throw_parse_error(p, JSNPG_ERROR_SURROGATE);

                                codepoint = (((codepoint - 0xD800) << 10)
                                                | (codepoint2 - 0xDC00)) + 0x10000;
                        } else {
                                throw_parse_error(p, JSNPG_ERROR_SURROGATE);
                        }
                }
                return codepoint;
        } else {
                throw_parse_error(p, JSNPG_ERROR_ESCAPE);
        }
}

static size_t parse_string_in_stream(parser *p, byte **bytes, const bool validate_utf8)
{
        memory_input_stream *const mis = p->mis;

        mis_string_start(mis);

        while(true) {
                unsigned type;
                byte c;
                while((type = byte_map[(c = mis_peek(mis))]) & BYTE_ASCII_STRING)
                        mis_take(mis);

                if(c == '"') {
                        return mis_string_complete(mis, bytes);
                } else if(c == '\\') {
                        mis_string_update(mis);
                        unsigned codepoint = parse_escape(p);
                        utf8_encode(codepoint, mis_writer(mis));
                        mis_string_restart(mis);
                } else if(c >= 0x80) {
                        mis_take(mis);
                        if(!validate_utf8) {
                                while(mis_peek(mis) >= 0x80)
                                        mis_take(mis);
                        }else if(type & BYTE_LEADER_2) {
                                if(!(byte_map[mis_peek(mis)] & BYTE_CONTINUATION))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                        } else if(type & BYTE_LEADER_3) {
                                unsigned next = byte_map_extra[c];
                                if(!(byte_map[mis_peek(mis)] & next))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                                if(!(byte_map[mis_peek(mis)] & BYTE_CONTINUATION))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                        } else if(type & BYTE_LEADER_4) {
                                unsigned next = byte_map_extra[c];
                                if(!(byte_map[mis_peek(mis)] & next))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                                if(!(byte_map[mis_peek(mis)] & BYTE_CONTINUATION))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                                if(!(byte_map[mis_peek(mis)] & BYTE_CONTINUATION))
                                        throw_parse_error(p, JSNPG_ERROR_UTF8);
                                mis_take(mis);
                        } else {
                                throw_parse_error(p, JSNPG_ERROR_UTF8);
                        }
                } else { // if(c < 0x20) {
                        throw_parse_error(p, JSNPG_ERROR_INVALID);
                }
        }
}

static inline size_t parse_string(parser *p, byte **bytes, const bool validate_utf8)
{
        assert(mis_peek(p->mis) == '"');

        mis_take(p->mis); // "
        return parse_string_in_stream(p, bytes, validate_utf8);
}

// This function, along with formatting numbers, takes much more cpu
// than any other parse function so we try and save as many tests/branches as we can
//
// If the number is supplied without decimal point and exponent, and it falls 
// in the range of a signed long, we treat it as a signed long.
// Otherwise it is stored in a double.
// We try to parse doubles with our C conversion of 
// https://github.com/lemire/fast_double_parser which can be significantly
// faster than the standard library strtod but does not work for all
// valid double precision numbers.  We fallback to strtod where necessary.

static json_type parse_number(parser *p, double *real_result, long *integer_result)
{
        uint64_t sum = 0;
        size_t pos = 0;
        int exponent = 0;
        bool exp_negative = false;
        int exp = 0;
        bool is_real = false;

        memory_input_stream *const mis = p->mis;

        // If fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        size_t start_pos = mis_tell(mis);

        const byte *src = mis_current(mis);

        bool negative = *src == '-';
        src += negative;

        unsigned dtype = byte_map_digits[*src];
        if(dtype > 9)
                throw_parse_error(p, JSNPG_ERROR_NUMBER);
        
        sum = dtype;
        pos = 1;
        if(sum) {
                while(pos < 19) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_DP;
                        sum = sum * 10 + dtype;
                }
                while((dtype = byte_map_digits[src[pos++]]) < 10)
                        exponent++;
        } else {
                dtype = byte_map_digits[src[pos++]];
        }
L_DP: 
        if(dtype == BYTE_MAP_DIGIT_DECIMAL_POINT) {
                int mark = pos;
                is_real = true;

                dtype = byte_map_digits[src[pos++]];
                if(dtype > 9)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

                if(sum == 0 && dtype == 0) {
                        do {
                                exponent--;
                                dtype = byte_map_digits[src[pos++]];
                        } while(dtype == 0);
                        if(dtype > 9) goto L_EXP;
                }
                exponent--;
                sum = sum * 10 + dtype;

                size_t max_pos = 19 + pos - mark;
                while(pos < max_pos) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_EXP;
                        sum = sum * 10 + dtype;
                        exponent--;
                }
                while((dtype = byte_map_digits[src[pos++]]) < 10)
                                ;
        }
L_EXP:
        if(dtype == BYTE_MAP_DIGIT_EXPONENT) {
                is_real = true;
                dtype = byte_map_digits[src[pos++]];
                
                exp_negative = dtype == BYTE_MAP_DIGIT_MINUS;
                if(exp_negative || dtype == BYTE_MAP_DIGIT_PLUS)
                        dtype = byte_map_digits[src[pos++]];

                if(dtype > 9)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

                exp = dtype;
                for(int i = 0 ; i < 10 ; i++) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_DONE;
                        exp = exp * 10 + dtype;
                }
                if(!exp_negative)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);
        }

L_DONE:
        if(exp_negative && exp > 0)
                exponent -= exp;
        else
                exponent += exp; 

        mis_adjust(mis, (byte *)(src + (pos - 1)));

        if(is_real || exponent != 0 || sum > (((uint64_t)LONG_MAX) + negative)) {
                bool success = false;
                if (exponent >= FASTFLOAT_SMALLEST_POWER &&
                                exponent <= FASTFLOAT_LARGEST_POWER) {
                        *real_result = compute_float_64(exponent, sum, negative, &success);
                }
                if(!success) {
                        const char *start = (const char *)mis_at(mis, start_pos);
                        char *end = parse_float_strtod(start, real_result);
                        if(!end) {
                                if(exponent < FASTFLOAT_SMALLEST_POWER)
                                        *real_result = negative ? -0.0 : 0.0;
                                else
                                        throw_parse_error(p, JSNPG_ERROR_NUMBER);
                        } else {
                                mis_adjust(mis, (byte *)end);
                        }
                }
                return JSNPG_REAL;
        } else {
                *integer_result = negative ? -sum : sum;
                return JSNPG_INTEGER;
        }
}

static byte *copy_bytes(allocator *a, byte *bytes, size_t count)
{
        byte *b = allocator_alloc(a, count + JSNPG_WRITEABLE_PADDING);
        if(b)
                memcpy(b, bytes, count);
        return b;
}

static parser *parser_set_bytes(parser *p, byte *bytes, size_t count, bool writeable)
{
        // Skip leading byte order mark
        unsigned skip = utf8_bom_bytes(bytes, count);
        bytes += skip;
        count -= skip;
        byte *b;

        // If not writeable, we make a copy, as the advantages of having a null
        // terminated, padded, writeable, byte array outweighs the cost of copying
        if(writeable) {
                b = bytes;
        } else {
                b = copy_bytes(p->allocator, bytes, count);
                if(!b)
                        return NULL;
        }

        memset(b + count, '\0', JSNPG_WRITEABLE_PADDING); 

        mis_set_bytes(p->mis, b, count);

        return p;
}

static parser *parser_set_dom_info(parser *p, dom_info di)
{
        p->dom_info = di;
        return p;
}

static parser *parser_new(allocator *a, unsigned stack_size, unsigned flags)
{
        // The bit stack (keeps track of object/array nesting)
        // Is allocated space directly after the parser struct
        size_t struct_bytes = sizeof(parser);
        parser *p = allocator_alloc(a, struct_bytes + (unsigned)((stack_size + 7) / 8));
        if(!p)
                return NULL;

        p->allocator = a;
        p->result = (parse_result) {};

        p->mis = mis_new(a, flags & JSNPG_ALLOW_COMMENTS);
        if(!p->mis)
                return NULL;

        p->dom_info = (dom_info){};

        p->stack = (stack) {
                .ptr = 0,
                .size = stack_size,
                .stack = (((byte *)p) + struct_bytes)
        };
        p->state = STATE_START;
        p->flags = flags;

        return p;
}

// External API calls
// Make sure to check valid arguments

bool jsnpg_parse_streq(parser *p, char *str)
{
        if(!p || !str) return false;

        parse_result result = p->result;

        if(result.type != JSNPG_KEY && result.type != JSNPG_STRING)
                return false;

        const byte *bytes = result.string.bytes;
        size_t count = result.string.count;

        // if strncmp returns true then str must be at least count bytes long
        return 0 == strncmp(str, (const char *)bytes, count)
                        && '\0' == str[count];
}

void jsnpg_parser_free(parser *p)
{
        if(p)
                allocator_free(p->allocator);
}


parser *jsnpg_parser_new_opt(parser_opts opts)
{
        unsigned stack_size = get_stack_size(opts.max_nesting);
        unsigned flags = opts.allow;

        allocator *a = allocator_new();
        if(!a)
                return NULL;

        parser *p = parser_new(a, stack_size, flags);

        if(!p) {
                allocator_free(a);
                return NULL;
        }

        if(1 != (opts.bytes != NULL) + (opts.string != NULL) + (opts.dom != NULL)) {
                p->result = make_error_return(JSNPG_ERROR_OPT, 0);
                return p;
        }

        if(opts.bytes) {
                p = parser_set_bytes(p, opts.bytes, opts.count, opts.writeable);
        } else if(opts.string) {
                p = parser_set_bytes(p, (byte *)opts.string, strlen(opts.string), false);
        } else if(opts.dom) {
                p = parser_set_dom_info(p, dom_parser_info(opts.dom));
        }

        if(!p)
                allocator_free(a);

        return p;
}

parse_result jsnpg_parse_result(parser *p)
{
        if(p)
                return p->result;
        else
                return (parse_result){};
}
