#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <immintrin.h>

typedef unsigned char byte;

static uint32_t parse_8_digits(const byte *str)
{
        uint64_t v;
        memcpy(&v, str, sizeof(v));

        // What magic is this?
        // https://www.youtube.com/watch?v=wlvKAT7SZIQ
        v = (v & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
        v = (v & 0x00FF00FF00FF00FF) * 6553601 >> 16;
        return (v & 0x0000FFFF0000FFFF) * 42949672960001 >> 32;
}

static uint32_t parse_4_digits(const byte *str)
{
        return 1000 * (str[0] - '0')
                + 100 * (str[1] - '0')
                + 10 * (str[2] - '0')
                + str[3] - '0';
        // uint32_t v = 0;
        // memcpy(&v, str, sizeof(v)); 
        //
        // v = (v & 0x0F0F0F0F) * 2561 >> 8;
        // return (v & 0x00FF00FF) * 6553601 >> 16;
}

static uint32_t parse_2_digits(const byte *str)
{
        return (10 * (str[0] - '0')) + (str[1] - '0');
}

typedef unsigned char u8x16 __attribute__ ((vector_size (16)));
typedef unsigned long u64x2 __attribute__ ((vector_size (16)));

typedef union {
        u8x16 mask;
        u64x2 longs;
} mask_long;

typedef struct {
        uint64_t sum;
        int count;
        int trailing;
} digit_parse;

// Parse up to 19 digits, with some having already been parsed
// Consume all trailing digits
// Return 
//      value of digits parsed, 
//      count of digits parsed, 
//      count of trailing digits skipped
__attribute__((always_inline))
static inline digit_parse parse_upto_19(const byte *str, int done)
{
        uint64_t tot = 0;

        int max = 19 - done;
        int trailing = 0;

        mask_long ml;
        mask_long mv;

        u8x16 bytes;
        memcpy(&bytes, str, sizeof(bytes));
        mv.mask = bytes;
        bytes -= 0x30;
        ml.mask = bytes > 9;

        uint64_t l = ml.longs[0];
        
        int d8 = (0 == l);
        int d16 = d8 && (0 == (l = ml.longs[1]));

        int digits = 8 * (d8 + d16);
        digits += __builtin_ffsl(l) / 8;

        int count = max < digits ? max : digits;

        int i = 0;

        if(count >= 16) {
                uint64_t v1 = mv.longs[0];
                v1 = (v1 & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
                v1 = (v1 & 0x00FF00FF00FF00FF) * 6553601 >> 16;
                v1 = (v1 & 0x0000FFFF0000FFFF) * 42949672960001 >> 32;
                uint64_t v2 = mv.longs[1];
                v2 = (v2 & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
                v2 = (v2 & 0x00FF00FF00FF00FF) * 6553601 >> 16;
                v2 = (v2 & 0x0000FFFF0000FFFF) * 42949672960001 >> 32;
                tot = (v1 * 100000000) + v2;
                i = 16;
        } else if(count >= 8) {
                uint64_t v = mv.longs[0];
                v = (v & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
                v = (v & 0x00FF00FF00FF00FF) * 6553601 >> 16;
                v = (v & 0x0000FFFF0000FFFF) * 42949672960001 >> 32;
                tot = v;
                i = 8;
        }

        for( ; i < count ; i++) {
                tot *= 10;
                tot += bytes[i];
        }

        if(digits > max) {
                trailing = digits - max;
        }

        if(d16) {
                str += 16;
                max -= 16;
                byte c = *str - '0';
                while(c < 10 && max > 0) {
                        str++;
                        count++;
                        max--;
                        tot *= 10;
                        tot += c;
                        c = *str - '0';
                }
                while(c < 10) {
                        trailing++;
                        str++;
                        c = *str - '0';
                }
        }

        return (digit_parse) { 
                        .sum = tot, 
                        .count = count, 
                        .trailing = trailing
        };
}



static int count_zeros(const byte *str)
{
        int zeros = 0;
        mask_long bl;
        const byte *src = str;
        while(true) {
                memcpy(&bl.mask, src, sizeof(bl.mask));
                bl.mask = (bl.mask != '0');
                uint64_t l = bl.longs[0];
                if(!l) {
                        l = bl.longs[1];
                        if(!l) {
                                src += 16;
                                zeros += 16;
                                continue;
                        }
                        zeros += 8;
                }

                zeros += __builtin_ffsl(l) / 8;
                return zeros;
        }
}

static int count_digits(const byte *str)
{
        int digits = 0;
        mask_long bl;
        const byte *src = str;
        while(true) {
                memcpy(&bl.mask, src, sizeof(bl.mask));
                bl.mask -= 0x30;
                bl.mask = bl.mask > 9;

                uint64_t l = bl.longs[0];
                if(!l) {
                        l = bl.longs[1];
                        if(!l) {
                                src += 16;
                                digits +=16;
                                continue;
                        }
                        digits += 8;
                }

                digits += __builtin_ffsl(l) / 8;
                return digits;
        }
}

static uint64_t parse_digits(const byte *str, uint64_t tot, int use)
{
        static uint64_t pow10[] = {
                1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
                1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18 };

        int used = 0;

        if(tot) {
                assert(use <= 18);
                tot *= pow10[use];
        }

        while(use > 7) {
                use -= 8;
                tot += pow10[use] * parse_8_digits(str + used);
                used += 8;
        }
        if(use > 3) {
                use -= 4;
                tot += pow10[use] * parse_4_digits(str + used);
                used += 4;
        }
        if(use > 1) {
                use -= 2;
                tot += pow10[use] * parse_2_digits(str + used);
                used += 2;
        }
        if(use > 0)
                tot += str[used] - '0';

        return tot;
}

// typedef struct {
//         uint64_t sum;
//         int count;
// } digit_parse;
//
// static digit_parse parse_upto_16(const byte *str)
// {
//         uint64_t tot = 0;
//
//         u8x16 bytes;
//         byte_long bl;
//         memcpy(&bytes, str, sizeof(bytes));
//         bytes -= 0x30;
//         bl.bytes = bytes > 9;
//
//         uint64_t l = bl.longs[0];
//
//         int d8 = (0 == l);
//         int d16 = d8 && (0 == (l = bl.longs[1]));
//
//         int digits = 8 * (d8 + d16);
//         digits += __builtin_ffsl(l) / 8;
//
//         for(int i = 0 ; i < digits ; i++) {
//                 tot *= 10;
//                 tot += bytes[i];
//         }
//
//         return (digit_parse) { .sum = tot, .count = digits };
// }

