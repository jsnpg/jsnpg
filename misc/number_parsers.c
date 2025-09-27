
#include <stdio.h>

#include "digits.c"
#include "util_timer.c"

#define USAGE "number_parsers count filename"
#define FIRST_INT_DIGIT "First character must be a digit"
#define FIRST_FRAC_DIGIT "First fractional character must be a digit"
#define FIRST_EXP_DIGIT "First exponent character must be a digit"

[[noreturn]]
static void *log_error(char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}

static const byte *parse_number_1(const byte *str, long *integer_result, int *exponent_result)
{
        static uint64_t pow10[] = {
                1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 
                1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18 };
        // We only take the most significant digits
        // Max digits for long is 19
        // double is 15-17 so we may lose some digits when converting
        static const int max_sig_digits = 19;

        const byte *src = str;

        bool force_double = false;
        int sig_digits = 0;
        int exponent = 0;
        uint64_t sum = 0;

        bool negative = (*src == '-');
        src += negative;
        if(*src != '0') {
                digit_parse i = parse_upto_19(src, 0);
                if(i.count == 0)
                        return log_error(FIRST_INT_DIGIT);
                src += i.count + i.trailing;
                sig_digits = i.count;
                sum = i.sum;
                exponent = i.trailing;
                // X.xxx
                if(*src == '.') {
                        force_double = true;
                        src++;
                        digit_parse f = parse_upto_19(src, sig_digits);
                        int fcount = f.count;
                        if(fcount == 0)
                                return log_error(FIRST_FRAC_DIGIT);
                        src += fcount + f.trailing;
                        sum *= pow10[fcount];
                        sum += f.sum;
                        exponent -= fcount;
                        sig_digits += fcount;
                        if(sig_digits == max_sig_digits) {
                                while(((byte)(*src - '0')) < 10)
                                        src++;
                        }
                }
        } else {
                src++;
                // 0.xxx
                if(*src == '.') {
                        src++;
                        force_double = true;
                        if(((byte)(*src - '0')) > 9)
                                return log_error(FIRST_FRAC_DIGIT);

                        while(*src == '0') {
                                src++;
                                exponent--;
                        }

                        digit_parse f = parse_upto_19(src, 0);
                        int fcount = f.count;
                        src += fcount + f.trailing;
                        sum = f.sum;
                        exponent -= fcount;
                        while(((byte)(*src - '0')) < 10)
                                src++;
                }
        }
        if(*src == 'e' || *src == 'E') {
                force_double = true;
                src++;
                bool exp_negative = (*src == '-');
                src += exp_negative || (*src == '+');
                uint32_t exp = *src - '0';
                src++;
                if(exp > 9)
                        return log_error(FIRST_EXP_DIGIT);
                byte exp_n = *src - '0';
                if(exp_n < 10) {
                        src++;
                        exp *= 10;
                        exp += exp_n;
                        exp_n = *src - '0';
                        if(exp_n < 10) {
                                src++;
                                exp *= 10;
                                exp += exp_n;
                        }
                }
                exponent += ((int[]){exp, -exp})[exp_negative];
        }

        (void)force_double;
        *exponent_result = exponent;
        *integer_result = ((int64_t[]){sum, -sum})[negative];
        return src;
}

static const byte *parse_number_2(const byte *str, long *integer_result, int *exponent_result)
{
        // We only take the most significant digits
        // Max digits for long is 19
        // double is 15-17 so we may lose some digits when converting
        static const int max_sig_digits = 19;
        static int sig_digits[2] = { 0, max_sig_digits };


        const byte *src = str;

        int digits = 0;
        int zeros = 0;
        int fdigits = 0;
        int edigits = 0;
        int exponent = 0;
        int use_digits;
        uint64_t sum = 0;
        const byte *nums;
        bool ep = false;

        bool negative = (*src == '-');
        src += negative;
        if(*src != '0') {
                digits = count_digits(src);
                sig_digits[0] = digits;
                const bool dgt = digits > max_sig_digits;
                nums = src;
                src += digits;
                const bool dp = *src == '.';
                ep = (!dp) && (*src == 'e' || *src == 'E');

                if(digits == 0)
                        return log_error(FIRST_INT_DIGIT);

                use_digits = sig_digits[dgt];
                exponent = digits - use_digits;
                sum = parse_digits(nums, 0, use_digits);
                if(dp) {
                        src++;
                        fdigits = count_digits(src);
                        const bool err = fdigits == 0; 
                        sig_digits[0] += fdigits;
                        const bool fgt = sig_digits[0] > max_sig_digits;
                        nums = src;
                        src += fdigits;
                        ep = *src == 'e' || *src == 'E';

                        if(err)
                                return log_error(FIRST_FRAC_DIGIT);

                        use_digits = sig_digits[fgt] - use_digits;
                        exponent -= use_digits;
                        sum = parse_digits(nums, sum, use_digits);
                }
        } else {
                src++;
                const bool dp = *src == '.';
                ep = (!dp) && (*src == 'e' || *src == 'E');

                if(dp) {
                        src++;
                        if(*src == '0') {
                                zeros = 1 + count_zeros(src + 1);
                                src += zeros;
                        }
                        fdigits = count_digits(src);
                        const bool err = (zeros + fdigits) == 0;
                        sig_digits[0] = fdigits;
                        const bool fgt = fdigits > max_sig_digits;
                        nums = src;
                        src += fdigits;
                        ep = *src == 'e' || *src == 'E';

                        if(err)
                                return log_error(FIRST_FRAC_DIGIT);

                        use_digits = sig_digits[fgt];
                        sum = parse_digits(nums, 0, use_digits);
                        exponent = -(zeros + use_digits);
                }
        }
        if(ep) {
                src++;
                bool exp_negative = (*src == '-');
                src += exp_negative || (*src == '+');
                uint32_t exp = *src - '0';
                src++;
                if(exp > 9)
                        return log_error(FIRST_EXP_DIGIT);
                uint32_t exp_n = *src - '0';
                if(exp_n < 10) {
                        src++;
                        exp *= 10;
                        exp += exp_n;
                        exp_n = *src - '0';
                        if(exp_n < 10) {
                                src++;
                                exp *= 10;
                                exp += exp_n;
                        }
                }
                exponent += ((int[]){exp, -exp})[exp_negative];
        }

        (void)edigits;
        *exponent_result = exponent;
        *integer_result = ((int64_t[]){sum, -sum})[negative];
        return src;

}

