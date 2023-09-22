#ifndef CTRACE_H
#define CTRACE_H

#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#undef  ON
#define ON 1
#undef  OFF
#define OFF 0

#if defined(__cplusplus)
# define C_LINKAGE extern "C"
# define BEGIN_CTRACE C_LINKAGE {
# define END_CTRACE }
#else
# define C_LINKAGE
# define BEGIN_CTRACE
# define END_CTRACE
#endif

#if !defined(__cplusplus)
# if defined(__STDC_VERSION__)
#  if __STDC_VERSION__ >= 201112L
#   define CTRACE_C11_FEATURES ON
#  else
#   define CTRACE_C11_FEATURES OFF
#  endif
# else
#  define CTRACE_C11_FEATURES OFF
# endif
#else
# define CTRACE_C11_FEATURES OFF
#endif

#if defined(__cplusplus)
# define CTRACE_NORETURN
#elif CTRACE_C11_FEATURES == ON
# include <stdnoreturn.h>
# define CTRACE_NORETURN noreturn
#else
# define CTRACE_NORETURN
#endif

#if defined(__cplusplus)
# define CTRACE_NULL nullptr
#else
# define CTRACE_NULL NULL
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
# define CTRACE_API C_LINKAGE __declspec(dllexport)
#else
# define CTRACE_EXPORT C_LINKAGE
#endif

#if !defined(CTRACE_EXCEPTIONS)
# define CTRACE_EXCEPTIONS OFF
#endif

#if !defined(CTRACE_FREESTANDING)
# if defined(__STDC_HOSTED__)
#  if __STDC_HOSTED__ == 1
#   define CTRACE_FREESTANDING OFF
#  else
#   define CTRACE_FREESTANDING ON
#  endif
# else
#  define CTRACE_FREESTANDING OFF
# endif
#endif

#define CTRACE_INTERNAL static
#define CTRACE_BITCOUNT(x) (sizeof(x) * CHAR_BIT)
#define CTRACE_EFLAG(n) (1 << (n))

#define CTRACE_ESC     "\033["
#define CTRACE_RESET   CTRACE_ESC "0m"
#define CTRACE_RED     CTRACE_ESC "31m"
#define CTRACE_GREEN   CTRACE_ESC "32m"
#define CTRACE_YELLOW  CTRACE_ESC "33m"

