# Cpptrace

[![build](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml)

🚧 WIP 🏗️

Libcpptrace is a lightweight C++ stacktrace library supporting C++11 and greater on Linux, Unix, and Windows. The goal:
Make stack traces simple for once.

Support for MacOS and cygwin/mingw will be added soon.

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
export information which may require `-rdynamic` or manually marking symbols for exporting.

```cpp
namespace cpptrace {
    struct stacktrace_frame {
        uintptr_t address;
        int line;
        int col;
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
| execinfo.h | `LIBCPPTRACE_UNWIND_WITH_EXECINFO` | ❌ | ✔️ | Frames are captured with `execinfo.h`'s `backtrace`, part of libc. |
| winapi | `LIBCPPTRACE_UNWIND_WITH_WINAPI` | ✔️ | ❌ | Frames are captured with `CaptureStackBackTrace`. |
| N/A | `LIBCPPTRACE_UNWIND_WITH_NOTHING` | ✔️ | ✔️ | Unwinding is not done, stack traces will be empty. |

Some back-ends require a fixed buffer has to be created to read addresses into while unwinding. By default the buffer
can hold addresses for 100 frames. This is configurable with `LIBCPPTRACE_HARD_MAX_FRAMES`.

**Symbol resolution**

| Library | CMake config | Windows | Linux | Info |
|---------|--------------|---------|-------|------|
| libbacktrace | `LIBCPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE` | ❌ | ✔️ | Libbacktrace is already installed on most systems, or available through the compiler directly. If it is installed but backtrace.h is not already in the include path (this can happen when using clang when backtrace lives in gcc's include folder), `LIBCPP_BACKTRACE_PATH` can be used to specify where the library should be looked for. |
| libdl | `LIBCPPTRACE_GET_SYMBOLS_WITH_LIBDL` | ❌ | ✔️ | Libdl uses dynamic export information. Compiling with `-rdynamic` is often needed. |
| addr2line | `LIBCPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE` | ❌ | ✔️ | Symbols are resolved by invoking `addr2line` via `fork()`. |
| dbghelp.h | `LIBCPPTRACE_GET_SYMBOLS_WITH_DBGHELP` | ✔️ | ❌ | Dbghelp.h allows access to symbols via debug info. |
| N/A | `LIBCPPTRACE_GET_SYMBOLS_WITH_NOTHING` | ✔️ | ✔️ | No attempt is made to resolve symbols. |

**Demangling**

Lastly, on unix systems symbol demangling is done with `<cxxabi.h>`. On windows symbols extracted with dbghelp.h aren't
mangled.

| Library | CMake config | Windows | Linux | Info |
|---------|--------------|---------|-------|------|
| cxxabi.h  | `LIBCPPTRACE_DEMANGLE_WITH_CXXABI` | | | Should be available everywhere other than [msvc](https://godbolt.org/z/93ca9rcdz). |
| N/A  | `LIBCPPTRACE_DEMANGLE_WITH_NOTHING` | | | Don't attempt to do anything beyond what the symbol resolution back-end does. |

**Full tracing**

Libbacktrace can generate a full stack trace itself, both unwinding and resolving symbols. This can be chosen with
`LIBCPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE`. This is also the first configuration the auto config attempts to use. Full
tracing with libbacktrace ignores `LIBCPPTRACE_HARD_MAX_FRAMES`.

There are plenty more libraries that can be used for unwinding, parsing debug information, and demangling. In the future
more back-ends can be added. Ideally this library can "just work" on systems, without additional installation work.

## License

The library is under the MIT license.
