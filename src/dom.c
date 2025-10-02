/*
 * jsnpg - a JSON parser/generator
 * © 2025 Bob Davison (see also: LICENSE)
 *
 * dom.c
 *   some json parse test frameworks require a dom style parse
 *   this is a minimal implementation which just
 *   stores parse results in memory so that they can be replayed
 *
 *   dom_generator creates the in memory data structure
 *   dom_parse/dom_parse_next replays the data as if from a regular parse
 */

/*
 * Most JSON items just require one dom_node to store the type
 *
 * Booleans require an extra node to store true/false (this could be
 * stored in the high byte of the type, saving 64bits, at the cost of
 * additional complexity)
 *
 * Numbers require an extra node to store the long or double value
 *
 * Strings and keys need two extra nodes, one for the byte count in the string
 * and one for a pointer to the string. (They could be stored together if we used
 * unsigned count and offset and limited ourselves to 4GB JSON, not unreasonable)
*
 * The actual string bytes are held by the allocator that holds the dom nodes
 * so they will be deallocated together.
 */

#include <stdint.h> 

#define DOM_MIN_SIZE 8192
#define NODE_SIZE (sizeof(dom_node))

typedef struct dom_node dom_node;

struct dom_node {
        union {
                json_type type;
                bool boolean;
                double real;
                long integer;
                size_t count;
                byte *bytes;
        } is;
};

// Dom Info is stored by parser when pull-parsing
// to maintain its position in the parse
static dom_info dom_parser_info(dom *root)
{
        dom_info di;
        di.hdr = root;
        di.offset = sizeof(dom);
        return di;
}

// Ensure all sizes align with structure size
static inline size_t dom_size_align(size_t size)
{
        return NODE_SIZE * (1 + ((size - 1) / NODE_SIZE));
}


// Ensure minimum size
static inline size_t dom_alloc_size(size_t size)
{
        size = size < DOM_MIN_SIZE 
                ? DOM_MIN_SIZE 
                : size;
        return dom_size_align(size);
}

static dom *dom_hdr_new(allocator *a, size_t size)
{
        size = dom_alloc_size(size + sizeof(dom));

        dom *hdr = allocator_alloc(a, size);

        if(!hdr)
                return NULL;

        hdr->next = NULL;
        hdr->current = NULL;
        hdr->size = size;
        hdr->count = sizeof(dom);

        return hdr;
}

static dom_node *dom_node_next(dom *root, int nodes)
{
        size_t required = dom_size_align(NODE_SIZE * nodes);
        dom *hdr = root->current;
        if(required > hdr->size - hdr->count) {
                dom *new = dom_hdr_new(root->allocator, required);

                if(!new)
                        return NULL;

                hdr->next = new;
                root->current = new;
                hdr = new;
        }
        size_t offset = hdr->count;
        hdr->count += required;
        return (dom_node *)(offset + (char *)hdr);
}

static dom_node *dom_add_type(dom *root, json_type type, int extra)
{
        dom_node *node = dom_node_next(root, 1 + extra);
        if(!node)
                return NULL;
        node->is.type = type;

        return node;
}

static inline dom_node *dom_add_boolean(dom *root, bool boolean)
{
        dom_node *node = dom_add_type(root, JSNPG_BOOLEAN, 1);
        if(!node)
                return NULL;

        node++;
        node->is.boolean = boolean;

        return node;
}

static inline dom_node *dom_add_integer(dom *root, long integer)
{
        dom_node *node = dom_add_type(root, JSNPG_INTEGER, 1);
        if(!node)
                return NULL;

        node++;
        node->is.integer = integer;

        return node;
}

static inline dom_node *dom_add_real(dom *root, double real)
{
        dom_node *node = dom_add_type(root, JSNPG_REAL, 1);
        if(!node)
                return NULL;

        node++;
        node->is.real = real;

        return node;
}

static inline dom_node *dom_add_bytes(dom *root, json_type type, const byte *bytes, size_t count)
{
        dom_node *node = dom_add_type(root, type, 2);
        if(!node)
                return NULL;

        node++;
        node->is.count = count;
        node++;
        node->is.bytes = (byte *)bytes;

        return node;
}

static inline bool dom_boolean(void *ctx, bool boolean)
{
        dom *root = ctx;
        return dom_add_boolean(root, boolean);
}

static inline bool dom_null(void *ctx)
{
        dom *root = ctx;
        return dom_add_type(root, JSNPG_NULL, 0);
}

static inline bool dom_integer(void *ctx, long integer)
{
        dom *root = ctx;
        return dom_add_integer(root, integer);
}

static inline bool dom_real(void *ctx, double real)
{
        dom *root = ctx;
        return dom_add_real(root, real);
}

static inline bool dom_string(void *ctx, const byte *bytes, size_t count)
{
        dom *root = ctx;
        return dom_add_bytes(root, JSNPG_STRING, bytes, count);
}

static inline bool dom_key(void *ctx, const byte *bytes, size_t count)
{
        dom *root = ctx;
        return dom_add_bytes(root, JSNPG_KEY, bytes, count);
}

static inline bool dom_start_array(void *ctx)
{
        dom *root = ctx;
        return dom_add_type(root, JSNPG_START_ARRAY, 0);
}

