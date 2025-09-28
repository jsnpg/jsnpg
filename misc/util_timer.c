#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#include <time.h>


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

// static timespec time_add(timespec t1, timespec t2)
// {
//         long nsec = t1.tv_nsec + t2.tv_nsec;
//         time_t sec = t1.tv_sec + t2.tv_sec;
//         if(nsec >= 1000000000) {
//                 nsec -= 1000000000;
//                 sec++;
//         }
//         return (timespec) { .tv_sec = sec, .tv_nsec = nsec };
// }

static timespec time_now()
{
        timespec now_time;
        clock_gettime(CLOCK_REALTIME, &now_time);
        return now_time;
}

static unsigned char *read_file(char *filename)
{
        FILE *fh = fopen(filename, "rb");
        if(fh) {
                fseek(fh, 0L, SEEK_END);
                size_t length = (size_t)ftell(fh);
                rewind(fh);
                unsigned char *buf = malloc(length + 32);
                if(buf) {
                        fread(buf, length, 1, fh);
                        memset(buf + length, '\0', 32);
                        return buf;
                }
                fclose(fh);
        }
        return NULL;
}
