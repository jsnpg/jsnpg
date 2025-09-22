#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

// What magic is this?
// https://www.youtube.com/watch?v=wlvKAT7SZIQ

typedef union {
        uint64_t i;
        unsigned char b[8];
} map;

map F0F0 = { .i = 0xF0F0F0F0F0F0F0F0 };
map Z6Z6 = { .i = 0x0606060606060606 };
map E333 = { .i = 0x3333333333333333 };



void print_bytes(const char *msg, map m)
{
        printf("%s", msg);
        for(int i = 0 ; i < 8 ;i++) {
                printf(" %02X", m.b[i]);
        }
        printf("\n");
}

bool is_8_digits(const char *str)
{
        map m1;
        memcpy(&m1.i, str, sizeof(uint64_t));
        print_bytes("m1   : ", m1);
        print_bytes("F0F0 : ", F0F0);
        map m2 = { .i = m1.i & F0F0.i };
        print_bytes("m2   : ", m2);
        map m3 = { .i = m1.i + Z6Z6.i };
        print_bytes("Z6Z6 : ", Z6Z6);
        print_bytes("m3   : ", m3);
        map m4 = { .i = m3.i & F0F0.i };
        print_bytes("m4   : ", m4);
        map m5 = { .i = m4.i >> 4 };
        print_bytes("m5   : ", m5);
        map m6 = { .i = m2.i | m5.i };
        print_bytes("m6   : ", m6);

        uint64_t v = m1.i;

        return (((v & 0xF0F0F0F0F0F0F0F0) |
                                (((v + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0) >> 4))
                                == 0x3333333333333333);
}

bool is_4_digits(const char *str)
{
        uint64_t v;
        memcpy(&v, str, 8);
        v = (v & 0xF0F0F0F0F0F0F0F0) |
                                (((v + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0) >> 4);

        return 0x33333333 == (v & 0xFFFFFFFF);
}

bool is_2_digits(const char *str)
{
        uint64_t v;
        memcpy(&v, str, 8);
        v = (v & 0xF0F0F0F0F0F0F0F0) |
                                (((v + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0) >> 4);

        return 0x3333 == (v & 0xFFFF);
}

bool num_digits(const char *str)
{
        map m;
        memcpy(&m.i, str, sizeof(m.i));
        m.i -= 0x3030303030303030;
        print_bytes("-0x30: ", m);
        map m4 = { .i = m.i & 0x0F0F0F0F0F0F0F0F };
        print_bytes("&0x0F: ", m4);
        map m2 = { .i = m4.i + 0x060606060606 };
        print_bytes("+0x06: ", m2);
        map m3 = { .i = m2.i & 0x0F0F0F0F0F0F0F0F };
        print_bytes("&0x0F: ", m3);
        map m5 = { .i = m3.i - 0x060606060606 };
        print_bytes("+0x06: ", m5);
        map m6 = { .i = m5.i & 0x0F0F0F0F0F0F0F0F };
        print_bytes("&0x0F: ", m6);
}

map ZFZF = { .i = 0x0F0F0F0F0F0F0F0F };
map ZZFF = { .i = 0x00FF00FF00FF00FF };
map ZZZZ = { .i = 0x0000FFFF0000FFFF };

uint32_t parse_8_digits(const char *str)
{
        uint64_t v;
        memcpy(&v, str, sizeof(v));

        v = (v & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
        v = (v & 0x00FF00FF00FF00FF) * 6553601 >> 16;
        return (v & 0x0000FFFF0000FFFF) * 42949672960001 >> 32;
}

uint32_t parse_4_digits(const char *str)
{
        uint32_t v = 0;
        
        memcpy(&v, str, 4); 


        v = (v & 0x0F0F0F0F) * 2561 >> 8;
        return (v & 0x00FF00FF) * 6553601 >> 16;
}

uint32_t parse_2_digits(const char *str)
{
        return (10 * (str[0] - '0')) + (str[1] - '0');
}

typedef unsigned char u8x16 __attribute__ ((vector_size (16)));
typedef unsigned long u64x2 __attribute__ ((vector_size (16)));

typedef union {
        u8x16 bytes;
        u64x2 longs;
} byte_long;

int count_digits(const char *str, unsigned char max)
{
        int digits = 0;
        byte_long bl;
        memcpy(&bl.bytes, str, 16);
        bl.bytes -= 0x30;
        bl.bytes = bl.bytes > max;

        unsigned long l = bl.longs[0];
        if(!l) {
                l = bl.longs[1];
                if(!l)
                        return 16 + count_digits(str + 16, max);
                digits = 8;
        }

        return digits + __builtin_ffsl(l) / 8;
}

unsigned long parse_digits(const char *str)
{
        static unsigned long pow10[] = {
                1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
                1e10, 1e11 };

        static int ls[2] = {0, 19};
        int count = count_digits(str, (unsigned char)0x09);
        ls[0] = count;
        int use = ls[count > 19];
        int exponent = count - use;
        int used = 0;
        unsigned long tot = 0;
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

        printf("Number = %ld, Exponent = %d\n", tot, exponent);

        return tot;
}








int main(int argc, char **argv)
{
        int lzds = count_digits(argv[1], (unsigned char)0x00);
        printf("Leading zeros : %d\n", lzds);
        parse_digits(argv[1] + lzds);

        return 0;

        if(is_8_digits(argv[1]))
                printf("%s = %u\n", argv[1], parse_8_digits(argv[1]));
        else if(is_4_digits(argv[1]))
                printf("%s = %u\n", argv[1], parse_4_digits(argv[1]));
        else if(is_2_digits(argv[1]))
                printf("%s = %u\n", argv[1], parse_2_digits(argv[1]));
        else
                printf("Not 2, 4 or 8 digits\n");
}
