#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../src/include/jsnpg.h"

#include "../src/include/def_gen_macros.h"

#define test_start() jsnpg_generator *gen = ctx
#define test_end()   return true;

static bool test_null(void *ctx)
{
        test_start();

        null();

        test_end();
}

static bool test_boolean(void *ctx, bool is_true)
{
        test_start();

        boolean(is_true);
        
        test_end();
}

static bool test_integer(void *ctx, long l)
{
        test_start();

        integer(l);

        test_end();
}

static bool test_real(void *ctx, double d)
{
        test_start();

        real(d);

        test_end();
}

static bool test_string(void *ctx, const unsigned char *bytes, size_t count)
{
        test_start();

        string_bytes(bytes, count);

        test_end();
}

static bool test_key(void *ctx, const unsigned char *bytes, size_t count)
{
        test_start();

        key_bytes(bytes, count);

        test_end();
}

static bool test_start_object(void *ctx)
{
        test_start();

        start_object();

        test_end();
}

static bool test_end_object(void *ctx)
{
        test_start();

        end_object();

        test_end();
}

static bool test_start_array(void *ctx)
{
        test_start();

        start_array();

        test_end();
}

static bool test_end_array(void *ctx)
{
        test_start();

        end_array();

        test_end();
}

#include "../src/include/undef_gen_macros.h"

static void *ctx_generator(void) {
        return jsnpg_generator_new(.max_nesting = 0);
}

static jsnpg_callbacks test_callbacks = {
        .null = test_null,
        .boolean = test_boolean,
        .integer = test_integer,
        .real = test_real,
        .string = test_string,
        .key = test_key,
        .start_object = test_start_object,
        .end_object = test_end_object,
        .start_array = test_start_array,
        .end_array = test_end_array
};

static void fail(const char *msg)
{
        fprintf(stderr, "%s", msg);
        exit(1);
}

static void run_parse_next(jsnpg_parser *p, jsnpg_generator *g)
{
        bool abort = false;
        jsnpg_result res;
        while(!(abort || JSNPG_EOF == jsnpg_parse_next(p))) {
                res = jsnpg_parse_result(p);
                switch(res.type) {
                case JSNPG_TRUE:
                case JSNPG_FALSE:
                        abort = !jsnpg_boolean(g, res.type == JSNPG_TRUE);
                        break;
                case JSNPG_NULL:
                        abort = !jsnpg_null(g);
                        break;
                case JSNPG_STRING:
                        abort = !jsnpg_string(g, res.string.bytes, res.string.count);
                        break;
                case JSNPG_KEY:
                        abort = !jsnpg_key(g, res.string.bytes, res.string.count);
                        break;
                case JSNPG_INTEGER:
                        abort = !jsnpg_integer(g, res.number.integer);
                        break;
                case JSNPG_REAL:
                        abort = !jsnpg_real(g, res.number.real);
                        break;
                case JSNPG_START_ARRAY:
                        abort = !jsnpg_start_array(g);
                        break;
                case JSNPG_END_ARRAY:
                        abort = !jsnpg_end_array(g);
                        break;
                case JSNPG_START_OBJECT:
                        abort = !jsnpg_start_object(g);
                        break;
                case JSNPG_END_OBJECT:
                        abort = !jsnpg_end_object(g);
                        break;
                default:
                        abort = true;
                }
        }
}


