#ifndef CPPTRACE_COMMON_HPP
#define CPPTRACE_COMMON_HPP

#ifdef _MSC_VER
#define CPPTRACE_FORCE_NO_INLINE __declspec(noinline)
#define CPPTRACE_PFUNC __FUNCSIG__
#define CPPTRACE_MAYBE_UNUSED
#pragma warning(push)
#pragma warning(disable: 4505) // Unused local function
#else
#define CPPTRACE_FORCE_NO_INLINE __attribute__((noinline))
#define CPPTRACE_PFUNC __extension__ __PRETTY_FUNCTION__
#define CPPTRACE_MAYBE_UNUSED __attribute__((unused))
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <sstream>

// Lightweight std::source_location.
struct source_location {
    const char* const file;
    //const char* const function; // disabled for now due to static constexpr restrictions
    const int line;
    constexpr source_location(
        //const char* _function /*= __builtin_FUNCTION()*/,
        const char* _file     = __builtin_FILE(),
        int _line             = __builtin_LINE()
    ) : file(_file), /*function(_function),*/ line(_line) {}
};

CPPTRACE_MAYBE_UNUSED
static void primitive_assert_impl(
    bool condition,
    bool verify,
    const char* expression,
    const char* signature,
    source_location location,
    const char* message = nullptr
) {
    if(!condition) {
        const char* action = verify ? "verification" : "assertion";
        const char* name   = verify ? "verify"       : "assert";
        if(message == nullptr) {
            fprintf(stderr, "Cpptrace %s failed at %s:%d: %s\n",
                action, location.file, location.line, signature);
        } else {
            fprintf(stderr, "Cpptrace %s failed at %s:%d: %s: %s\n",
                action, location.file, location.line, signature, message);
        }
        fprintf(stderr, "    primitive_%s(%s);\n", name, expression);
        abort();
    }
}

template<typename T>
void nothing() {}
#define PHONY_USE(E) (nothing<decltype(E)>())

// Still present in release mode, nonfatal
#define internal_verify(c, ...) primitive_assert_impl(c, true, #c, CPPTRACE_PFUNC, {}, ##__VA_ARGS__)

#ifndef NDEBUG
    #define CPPTRACE_PRIMITIVE_ASSERT(c, ...) \
    primitive_assert_impl(c, false, #c, CPPTRACE_PFUNC, {}, ##__VA_ARGS__)
#else
    #define CPPTRACE_PRIMITIVE_ASSERT(c, ...) PHONY_USE(c)
#endif

CPPTRACE_MAYBE_UNUSED
static std::vector<std::string> split(const std::string& s, const std::string& delims) {
    std::vector<std::string> vec;
    size_t old_pos = 0;
    size_t pos = 0;
    while((pos = s.find_first_of(delims, old_pos)) != std::string::npos) {
        vec.emplace_back(s.substr(old_pos, pos - old_pos));
        old_pos = pos + 1;
    }
    vec.emplace_back(std::string(s.substr(old_pos)));
    return vec;
}

template<typename C>
CPPTRACE_MAYBE_UNUSED
static std::string join(const C& container, const std::string& delim) {
    auto iter = std::begin(container);
    auto end = std::end(container);
    std::string str;
    if(std::distance(iter, end) > 0) {
        str += *iter;
        while(++iter != end) {
            str += delim;
            str += *iter;
        }
    }
    return str;
}

constexpr const char* const ws = " \t\n\r\f\v";

CPPTRACE_MAYBE_UNUSED
static std::string trim(const std::string& s) {
    if(s == "") {
        return "";
    }
    size_t l = s.find_first_not_of(ws);
    size_t r = s.find_last_not_of(ws) + 1;
    return s.substr(l, r - l);
}

CPPTRACE_MAYBE_UNUSED
static std::string to_hex(uintptr_t addr) {
    std::stringstream sstream;
    sstream<<std::hex<<uintptr_t(addr);
    return std::move(sstream).str();
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
