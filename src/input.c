/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * input.c
 *   an abstraction over the input data
 *   allows modification of JSON strings as escapes are applied
 */

#include <assert.h>

struct memory_input_stream {
        byte *start;
        byte *string;
        byte *read;
        byte *write;
        byte *mark;
        size_t count;
        bool allow_comments;
};

static memory_input_stream *mis_new(allocator *a, bool allow_comments)
{
        memory_input_stream *mis = allocator_alloc(a, sizeof(memory_input_stream));

        if(!mis)
                return NULL;

        mis->count = 0;
        mis->start = NULL;
        mis->string = NULL;
        mis->read = NULL;
        mis->write = NULL;
        mis->mark = NULL;
        mis->allow_comments = allow_comments;

        return mis;
}

static const byte *mis_current(memory_input_stream *mis)
{
        return mis->read;
}

static void mis_set_bytes(memory_input_stream *mis, byte *bytes, size_t count)
{
        mis->start = bytes;
        mis->read = bytes;
        mis->count = count;
}

static inline size_t mis_tell(memory_input_stream *mis)
{
        return (size_t)(mis->read - mis->start);
}

static inline void mis_adjust(memory_input_stream *mis, byte *ptr)
{
        assert(0 <= ptr - mis->start && ptr - mis->start <= (long)mis->count);
        mis->read = ptr;
}

static inline const byte *mis_at(memory_input_stream *mis, size_t pos)
{
        if(pos >= mis->count)
                return NULL;

        return mis->start + pos;
}

static inline bool mis_eof(memory_input_stream *mis)
{
        assert(mis_tell(mis) <= mis->count);

        return mis_tell(mis) == mis->count;
}

static inline byte mis_peek(memory_input_stream *mis)
{
        return *mis->read;
}

static inline byte mis_take(memory_input_stream *mis)
{
        return *mis->read++;
}

static inline byte mis_find(memory_input_stream *mis, byte c)
{
        byte *p = (byte *)strchr((const char *)mis->read, (char)c);
        if(!p) {
                mis->read = mis->start + mis->count;
                return '\0';
        }
        mis->read = p;
        return *p;
}

static inline bool mis_consume(memory_input_stream *mis, byte b)
{
        if(*mis->read != b)
                return false;

        mis->read++;
        return true;
}

static inline byte mis_consume_whitespace_only(memory_input_stream *mis)
{
        byte c;
        while(byte_map[(c = mis_peek(mis))] & BYTE_WHITESPACE)
                 mis_take(mis);
        return c;
}

static byte mis_consume_whitespace_comments(memory_input_stream *mis)
{
        while(true) {
                byte c = mis_consume_whitespace_only(mis);

                if(c != '/')
                        return c;

                mis_take(mis); // '/'
                c = mis_peek(mis);
                if(c == '*') {
                        mis_take(mis); // '*'
                        while(true) {
                                c = mis_find(mis, '*');
                                if(c == '*') {
                                        mis_take(mis); // '*'
                                        if(mis_consume(mis, '/'))
                                                break;
                                }
                                if(mis_eof(mis))
                                        return '\0';
                        }
                } else if(c == '/') {
                        c = mis_find(mis, '\n');
                        if(mis_eof(mis))
                                return '\0';
                } else {        
                        return '\0';
                }
        }
}

static inline byte mis_consume_whitespace(memory_input_stream *mis)
{
        if(mis->allow_comments)
                return mis_consume_whitespace_comments(mis);
        else
                return mis_consume_whitespace_only(mis);
}


static inline bool mis_next(memory_input_stream *mis, byte b)
{
        return *mis->read == b 
                || mis_consume_whitespace(mis) == b;
}

static inline bool mis_consume_next(memory_input_stream *mis, byte b)
{
        if(!mis_next(mis, b))
                return false;
        mis->read++;
        return true;
}

static inline void mis_string_start(memory_input_stream *mis)
{
        mis->string = mis->read;
        mis->write = mis->read;
        mis->mark = mis->read;
}

static inline void mis_string_update(memory_input_stream *mis)
{
        if(mis->mark != mis->write) {
                size_t amt = (size_t)(mis->read - mis->mark);
                if(amt) {
                        memmove(mis->write, mis->mark, amt);
                        mis->write += amt;
                }
        } else {
                mis->write = mis->read;
        }
}

static inline void mis_string_restart(memory_input_stream *mis)
{
        mis->mark = mis->read;
}

static inline byte **mis_writer(memory_input_stream *mis)
{
        return &mis->write;
}

static inline void mis_byte_copy(memory_input_stream *mis)
{
        *mis->write++ = *mis->read++;
}

static inline size_t mis_string_complete(memory_input_stream *mis, byte **bytes)
{
        size_t len;

        *bytes = mis->string;
        if(mis->mark == mis->string) {
                // No escapes
                len = (size_t)(mis->read - mis->string);
        } else {
                mis_string_update(mis);
                len = (size_t)(mis->write - mis->string);
        }
        mis->read++;
        return len;
}
