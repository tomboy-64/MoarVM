#include "moar.h"

/* This representation's function pointer table. */
static const MVMREPROps this_repr;

/* Creates a new type object of this representation, and associates it with
 * the given HOW. */
static MVMObject * type_object_for(MVMThreadContext *tc, MVMObject *HOW) {
    MVMSTable *st = MVM_gc_allocate_stable(tc, &this_repr, HOW);

    MVMROOT(tc, st, {
        MVMObject *obj = MVM_gc_allocate_type_object(tc, st);
        MVM_ASSIGN_REF(tc, &(st->header), st->WHAT, obj);
        st->size = sizeof(MVMHash);
    });

    return st->WHAT;
}

/* Initializes a new instance. */
static void initialize(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data) {
    MVMHashBody *body = (MVMHashBody *)data;

    tommy_hashlin_init(&body->hash_head);
}

MVM_STATIC_INLINE void extract_key(MVMThreadContext *tc, void **kdata, size_t *klen, MVMObject *key) {
    MVM_HASH_EXTRACT_KEY(tc, kdata, klen, key, "MVMHash representation requires MVMString keys")
}

/* Copies the body of one object to another. */
static void copy_to(MVMThreadContext *tc, MVMSTable *st, void *src, MVMObject *dest_root, void *dest) {
    MVMHashBody *src_body  = (MVMHashBody *)src;
    MVMHashBody *dest_body = (MVMHashBody *)dest;
    tommy_count_t bucket_max;
    tommy_count_t pos;

    /* number of valid buckets */
    bucket_max = src_body->hash_head.low_max + src_body->hash_head.split;

    for (pos = 0; pos < bucket_max; ++pos) {
        tommy_hashlin_node* node = *tommy_hashlin_pos(&src_body->hash_head, pos);

        while (node) {
            MVMHashEntry *current = (MVMHashEntry *)node->data;
            MVMHashEntry *new_entry = MVM_fixed_size_alloc(tc, tc->instance->fsa,
                sizeof(MVMHashEntry));

            /* cached hashval is in node->key */
            MVM_HASH_ADD(dest_body, new_entry, node->key);
            node = node->next;

            MVM_ASSIGN_REF(tc, &(dest_root->header), new_entry->key, current->key);
            MVM_ASSIGN_REF(tc, &(dest_root->header), new_entry->value, current->value);
        }
    }
}

/* Adds held objects to the GC worklist. */
static void gc_mark(MVMThreadContext *tc, MVMSTable *st, void *data, MVMGCWorklist *worklist) {
    MVMHashBody *body = (MVMHashBody *)data;
    tommy_count_t bucket_max;
    tommy_count_t pos;

    /* number of valid buckets */
    bucket_max = body->hash_head.low_max + body->hash_head.split;

    for (pos = 0; pos < bucket_max; ++pos) {
        tommy_hashlin_node* node = *tommy_hashlin_pos(&body->hash_head, pos);

        while (node) {
            MVMHashEntry *current = (MVMHashEntry *)node->data;
            node = node->next;
            MVM_gc_worklist_add(tc, worklist, current->key);
            MVM_gc_worklist_add(tc, worklist, current->value);
        }
    }
}

/* Called by the VM in order to free memory associated with this object. */
static void gc_free(MVMThreadContext *tc, MVMObject *obj) {
    MVMHashBody *body = &((MVMHash *)obj)->body;
    tommy_count_t bucket_max;
    tommy_count_t pos;

    /* number of valid buckets */
    bucket_max = body->hash_head.low_max + body->hash_head.split;

    for (pos = 0; pos < bucket_max; ++pos) {
        tommy_hashlin_node* node = *tommy_hashlin_pos(&body->hash_head, pos);

        while (node) {
            MVMHashEntry *current = (MVMHashEntry *)node->data;
            node = node->next;
            MVM_fixed_size_free(tc, tc->instance->fsa, sizeof(MVMHashEntry), current);
        }
    }

    tommy_hashlin_done(&body->hash_head);
}

static void at_key(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMObject *key, MVMRegister *result, MVMuint16 kind) {
    MVMHashBody *body = (MVMHashBody *)data;
    void *kdata;
    MVMHashEntry *entry;
    size_t klen;

    extract_key(tc, &kdata, &klen, key);
    MVM_HASH_FIND(tc, body, key, MVM_HASHVAL(kdata, klen), entry);
    if (kind == MVM_reg_obj)
        result->o = entry != NULL ? entry->value : tc->instance->VMNull;
    else
        MVM_exception_throw_adhoc(tc,
            "MVMHash representation does not support native type storage");
}

static void bind_key(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMObject *key, MVMRegister value, MVMuint16 kind) {
    MVMHashBody *body = (MVMHashBody *)data;
    void *kdata;
    MVMHashEntry *entry;
    size_t klen;
    MVM_hash_t hashval;

    extract_key(tc, &kdata, &klen, key);
    hashval = MVM_HASHVAL(kdata, klen);

    /* first check whether we can must update the old entry. */
    MVM_HASH_FIND(tc, body, key, hashval, entry);
    if (!entry) {
        entry = MVM_fixed_size_alloc(tc, tc->instance->fsa, sizeof(MVMHashEntry));
        MVM_HASH_ADD(body, entry, hashval);
    }
    MVM_ASSIGN_REF(tc, &(root->header), entry->key, key);
    if (kind == MVM_reg_obj) {
        MVM_ASSIGN_REF(tc, &(root->header), entry->value, value.o);
    }
    else {
        MVM_exception_throw_adhoc(tc,
            "MVMHash representation does not support native type storage");
    }
}