BEGIN_CTRACE
    typedef enum { ctrace_false, ctrace_true } ctrace_boolean;
    typedef const char* ctrace_format_t;

    // general::string:
    typedef struct {
        const void* data;
        unsigned type;
    } ctrace_string;

    struct ctrace_buffered_string;
    struct ctrace_exception_ctx;

    /* Signifies `ctrace_free_string` should not be called.
     */
    typedef ctrace_string ctrace_managed_string;

    /* Signifies a buffered string is required for effects.
     */
    typedef ctrace_string ctrace_mut_string;

    /* Allocates a buffer of len + 1.
     */
    CTRACE_API ctrace_mut_string ctrace_make_buffered_string(unsigned len);
    CTRACE_API void ctrace_clear_string(ctrace_mut_string string);
    CTRACE_API void ctrace_write_string(ctrace_mut_string to, ctrace_string from);
    CTRACE_API void ctrace_write_move_string(ctrace_mut_string to, ctrace_string from);
    CTRACE_API void ctrace_format_string(ctrace_mut_string to, ctrace_format_t, ...);

    CTRACE_API ctrace_string ctrace_make_string(const char* cstring);
    CTRACE_API const char* ctrace_get_cstring(ctrace_string string);
    CTRACE_API const char* ctrace_get_string_type(ctrace_string string);
    CTRACE_API const char* ctrace_get_underlying_string_type(ctrace_string string);
    CTRACE_API unsigned ctrace_get_string_size(ctrace_string string);
    CTRACE_API void ctrace_free_string(ctrace_string string);
    CTRACE_API int ctrace_fputs(ctrace_string string, FILE* f);
    CTRACE_API int ctrace_puts(ctrace_string string);

    // api:
    typedef void* ctrace_frame_types;

    typedef struct {
        ctrace_frame_types data;
        struct {
            unsigned type : CTRACE_BITCOUNT(unsigned) - 1;
            ctrace_boolean resolved : 1;
        };
    } ctrace_wrapped_trace,
        ctrace_raw_trace,
        ctrace_object_trace,
        ctrace_trace;

    typedef struct {
        void* curr;
        const void* end;
        unsigned type;
    } ctrace_iterator;

    typedef enum {
        ectrace_default,     // same as ectrace_out
        ectrace_cstream      = CTRACE_EFLAG(1),
        ectrace_out          = CTRACE_EFLAG(2),
        ectrace_err          = CTRACE_EFLAG(3),
        ectrace_custom       = CTRACE_EFLAG(4),
        ectrace_no_color     = CTRACE_EFLAG(5),
        ectrace_no_newline   = CTRACE_EFLAG(6),
    } ctrace_print_options;

    CTRACE_API ctrace_trace ctrace_generate_raw_trace(unsigned skip);
    CTRACE_API ctrace_trace ctrace_generate_object_trace(unsigned skip);
    CTRACE_API ctrace_trace ctrace_generate_trace(unsigned skip);
    CTRACE_API ctrace_trace ctrace_resolve(ctrace_trace trace);
    CTRACE_API ctrace_trace ctrace_move_resolve(ctrace_trace trace);
    CTRACE_API ctrace_trace ctrace_safe_move_resolve(ctrace_trace* trace);
    CTRACE_API ctrace_iterator ctrace_begin(ctrace_trace trace);
    CTRACE_API ctrace_boolean ctrace_end(ctrace_iterator iter);
    CTRACE_API void ctrace_next(ctrace_iterator* iter);
    CTRACE_API void ctrace_print_trace(ctrace_trace trace, int opts, ...);
    CTRACE_API ctrace_string ctrace_stringify_trace(ctrace_trace trace);
    CTRACE_API void ctrace_clear_trace(ctrace_trace trace);
    CTRACE_API void ctrace_free_trace(ctrace_trace trace);

    // utilities:
    CTRACE_API ctrace_string ctrace_demangle(ctrace_string symbol);
    CTRACE_API ctrace_string ctrace_move_demangle(ctrace_string symbol);

    // general::logging
    typedef struct ctrace_logging_ctx* ctrace_plog_ctx;
    typedef void(*ctrace_logger_t)(ctrace_plog_ctx, ctrace_format_t, va_list*);

    typedef struct {
        const ctrace_string* curr;
        const ctrace_string* end;
    } ctrace_log_iterator;

    /* Not thread safe. Only set on the main thread.
     */
    CTRACE_API void ctrace_log_mode(ctrace_logger_t logger);
    CTRACE_API void ctrace_log_to(FILE* stream);

    CTRACE_API void ctrace_log(ctrace_format_t format, ...);
    CTRACE_API ctrace_log_iterator ctrace_log_begin();
    CTRACE_API ctrace_boolean ctrace_log_end(ctrace_log_iterator iter);
    CTRACE_API void ctrace_log_next(ctrace_log_iterator* iter);

    /* The default logging functions work as follows:
     * off: noop, messages are not recorded or printed
     * quiet: messages are recorded but not printed
     * debug: messages are immediately printed to the bound FILE*
     * trace: same as debug, but also prints a stacktrace [heavy]
     */

    CTRACE_API void ctrace_loff(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs);
    CTRACE_API void ctrace_lquiet(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs);
    CTRACE_API void ctrace_ldebug(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs);
    CTRACE_API void ctrace_ltrace(ctrace_plog_ctx ctx, ctrace_format_t format, va_list* pargs);

    // general::exception:
    typedef struct ctrace_registry_data* (*ctrace_exception_t)();
    typedef const struct ctrace_registry_data* ctrace_exception_ptr_t;
    typedef ctrace_exception_ptr_t ctrace_ex_t;
    typedef ctrace_string(*ctrace_message_t)();

    struct ctrace_registry_data {
        unsigned id;
        const char* name;
        ctrace_message_t what;
        ctrace_trace trace;
    };

