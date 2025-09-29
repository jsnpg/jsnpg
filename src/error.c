/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * error.c
 *   create error information for parser/generator
 */

#include <stdio.h>

static const char *error_msgs[] = {
        [JSNPG_ERROR_OPT]               = "Invalid option",
        [JSNPG_ERROR_ALLOC]             = "Out of memory",
        [JSNPG_ERROR_NUMBER]            = "Invalid number",
        [JSNPG_ERROR_UTF8]	        = "Invalid UTF-8",
        [JSNPG_ERROR_SURROGATE]	        = "Invalid surrogate",
        [JSNPG_ERROR_STACK_OVERFLOW]	= "Stack overflow",
        [JSNPG_ERROR_STACK_UNDERFLOW]	= "Stack underflow",
        [JSNPG_ERROR_EXPECTED_VALUE]	= "Value expected",
        [JSNPG_ERROR_EXPECTED_KEY]	= "Key expected",
        [JSNPG_ERROR_NO_OBJECT]	        = "Not in object",
        [JSNPG_ERROR_NO_ARRAY]	        = "Not in array",
        [JSNPG_ERROR_ESCAPE]	        = "Invalid escape",
        [JSNPG_ERROR_UNEXPECTED]	= "Unexpected input",
        [JSNPG_ERROR_INVALID]	        = "Invalid input",
        [JSNPG_ERROR_TERMINATED]	= "Generator terminated",
        [JSNPG_ERROR_EOF]	        = "Unexpected end of input"
};
        

static const char *error_text(error_code code)
{
        static const int msg_count = sizeof(error_msgs) / sizeof(error_msgs[0]);

        if(code >= 0 && code < msg_count)
                return error_msgs[code];
        else
                return "Unknown error";
}

static error_info make_error(error_code code)
{
        return (error_info){ 
                .code = code, 
                .text = error_text(code)
        };
}

static parse_result make_error_return(error_code code, size_t at)
{
        return (parse_result) {
                        .type = JSNPG_ERROR,
                        .position = at,
                        .error = make_error(code)
        };
}

static parse_result make_pg_error_return(parser *p, generator *g)
{
        parse_result r = p->result;
        if(r.type == JSNPG_ERROR) {
                // Terminations come from generator
                // Which MAY have set error info
                if(r.error.code == JSNPG_ERROR_TERMINATED
                                && g->error.code) {
                        r.error = g->error;
                }
        } else {
                r = make_error_return(JSNPG_ERROR_UNEXPECTED, 0);
        }
        return r;
}