static inline bool dom_end_array(void *ctx)
{
        dom *root = ctx;
        return dom_add_type(root, JSNPG_END_ARRAY, 0);
}

static inline bool dom_start_object(void *ctx)
{
        dom *root = ctx;
        return dom_add_type(root, JSNPG_START_OBJECT, 0);
}

static inline bool dom_end_object(void *ctx)
{
        dom *root = ctx;
        return dom_add_type(root, JSNPG_END_OBJECT, 0);
}


static callbacks dom_callbacks = {
        .boolean = dom_boolean,
        .null = dom_null,
        .integer = dom_integer,
        .real = dom_real,
        .string = dom_string,
        .key = dom_key,
        .start_array = dom_start_array,
        .end_array = dom_end_array,
        .start_object = dom_start_object,
        .end_object = dom_end_object,
};

static dom *dom_new(allocator *a, size_t size)
{
        dom *root = dom_hdr_new(a, size);
        if(!root)
                return NULL;

        root->allocator = a;
        root->current = root;
        return root;
}

static generator *dom_generator(generator *g)
{
        dom *root = dom_new(g->allocator, 0);
        if(!root)
                return NULL;

        return generator_set_callbacks(g, &dom_callbacks, root);
}

// Pull Parse from DOM
static json_type dom_parse_next(parser *p)
{
        dom *hdr = p->dom_info.hdr;
        size_t offset = p->dom_info.offset;

        if(offset >= hdr->count) {
                hdr = hdr->next;
                if(!hdr) {
                        p->result.type = JSNPG_EOF;
                        return JSNPG_EOF;
                }
                offset = sizeof(dom);
        }

        dom_node *node = (dom_node *)(offset + (char *)hdr);
        offset += NODE_SIZE;
        json_type type = node->is.type;
        switch(type) {
        case JSNPG_BOOLEAN:
                node++;
                offset += NODE_SIZE;
                p->result.boolean = node->is.boolean;
                break;
        case JSNPG_INTEGER:
                node++;
                offset += NODE_SIZE;
                p->result.integer = node->is.integer;
                break;
        case JSNPG_REAL:
                node++;
                offset += NODE_SIZE;
                p->result.real = node->is.real;
                break;
        case JSNPG_STRING:
        case JSNPG_KEY:
                node++;
                offset += NODE_SIZE;
                p->result.string.count = node->is.count;
                node++;
                offset += NODE_SIZE;
                p->result.string.bytes = node->is.bytes;
                break;
        default:
        }
        p->dom_info.hdr = hdr;
        p->dom_info.offset = offset;

        p->result.type = type;
        return type;
}

// SAX parse from DOM
static parse_result dom_parse(parser *p, generator *g)
{
        // const boolean gen_boolean = g->callbacks->boolean ? g->callbacks->boolean : void_boolean;
        // for all callbacks
        DECLARE_CALLBACKS(g, gen_);

        void *gen_ctx = g->ctx;

        size_t count;
        dom *hdr = p->dom_info.hdr;
        size_t offset = sizeof(dom);
        bool ok = true;

        while(hdr && ok) {
                dom_node *node = (dom_node *)(offset + (char *)hdr);
                offset += NODE_SIZE;
                json_type type = node->is.type;

                switch(type) {
                case JSNPG_STRING:
                        node++;
                        offset += NODE_SIZE;
                        count = node->is.count;
                        node++;
                        offset += NODE_SIZE;
                        ok = gen_string(gen_ctx, node->is.bytes, count);
                        break;

                case JSNPG_KEY:
                        node++;
                        offset += NODE_SIZE;
                        count = node->is.count;
                        node++;
                        offset += NODE_SIZE;
                        ok = gen_key(gen_ctx, node->is.bytes, count);
                        break;

                case JSNPG_BOOLEAN:
                        node++;
                        offset += NODE_SIZE;
                        ok = gen_boolean(gen_ctx, node->is.boolean);
                        break;

                case JSNPG_NULL:
                        ok = gen_null(gen_ctx);
                        break;

                case JSNPG_START_OBJECT:
                        ok = gen_start_object(gen_ctx);
                        break;

                case JSNPG_END_OBJECT:
                        ok = gen_end_object(gen_ctx);
                        break;

                case JSNPG_START_ARRAY:
                        ok = gen_start_array(gen_ctx);
                        break;

                case JSNPG_END_ARRAY:
                        ok = gen_end_array(gen_ctx);
                        break;

                case JSNPG_INTEGER:
                        node++;
                        offset += NODE_SIZE;
                        ok = gen_integer(gen_ctx, node->is.integer);
                        break;

                case JSNPG_REAL:
                        node++;
                        offset += NODE_SIZE;
                        ok = gen_real(gen_ctx, node->is.real);
                        break;

                default:
                        ok = false;
                }
                
                if(offset >= hdr->count && ok) {
                        offset = sizeof(dom);
                        hdr = hdr->next;
                }
        }

        if(!ok)
                return make_pg_error_return(p, g);

        return (parse_result) { .type = JSNPG_EOF };

}

// Public API
// Validate arguments

dom *jsnpg_result_dom(generator *g)
{
        return g ? g->ctx : NULL;
}

