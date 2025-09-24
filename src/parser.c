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
        ASSERT(mis_peek(p->mis) == '{');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_OBJECT))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_OBJECT;
}

static inline int parse_end_object(parser *p)
{
        ASSERT(mis_peek(p->mis) == '}');
        ASSERT(stack_peek(&p->stack) == STACK_OBJECT);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSNPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline int parse_start_array(parser *p)
{
        ASSERT(mis_peek(p->mis) == '[');

        mis_take(p->mis);
        if(-1 == stack_push(&p->stack, STACK_ARRAY))
                throw_parse_error(p, JSNPG_ERROR_STACK_OVERFLOW);
        return STACK_ARRAY;
}

static inline int  parse_end_array(parser *p)
{
        ASSERT(mis_peek(p->mis) == ']');
        ASSERT(stack_peek(&p->stack) == STACK_ARRAY);

        mis_take(p->mis);
        int type = stack_pop(&p->stack);

        if(-1 == type)
                throw_parse_error(p, JSNPG_ERROR_STACK_UNDERFLOW);
        return type;
}

static inline void parse_true(parser *p)
{
        memory_input_stream *const mis = p->mis;

        ASSERT(mis_peek(mis) == 't');

        mis_take(mis);
        if(!mis_consume(mis, 'r')
                        || !mis_consume(mis, 'u')
                        || !mis_consume(mis, 'e'))
                throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
}

static inline void parse_false(parser *p)
{
        memory_input_stream *const mis = p->mis;

        ASSERT(mis_peek(mis) == 'f');

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

        ASSERT(mis_peek(mis) == 'n');

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

static inline byte mis_consume_whitespace(memory_input_stream *mis)
{
        byte c;

        while(byte_map[(c = mis_peek(mis))] & BYTE_WHITESPACE)
                 mis_take(mis);
       
        return c;

}

static byte consume_whitespace(parser *p, const bool allow_comments)
{
        memory_input_stream *const mis = p->mis;

        byte c = mis_peek(mis);

        if(!((byte_map[c] & BYTE_WHITESPACE) || (allow_comments && c == '/')))
                return c;

        if(!allow_comments) {
                mis_take(mis); // c, whitespace

                return mis_consume_whitespace(mis);
        }

        while(true) {
                c = mis_consume_whitespace(mis);

                if(c != '/')
                        return c;

                mis_take(mis); // '/'
                c = mis_peek(mis);
                if(c == '*') {
                        mis_take(mis); // '*'
                        while(true) {
                                c = mis_find(mis, '*');
                                if(c == '*') {
                                        mis_take(mis); // '*'
                                        if(mis_consume(mis, '/'))
                                                break;
                                }
                                if(mis_eof(mis))
                                        return '\0';
                        }
                } else if(c == '/') {
                        c = mis_find(mis, '\n');
                        if(mis_eof(mis))
                                return '\0';
                } else {        
                        throw_parse_error(p, JSNPG_ERROR_UNEXPECTED);
                }
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
        ASSERT(mis_peek(p->mis) == '"');

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

// static json_type parse_number(parser *p, double *real_result, long *integer_result)
// {
//         static uint64_t pow10[] = {
//                 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
//                 1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18 };
//         // We only take the most significant digits
//         // Max digits for long is 19
//         // double is 15-17 so we may lose some digits when converting
//         static const int max_sig_digits = 19;
//
//         memory_input_stream *const mis = p->mis;
//
//         // If fast parsing fails might need to call
//         // strtod, which needs to start from the beginning
//         size_t start_pos = mis_tell(mis);
//
//         const byte *src = mis_current(mis);
//
//         bool force_double = false;
//         int sig_digits = 0;
//         int exponent = 0;
//         uint64_t sum = 0;
//
//         bool negative = (*src == '-');
//         src += negative;
//         if(*src != '0') {
//                 digit_parse i = parse_upto_19(src, 0);
//                 if(i.count == 0)
//                         throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                 src += i.count + i.trailing;
//                 sig_digits = i.count;
//                 sum = i.sum;
//                 exponent = i.trailing;
//                 // X.xxx
//                 if(*src == '.') {
//                         force_double = true;
//                         src++;
//                         digit_parse f = parse_upto_19(src, sig_digits);
//                         int fcount = f.count;
//                         if(fcount == 0)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                         src += fcount + f.trailing;
//                         sum *= pow10[fcount];
//                         sum += f.sum;
//                         exponent -= fcount;
//                         sig_digits += fcount;
//                         if(sig_digits == max_sig_digits) {
//                                 while(((byte)(*src - '0')) < 10)
//                                         src++;
//                         }
//                 }
//         } else {
//                 src++;
//                 // 0.xxx
//                 if(*src == '.') {
//                         src++;
//                         force_double = true;
//                         if(((byte)(*src - '0')) > 9)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//
//                         while(*src == '0') {
//                                 src++;
//                                 exponent--;
//                         }
//
//                         digit_parse f = parse_upto_19(src, 0);
//                         int fcount = f.count;
//                         src += fcount + f.trailing;
//                         sum = f.sum;
//                         exponent -= fcount;
//                         while(((byte)(*src - '0')) < 10)
//                                 src++;
//                 }
//         }
//         if(*src == 'e' || *src == 'E') {
//                 force_double = true;
//                 src++;
//                 bool exp_negative = (*src == '-');
//                 src += exp_negative || (*src == '+');
//                 uint32_t exp = *src - '0';
//                 src++;
//                 if(exp > 9)
//                         throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                 byte exp_n = *src - '0';
//                 if(exp_n < 10) {
//                         src++;
//                         exp *= 10;
//                         exp += exp_n;
//                         exp_n = *src - '0';
//                         if(exp_n < 10) {
//                                 src++;
//                                 exp *= 10;
//                                 exp += exp_n;
//                         }
//                 }
//                 exponent += ((int[]){exp, -exp})[exp_negative];
//         }
//
//         mis_adjust(mis, (byte *)src);
//         if(force_double || exponent || sum > (((uint64_t)LONG_MAX) + negative)) {
//                 bool success = false;
//                 if (exponent >= FASTFLOAT_SMALLEST_POWER &&
//                                 exponent <= FASTFLOAT_LARGEST_POWER) {
//                         *real_result = compute_float_64(exponent, sum, negative, &success);
//                 }
//                 if(!success) {
//                         const char *start = (const char *)mis_at(mis, start_pos);
//                         char *end = parse_float_strtod(start, real_result);
//                         if(!end)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                         mis_adjust(mis, (byte *)end);
//                 }
//                 return JSNPG_REAL;
//         } else {
//                 *integer_result = ((int64_t[]){sum, -sum})[negative];
//                 return JSNPG_INTEGER;
//         }
// }

// static json_type parse_number(parser *p, double *real_result, long *integer_result)
// {
//         // We only take the most significant digits
//         // Max digits for long is 19
//         // double is 15-17 so we may lose some digits when converting
//         static const int max_sig_digits = 19;
//         static int sig_digits[2] = { 0, max_sig_digits };
//
//         memory_input_stream *const mis = p->mis;
//
//         // If fast parsing fails might need to call
//         // strtod, which needs to start from the beginning
//         size_t start_pos = mis_tell(mis);
//
//         const byte *src = mis_current(mis);
//
//         int digits = 0;
//         int zeros = 0;
//         int fdigits = 0;
//         int edigits = 0;
//         int exponent = 0;
//         int use_digits;
//         uint64_t sum = 0;
//         const byte *nums;
//         bool ep = false;
//
//         bool negative = (*src == '-');
//         src += negative;
//         if(*src != '0') {
//                 digits = count_digits(src);
//                 sig_digits[0] = digits;
//                 const bool dgt = digits > max_sig_digits;
//                 nums = src;
//                 src += digits;
//                 const bool dp = *src == '.';
//                 ep = (!dp) && (*src == 'e' || *src == 'E');
//
//                 if(digits == 0)
//                         throw_parse_error(p, JSNPG_ERROR_NUMBER);
//
//                 use_digits = sig_digits[dgt];
//                 exponent = digits - use_digits;
//                 sum = parse_digits(nums, 0, use_digits);
//                 if(dp) {
//                         src++;
//                         fdigits = count_digits(src);
//                         const bool err = fdigits == 0; 
//                         sig_digits[0] += fdigits;
//                         const bool fgt = sig_digits[0] > max_sig_digits;
//                         nums = src;
//                         src += fdigits;
//                         ep = *src == 'e' || *src == 'E';
//
//                         if(err)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//
//                         use_digits = sig_digits[fgt] - use_digits;
//                         exponent -= use_digits;
//                         sum = parse_digits(nums, sum, use_digits);
//                 }
//         } else {
//                 src++;
//                 const bool dp = *src == '.';
//                 ep = (!dp) && (*src == 'e' || *src == 'E');
//
//                 if(dp) {
//                         src++;
//                         if(*src == '0') {
//                                 zeros = 1 + count_zeros(src + 1);
//                                 src += zeros;
//                         }
//                         fdigits = count_digits(src);
//                         const bool err = (zeros + fdigits) == 0;
//                         sig_digits[0] = fdigits;
//                         const bool fgt = fdigits > max_sig_digits;
//                         nums = src;
//                         src += fdigits;
//                         ep = *src == 'e' || *src == 'E';
//
//                         if(err)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//
//                         use_digits = sig_digits[fgt];
//                         sum = parse_digits(nums, 0, use_digits);
//                         exponent = -(zeros + use_digits);
//                 }
//         }
//         if(ep) {
//                 src++;
//                 bool exp_negative = (*src == '-');
//                 src += exp_negative || (*src == '+');
//                 uint32_t exp = *src - '0';
//                 src++;
//                 if(exp > 9)
//                         throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                 uint32_t exp_n = *src - '0';
//                 if(exp_n < 10) {
//                         src++;
//                         exp *= 10;
//                         exp += exp_n;
//                         exp_n = *src - '0';
//                         if(exp_n < 10) {
//                                 src++;
//                                 exp *= 10;
//                                 exp += exp_n;
//                         }
//                 }
//                 exponent += ((int[]){exp, -exp})[exp_negative];
//         }
//
//         mis_adjust(mis, (byte *)src);
//         if((zeros + fdigits + edigits) || exponent || ep || sum > (((uint64_t)LONG_MAX) + negative)) {
//                 bool success = false;
//                 if (exponent >= FASTFLOAT_SMALLEST_POWER &&
//                                 exponent <= FASTFLOAT_LARGEST_POWER) {
//                         *real_result = compute_float_64(exponent, sum, negative, &success);
//                 }
//                 if(!success) {
//                         const char *start = (const char *)mis_at(mis, start_pos);
//                         char *end = parse_float_strtod(start, real_result);
//                         if(!end)
//                                 throw_parse_error(p, JSNPG_ERROR_NUMBER);
//                         mis_adjust(mis, (byte *)end);
//                 }
//                 return JSNPG_REAL;
//         } else {
//                 *integer_result = ((int64_t[]){sum, -sum})[negative];
//                 return JSNPG_INTEGER;
//         }
// }

//Lots of sign changing in parse_number so turn off warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

static json_type parse_number(parser *p, double *real_result, long *integer_result)
{
        // We only take the most significant digits
        // Max digits for long is 19
        // double is 15-17 so we may lose some digits when converting
        static const int max_sig_digits = 19;

        // By taking ascii '0' from unsigned char
        // We convert '0' => 0 etc, which we will need to do anyway
        // plus we can test for digits with a single comparison (<10)
        // Rather than two ('0' <= x && x <= '9')
        // It does make comparing with '.', 'e', 'E' more complex but
        // the -'0' for these can be done at compile time
        static const byte point = ((byte)'.') - '0';
        static const byte lower_e = ((byte)'e') - '0';
        static const byte upper_e = ((byte)'E') - '0';

        memory_input_stream *const mis = p->mis;

        // If fast parsing fails might need to call
        // strtod, which needs to start from the beginning
        size_t start_pos = mis_tell(mis);

        bool force_double = false;
        bool negative = false;
        uint64_t sum;
        int64_t exponent = 0;
        int sig_digits = 0;

        byte c = mis_take(mis);

        if(c == '-') {
                negative = true;
                c = mis_take(mis);
        }
        c -= '0';
        if(c < 10) {
                sum = c;
                sig_digits += (sum != 0);
        } else {
                throw_parse_error(p, JSNPG_ERROR_NUMBER);
        }

        c = mis_peek(mis) - '0';
        if(sum) {
                while(c < 10) {
                        mis_take(mis);
                        if(sig_digits++ < max_sig_digits) {
                                sum = sum * 10 + c;
                        } else {
                                exponent++;
                        }
                        c = mis_peek(mis) - '0';
                }
        }
        if(c == point) {
                mis_take(mis);
                force_double = true;

                c = mis_peek(mis) - '0';
                if(c >= 10)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

                do {
                        mis_take(mis);
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c;
                                exponent--;
                                sig_digits += (sum != 0);
                        }
                        c = mis_peek(mis) - '0';
                } while(c < 10);

        }
        if(c == lower_e || c == upper_e) {
                mis_take(mis);
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = mis_peek(mis);
                if(c == '-') {
                        mis_take(mis);
                        exp_sign = -1;
                        c = mis_peek(mis);
                } else if(c == '+') {
                        mis_take(mis);
                        c = mis_peek(mis);
                }
                c -= '0';
                if(c >= 10)
                        throw_parse_error(p, JSNPG_ERROR_NUMBER);

               do {
                        mis_take(mis);
                        exp = 10 * exp + c;
                        if(exp > 1000000000) {
                                if(exp_sign == -1) {
                                        *real_result = negative ? -0.0 : 0.0;
                                        return JSNPG_REAL;
                                }
                                throw_parse_error(p, JSNPG_ERROR_NUMBER);
                        }
                        c = mis_peek(mis) - '0';
                } while (c < 10);

                exponent += exp_sign * exp;
        }

        // Force double if either too many significant digits 
        // or sum is too big for signed long
        force_double = force_double 
                || sig_digits > max_sig_digits
                || (negative ? sum > 1 + (uint64_t)LONG_MAX : sum > LONG_MAX);

        if(force_double) {
                bool success = false;
                if (exponent >= FASTFLOAT_SMALLEST_POWER &&
                                exponent <= FASTFLOAT_LARGEST_POWER) {
                        *real_result = compute_float_64(exponent, sum, negative, &success);
                }
                if(!success) {
                        const char *start = (const char *)mis_at(mis, start_pos);
                        char *end = parse_float_strtod(start, real_result);
                        if(!end)
                                throw_parse_error(p, JSNPG_ERROR_NUMBER);
                        mis_adjust(mis, (byte *)end);
                }
                return JSNPG_REAL;
        } else {
                *integer_result = negative ? -sum : sum;
                return JSNPG_INTEGER;
        }
}
#pragma GCC diagnostic pop

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
                b = allocator_alloc(p->allocator, count + 8);
                if(!b)
                        return NULL;
                memcpy(b, bytes, count);
        }

        memset(b + count, '\0', 8); 

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

        p->mis = mis_new(a);
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

void jsnpg_parser_free(parser *p)
{
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
        return p->result;
}