static jsnpg_result parse_solution(int soln, FILE *fh)
{
        // Input - 
        //      buffer
        //      dom (covered by dom output tests below)
        //
        // Output (not JSN, with/without validation) -
        //      dom (1 - 2)
        //      callback (specified in parse) (3 - 4)
        //      callback (generator) (5 - 6)
        //
        // Output (pretty/not pretty, with/without validation) -
        //         buffer (7 - 10)
        //
        // Special tests 11-20, checks optional variations to parsing

        bool create_dom = false;
        bool parse_callback = false;
        jsnpg_generator *g = NULL;
        jsnpg_generator *ctx_g = NULL;

        if(soln < 3) {
                create_dom = true;
                g = jsnpg_generator_new(.dom = true);
        } else if(soln < 4) { // not < 5 as 4 is parse_next an it cannot do this
                parse_callback = true;
        } else if(soln < 7) {
                ctx_g = ctx_generator();
                g = jsnpg_generator_new(
                                .callbacks = &test_callbacks,
                                .ctx = ctx_g);
        } else if(soln < 9) {
                g = jsnpg_generator_new(
                                .indent = 4);
        } else if(soln < 20) {
                g = jsnpg_generator_new();
        } else if(soln < 21) {
                // Test 20 needs to create generator with this set up front
                g = jsnpg_generator_new(.allow = JSNPG_ALLOW_INVALID_UTF8_OUT);
        }

        jsnpg_result res;

        fseek(fh, 0L, SEEK_END);
        size_t length = (size_t)ftell(fh);
        rewind(fh);
        unsigned char *buf = malloc(length + 1);
        if(!buf)
                fail("Failed to allocate memory to read file content");

        fread(buf, length, 1, fh);

        if(soln <= 10) {
                if(soln % 2) {
                        if(create_dom) {
                                res = jsnpg_parse(.bytes = buf, .count = length, .generator = g);
                                ctx_g = ctx_generator();
                                if(res.type == JSNPG_EOF) {
                                        res = jsnpg_parse(
                                                        .dom = jsnpg_result_dom(g),
                                                        .generator = ctx_g);
                                }
                        } else if(parse_callback) {
                                ctx_g = ctx_generator();
                                res = jsnpg_parse(.bytes = buf, .count = length,
                                                .callbacks = &test_callbacks,
                                                .ctx = ctx_g);
                        } else {
                                res = jsnpg_parse(.bytes = buf, 
                                                .count = length, 
                                                .generator = g);
                        }
                } else {
                        jsnpg_parser *p = NULL;
                        if(create_dom) {
                                res = jsnpg_parse(.bytes = buf, .count = length, .generator = g);
                                if(res.type == JSNPG_EOF) {
                                        ctx_g = ctx_generator();
                                        p = jsnpg_parser_new(.dom = jsnpg_result_dom(g));
                                        run_parse_next(p, ctx_g);
                                } else {
                                        printf("Returned type: %d\n", res.type);
                                        fail("Failed to create DOM\n");
                                }
                        } else {
                                p = jsnpg_parser_new(.bytes = buf, .count = length);
                                run_parse_next(p, g);
                        }
                        res = jsnpg_parse_result(p);
                        jsnpg_parser_free(p);
                }
        } else if(soln < 21) {
                static unsigned flags[] = {
                        JSNPG_ALLOW_COMMENTS,
                        JSNPG_ALLOW_TRAILING_COMMAS,
                        JSNPG_ALLOW_TRAILING_CHARS,
                        JSNPG_ALLOW_MULTIPLE_VALUES,
                        // Have to turn on allow out as well otherwise test will fail
                        JSNPG_ALLOW_INVALID_UTF8_IN | JSNPG_ALLOW_INVALID_UTF8_OUT};

                unsigned allow = flags[(soln - 11) / 2];

                if(soln % 2) {
                        res = jsnpg_parse(.allow = allow, .bytes = buf, .count = length, .generator = g);
                } else {
                        jsnpg_parser *p = jsnpg_parser_new(.allow = allow, .bytes = buf, .count = length);
                        run_parse_next(p, g);
                        res = jsnpg_parse_result(p);
                        jsnpg_parser_free(p);
                }
        }

        free(buf);
        if(ctx_g)
                printf("%s", jsnpg_result_string(ctx_g));
        else
                printf("%s", jsnpg_result_string(g));

        jsnpg_generator_free(g);
        jsnpg_generator_free(ctx_g);

        return res;     
}

static void usage(char *progname)
{       
        printf("%s [-s <solution number>] <json filename>\n\n", progname);
        printf("Where solution number (default: 9) is:\n");
        printf("  N - parse/generate route [Stringified | Prettified : Parse | parse Next]\n"); 
        printf("  1 - byte buffer => dom => stdout                [S:P]\n");
        printf("  2 - byte buffer => dom => stdout                [S:N]\n");
        printf("  3 - byte buffer => parse/callback => stdout     [S:P]\n");
        printf("  4 - No Parse Next solution, treat as N = 6      [S:N]\n");
        printf("  5 - byte buffer => generator/callback => stdout [S:P]\n");
        printf("  6 - byte buffer => generator/callback => stdout [S:N]\n");
        printf("  7 - byte buffer => buffer => stdout             [P:P]\n");
        printf("  8 - byte buffer => buffer => stdout             [P:N]\n");
        printf("  9 - byte buffer => buffer => stdout             [S:P]\n");
        printf(" 10 - byte buffer => buffer => stdout             [S:N]\n");
        printf(" 11 - allow comments                              [S:P]\n");
        printf(" 12 - allow comments                              [S:N]\n");
        printf(" 13 - allow trailing commas                       [S:P]\n");
        printf(" 14 - allow trailing commas                       [S:N]\n");
        printf(" 15 - allow trailing chars                        [S:P]\n");
        printf(" 16 - allow trailing chars                        [S:N]\n");
        printf(" 17 - allow multiple values                       [S:P]\n");
        printf(" 18 - allow multiple values                       [S:N]\n");
        printf(" 19 - allow invalid utf8 in input & output        [S:P]\n");
        printf(" 20 - allow invalid utf8 in input & output        [S:N]\n");

}
                
int main(int argc, char *argv[]) {
        int soln = 0;

        if(2 == argc) {
                if(0 == strcmp("-h", argv[1])) {
                        usage(argv[0]);
                        exit(0);
                } else {
                        soln = 9;
                }
        } else if(4 == argc && 0 == strcmp("-s", argv[1])) {
                int l = (int)strtol(argv[2], NULL, 10);
                if(l > 0 && l < 21)
                        soln = l;
        }

        if(!soln)
                fail("Usage: jsnpgtest [-s solution (1-20)] infile\n       jsnpgtest -h\n");


        char *infile = argv[(2 == argc) ? 1 : 3];
        FILE *fh = fopen(infile, "rb");
        if(!fh)
                fail("Failed to open input file\n");



        jsnpg_result v = parse_solution(soln, fh);
        fclose(fh);
        int ret = (v.type == JSNPG_EOF) ? 0 : 1;
        if(ret)
                printf("Type: %d, Returned %d\n", v.type, ret);
        else
                printf("\n");
        return ret;
}


                        



