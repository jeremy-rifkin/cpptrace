# Changelog

- [Changelog](#changelog)
- [v0.2.0](#v020)
- [v0.1.1](#v011)
- [v0.1](#v01)

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
