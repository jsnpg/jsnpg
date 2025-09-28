// Print numbers for performance testing of number parsing
// Don't need to worry about exponent as that is fast enough

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

static unsigned random_int(unsigned num_digits)
{
        static unsigned int seed = 0;
        static unsigned pow10[] = {
                1, 10, 100, 1000, 10000, 100000, 1000000,
                10000000, 100000000, 1000000000
        };


        if(!seed) {
                seed = (unsigned)time(NULL);
                srand(seed);
        }
        
        if(num_digits == 0) return 0; // what you going to 
        if(num_digits > 9) num_digits = 9;


        return rand() % pow10[num_digits];
}

static void print_sign()
{
        if(random_int(1) > 4)
                printf("-");
}

static void print_int(unsigned num_digits)
{
        while(num_digits > 9) {
                printf("%d", random_int(9));
                num_digits -= 9;
        }
        if(num_digits)
                printf("%d", random_int(num_digits));
}

static void newline()
{
        printf("\n");
}

static void print_point()
{
        printf(".");
}

static void print_zero(unsigned num_zeros) {
        printf("%.*d", num_zeros, 0);
}

static int printn_a(unsigned n, unsigned a)
{
        for(int i = 0 ; i < n ; i++) {
                print_sign();
                print_int(a);
                newline();
        }
        return 0;

}

static int printn_b_dot_c(unsigned n, unsigned b, unsigned c)
{
        for(int i = 0 ; i < n ; i++) {
                print_sign();
                print_int(b);
                print_point();
                print_int(c);
                newline();
        }
        return 0;
}

static int printn_0_dot_d(unsigned n, unsigned d)
{
        for(int i = 0 ; i < n ; i++) {
                print_sign();
                print_zero(1);
                print_point();
                print_int(d);
                newline();
        }
        return 0;
}

static int printn_0_dot_e0_f(unsigned n, unsigned e, unsigned f)
{
        for(int i = 0 ; i < n ; i++) {
                print_sign();
                print_zero(1);
                print_point();
                print_zero(e);
                print_int(f);
                newline();
        }
        return 0;
}

static int parse_num(char *str)
{
        errno = 0;
        int x = strtol(str, NULL, 10);
        if(errno || x < 1)
                fprintf(stderr, "Invalid argument: %s\n", str);
        return x;
}

int main(int argc, char **argv)
{
        int n, v1, v2;

        if(argc >= 4) {
                n = parse_num(argv[1]);
                v1 = parse_num(argv[3]);
                if(0 == strcmp("a", argv[2]))
                        return printn_a(n, v1);
                else if(0 == strcmp("0.d", argv[2])) 
                        return printn_0_dot_d(n, v1);
                if(argc >= 5) {
                        v2 = parse_num(argv[4]);
                        if(0 == strcmp("b.c", argv[2]))
                                return printn_b_dot_c(n, v1, v2);
                        else if(0 == strcmp("0.e0f", argv[2]))
                                return printn_0_dot_e0_f(n, v1, v2);
                }
        }
        printf("Usage: make_digits n spec v1 [v2]\n");
        printf("       one of:\n");
        printf("              n a v1\n");
        printf("              n b.c v1 v2\n");
        printf("              n 0.d v1\n");
        printf("              n 0.e0f v1 v2\n");
        return 1;
}
