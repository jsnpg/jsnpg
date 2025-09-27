#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef __USE_POSIX199309
#define __USE_POSIX199309 
#endif

#include <time.h>

#include "../src/include/jsnpg.h"

static size_t total_size;
static size_t total_realloc_size;
static size_t total_realloc_moved_size;
static int total_alloc;
static int total_realloc;
static int total_realloc_move;
static int total_free;

static void test_alloc_print()
{
        printf("Allocations       : %d\n", total_alloc);
        printf("Allocated memory  : %ld\n", total_size);
        printf("Reallocations     : %d\n", total_realloc);
        printf("Memory moves      : %d\n", total_realloc_move);
        printf("Reallocated memory: %ld\n", total_realloc_size);
        printf("Moved memory      : %ld\n", total_realloc_moved_size);
        printf("Frees             : %d\n", total_free);
}

static void *test_alloc(size_t size) 
{
        void *p = malloc(size);
        if(p) {
                total_alloc++;
                total_size += size;
        }
        return p;
}

static void *test_realloc(void *o, size_t size)
{
        void *p = realloc(o, size);
        if(p) {
                total_realloc++;
                total_realloc_size += size;
                if(p != o) {
                        total_realloc_move++;
                        // this is inaccurate as only original size moved
                        // but we dont remember what we allocated
                        total_realloc_moved_size += size;
                }
        }
        return p;
}

static void test_free(void *p)
{
        free(p);
        if(p) {
                total_free++;
        }
}

typedef struct timespec timespec;
        
static timespec time_sub(timespec end, timespec start)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

static timespec time_add(timespec t1, timespec t2)
{
        long nsec = t1.tv_nsec + t2.tv_nsec;
        time_t sec = t1.tv_sec + t2.tv_sec;
        if(nsec >= 1000000000) {
                nsec -= 1000000000;
                sec++;
        }
        return (timespec) { .tv_sec = sec, .tv_nsec = nsec };
}

static timespec time_base(int count)
{
        timespec start_time, end_time, temp;

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
        for(int i = 0 ; i < count ; i++) {
                clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &temp);
                clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &temp);
        }
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
        return time_sub(end_time, start_time);
}


int main(int argc, char *argv[])
{
        timespec start_time, end_time;
        timespec total_time = {};

        jsnpg_generator *g;
        jsnpg_result res = {};
        if(argc == 3) {
                if(0 == strcmp("-e", argv[1])) {
                        puts(argv[2]);
                        uint8_t buf[1024];
                        char *json = argv[2];
                        size_t len = strlen(json);
                        memcpy(buf, json, len + 1);
                        g = jsnpg_generator_new(.indent = 2);
                        res = jsnpg_parse(.bytes = buf, .count = len, .generator = g, .writeable = true);
                        char *s = jsnpg_result_string(g);
                        printf("%s", s);
                        jsnpg_generator_free(g);
                        if(JSNPG_ERROR == res.type) {
                                printf("Error: %d\n", res.error.code);
                                exit(1);
                        }
                }
        } else if(argc == 4) {
                if(0 == strcmp("-t", argv[1])) {
                        errno = 0;
                        long times = strtol(argv[2], NULL, 10);
                        if(errno) {
                                perror("Not a number");
                                exit(1);
                        }
                        if(times == 1) {
                                jsnpg_set_allocators(test_alloc, test_realloc, test_free);
                        }
                        FILE *fh = fopen(argv[3], "rb");
                        if(fh) {
                                fseek(fh, 0L, SEEK_END);
                                size_t length = (size_t)ftell(fh);
                                rewind(fh);
                                uint8_t *buf = malloc(length + 32);
                                uint8_t *tmp_buf = malloc(length + 32);
                                if(buf && tmp_buf) {
                                        fread(buf, length, 1, fh);
                                        res = (jsnpg_result){};
                                        timespec base_time = time_base(times);

                                        for(int i = 0 ; i < times ; i++) {
                                                memcpy(tmp_buf, buf, length);
                                                clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
                                                g = jsnpg_generator_new(.max_nesting = 0);
                                                res = jsnpg_parse(.bytes = tmp_buf, .count = length, .generator = g, .writeable = true);
                                                if(res.type == JSNPG_ERROR) {
                                                        printf("Parse failed: %d at %ld\n", res.error.code, res.position);
                                                        return 1;
                                                }
                                                //char *s = jsnpg_result_string(g);
                                                //puts(s);
                                                //printf("JSON length: %ld\n", strlen(s));
                                                jsnpg_generator_free(g);
                                                clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);

                                                total_time = time_add(
                                                                total_time, 
                                                                time_sub(end_time, start_time));

                                        }
                                        total_time = time_sub(total_time, base_time);

                                        free(buf);
                                        int ret = (res.type == JSNPG_EOF) ? 0 : 1;
                                        printf("Type: %d, Returned %d\n", res.type, ret);
                                        printf("Total time: %ld.%09lds\n", total_time.tv_sec, total_time.tv_nsec);
                                        if(times == 1) {
                                                test_alloc_print();
                                        }
                                        return ret;
                                } else {
                                        perror("Failed to allocate buffer");
                                        exit(1);
                                }
                                fclose(fh);


                        }
                }
        } else {
                printf("tests2 -e <json> or tests2 -t <num> <json file>\n");
                return 1;
        }

        if(res.type == JSNPG_EOF)
                printf("\n\nResult EOF: %d\n", res.type);
        else
                printf("\n\nResult : %d (%d[%ld])\n", res.type, res.error.code, res.position);
}


