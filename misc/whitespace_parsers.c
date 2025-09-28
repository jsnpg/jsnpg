
#include <stdio.h>
#include <string.h>
#include <immintrin.h>
#include "bytemap.h"
#include "bytemap.c"
#include "util_timer.c"

typedef unsigned char  byte;

#define USAGE "whitespace_parsers count\n"

static byte buffer[256];

[[noreturn]]
static void fail(char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}

static inline const byte *consume_whitespace_1(const byte *bytes)
{
        const byte *s = bytes;

        while(byte_map[*s] & BYTE_WHITESPACE)
                 s++;

        // printf("1: %ld\n", s - *bytes);
        return s;
}

static inline const byte *consume_whitespace_2(const byte *bytes)
{
        const byte *s = bytes;
        int ls = 0;

        while(byte_map[*s] & BYTE_LINE_TERMINATOR)
                s++;

        if(*s == ' ') {
                s++;
                __m256i b256 = _mm256_loadu_si256((const __m256i *)s);
                __m256i s256 = _mm256_set1_epi8(' ');
                __m256i e256 = _mm256_cmpeq_epi8(b256, s256);
                int res = _mm256_movemask_epi8(e256);
                ls = __builtin_ffs(~(unsigned)res);
//                printf("%d\n", res);
                // if(res == -1)
                //         printf("2: 33\n");
                // else
                //         printf("2: %d\n", __builtin_ffs(~(unsigned)res));
        }
        return s + ls;

}

static inline const byte *consume_whitespace_3(const byte *bytes)
{
        const byte *s = bytes;
        int i = 0;

        while(byte_map[s[i++]] & BYTE_WHITESPACE
                        && byte_map[s[i++]] & BYTE_WHITESPACE
                        && byte_map[s[i++]] & BYTE_WHITESPACE
                        && byte_map[s[i++]] & BYTE_WHITESPACE)
                ;
        i--;
        return bytes + i;
}
        
typedef const byte *(*whitespace_parser)(const byte *);

static void run_n(unsigned times,
                char *comment,
                whitespace_parser fn,
                const byte *bytes)
{
        const byte *b;
        timespec start = time_now();

        for(unsigned i = 0 ; i < times ; i++) {
                b = bytes;
                while(b) {
                        b = fn(b);
                        b = strchr(b, '\n');
                }
        }

        timespec duration = time_sub(time_now(), start);

        printf("%s: %ld.%09ld\n", comment, duration.tv_sec, duration.tv_nsec);
}
        

int main(int argc, char **argv)
{
        const byte *bytes = read_file(argv[2]);

        int times = strtol(argv[1], NULL, 10);
        if(times <1) fail(USAGE);

        run_n(times, "Parse 1", consume_whitespace_1, bytes);
        run_n(times, "Parse 2", consume_whitespace_2, bytes);
        run_n(times, "Parse 3", consume_whitespace_3, bytes);

}
