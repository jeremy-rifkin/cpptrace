# libcpptrace

Libcpptrace is a lightweight C++ stacktrace library supporting C++11 and greater on Linux, Unix, MacOS, and Windows
(including cygwin / mingw environments). The goal: Make traces simple for once.

*Some day C++23's `<stacktrace>` will be ubiquitous*

## Table of contents

- [libcpptrace](#libcpptrace)
  - [Table of contents](#table-of-contents)
  - [Docs](#docs)
  - [Backends](#backends)
  - [License](#license)

## Docs

`cpptrace::print_trace()` can be used to print a stacktrace at the current call site, `cpptrace::generate_trace()` can
be used to get raw frame information for custom use.

**Note:** Debug info (`-g`) is generally required for good trace information. Some back-ends read symbols from dynamic
export information which requires `-rdynamic` or manually marking symbols for exporting.

```cpp
namespace cpptrace {
    struct stacktrace_frame {
        size_t line;
        size_t col;
        std::string filename;
        std::string symbol;
    };
    std::vector<stacktrace_frame> generate_trace();
    void print_trace();
}
```

## Back-ends

Back-end libraries are required for unwinding the stack and resolving symbol information (name and source location) in
order to generate a stacktrace.

The CMake script attempts to automatically choose a good back-end based on what is available on your system. You can
also manually set which back-end you want used.

**Unwinding**

| Library | CMake config | Windows | Linux | Info |
|---------|--------------|---------|-------|------|
| execinfo.h | `LIBCPPTRACE_UNWIND_WITH_EXECINFO` | | ✔️ | Frames are captured with `execinfo.h`'s `backtrace`, part of libc. |
| winapi | `LIBCPPTRACE_UNWIND_WITH_WINAPI` | ✔️ | | Frames are captured with `CaptureStackBackTrace`. |
| N/A | `LIBCPPTRACE_UNWIND_WITH_NOTHING` | ✔️ | ✔️ | Unwinding is not done, stack traces will be empty. |

**Symbol resolution**

| Library | CMake config | Windows | Linux | Info |
|---------|--------------|---------|-------|------|
| libbacktrace | `LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE` | ❌ | ✔️ | Libbacktrace is already installed on most systems, or available through the compiler directly. |
| libdl | `LIBCPPTRACE_GET_SYMBOLS_WITH_LIBDL` |  | ✔️ | Libdl uses dynamic export information. Compiling with `-rdynamic` is often needed. |
| addr2line | `LIBCPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE` | | ✔️ | Symbols are resolved by invoking `addr2line` via `fork()`. |
| dbghelp.h | `LIBCPPTRACE_GET_SYMBOLS_WITH_DBGHELP` | ✔️ | ❌ | Dbghelp.h allows access to symbols via debug info. |
| N/A | `LIBCPPTRACE_GET_SYMBOLS_WITH_NOTHING` | ✔️ | ✔️ | Don't attempt to resolve symbols. |

Lastly, C++ symbol demangling is done with `<cxxabi.h>`. Under dbghelp.h this is not needed, the symbols aren't mangled
when they are first extracted.

Libbacktrace can generate a full stack trace itself, both unwinding and resolving symbols, and this can be chosen with
`LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE`.

There are plenty more libraries that can be used for unwinding, parsing debug information, and demangling. In the future
more options may be added.

## License

The library is under the MIT license.
