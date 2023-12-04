# Changelog

- [Changelog](#changelog)
- [v0.3.1](#v031)
- [v0.3.0](#v030)
- [v0.2.1](#v021)
- [v0.2.0](#v020)
- [v0.1.1](#v011)
- [v0.1](#v01)

# v0.3.1

Tiny patch:
- Fix `CPPTRACE_EXPORT` annotations
- Add workaround for [msvc bug][msvc bug] affecting msvc 19.38.

[msvc bug]: https://developercommunity.visualstudio.com/t/MSVC-1938331290-preview-fails-to-comp/10505565

# v0.3.0

Interface Changes:
- Overhauled the API for traced `cpptrace::exception` objects
- Added `cpptrace::isatty` utility
- Added specialized `std::terminate` handler and `cpptrace::register_terminate_handler` utility
- Added `cpptrace::frame_ptr` as an alias for the appropriate type capable of representing an instruction pointer
- Added signal-safe tracing support and a guide for [how to trace safely](signal-safe-tracing.md)
- Added `cpptrace::nullable<T>` utility for better indicating when line / column information is not present
- Added `CPPTRACE_FORCE_NO_INLINE` utility macro to cpptrace.hpp
- Added `CPPTRACE_WRAP` and `CPPTRACE_WRAP_BLOCK` utilities to catch non-`cpptrace::exception`s and rethrow wrapped in a
  traced exception.
- Updated `cpptrace::stacktrace::to_string` to take a `bool color` parameter
- Eliminated uses of `std::uint_least32_t` in favor of other types
- Updated `object_frame` data member names

Other changes:
- Added object resolution with `_dl_find_object` which is much faster than `dladdr`
- Added column support for dwarf
- Added inlined call resolution
  - Added `cpptrace::stacktrace_frame::is_inline`
- Added libunwind as a back-end
- Unbundled libdwarf
- Increased hard max frame count, used by some back-end requiring fixed buffers, from 100 to 200
- Improved libgcc unwind backend
- Improved trace output when information is missing
- Added a lookup table for faster dwarf line information lookup

News:
- The library is now on conan and vcpkg

Minor changes:
- Assorted bug fixes
- Various code quality improvements
- CI improvements
- Documentation improvements
- CMake improvements
- Internal refactoring

# v0.2.1

Patches:
- Fixed uintptr_t implicit conversion issue for msvc
- Better handling for PIC and static linkage in CMake
- Added gcc 5 support
- Various warning fixes
- Added stackwalk64 support for 32-bit x86 mingw/clang and architecture detection
- Added check for stackwalk64 support and CaptureStackBacktrace as a fallback
- Various cmake cleanup and changes to use cpptrace through package managers
- Added sonarlint and implemented some sonarlint fixes

# v0.2.0

Key changes:
- Added libdwarf as a back-end so cpptrace doesn't have to rely on addr2line or libbacktrace
- Overhauled library's public-facing interface to make the library more useful
  - Added `raw_trace` interface
  - Added `object_trace` interface
  - Added `stacktrace` interface
  - Updated `generate_trace` to return a `stacktrace` rather than a vector of frames
  - Added `generate_trace` counterparts for raw and object traces
  - Added `generate_trace` overloads with max_depth
  - Added interface for internal demangling utility
  - Added cache mode configuration
  - Added option to absorb internal trace exceptions (by default it absorbs)
  - Added `cpptrace::exception`, which automatically generates and stores a stacktrace when thrown
  - Added `exception_with_message`
  - Added traced analogs for stdexcept errors: `logic_error`, `domain_error`, `invalid_argument`, `length_error`,
    `out_of_range`, `runtime_error`, `range_error`, `overflow_error`, and `underflow_error`.

Other changes:
- Bundled libdwarf with cpptrace so the library can essentially be self-contained and not have to rely on libraries that
  might not already be on a system
- Added StackWalk64 as an unwinding back-end on windows
- Added system for multiple symbol back-ends to be used, mainly for more complete stack traces on mingw
- Fixed sporadic line number reporting errors due to not adjusting the program counter from the unwinder
- Improved addr2line/atos invocation back-end on macos
- Lots of error handling improvements
- Performance improvements
- Updated default back-ends for most systems
- Removed full tracing backends
- Cleaned up library cmake
- Lots of internal cleanup and refactoring
- Improved library usage instructions in README

# v0.1.1

Fixed:
- Handle errors when object files don't exist or can't be opened for reading
- Handle paths with spaces when using addr2line on windows

# v0.1

Initial release of the library 🎉
