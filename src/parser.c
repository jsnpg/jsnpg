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
        assert(p);

        return stack_peek(&p->stack) == STACK_OBJECT;
}

static inline bool parser_in_array(parser *p)
{
        assert(p);

        return stack_peek(&p->stack) == STACK_ARRAY;
}

static size_t parse_position(parser *p)
{
        assert(p);
        assert(p->mis);

        return p->mis->start ? mis_tell(p->mis) : 0;
}

static parse_result make_parse_result(parser *p, json_type type, ...)
{
        assert(p);

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
        case JSNPG_BOOLEAN:
                result.boolean = va_arg(ap, int); // ... promotes bool to int
                break;
        case JSNPG_REAL:
                result.real = va_arg(ap, double);
                break;
        case JSNPG_INTEGER:
                result.integer = va_arg(ap, long);
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
        assert(p);

        p->result = make_error_return(code, at);
        longjmp(p->env, 1);
}

[[noreturn]]
static void throw_parse_error(parser *p, error_code code)
{
        assert(p);

        throw_parse_error_at(p, code, parse_position(p));
}

static inline int parse_start_object(parser *p)
{
        assert(p);
        assert(p->mis);

        assert(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static inline int parse_end_object(parser *p)
{
        assert(p);
        assert(p->mis);

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
        assert(p);
        assert(p->mis);

        assert(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static inline int  parse_end_array(parser *p)
{
        assert(p);
        assert(p->mis);

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
        assert(p);
        assert(p->mis);

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
        assert(p);
        assert(p->mis);

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
        assert(p);
        assert(p->mis);

        memory_input_stream *const mis = p->mis;

        assert(mis_peek(mis) == 'n');

        mis_take(mis); // 'n'
        if(!mis_consume(mis, 'u')
                        || !mis_consume(mis, 'l')
                        || !mis_consume(mis, 'l'))
                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
}

// Parse four hex digits to unsigned int
// Allows 0-9, a-f, A-F
static unsigned parse_hex4(parser *p)
{
        assert(p);
        assert(p->mis);

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

// Parse JSON escape into Unicode codepoint
// Simple, one char, escapes
// \uXXXX - four hex digits representing codepoint except
//          surrogate values are allowed
// If a high-surrogate is parsed then it MUST be followed by 
// another \uXXXX represnting the low surrogate to establish
// the correct codepoint
static unsigned parse_escape(parser *p)
{
        assert(p);
        assert(p->mis);

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

// Parse string after initial " has been consumed
// Rewrites the input stream to resolve escapes to their correct bytes
// Validates UTF8 multi-byte sequences
// Rejects <0x20 bytes
//
// Returns length of string and sets string bytes in out param <bytes>
static size_t parse_string_in_stream(parser *p, byte **bytes, const bool validate_utf8)
{
        assert(p);
        assert(bytes);
        assert(p->mis);
        
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
                        // utf8 multibyte sequences
                        // should be in separate function
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
        assert(p);
        assert(bytes);
        assert(p->mis);

        assert(mis_peek(p->mis) == '"');

        mis_take(p->mis); // "
        return parse_string_in_stream(p, bytes, validate_utf8);
}

// this function, along with formatting numbers, takes much more cpu
// than any other parse function so we try and save as many tests/branches as we can
//
// if the number is supplied without decimal point and exponent, and it falls 
// in the range of a signed long, we treat it as a signed long.
// otherwise it is stored in a double.
// we try to parse doubles with our c conversion of 
// https://github.com/lemire/fast_double_parser which can be significantly
// faster than the standard library strtod but does not work for all
// valid double precision numbers.  we fallback to strtod where necessary.

static json_type parse_number(parser *p, double *real_result, long *integer_result)
{
        assert(p);
        assert(real_result);
        assert(integer_result);
        assert(p->mis);

        uint64_t sum = 0;
        size_t pos = 0;
        int exponent = 0;
        bool exp_negative = false;
        int exp = 0;
        bool is_real = false;

        memory_input_stream *const mis = p->mis;

        // if fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        size_t start_pos = mis_tell(mis);

        // we extract the actual bytes from the stream here and correct
        // the read position in the stream at the end
        const byte *src = mis_current(mis);

        bool negative = *src == '-';
        src += negative;

        // byte_map_digits '0'->0, ..., '9'->9 + special values for .ee+-
        // everything else is 0xff
        unsigned dtype = byte_map_digits[*src];

        if(dtype > 9)
                throw_parse_error(p, JSNPG_ERROR_NUMBER);
        
        sum = dtype;
        pos = 1;
        // can't have leading 0s so if we have 0 then dont look for more digits
        if(sum) {
                // 19 digits is the most that can be stored in signed long
                while(pos < 19) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_DP;
                        sum = sum * 10 + dtype;
                }

                // we can't store these digits but need to track the size
                // of the number
                while((dtype = byte_map_digits[src[pos++]]) < 10)
                        exponent++;
        } else {
                dtype = byte_map_digits[src[pos++]];
        }

        // all pre decimal point digits have been processed

L_DP: 
        if(dtype == BYTE_MAP_DIGIT_DECIMAL_POINT) {
                int mark = pos;
                is_real = true;

                dtype = byte_map_digits[src[pos++]];

                // must have at least one digit after .
                if(dtype > 9)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

                // skip over 0.0000... 
                // keeping track of size of eventual number
                if(sum == 0 && dtype == 0) {
                        do {
                                exponent--;
                                dtype = byte_map_digits[src[pos++]];
                        } while(dtype == 0);
                        if(dtype > 9) goto L_EXP;
                }

                exponent--;
                sum = sum * 10 + dtype;

                // if we have any digits left out of our allowance of 19
                // then collect them
                size_t max_pos = 19 + pos - mark;
                while(pos < max_pos) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_EXP;
                        sum = sum * 10 + dtype;
                        exponent--;
                }

                // discard any after that
                while((dtype = byte_map_digits[src[pos++]]) < 10)
                                ;
        }

        // all digits up to exponent now processed
        //
L_EXP:
        if(dtype == BYTE_MAP_DIGIT_EXPONENT) {
                is_real = true;
                dtype = byte_map_digits[src[pos++]];
                
                // check for +- after ee
                exp_negative = dtype == BYTE_MAP_DIGIT_MINUS;
                if(exp_negative || dtype == BYTE_MAP_DIGIT_PLUS)
                        dtype = byte_map_digits[src[pos++]];

                // then insist we have at least one digit
                if(dtype > 9)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

                exp = dtype;
                // too many numbers after exponent will error anyway
                // if we stop before they are gone, but more than 9
                // is just too much
                for(int i = 0 ; i < 9 ; i++) {
                        dtype = byte_map_digits[src[pos++]];
                        if(dtype > 9) goto L_DONE;
                        exp = exp * 10 + dtype;
                }

                // numbers too small for a double we parse as +-0.0
                // but we cannot do anything with numbers that are too big
                if(!exp_negative)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);
        }

        // All done collecting digits, now convert to numbers

L_DONE:
        // Combine implicit exponent from position of digits and decimal point
        // and explicit exponent from eE+- digits
        if(exp_negative)
                exponent -= exp;
        else
                exponent += exp; 

        // Update read position in input stream
        mis_adjust(mis, (byte *)(src + (pos - 1)));

        if(is_real || exponent != 0 || sum > (((uint64_t)LONG_MAX) + negative)) {
                bool success = false;

                // First try fast float conversion (if in the allowable range)
                if (exponent >= FASTFLOAT_SMALLEST_POWER &&
                                exponent <= FASTFLOAT_LARGEST_POWER) {
                        *real_result = compute_float_64(exponent, sum, negative, &success);
                }
                // If that didn't work (or we didn't even try)
                // Then go the slow route
                if(!success) {
                        const char *start = (const char *)mis_at(mis, start_pos);
                        char *end = parse_float_strtod(start, real_result);

                        if(!end) {
                                // strtod failed so ...
                                // if number is very small => +-0.0
                                // otherwise give up
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
        assert(a);
        assert(bytes);

        byte *b = allocator_alloc(a, count + JSNPG_WRITEABLE_PADDING);
        if(b)
                memcpy(b, bytes, count);
        return b;
}

// Assign to the parser the JSON input it is to parse
// It skips any leaning Byte Order Mark
// and, if the bytes are not writeable, makes a writeable (+padded) copy
static parser *parser_set_bytes(parser *p, byte *bytes, size_t count, bool writeable)
{
        assert(p);
        assert(bytes);
        assert(p->mis);

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

// For a DOM pull parse the parser needs to keep track of where it is up to
static parser *parser_set_dom_info(parser *p, dom_info di)
{
        assert(p);

        p->dom_info = di;
        return p;
}

static parser *parser_new(allocator *a, unsigned stack_size, unsigned flags)
{
        assert(a);

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
