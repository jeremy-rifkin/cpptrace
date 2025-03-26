#ifndef JIT_INTERFACE_HPP
#define JIT_INTERFACE_HPP

#include <stdint.h>

// https://sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html

extern "C" {

typedef enum
{
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
} jit_actions_t;

struct jit_code_entry
{
    struct jit_code_entry *next_entry;
    struct jit_code_entry *prev_entry;
    const char *symfile_addr;
    uint64_t symfile_size;
};

struct jit_descriptor
{
    uint32_t version;
    /* This type should be jit_actions_t, but we use uint32_t
        to be explicit about the bitwidth.  */
    uint32_t action_flag;
    struct jit_code_entry *relevant_entry;
    struct jit_code_entry *first_entry;
};

// /* GDB puts a breakpoint in this function.  */
// void __attribute__((noinline)) __jit_debug_register_code() { };

/* Make sure to specify the version statically, because the
    debugger may check the version before we can set it.  */
extern struct jit_descriptor __jit_debug_descriptor; // = { 1, 0, 0, 0 };

}

#endif
