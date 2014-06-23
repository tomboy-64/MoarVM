struct MVMJitGraph {
    MVMSpeshGraph * spesh;
    MVMJitIns * first_ins;
    MVMJitIns * last_ins;

    MVMint32 num_labels;
    MVMJitLabel * labels;
};

/* A label */
struct MVMJitLabel {
    MVMint32 name;
    MVMSpeshBB *bb;
};

struct MVMJitPrimitive {
    MVMSpeshIns * ins;
};


/* Special branch target for the exit */
#define MVM_JIT_BRANCH_EXIT -1

/* What does a branch need? a label to go to, an instruction to read */

struct MVMJitBranch {
    MVMJitLabel dest;
    MVMSpeshIns * ins;
};

typedef enum {
    MVM_JIT_ADDR_STACK,    // relative to stack base
    MVM_JIT_ADDR_INTERP,   // interpreter variable
    MVM_JIT_ADDR_REG,      // relative to register base
    MVM_JIT_ADDR_LITERAL,  // constant value
} MVMJitAddrBase;

/* Some interpreter address definition */
#define MVM_JIT_INTERP_TC     0
#define MVM_JIT_INTERP_FRAME  1

struct MVMJitAddr {
    MVMJitAddrBase base;
    MVMint32 idx;
};

/* We support a few operations with return values.
 * (and I might add more :-))
 * a): store to register
 * b): store pointer value to register
 * c): store register to memory pointer */

typedef enum {
    MVM_JIT_RV_VAL_TO_REG,
    MVM_JIT_RV_REF_TO_REG,
    MVM_JIT_RV_REG_TO_PTR,
} MVMJitRVMode;;

struct MVMJitRVH { // return value handler
    MVMJitRVMode mode;
    MVMJitAddr   addr;
};


struct MVMJitCallC {
    void * func_ptr;     // what do we call
    MVMJitAddr * args;   // a list of arguments
    MVMuint16 num_args;  // how many arguments we pass
    MVMuint16 has_vargs; // does the receiver consider them variable
};



/* A non-final list of node types */
typedef enum {
    MVM_JIT_INS_PRIMITIVE,
    MVM_JIT_INS_CALL_C,
    MVM_JIT_INS_RVH,
    MVM_JIT_INS_BRANCH,
    MVM_JIT_INS_LABEL,
} MVMJitInsType;

struct MVMJitIns {
    MVMJitIns * next;   // linked list
    MVMJitInsType type; // tag
    union {
        MVMJitPrimitive prim;
        MVMJitCallC     call;
        MVMJitRVH       rvh;
        MVMJitBranch    branch;
        MVMJitLabel     label;
    } u;
};


void MVM_jit_log(MVMThreadContext *tc, const char *fmt, ...);
MVMJitGraph* MVM_jit_try_make_graph(MVMThreadContext *tc, MVMSpeshGraph *spesh);
MVMJitCode MVM_jit_compile_graph(MVMThreadContext *tc, MVMJitGraph *graph, size_t *codesize_out);
MVMuint8* MVM_jit_magic_bytecode(MVMThreadContext *tc);
void MVM_enter_jit(MVMThreadContext *tc, MVMFrame *frame, MVMJitCode jitcode);