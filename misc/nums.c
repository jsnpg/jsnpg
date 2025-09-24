#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <immintrin.h>

// What magic is this?
// https://www.youtube.com/watch?v=wlvKAT7SZIQ

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

int count_zeros(const char *str)
{
        int zeros = 0;
        byte_long bl;
        const char *src = str;
        while(true) {
                memcpy(&bl.bytes, src, 16);
                bl.bytes = (bl.bytes != '0');
                unsigned long l = bl.longs[0];
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

int count_digits(const char *str)
{
        int digits = 0;
        byte_long bl;
        const char *src = str;
        while(true) {
                memcpy(&bl.bytes, src, 16);
                bl.bytes -= 0x30;
                bl.bytes = bl.bytes > 9;

                unsigned long l = bl.longs[0];
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

unsigned long parse_digits(const char *str)
{
        static unsigned long pow10[] = {
                1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
                1e10, 1e11 };

        static int ls[2] = {0, 19};
        int count = count_digits(str);
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

unsigned long parse_digits2(const char *str, uint64_t tot, int use)
{
        static unsigned long pow10[] = {
                1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
                1e10, 1e11 };

        int used = 0;

        if(tot) {
                assert(use <= 11);
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



static int parse_number(const unsigned char *src) //, double *real_result, long *integer_result)
{
        // We only take the most significant digits
        // Max digits for long is 19
        // double is 15-17 so we may lose some digits when converting
        static const int max_sig_digits = 19;
        static int sig_digits[2] = { 0, max_sig_digits };

        int digits = 0;
        int fdigits = 0;
        int edigits = 0;
        uint64_t exponent = 0;
        int use_digits;
        uint64_t sum = 0;

        bool negative = *src == '-';
        src += negative;
        if(*src != '0') {
                digits = count_digits(src);
                if(digits == 0)
                        return -1;
                sig_digits[0] = digits;
                use_digits = sig_digits[digits > max_sig_digits];
                exponent = digits - use_digits;
                sum = parse_digits2(src, 0, use_digits);
                src += digits;
                if(*src == '.') {
                        src++;
                        fdigits = count_digits(src);
                        if(fdigits == 0)
                                return -1;
                        sig_digits[0] += fdigits;
                        use_digits = sig_digits[(digits + fdigits) > max_sig_digits] - use_digits;
                        exponent -= use_digits;
                        sum = parse_digits2(src, sum, use_digits);
                        src += fdigits;
                }
        } else {
                src++;
                if(*src == '.') {
                        src++;
                        int zeros = count_zeros(src);
                        exponent = -zeros;
                        src += zeros;
                        fdigits = count_digits(src);
                        if((zeros + fdigits) == 0)
                                return -2;
                        sig_digits[0] = fdigits;
                        use_digits = sig_digits[fdigits > max_sig_digits];
                        sum = parse_digits2(src, 0, use_digits);
                        src += fdigits;
                }
        }
        if(*src == 'e' || *src == 'E') {
                src++;
                bool exp_negative = (*src == '-');
                src += exp_negative || (*src == '+');
                edigits = count_digits(src);
                if(edigits == 0)
                        return -1;
                sig_digits[0] = edigits;
                use_digits = sig_digits[edigits > max_sig_digits];
                uint64_t exp = parse_digits2(src, 0, use_digits);
                if(exp > LONG_MAX)
                        return -3;
                src += edigits;
                exponent += ((int64_t[]){exp, -exp})[exp_negative];
        }
        if((fdigits + edigits) || exponent || sum > (((uint64_t)LONG_MAX) + negative)) {
                printf("Parse float: %s%luE%ld\n", 
                                (negative ? "-" : ""),
                                sum,
                                exponent);
                return 2;
        } else {
                printf("Parser int: %s%lu\n",
                                (negative ? "-" : ""),
                                sum);
                return 1;
        }
}





int main(int argc, char **argv)
{
        int res = parse_number(argv[1]);
        if(res < 0) {
                printf("Parse error: %d\n", res);
        }

        // int lzds = count_zeros(argv[1]);
        // printf("Leading zeros : %d\n", lzds);
        // parse_digits(argv[1] + lzds);

}