// Point past last digit accepted, NULL on error
static const byte *parse_number_3(const byte *str, long *integer_result, int *exponent_result)
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


        const byte *src = str;

        bool force_double = false;
        bool negative = false;
        uint64_t sum;
        int64_t exponent = 0;
        int sig_digits = 0;

        byte c = *src++;

        if(c == '-') {
                negative = true;
                c = *src++;
        }
        c -= '0';
        if(c < 10) {
                sum = c;
                sig_digits += (sum != 0);
        } else {
                return log_error(FIRST_INT_DIGIT);
        }

        c = *src - '0';
        if(sum) {
                while(c < 10) {
                        src++;
                        if(sig_digits++ < max_sig_digits) {
                                sum = sum * 10 + c;
                        } else {
                                exponent++;
                        }
                        c = *src - '0';
                }
        }
        if(c == point) {
                src++;
                force_double = true;

                c = *src - '0';
                if(c >= 10)
                        return log_error(FIRST_FRAC_DIGIT);

                do {
                        src++;
                        if(sig_digits < max_sig_digits) {
                                sum = 10 * sum + c;
                                exponent--;
                                sig_digits += (sum != 0);
                        }
                        c = *src - '0';
                } while(c < 10);

        }
        if(c == lower_e || c == upper_e) {
                src++;
                force_double = true;
                int exp_sign = 1;
                int exp = 0;

                c = *src;
                if(c == '-') {
                        src++;
                        exp_sign = -1;
                        c = *src;
                } else if(c == '+') {
                        src++;
                        c = *src;
                }
                c -= '0';
                if(c >= 10)
                        return log_error(FIRST_EXP_DIGIT);

               do {
                        src++;
                        exp = 10 * exp + c;
                        if(exp > 1000000000) {
                                if(exp_sign == -1) {
                                        sum = 0;
                                }
                        }
                        c = *src - '0';
                } while (c < 10);

                exponent += exp_sign * exp;
        }

        (void)force_double;
        *exponent_result = exponent;
        *integer_result = negative ? -sum : sum;

        return src;
}

static const byte *parse_number_4(const byte *str, long *integer_result, int *exponent_result)
{
        byte d;
        unsigned dcount = 0;
        unsigned long sum;
        bool negative;
        bool exp_negative;
        int exponent = 0;
        unsigned exp = 0;

        const byte *src = str;

        negative = *src == '-';
        src += negative;
        d = *src - '0';

        if(d > 9) return NULL;

        sum = d;
        src++;
        if(sum) {
                for(dcount = 1 ; dcount < 19 ; dcount++, src++) {
                        d = *src - '0';
                        if(d > 9) goto L_DP;
                        sum = sum * 10 + d;
                }
                while(true) {
                        d = *src - '0';
                        if(d > 9) goto L_DP;
                        exponent++;
                        src++;
                }
        }
        
L_DP:
        if(*src == '.') {
                src++;
                d = *src - '0';
                if(d > 9) return NULL;

                if(sum == 0 && d == 0) {
                        do {
                                src++;
                                exponent--;
                                d = *src - '0';
                        } while(d == 0);

                        if(d > 9) goto L_EXP;
                }

                sum = sum * 10 + d;
                exponent--;
                src++;
                for( ; dcount < 19 ; dcount++, src++) {
                        d = *src - '0';
                        if(d > 9) goto L_EXP;
                        sum = sum * 10 + d;
                        exponent--;
                }
                while((*src - '0') < 10) {
                        src++;
                }
        }

L_EXP:
        if(*src == 'e' || *src == 'E') {
                src++;
                exp_negative = *src == '-';
                src += exp_negative || *src == '+';
                d = *src - '0';
                if(d > 9) return NULL;

                exp = d;
                for(int i = 0 ; i < 10 ; i++) {
                        src++;
                        d = *src - '0';
                        if(d > 9) goto L_DONE;
                        exp = exp * 10 + d;
                }
                if(!exp_negative) return NULL;
        }

L_DONE:
        if(exp_negative && exp > 0)
                exponent -= exp;
        else
                exponent += exp;

        *integer_result = sum;
        *exponent_result = exponent;

        return src;
}

