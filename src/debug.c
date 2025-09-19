/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * debug.c
 *   debug logging macros
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#ifdef JSNPG_DEBUG 

FILE *log_stream;

static void log_open(char *logfile)
{
        log_stream = fopen(logfile, "w");
        if(!log_stream)
                log_stream = stderr;
}

static uint8_t log_printablechar(uint8_t c)
{
        if(0x20 <= c)
                return c;
        else
                return '.';
}

static void log_printf(char *fmt, ...)
{
        if(!log_stream)
                log_open("jsnpg.log");

        va_list args;
        va_start(args, fmt);
        vfprintf(log_stream, fmt, args);
        va_end(args);
        fflush(log_stream);
}


#define JSNPG_LOG(A, ...) (log_printf((A) __VA_OPT__(,) __VA_ARGS__))
#else
#define JSNPG_LOG(...) 
#endif
