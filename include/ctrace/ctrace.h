#ifndef CTRACE_H
#define CTRACE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
 #define CTRACE_BEGIN_DEFINITIONS extern "C" {
 #define CTRACE_END_DEFINITIONS }
#else
 #define CTRACE_BEGIN_DEFINITIONS
 #define CTRACE_END_DEFINITIONS
#endif

#ifdef _MSC_VER
 #define CTRACE_FORCE_NO_INLINE __declspec(noinline)
#else
 #define CTRACE_FORCE_NO_INLINE __attribute__((noinline))
#endif

#ifdef _MSC_VER
 #define CTRACE_FORCE_INLINE __forceinline
#elif defined(__clang__) || defined(__GNUC__)
 #define CTRACE_FORCE_INLINE __attribute__((always_inline)) inline
#else
 #define CTRACE_FORCE_INLINE inline
#endif

// See `CPPTRACE_PATH_MAX` for more info.
#define CTRACE_PATH_MAX 4096

CTRACE_BEGIN_DEFINITIONS
    typedef struct raw_trace ctrace_raw_trace;
    typedef struct object_trace ctrace_object_trace;
    typedef struct stacktrace ctrace_stacktrace;

    // Represents a boolean value, ensures a consistent ABI.
    typedef int8_t ctrace_bool;
    // A type that can represent a pointer, alias for `uintptr_t`.
    typedef uintptr_t ctrace_frame_ptr;
    typedef struct object_frame ctrace_object_frame;
    typedef struct stacktrace_frame ctrace_stacktrace_frame;

    // Type-safe null-terminated string wrapper
    typedef struct {
        const char* data;
    } ctrace_owning_string;

    struct object_frame {
        ctrace_frame_ptr raw_address;
        ctrace_frame_ptr obj_address;
        const char* obj_path;
        // const char* symbol;
    };

    struct stacktrace_frame {
        ctrace_frame_ptr address;
        uint32_t line;
        uint32_t column;
        const char* filename;
        const char* symbol;
        ctrace_bool is_inline;
    };

    struct raw_trace {
        ctrace_frame_ptr* frames;
        size_t count;
    };

    struct object_trace {
        ctrace_object_frame* frames;
        size_t count;
    };

    struct stacktrace {
        ctrace_stacktrace_frame* frames;
        size_t count;
    };

    typedef enum {
        // Only minimal lookup tables
        ctrace_prioritize_memory = 0,
        // Build lookup tables but don't keep them around between trace calls
        ctrace_hybrid = 1,
        // Build lookup tables as needed
        ctrace_prioritize_speed = 2
    } ctrace_cache_mode;

    // ctrace::string:
    ctrace_owning_string ctrace_generate_owning_string(const char* raw_string);
    void ctrace_free_owning_string(ctrace_owning_string* string);

    // ctrace::generation:
    ctrace_raw_trace    ctrace_generate_raw_trace(size_t skip, size_t max_depth);
    ctrace_object_trace ctrace_generate_object_trace(size_t skip, size_t max_depth);
    ctrace_stacktrace   ctrace_generate_trace(size_t skip, size_t max_depth);

    // ctrace::freeing:
    void ctrace_free_raw_trace(ctrace_raw_trace* trace);
    void ctrace_free_object_trace(ctrace_object_trace* trace);
    void ctrace_free_stacktrace(ctrace_stacktrace* trace);

    // ctrace::resolve:
    ctrace_stacktrace ctrace_raw_trace_resolve(const ctrace_raw_trace* trace);
    ctrace_object_trace ctrace_raw_trace_resolve_object_trace(const ctrace_raw_trace* trace);
    ctrace_stacktrace ctrace_object_trace_resolve(const ctrace_object_trace* trace);

    // ctrace::io:
    ctrace_owning_string ctrace_stacktrace_to_string(const ctrace_stacktrace* trace, ctrace_bool use_color);
    void ctrace_stacktrace_print(const ctrace_stacktrace* trace, FILE* to, ctrace_bool use_color);

    // utility::demangle:
    ctrace_owning_string ctrace_demangle(const char* mangled);

    // utility::io:
    int ctrace_stdin_fileno(void);
    int ctrace_stderr_fileno(void);
    int ctrace_stdout_fileno(void);
    ctrace_bool ctrace_isatty(int fd);
    
    // utility::cache:
    void ctrace_set_cache_mode(ctrace_cache_mode mode);
    ctrace_cache_mode ctrace_get_cache_mode(void);

CTRACE_END_DEFINITIONS

#endif
