#include <stdio.h>
#include <memory.h>

#include "bytemap.h"
#include "bytemap.c"

[[noreturn]]
static void *fail(char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}


static unsigned parse_hex4(byte *bytes)
{

        unsigned codepoint = 0;
        for(int i = 0 ; i < 4 ; i++) {
                byte c = *bytes;
                if(!(byte_map[c] & BYTE_HEX_DIGIT))
                        fail(HEX_DIGIT);

                codepoint <<= 4;
                codepoint += byte_map_extra[c];

                bytes++;
        }
        return codepoint;
}

static unsigned parse_escape(byte *bytes, int *length)
{
        static const unsigned char escape[256] = {
                ['"'] = '"',  ['/'] = '/',  ['\\'] = '\\', ['b'] = '\b', 
                ['f'] = '\f', ['n'] = '\n', ['r'] = '\r',  ['t'] = '\t'
        };

        bytes *src = bytes;
        const byte e = *src;

        if(escape[e]) {
                *length = 1;
                return (unsigned)escape[e];
        }

        if(e == 'u') {
                src++;
                unsigned codepoint = parse_hex4(src);
                src += 4;
                if(codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        // Got surrogate but high (first one) must be 0xD800-0xDBFF
                        if(codepoint <= 0xDBFF) {
                                // high surrogate must be followed by low
                                if(!(*src++ == '\\' && *src++ == 'u'))
                                        fail(SURROGATE);

                                const unsigned codepoint2 = parse_hex4(src);
                                src += 4;

                                if(codepoint2 < 0xDC00 || codepoint2 > 0xDFFF)
                                        fail(SURROGATE);

                                codepoint = (((codepoint - 0xD800) << 10)
                                                | (codepoint2 - 0xDC00)) + 0x10000;
                        } else {
                                fail(SURROGATE);
                        }
                }
                *length = src - bytes;
                return codepoint;
        } else {
                fail(ESCAPE);
        }
}

static size_t parse_string_1(byte *bytes)
{
        if('"' != *bytes) fail(NOT_STRING);

        byte *src = bytes + 1;
        byte *dst = NULL;
        byte *mark = NULL;

        while(true) {
                unsigned type;
                byte c;
                while((type = byte_map[(c = *src)]) & BYTE_ASCII_STRING)
                        src++;

                if(c == '"') {
                        if(mark) {
                                memmove(dst, mark, src - mark - 1);
                                return dst - bytes - 1;
                        } else {
                                return (src - bytes) - 2;
                        }
                } else if(c == '\\') {
                        if(mark) {
                                memmove(dst, mark, src - mark - 1);
                        }

                        int escape_length;
                        int utf8_length;
                        dst = src;
                        unsigned codepoint = parse_escape(p, &escape_length);
                        utf8_encode(codepoint, dst, &utf8_length);
                        src += escape_length;
                        mark = src;
                        dst += utf8_length;
                        
                } else if(c >= 0x80) {
                        src++;
                        if(type & BYTE_LEADER_2) {
                                if(!(byte_map[*src] & BYTE_CONTINUATION))
                                        fail(UTF8);
                                src++;
                        } else if(type & BYTE_LEADER_3) {
                                unsigned next = byte_map_extra[c];
                                if(!(byte_map[*src] & next))
                                        fail(UTF8);
                                src++;
                                if(!(byte_map[*src] & BYTE_CONTINUATION))
                                        fail(UTF8);
                                src++;
                        } else if(type & BYTE_LEADER_4) {
                                unsigned next = byte_map_extra[c];
                                if(!(byte_map[*src] & next))
                                        fail(UTF8);
                                src++;
                                if(!(byte_map[*src] & BYTE_CONTINUATION))
                                        fail(UTF8);
                                src++;
                                if(!(byte_map[*src] & BYTE_CONTINUATION))
                                        fail(UTF8);
                                src++;
                        } else {
                                fail(UTF8);
                        }
                } else { // if(c < 0x20) {
                        fail(INVALID);
                }
        }
}

int main(int argc, char **argv)
{
}