static MVMuint64 elems(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data) {
    MVMHashBody *body = (MVMHashBody *)data;
    return tommy_hashlin_count(&body->hash_head);
}

static MVMint64 exists_key(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMObject *key) {
    MVMHashBody *body = (MVMHashBody *)data;
    void *kdata;
    MVMHashEntry *entry;
    size_t klen;
    MVM_hash_t hashval;

    extract_key(tc, &kdata, &klen, key);
    hashval = MVM_HASHVAL(kdata, klen);

    MVM_HASH_FIND(tc, body, key, hashval, entry);
    return entry != NULL;
}

static void delete_key(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMObject *key) {
    MVMHashBody *body = (MVMHashBody *)data;
    MVMHashEntry *old_entry;
    size_t klen;
    void *kdata;
    MVM_hash_t hashval;

    extract_key(tc, &kdata, &klen, key);
    hashval = MVM_HASHVAL(kdata, klen);

    MVM_HASH_FIND(tc, body, key, hashval, old_entry);
    if (old_entry) {
        tommy_hashlin_remove_existing(&body->hash_head, &old_entry->hash_node);
        MVM_fixed_size_free(tc, tc->instance->fsa, sizeof(MVMHashEntry), old_entry);
    }
}

static MVMStorageSpec get_value_storage_spec(MVMThreadContext *tc, MVMSTable *st) {
    MVMStorageSpec spec;
    spec.inlineable      = MVM_STORAGE_SPEC_REFERENCE;
    spec.boxed_primitive = MVM_STORAGE_SPEC_BP_NONE;
    spec.can_box         = 0;
    spec.bits            = 0;
    spec.align           = 0;
    spec.is_unsigned     = 0;
    return spec;
}

static const MVMStorageSpec storage_spec = {
    MVM_STORAGE_SPEC_REFERENCE, /* inlineable */
    0,                          /* bits */
    0,                          /* align */
    MVM_STORAGE_SPEC_BP_NONE,   /* boxed_primitive */
    0,                          /* can_box */
    0,                          /* is_unsigned */
};

/* Gets the storage specification for this representation. */
static const MVMStorageSpec * get_storage_spec(MVMThreadContext *tc, MVMSTable *st) {
    return &storage_spec;
}

/* Compose the representation. */
static void compose(MVMThreadContext *tc, MVMSTable *st, MVMObject *info) {
    /* XXX key and value types will be communicated here */
}

/* Set the size of the STable. */
static void deserialize_stable_size(MVMThreadContext *tc, MVMSTable *st, MVMSerializationReader *reader) {
    st->size = sizeof(MVMHash);
}

/* Bytecode specialization for this REPR. */
static void spesh(MVMThreadContext *tc, MVMSTable *st, MVMSpeshGraph *g, MVMSpeshBB *bb, MVMSpeshIns *ins) {
    switch (ins->info->opcode) {
    case MVM_OP_create: {
        if (!(st->mode_flags & MVM_FINALIZE_TYPE)) {
            MVMSpeshOperand target   = ins->operands[0];
            MVMSpeshOperand type     = ins->operands[1];
            ins->info                = MVM_op_get_op(MVM_OP_sp_fastcreate);
            ins->operands            = MVM_spesh_alloc(tc, g, 3 * sizeof(MVMSpeshOperand));
            ins->operands[0]         = target;
            ins->operands[1].lit_i16 = sizeof(MVMHash);
            ins->operands[2].lit_i16 = MVM_spesh_add_spesh_slot(tc, g, (MVMCollectable *)st);
            MVM_spesh_get_facts(tc, g, type)->usages--;
        }
        break;
    }
    }
}

/* Initializes the representation. */
const MVMREPROps * MVMHash_initialize(MVMThreadContext *tc) {
    return &this_repr;
}

static const MVMREPROps this_repr = {
    type_object_for,
    MVM_gc_allocate_object,
    initialize,
    copy_to,
    MVM_REPR_DEFAULT_ATTR_FUNCS,
    MVM_REPR_DEFAULT_BOX_FUNCS,
    MVM_REPR_DEFAULT_POS_FUNCS,
    {
        at_key,
        bind_key,
        exists_key,
        delete_key,
        get_value_storage_spec
    },    /* ass_funcs */
    elems,
    get_storage_spec,
    NULL, /* change_type */
    NULL, /* serialize */
    NULL, /* deserialize */
    NULL, /* serialize_repr_data */
    NULL, /* deserialize_repr_data */
    deserialize_stable_size,
    gc_mark,
    gc_free,
    NULL, /* gc_cleanup */
    NULL, /* gc_mark_repr_data */
    NULL, /* gc_free_repr_data */
    compose,
    spesh,
    "VMHash", /* name */
    MVM_REPR_ID_MVMHash,
    0, /* refs_frames */
};
