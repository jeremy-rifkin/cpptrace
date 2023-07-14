# Cpptrace

[![build](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml)
[![test](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/test.yml/badge.svg?branch=master)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/test.yml)
[![performance-test](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/performance-tests.yml/badge.svg)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/performance-tests.yml)

🚧 WIP 🏗️

Cpptrace is a lightweight C++ stacktrace library supporting C++11 and greater on Linux, Unix, macOS and Windows. The goal:
Make stack traces simple for once.

Support for cygwin/mingw will be added soon.

*Some day C++23's `<stacktrace>` will be ubiquitous, and maybe one day the msvc implementation will be acceptable*

## Table of contents

- [cpptrace](#cpptrace)
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
        std::uint_least32_t line;
        std::uint_least32_t col;
        std::string filename;
        std::string symbol;
    };
    std::vector<stacktrace_frame> generate_trace(std::uint32_t skip = 0);
    void print_trace(std::uint32_t skip = 0);
}
```

## Back-ends

Back-end libraries are required for unwinding the stack and resolving symbol information (name and source location) in
order to generate a stacktrace.

The CMake script attempts to automatically choose a good back-end based on what is available on your system. You can
also manually set which back-end you want used.

**Unwinding**

| Library    | CMake config                    | Windows | Linux | macOS | Info                                                               |
|------------|---------------------------------|---------|-------|-------|--------------------------------------------------------------------|
| execinfo.h | `CPPTRACE_UNWIND_WITH_EXECINFO` | ❌       | ✔️    | ✔️    | Frames are captured with `execinfo.h`'s `backtrace`, part of libc. |
| winapi     | `CPPTRACE_UNWIND_WITH_WINAPI`   | ✔️      | ❌     | ❌     | Frames are captured with `CaptureStackBackTrace`.                  |
| N/A        | `CPPTRACE_UNWIND_WITH_NOTHING`  | ✔️      | ✔️    | ✔️    | Unwinding is not done, stack traces will be empty.                 |

These back-ends require a fixed buffer has to be created to read addresses into while unwinding. By default the buffer
can hold addresses for 100 frames (beyond the `skip` frames). This is configurable with `CPPTRACE_HARD_MAX_FRAMES`.

**Symbol resolution**

| Library      | CMake config                             | Windows | Linux | macOS | Info                                                                                                                                                                                                                                                                                                                                           |
|--------------|------------------------------------------|---------|-------|-------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| libbacktrace | `CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE` | ❌       | ✔️    | ❌     | Libbacktrace is already installed on most systems, or available through the compiler directly. If it is installed but backtrace.h is not already in the include path (this can happen when using clang when backtrace lives in gcc's include folder), `CPPTRACE_BACKTRACE_PATH` can be used to specify where the library should be looked for. |
| libdl        | `CPPTRACE_GET_SYMBOLS_WITH_LIBDL`        | ❌       | ✔️    | ✔️    | Libdl uses dynamic export information. Compiling with `-rdynamic` is needed for symbol information to be retrievable.                                                                                                                                                                                                                           |
| addr2line    | `CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE`    | ❌       | ✔️    | ❌     | Symbols are resolved by invoking `addr2line` via `fork()`.                                                                                                                                                                                                                                                                                     |
| dbghelp      | `CPPTRACE_GET_SYMBOLS_WITH_DBGHELP`      | ✔️      | ❌     | ❌     | Dbghelp.h allows access to symbols via debug info.                                                                                                                                                                                                                                                                                             |
| N/A          | `CPPTRACE_GET_SYMBOLS_WITH_NOTHING`      | ✔️      | ✔️    | ✔️    | No attempt is made to resolve symbols.                                                                                                                                                                                                                                                                                                         |

**Demangling**

Lastly, depending on other back-ends used a demangler back-end may be needed. A demangler back-end is not needed when
doing full traces with libbacktrace, getting symbols with addr2line, or getting symbols with dbghelp.

| Library | CMake config | Windows | Linux | Info |
|---------|--------------|---------|-------|------|
| cxxabi.h  | `CPPTRACE_DEMANGLE_WITH_CXXABI` | | | Should be available everywhere other than [msvc](https://godbolt.org/z/93ca9rcdz). |
| N/A  | `CPPTRACE_DEMANGLE_WITH_NOTHING` | | | Don't attempt to do anything beyond what the symbol resolution back-end does. |

**Full tracing**

Libbacktrace can generate a full stack trace itself, both unwinding and resolving symbols. This can be chosen with
`CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE`. The auto config attempts to use this if it is available. Full tracing with
libbacktrace ignores `CPPTRACE_HARD_MAX_FRAMES`.

`<stacktrace>` can of course also generate a full trace, if you're using >=C++23 and your compiler supports it. This is
controlled by `CPPTRACE_FULL_TRACE_WITH_LIBBACKTRACE`. The cmake script will attempt to auto configure to this if
possible. `CPPTRACE_HARD_MAX_FRAMES` is ignored.

**More?**

There are plenty more libraries that can be used for unwinding, parsing debug information, and demangling. In the future
more back-ends can be added. Ideally this library can "just work" on systems, without additional installation work.

## License

The library is under the MIT license.
