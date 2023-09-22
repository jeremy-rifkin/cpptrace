# Ctrace <!-- omit in toc -->

Ctrace is the C API for Cpptrace. 
It allows you to use the library in a similar way to how you would in C++,
including using "exceptions". The interface uses the same underlying code as the C++ version,
so it can be compiled the same way.

This interface is experimental, and for the time being, is subject to change.

## Table of Contents <!-- omit in toc -->

- [30-Second Overview](#30-second-overview)
- [API](#api)
  - [Strings](#strings)
  - [Traces](#traces)
  - [Exceptions](#exceptions)
  - [Logging](#logging)
- [License](#license)

# 30-Second Overview

Generating traces is just as easy as in C++:

```c
#include <ctrace/ctrace.h>

void trace() {
    ctrace_trace trace = ctrace_generate_trace(0);
    ctrace_print_trace(trace, cprint_default);
    ctrace_free_trace(trace);
}

/* ... */
```

![Screenshot](res/ctrace-trace.png)

Ctrace provides functions to generate raw traces that
can be resolved later, just as Cpptrace does:

```c
const ctrace_trace raw_trace = ctrace_generate_raw_trace(0);
// then later
ctrace_trace trace = ctrace_move_resolve(raw_trace);
ctrace_print_trace(trace, cprint_default);
ctrace_free_trace(trace);
```

Ctrace also provides an (experimental) method of handling
*exceptional* circumstances:

```c
#define CTRACE_EXCEPTIONS ON
#include <ctrace/ctrace.h>

void trace() {
    ctrace_throw(exception);
}

/* ... */
// abort called after throwing an instance of `exception`
//    what():  ctrace::exception:
// Stack trace (most recent call first):
// #0 0x00007ff670c31466 in trace at demo.c:10
// #1 0x00007ff670c31477 in foo at demo.c:14
// #2 0x00007ff670c314a4 in main at demo.c:19
```

Do note that this doesn't use "real" exceptions, 
instead using ``longjmp``, which can easily invoke UB in C++.
They are also not thread safe (for now), and should only be used on
the main thread.
If you do not wish to use these features for that reason, 
simply omit the definition for ``CTRACE_EXCEPTIONS``.

# API

This section will only go over details specific to Ctrace.
For general information about how to use the library,
go to the [main documentation](README.md#in-depth-documentation).

Examples of the following can be found in [the demo](test/demo.c).

## Traces

*Coming soon...*

## Exceptions

``CTRACE_EXCEPTION(name)`` can be used to create an exception called ``name``.
You can then call ``CTRACE_REGISTER_EXCEPTION(name)`` in the main thread
to register the type. Once it has been registered, you can ``ctrace_throw(name)``
to handle exceptions. The standard type ``cpptrace_exception`` can be used without
manual registration.

``ctrace_exception_release()`` must be called at the end of a ``CATCH`` block.
It frees the managed string returned by the user's ``what()`` function.

```c
/* Must be defined to expose the exception interface */
#define CTRACE_EXCEPTIONS ON

/*
 * Create exception type with implicit `what()`.
 */

/* Creates exception type and `what()` */
#define CTRACE_EXCEPTION(name) ...
/* Registers exception made with `CTRACE_EXCEPTION` */
#define CTRACE_REGISTER_EXCEPTION(name) ...

/*
 * Create exception type with explicit `what()`.
 * This allows you to reuse a function (eg. "virtual").
 */

/* The type of the function used by `what()` */
typedef ctrace_string(*ctrace_message_t)();
/* Creates exception type without `what()` function */
#define CTRACE_DECLARE_EXCEPTION(name) ...
/* Tags exception type with an id. */
void ctrace_register_exception(... type, ctrace_message_t what);
/* Must be what your `what()` function returns */
ctrace_managed_string ctrace_exception_string(ctrace_string string);

/* 
 * Called to jump to exception context .
 */
_Noreturn void ctrace_throw(ctrace_exception_t type);

/*
 * Exception catching functions:
 */

/* Registers exception context */
#define CTRACE_TRY
/* Catches specific exception type */
#define CTRACE_CATCH(name)
/* Catches any exception, must come last */
#define CTRACE_CATCHALL()

/*
 * Exception interaction functions:
 */

/* The type returned by `ctrace_exception_ptr()` */
typedef ... ctrace_ex_t;
/* Get a pointer to the current exception */
ctrace_ex_t ctrace_exception_ptr();
/* Must be called at the end of a catch block, releases exception data */
void ctrace_exception_release();

/* 
 * Standard exception type. 
 * Does not need to be manually registered. 
 */
CTRACE_EXCEPTION(cpptrace_exception) { ... }
```

## Strings

*Coming soon...*

## Logging

Debugging interfaces like this can be tough, so logging functions have been added
to provide more information. The library internally calls these, but you can as well.

There are four standard inputs to ``CTRACE_LMODE``:
- **off**: A no-op, messages are not recorded or printed.
- **quiet**: Messages are recorded, but not printed.
- **debug**: Messages are immediately printed to the bound ``FILE*``.
- **trace**: Same as *debug*, but also prints a stacktrace (heavy).

``off`` is enabled by default, and prints to ``stdout``.

```c
/*
 * Functions for configuring logging.
 */

/* Used for calling functions beginning with `ctrace_l` */
#define CTRACE_LMODE(type)
/* Same as `ctrace_log_to`, only for consistency */
#define CTRACE_LSTREAM(stream)
/* Sets the logger function. Not thread safe. Only set on the main thread */
void ctrace_log_mode(ctrace_logger_t logger)
/* Sets the file to write to */
void ctrace_log_to(FILE* stream);

/*
 * Functions for invoking the logger function.
 */

void ctrace_log(ctrace_format_t format, ...);
/* For consistency with other macros */
#define CTRACE_LOG(format, args...)
/* Force prints trace log, then exits. Used for unrecoverable errors */
#define CTRACE_FATAL(format, args...)
/* Used for situations that may lead to undefined behaviour. */
#define CTRACE_ERR(format, args...)
/* Used for situations where behaviour is likely unintended, but not harmful */
#define CTRACE_WARN(format, args...)
/* Used for printing helpful information */
#define CTRACE_INFO(format, args...)

/* For interacting with `quiet` mode errors, not currently usable */
#define CTRACE_LOG_FOREACH(name)
```


# License

This library is under the MIT license.

Libdwarf is bundled as part of this library so the code in `bundled/libdwarf` is LGPL. If this library is statically
linked with libdwarf then the library's binary will itself be LGPL.