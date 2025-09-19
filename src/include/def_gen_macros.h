/*
 * Macros to make writing JSON via a generator more concise
 *
 * // This define specifies the name of your generator variable
 * // defaults to 'gen' 
 * #define JSNPG_GEN  my_gen
 * jsnpg_generator *my_gen = jsnpg_generator_new(...);
 *
 * #include <jsnpg/def_gen_macros.h>
 *
 * start_array();
 * object(
 *      key("k1"), real(12.5),
 *      key("k2"), array(boolean(true), boolean(false), null()),
 *      key("k3"), string("Value 3")
 *      );
 * end_array();
 *
 * #include <libjsnpg/undef_gen_macros.h>
 *
 * Produces:
 *
 * [
 *     {
 *         "k1": 12.5,
 *         "k2": [
 *             true,
 *             false,
 *             null
 *         ],
 *         "k3": "Value 3"
 *    }
 * ]
 *
 * Note: object() and array() macros expand to a single C statement
 *       so generating a large structure may hit compiler limits
 */

#include <string.h>

#ifndef JSNPG_GEN
#define JSNPG_GEN              gen
#endif

#ifndef JSNPG_MACROS
#define JSNPG_MACROS

#define object(...)             start_object(), __VA_ARGS__, end_object()
#define array(...)              start_array(), __VA_ARGS__, end_array()
#define keyval(K, ...)          key((K)), __VA_ARGS__

#define key(S)                  jsnpg_key((JSNPG_GEN), (unsigned char *)(S), strlen((S)))
#define string(S)               jsnpg_string((JSNPG_GEN), (unsigned char *)(S), strlen((S)))
#define boolean(B)              jsnpg_boolean((JSNPG_GEN), (B))
#define null()                  jsnpg_null((JSNPG_GEN))
#define integer(I)              jsnpg_integer((JSNPG_GEN), (I))
#define real(R)                 jsnpg_real((JSNPG_GEN), (R))
#define key_bytes(B, C)         jsnpg_key((JSNPG_GEN), (B), (C))
#define string_bytes(B, C)      jsnpg_string((JSNPG_GEN), (B), (C))
#define start_object()          jsnpg_start_object((JSNPG_GEN))
#define end_object()            jsnpg_end_object((JSNPG_GEN))
#define start_array()           jsnpg_start_array((JSNPG_GEN))
#define end_array()             jsnpg_end_array((JSNPG_GEN))

#endif