static const byte *parse_number_5(const byte *str, long *integer_result, int *exponent_result)
{
        byte d;
        byte n;
        unsigned dcount = 0;
        unsigned long sum;
        bool negative;
        bool exp_negative;
        int exponent = 0;
        unsigned exp = 0;

        const byte *src = str;

        negative = *src == '-';
        src += negative;
        d = *src - '0';

        if(d > 9) return NULL;

        sum = d;
        src++;
        if(sum) {
                for(dcount = 1 ; dcount < 19 ; dcount += 2, src += 2) {
                        d = *src - '0';
                        if(d > 9) goto L_DP;
                        n = *(src + 1) - '0';
                        if(n > 9) {
                                sum = sum * 10 + d;
                                dcount++;
                                src++;
                                goto L_DP;
                        }
                        sum = (sum * 100) + (d * 10) + n;
                }

                while(true) {
                        d = *src - '0';
                        if(d > 9) goto L_DP;
                        exponent++;
                        src++;
                }
        }
        
L_DP:
        if(*src == '.') {
                src++;
                d = *src - '0';
                if(d > 9) return NULL;

                if(sum == 0 && d == 0) {
                        do {
                                src++;
                                exponent--;
                                d = *src - '0';
                        } while(d == 0);

                        if(d > 9) goto L_EXP;
                }

                sum = sum * 10 + d;
                exponent--;
                src++;
                for( ; dcount < 19 ; dcount += 2, src += 2) {
                        d = *src - '0';
                        if(d > 9) goto L_EXP;
                        n = *(src + 1) - '0';
                        if(n > 9) {
                                sum = sum * 10 + d;
                                exponent--;
                                dcount++;
                                src++;
                                goto L_EXP;
                        }
                        sum = (sum * 100) + (d * 10) + n;
                        exponent -= 2;
                }
                while((*src - '0') < 10) {
                        src++;
                }
        }

L_EXP:
        if(*src == 'e' || *src == 'E') {
                src++;
                exp_negative = *src == '-';
                src += exp_negative || *src == '+';
                d = *src - '0';
                if(d > 9) return NULL;

                exp = d;
                for(int i = 0 ; i < 10 ; i++) {
                        src++;
                        d = *src - '0';
                        if(d > 9) goto L_DONE;
                        exp = exp * 10 + d;
                }
                if(!exp_negative) return NULL;
        }

L_DONE:
        if(exp_negative && exp > 0)
                exponent -= exp;
        else
                exponent += exp;

        *integer_result = sum;
        *exponent_result = exponent;

        return src;
}


static inline const byte *skip_whitespace(const byte *bytes)
{
        byte c;
        while((c = *bytes) == ' ' || c == '\n' || c == '\r')
                bytes++;
        return bytes;
}

typedef const byte *(*number_parser)(const byte *, long *, int *);

static void run_n(unsigned times, 
                char * comment, 
                number_parser fn,
                const byte *bytes)
{
        long sum = 0;
        int exp = 0;
        const byte *b;

        timespec start = time_now();

        for(unsigned i = 0 ; i < times ; i++) {
                b = bytes;
                while(*b) {
                        b = fn(b, &sum, &exp);
                        b = skip_whitespace(b);
                }
        }

        timespec duration = time_sub(time_now(), start);

        printf("%s: %ld.%09ld\n", comment, duration.tv_sec, duration.tv_nsec);
}


int main(int argc, char **argv)
{
        if(argc != 3) return log_error(USAGE),0;

        int times = strtol(argv[1], NULL, 10);
        if(times < 1) return log_error(USAGE),0;

        const byte *bytes = read_file(argv[2]);

        run_n(times, "Parse 1", parse_number_1, bytes);
        run_n(times, "Parse 2", parse_number_2, bytes);
        run_n(times, "Parse 3", parse_number_3, bytes);
        run_n(times, "Parse 4", parse_number_4, bytes);
        run_n(times, "Parse 5", parse_number_5, bytes);
        //
        // res = parse_number_1(bytes, &integer, &exponent);
        // printf("1: %ld %d\n", integer, exponent);
        //
        // res = parse_number_2(bytes, &integer, &exponent);
        // printf("2: %ld %d\n", integer, exponent);
        //
        // res = parse_number_3(bytes, &integer, &exponent);
        // printf("3: %ld %d\n", integer, exponent);
        //


}