#if CTRACE_EXCEPTIONS == ON
    CTRACE_API void ctrace_register_exception(ctrace_exception_t type, ctrace_message_t what);
    CTRACE_API ctrace_managed_string ctrace_exception_string(ctrace_string string);

    CTRACE_NORETURN CTRACE_API void ctrace_throw(ctrace_exception_t type);
    CTRACE_API ctrace_exception_ptr_t ctrace_exception_ptr();
    CTRACE_API unsigned ctrace_exception_id();
    CTRACE_API void ctrace_exception_release();

    CTRACE_API struct ctrace_exception_ctx* ctrace_internal_get_buf();
    CTRACE_API struct ctrace_registry_data* cpptrace_exception();
    CTRACE_API ctrace_managed_string ctrace_user_cpptrace_exception_what();
#endif
END_CTRACE

#define CTRACE_TRACE_FOREACH(name, trace)                               \
    for(ctrace_iterator m__iter = ctrace_begin(trace),                  \
        *name = &m__iter; !ctrace_end(m__iter); ctrace_next(name))

#define CTRACE_LMODE(type)  ctrace_log_mode(ctrace_l ## type)
#define CTRACE_LSTREAM(f)   ctrace_log_to(f)
#define CTRACE_LOG(...)     ctrace_log(__VA_ARGS__)
#define CTRACE_FATAL(...)   do { CTRACE_LMODE(trace);                   \
    CTRACE_LOG(CTRACE_RED "[fatal] " CTRACE_RESET __VA_ARGS__);         \
    exit(-1); } while(0)
#define CTRACE_ERR(...)     CTRACE_LOG(CTRACE_RED "[error] "   CTRACE_RESET   __VA_ARGS__)
#define CTRACE_WARN(...)    CTRACE_LOG(CTRACE_YELLOW "[warn] " CTRACE_RESET   __VA_ARGS__)
#define CTRACE_INFO(...)    CTRACE_LOG(CTRACE_GREEN "[info] "  CTRACE_RESET   __VA_ARGS__)

#define CTRACE_LOG_FOREACH(name)                                        \
    for(ctrace_log_iterator m__iter = ctrace_log_begin(),               \
        *name = &m__iter; !ctrace_log_end(m__iter);                     \
        ctrace_log_next(name))

/* Used for skipping the logging implementation frames on trace
 */
#define CTRACE_LOG_TRACE(name) ctrace_generate_ ## name(CTRACE_LOG_CALL_DEPTH)
#define CTRACE_LOG_CALL_DEPTH 3

#if CTRACE_EXCEPTIONS == ON
#define CTRACE_DECLARE_EXCEPTION(name)                                  \
    C_LINKAGE struct ctrace_registry_data* name() {                     \
        static struct ctrace_registry_data reg =                        \
            { 0, #name, CTRACE_NULL, {} };                              \
        return &reg;                                                    \
    }

#define CTRACE_EXCEPTION(name)                                          \
    CTRACE_DECLARE_EXCEPTION(name)                                      \
    C_LINKAGE ctrace_managed_string ctrace_user_ ## name ## _what()

#define CTRACE_REGISTER_EXCEPTION(name)                                 \
    ctrace_register_exception(name, ctrace_user_ ## name ## _what)

#define CTRACE_TRY if(!CTRACE_SETBUF_(buf))
#define CTRACE_CATCH(name) else if(name()->id == ctrace_exception_id())
#define CTRACE_CATCHALL(...) else

#define CTRACE_SETBUF_(name)                                            \
    (setjmp(*(jmp_buf*)(ctrace_internal_get_ ## name())))

#endif // CTRACE_EXCEPTIONS == ON


#endif
