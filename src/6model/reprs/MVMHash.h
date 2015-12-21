/* Representation used by VM-level hashes. */

#include <../../3rdparty/tommy/tommy.h>

struct MVMHashEntry {
    /* the tommy node inline struct. */
    tommy_node hash_node;

    /* key object (must be MVMString REPR) */
    MVMObject *key;

    /* value object */
    MVMObject *value;
};

struct MVMHashBody {

    /* linear chained hash */
    tommy_hashlin hash_head;

};
struct MVMHash {
    MVMObject common;
    MVMHashBody body;
};

typedef tommy_hash_t MVM_hash_t;

/* Function for REPR setup. */
const MVMREPROps * MVMHash_initialize(MVMThreadContext *tc);

#define MVM_HASHVAL(kdata, klen) tommy_hash_u32(0, (kdata), (klen))

#define MVM_HASH_ADD(body, entry, hashval) \
    tommy_hashlin_insert(&(body)->hash_head, &(entry)->hash_node, (entry), (hashval));

#define MVM_HASH_FIND(tc, body, keya, hashval, entry) do { \
    tommy_hashlin_node* node = tommy_hashlin_bucket(&(body)->hash_head, (hashval)); \
 \
    entry = NULL; \
    while (node) { \
        if (node->key == (hashval) && \
        MVM_string_equal(tc, (MVMString *)(keya), (MVMString *)(((MVMHashEntry *)node->data)->key))) { \
            entry = (MVMHashEntry *)node->data; \
            break; \
        } \
        else { \
            node = node->next; \
        } \
    } \
} while (0)

#define MVM_HASH_ACTION(tc, hash, name, entry, action, member, size) \
    action(hash_handle, hash, \
        name->body.storage.blob_32, MVM_string_graphs(tc, name) * sizeof(size), entry); \

#define MVM_HASH_ACTION_CACHE(tc, hash, name, entry, action, member, size) \
    action(hash_handle, hash, name->body.storage.blob_32, \
        MVM_string_graphs(tc, name) * sizeof(size), name->body.cached_hash_code, entry); \

#define MVM_HASH_ACTION_SELECT(tc, hash, name, entry, action) \
    MVM_HASH_ACTION(tc, hash, name, entry, action, storage.blob_32, MVMGrapheme32)

#define MVM_HASH_ACTION_SELECT_CACHE(tc, hash, name, entry, action) \
    MVM_HASH_ACTION_CACHE(tc, hash, name, entry, action, storage.blob_32, MVMGrapheme32) \

#define MVM_HASH_BIND(tc, hash, name, entry) \
    MVM_HASH_ACTION_SELECT_CACHE(tc, hash, name, entry, HASH_ADD_KEYPTR_CACHE)

#define MVM_HASH_GET(tc, hash, name, entry) \
    MVM_HASH_ACTION_SELECT_CACHE(tc, hash, name, entry, HASH_FIND_CACHE)

#define MVM_HASH_EXTRACT_KEY(tc, kdata, klen, key, error) \
if (REPR(key)->ID == MVM_REPR_ID_MVMString && IS_CONCRETE(key)) { \
    MVM_string_flatten(tc, (MVMString *)key); \
    *kdata = ((MVMString *)key)->body.storage.blob_32; \
    *klen  = ((MVMString *)key)->body.num_graphs * sizeof(MVMGrapheme32); \
} \
else { \
    MVM_exception_throw_adhoc(tc, error); \
}

#define MVM_HASH_DESTROY(hash_handle, hashentry_type, head_node) do { \
    hashentry_type *current, *tmp; \
    unsigned bucket_tmp; \
    HASH_ITER(hash_handle, head_node, current, tmp, bucket_tmp) { \
        if (current != head_node) \
            MVM_free(current); \
    } \
    tmp = head_node; \
    HASH_CLEAR(hash_handle, head_node); \
    MVM_free(tmp); \
} while (0)
