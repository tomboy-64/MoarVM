/* Representation used by HashAttrStore. */

typedef struct MVMHashAttrEntry {
    /* key object (must be MVMString REPR) */
    MVMObject *key;

    /* value object */
    MVMObject *value;

    /* the uthash hash handle inline struct. */
    UT_hash_handle hash_handle;
} MVMHashAttrEntry;

struct MVMHashAttrStoreBody {
    /* The head of the hash, or null if the hash is empty.
     * The UT_HASH macros update this pointer directly. */
    MVMHashAttrEntry *hash_head;
};
struct MVMHashAttrStore {
    MVMObject common;
    MVMHashAttrStoreBody body;
};

/* Function for REPR setup. */
const MVMREPROps * MVMHashAttrStore_initialize(MVMThreadContext *tc);
