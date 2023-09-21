# Cpptrace <!-- omit in toc -->

[![build](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/build.yml)
[![test](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/test.yml)
[![performance-test](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/performance-tests.yml/badge.svg?branch=main)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/performance-tests.yml)
[![cmake-integration](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/cmake-integration.yml/badge.svg?branch=main)](https://github.com/jeremy-rifkin/cpptrace/actions/workflows/cmake-integration.yml)
<br/>
[![Community Discord Link](https://img.shields.io/badge/Chat%20on%20(the%20very%20small)-Community%20Discord-blue?labelColor=2C3239&color=7289DA&style=flat&logo=discord&logoColor=959DA5)](https://discord.gg/7kv5AuCndG)

Cpptrace is a lightweight C++ stacktrace library supporting C++11 and greater on Linux, macOS, and Windows including
MinGW and Cygwin environments. The goal: Make stack traces simple for once.

## Table of Contents <!-- omit in toc -->

- [30-Second Overview](#30-second-overview)
  - [CMake FetchContent Usage](#cmake-fetchcontent-usage)
- [In-Depth Documentation](#in-depth-documentation)
  - [API](#api)
  - [Notable Library Configurations](#notable-library-configurations)
  - [Notes About the Library and Future Work](#notes-about-the-library-and-future-work)
    - [FAQ: What about C++23 `<stacktrace>`?](#faq-what-about-c23-stacktrace)
  - [Supported Debug Symbols](#supported-debug-symbols)
  - [Usage](#usage)
    - [CMake FetchContent](#cmake-fetchcontent)
    - [System-Wide Installation](#system-wide-installation)
    - [Local User Installation](#local-user-installation)
    - [Package Managers](#package-managers)
    - [Platform Logistics](#platform-logistics)
    - [Static Linking](#static-linking)
  - [Library Internals](#library-internals)
    - [Summary of Library Configurations](#summary-of-library-configurations)
  - [Testing Methodology](#testing-methodology)
- [License](#license)

# 30-Second Overview

Generating traces is as easy as calling `cpptrace::print_trace`:

```cpp
#include <cpptrace/cpptrace.hpp>

void trace() {
    cpptrace::generate_trace().print();
}

/* other stuff */
```

![Screenshot](res/screenshot.png)

Cpptrace provides access to resolved stack traces as well as lightweight raw traces (just addresses) that can be
resolved later:

```cpp
const auto raw_trace = cpptrace::generate_raw_trace();
// then later
raw_trace.resolve().print();
```

Cpptrace also provides exception types that store stack traces:
```cpp
#include <cpptrace/cpptrace.hpp>

void trace() {
    throw cpptrace::exception();
}

/* other stuff */
// terminate called after throwing an instance of 'cpptrace::exception'
//   what():  cpptrace::exception:
// Stack trace (most recent call first):
// #0 0x00005641c715a1b6 in trace() at demo.cpp:9
// #1 0x00005641c715a229 in foo(int) at demo.cpp:16
// #2 0x00005641c715a2ba in main at demo.cpp:34
```

## CMake FetchContent Usage

```cmake
include(FetchContent)
FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.2.0-beta # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)
target_link_libraries(your_target cpptrace)

# On windows copy cpptrace.dll to the same directory as the executable for your_target
if(WIN32)
  add_custom_command(
    TARGET your_target POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cpptrace> $<TARGET_FILE_DIR:your_target>
  )
endif()
```

On windows and macos some extra work is required, see [below](#platform-logistics).

# In-Depth Documentation

## API

`cpptrace::generate_trace()` can used to generate a stacktrace object at the current call site. Resolved frames can be
accessed from this object with `.frames` and also the trace can be printed with `.print()`. Cpptrace also provides a
method to get lightweight raw traces, which are just vectors of program counters, which can be resolved at a later time.

**Note:** Debug info (`-g`/`/Z7`/`/Zi`/`/DEBUG`) is generally required for good trace information.

**Note:** Currently on Mac .dSYM files are required, which can be generated with `dsymutil yourbinary`. A cmake snippet
for generating these is included above.

```cpp
namespace cpptrace {
    /*
     * Raw trace access
     */
    struct raw_trace {
        std::vector<uintptr_t> frames;
        explicit raw_trace(std::vector<uintptr_t>&& frames);
        object_trace resolve_object_trace() const;
        stacktrace resolve() const;
        void clear();
        bool empty() const noexcept;
        /* iterators exist for this object */
    };

    /*
     * Object trace with object file information for each frame, any accessible symbol information,
     * and the address in the object file for the frame's program counter.
     */
    struct object_frame {
        std::string obj_path;
        std::string symbol;
        uintptr_t raw_address = 0;
        uintptr_t obj_address = 0;
    };

    struct object_trace {
        std::vector<object_frame> frames;
        explicit object_trace(std::vector<object_frame>&& frames);
        stacktrace resolve() const;
        void clear();
        bool empty() const noexcept;
        /* iterators exist for this object */
    };

    /*
     * Resolved stacktrace object.
     */
    struct stacktrace_frame {
        uintptr_t address;
        std::uint_least32_t line;
        std::uint_least32_t column; // UINT_LEAST32_MAX if not present
        std::string filename;
        std::string symbol;
        bool operator==(const stacktrace_frame& other) const;
        bool operator!=(const stacktrace_frame& other) const;
        std::string to_string() const;
        /* operator<<(ostream, ..) and std::format support exist for this object */
    };

    struct stacktrace {
        std::vector<stacktrace_frame> frames;
        explicit stacktrace();
        explicit stacktrace(std::vector<stacktrace_frame>&& frames);
        void print() const;
        void print(std::ostream& stream) const;
        void print(std::ostream& stream, bool color) const;
        std::string to_string() const;
        void clear();
        bool empty() const noexcept;
        /* operator<<(ostream, ..), std::format support, and iterators exist for this object */
    };

    /*
     * Trace generation
     */
    raw_trace generate_raw_trace(std::uint32_t skip = 0);
    object_trace generate_object_trace(std::uint32_t skip = 0);
    stacktrace generate_trace(std::uint32_t skip = 0);

    /*
     * Utilities
     */
    std::string demangle(const std::string& name);

    // Traced exception class
    // Extending classes should call the exception constructor with a skip value of 1.
    class exception : public std::exception {
    protected:
        mutable raw_trace trace;
        mutable stacktrace resolved_trace;
        mutable std::string resolved_what;
        explicit exception(uint32_t skip) noexcept;
        const stacktrace& get_resolved_trace() const noexcept;
        virtual const std::string& get_resolved_what() const noexcept;
    public:
        explicit exception() noexcept;
        const char* what() const noexcept override;
        const std::string& get_what() const noexcept; // what(), but not a C-string
        const raw_trace& get_raw_trace() const noexcept;
        const stacktrace& get_trace() const noexcept;
    };

    class exception_with_message : public exception {
        mutable std::string message;
        explicit exception_with_message(std::string&& message_arg, uint32_t skip) noexcept;
        const std::string& get_resolved_what() const noexcept override;
    public:
        explicit exception_with_message(std::string&& message_arg);
        const char* what() const noexcept override;
        const std::string& get_message() const noexcept;
    };

    // All stdexcept errors have analogs here. Same constructor as exception_with_message.
    class logic_error      : exception_with_message { ... };
    class domain_error     : exception_with_message { ... };
    class invalid_argument : exception_with_message { ... };
    class length_error     : exception_with_message { ... };
    class out_of_range     : exception_with_message { ... };
    class runtime_error    : exception_with_message { ... };
    class range_error      : exception_with_message { ... };
    class overflow_error   : exception_with_message { ... };
    class underflow_error  : exception_with_message { ... };
}
```

## Notable Library Configurations

- `CPPTRACE_STATIC=On/Off`: Create cpptrace as a static library.
- `CPPTRACE_HARD_MAX_FRAMES=<number>`: Some back-ends write to a fixed-size buffer. This is the size of that buffer.
  Default is `100`.

## Notes About the Library and Future Work

For the most part I'm happy with the state of the library. But I'm sure that there is room for improvement and issues
will exist. If you encounter any issue, please let me know! If you find any pain-points in the library, please let me
know that too.

A note about performance: For handling of DWARF symbols there is a lot of room to explore for performance optimizations
and time-memory tradeoffs. If you find the current implementation is either slow or using too much memory, I'd be happy
to explore some of these options.

A couple things I'd like to fix in the future:
- On MacOS .dSYM files are required
- On Windows when collecting symbols with dbghelp (msvc/clang) parameter types are almost perfect but due to limitations
  in dbghelp the library cannot accurately show const and volatile qualifiers or rvalue references (these appear as
  pointers).
- On Windows unwinding with `CaptureStackBackTrace` (msvc/clang) can sometimes produce program counters that are after
  the call instruction. Execinfo suffers from the same problem, but libgcc's `_Unwind` provides a means to detect this.
  I would like to find a solution on windows so stack traces are more accurate.
- Support for universal binaries on macos (fat mach-o files) is hacky and inefficient at the moment. Libdwarf should be
  adding proper support for these soon thanks to an awesome maintainer.

### FAQ: What about C++23 `<stacktrace>`?

Some day C++23's `<stacktrace>` will be ubiquitous. And maybe one day the msvc implementation will be acceptable.
The main motivation for this library was  The original motivation for cpptrace was to support projects using older C++
standards and as the library has grown its functionality has extended beyond the standard library's implementation.

Plenty of additional functionality is planned too, such as stack tracing from signal handlers, stack tracing other
threads, and generating lightweight stack traces on embedded devices that can be resolved either on embedded or on
another system (this is theoretically possible currently but untested).

## Supported Debug Symbols

| Format                                           | Supported |
| ------------------------------------------------ | --------- |
| DWARF in binary                                  | ✔️      |
| DWARF in separate binary (binary gnu debug link) | Not yet   |
| DWARF in separate binary (split dwarf)           | Not yet   |
| DWARF in dSYM                                    | ✔️      |
| DWARF in via Mach-O debug map                    | Not yet   |
| Windows debug symbols in PE                      | Untested  |
| Windows debug symbols in PDB                     | ✔️      |

## Usage

### CMake FetchContent

With CMake FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.2.0-beta # <HASH or TAG>
)
FetchContent_MakeAvailable(cpptrace)
target_link_libraries(your_target cpptrace)
```

It's as easy as that. Cpptrace will automatically configure itself for your system. Note: On windows and macos some
extra work is required, see [below](#platform-logistics).

Be sure to configure with `-DCMAKE_BUILD_TYPE=Debug` or `-DDCMAKE_BUILD_TYPE=RelWithDebInfo` for symbols and line
information.

### System-Wide Installation

```sh
git clone https://github.com/jeremy-rifkin/cpptrace.git
git checkout v0.2.0-beta
mkdir cpptrace/build
cd cpptrace/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
sudo make install
```

Using through cmake:
```cmake
find_package(cpptrace REQUIRED)
target_link_libraries(<your target> cpptrace::cpptrace)
```
Be sure to configure with `-DCMAKE_BUILD_TYPE=Debug` or `-DDCMAKE_BUILD_TYPE=RelWithDebInfo` for symbols and line
information.

Or compile with `-lcpptrace`:

```sh
g++ main.cpp -o main -g -Wall -lcpptrace
./main
```

If you get an error along the lines of
```
error while loading shared libraries: libcpptrace.so: cannot open shared object file: No such file or directory
```
You may have to run `sudo /sbin/ldconfig` to create any necessary links and update caches so the system can find
libcpptrace.so (I had to do this on Ubuntu). Only when installing system-wide. Usually your package manger does this for
you when installing new libraries.

<details>
    <summary>System-wide install on windows</summary>

```ps1
git clone https://github.com/jeremy-rifkin/cpptrace.git
git checkout v0.2.0-beta
mkdir cpptrace/build
cd cpptrace/build
cmake .. -DCMAKE_BUILD_TYPE=Release
msbuild cpptrace.sln
msbuild INSTALL.vcxproj
```

Note: You'll need to run as an administrator in a developer powershell, or use vcvarsall.bat distributed with visual
studio to get the correct environment variables set.
</details>

### Local User Installation

To install just for the local user (or any custom prefix):

```sh
git clone https://github.com/jeremy-rifkin/cpptrace.git
git checkout v0.2.0-beta
mkdir cpptrace/build
cd cpptrace/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/wherever
make -j
sudo make install
```

Using through cmake:
```cmake
find_package(cpptrace REQUIRED PATHS $ENV{HOME}/wherever)
target_link_libraries(<your target> cpptrace::cpptrace)
```

Using manually:
```
g++ main.cpp -o main -g -Wall -I$HOME/wherever/include -L$HOME/wherever/lib -lcpptrace
```

### Package Managers

Coming soon

### Platform Logistics

Windows and macos require a little extra work to get everything in the right place

```cmake
# Copy the cpptrace.dll on windows to the same directory as the executable for your_target.
# Not required if static linking.
if(WIN32)
  add_custom_command(
    TARGET your_target POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:cpptrace>
    $<TARGET_FILE_DIR:your_target>
  )
endif()

# Create a .dSYM file on macos. Currently required, but hopefully not for long
if(APPLE)
  add_custom_command(
    TARGET your_target
    POST_BUILD
    COMMAND dsymutil $<TARGET_FILE:your_target>
  )
endif()
```

### Static Linking

To static link the library set `-DCPPTRACE_STATIC=On`.

## Library Internals

Cpptrace supports a number of back-ends and middle-ends to produce stack traces. Stack traces are produced in roughly
three steps: Unwinding, symbol resolution, and demangling. Cpptrace by default on linux / macos will generate traces
with `_Unwind_Backtrace`, libdwarf, and `__cxa_demangle`. On windows traces are generated by default with
`CaptureStackBackTrace` and dbghelp.h (no demangling is needed with dbghelp). Under mingw libdwarf and dbghelp.h are
used, along with `__cxa_demangle`. Support for these is the main focus of cpptrace and they should work well. If you
want to use a different back-end such as addr2line, however, you can configure the library to do so.

**Unwinding**

| Library       | CMake config                    | Platforms           | Info                                                                                                                                                                                                     |
| ------------- | ------------------------------- | ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| libgcc unwind | `CPPTRACE_UNWIND_WITH_UNWIND`   | linux, macos, mingw | Frames are captured with libgcc's `_Unwind_Backtrace`, which currently produces the most accurate stack traces on gcc/clang/mingw. Libgcc is often linked by default, and llvm has something equivalent. |
| execinfo.h    | `CPPTRACE_UNWIND_WITH_EXECINFO` | linux, macos        | Frames are captured with `execinfo.h`'s `backtrace`, part of libc on linux/unix systems.                                                                                                                 |
| winapi        | `CPPTRACE_UNWIND_WITH_WINAPI`   | windows, mingw      | Frames are captured with `CaptureStackBackTrace`.                                                                                                                                                        |
| N/A           | `CPPTRACE_UNWIND_WITH_NOTHING`  | all                 | Unwinding is not done, stack traces will be empty.                                                                                                                                                       |

These back-ends require a fixed buffer has to be created to read addresses into while unwinding. By default the buffer
can hold addresses for 100 frames (beyond the `skip` frames). This is configurable with `CPPTRACE_HARD_MAX_FRAMES`.

**Symbol resolution**

| Library      | CMake config                             | Platforms             | Info                                                                                                                                                                                         |
| ------------ | ---------------------------------------- | --------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| libdwarf     | `CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF`     | linux, macos, mingw   | Libdwarf is the preferred method for symbol resolution for cpptrace, and it's bundled in this repository for ease of use.                                                                    |
| dbghelp      | `CPPTRACE_GET_SYMBOLS_WITH_DBGHELP`      | windows               | Dbghelp.h is the preferred method for symbol resolution on windows under msvc/clang and is supported on all windows machines.                                                                |
| libbacktrace | `CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE` | linux, macos*, mingw* | Libbacktrace is already installed on most systems or available through the compiler directly. For clang you must specify the absolute path to `backtrace.h` using `CPPTRACE_BACKTRACE_PATH`. |
| addr2line    | `CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE`    | linux, macos, mingw   | Symbols are resolved by invoking `addr2line` (or `atos` on mac) via `fork()` (on linux/unix, and `popen` under mingw).                                                                       |
| libdl        | `CPPTRACE_GET_SYMBOLS_WITH_LIBDL`        | linux, macos          | Libdl uses dynamic export information. Compiling with `-rdynamic` is needed for symbol information to be retrievable. Line numbers won't be retrievable.                                     |
| N/A          | `CPPTRACE_GET_SYMBOLS_WITH_NOTHING`      | all                   | No attempt is made to resolve symbols.                                                                                                                                                       |

*: Requires installation

Note for addr2line: By default cmake will resolve an absolute path to addr2line to bake into the library. This path can
be configured with `CPPTRACE_ADDR2LINE_PATH`, or `CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH` can be used to have the library
search the system path for `addr2line` at runtime. This is not the default to prevent against path injection attacks.

**Demangling**

Lastly, depending on other back-ends used a demangler back-end may be needed.

| Library   | CMake config                     | Platforms           | Info                                                                               |
| --------- | -------------------------------- | ------------------- | ---------------------------------------------------------------------------------- |
| cxxabi.h  | `CPPTRACE_DEMANGLE_WITH_CXXABI`  | Linux, macos, mingw | Should be available everywhere other than [msvc](https://godbolt.org/z/93ca9rcdz). |
| dbghelp.h | `CPPTRACE_DEMANGLE_WITH_WINAPI`  | Windows             | Demangle with `UnDecorateSymbolName`. |
| N/A       | `CPPTRACE_DEMANGLE_WITH_NOTHING` | all                 | Don't attempt to do anything beyond what the symbol resolution back-end does.      |

**More?**

There are plenty more libraries that can be used for unwinding, parsing debug information, and demangling. In the future
more back-ends can be added. Ideally this library can "just work" on systems, without additional installation work.

### Summary of Library Configurations

Summary of all library configuration options:

Back-ends:
- `CPPTRACE_GET_SYMBOLS_WITH_LIBDWARF=On/Off`
- `CPPTRACE_GET_SYMBOLS_WITH_DBGHELP=On/Off`
- `CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE=On/Off`
- `CPPTRACE_GET_SYMBOLS_WITH_ADDR2LINE=On/Off`
- `CPPTRACE_GET_SYMBOLS_WITH_LIBDL=On/Off`
- `CPPTRACE_GET_SYMBOLS_WITH_NOTHING=On/Off`
- `CPPTRACE_UNWIND_WITH_UNWIND=On/Off`
- `CPPTRACE_UNWIND_WITH_EXECINFO=On/Off`
- `CPPTRACE_UNWIND_WITH_WINAPI=On/Off`
- `CPPTRACE_UNWIND_WITH_NOTHING=On/Off`
- `CPPTRACE_DEMANGLE_WITH_CXXABI=On/Off`
- `CPPTRACE_DEMANGLE_WITH_WINAPI=On/Off`
- `CPPTRACE_DEMANGLE_WITH_NOTHING=On/Off`

Back-end configuration:
- `CPPTRACE_STATIC=On/Off`: Create cpptrace as a static library.
- `CPPTRACE_BACKTRACE_PATH=<string>`: Path to libbacktrace backtrace.h, needed when compiling with clang/
- `CPPTRACE_HARD_MAX_FRAMES=<number>`: Some back-ends write to a fixed-size buffer. This is the size of that buffer.
  Default is `100`.
- `CPPTRACE_ADDR2LINE_PATH=<string>`: Specify the absolute path to the addr2line binary for cpptrace to invoke. By
  default the config script will search for a binary and use that absolute path (this is to prevent against path
  injection).
- `CPPTRACE_ADDR2LINE_SEARCH_SYSTEM_PATH=On/Off`: Specifies whether cpptrace should let the system search the PATH
  environment variable directories for the binary.

Testing:
- `CPPTRACE_BUILD_TEST` Build a small test program
- `CPPTRACE_BUILD_DEMO` Build a small demo program
- `CPPTRACE_BUILD_TEST_RDYNAMIC` Use `-rdynamic` when compiling the test program

## Testing Methodology

Cpptrace currently uses integration and functional testing, building and running under every combination of back-end
options. The implementation is based on [github actions matrices][1] and driven by python scripts located in the
[`ci/`](ci/) folder. Testing used to be done by github actions matrices directly, however, launching hundreds of two
second jobs was extremely inefficient. Test outputs are compared against expected outputs located in
[`test/expected/`](test/expected/). Stack trace addresses may point to the address after an instruction depending on the
unwinding back-end, and the python script will check for an exact or near-match accordingly.

[1]: https://docs.github.com/en/actions/using-jobs/using-a-matrix-for-your-jobs

# License

This library is under the MIT license.

Libdwarf is bundled as part of this library so the code in `bundled/libdwarf` is LGPL. If this library is statically
linked with libdwarf then the library's binary will itself be LGPL.
