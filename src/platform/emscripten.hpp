#ifndef EMSCRIPTEN_HPP
#define EMSCRIPTEN_HPP

#include "platform/platform.hpp"

#if IS_EMSCRIPTEN

#include <emscripten.h>
#include <memory>
#include "utils/microfmt.hpp"

namespace cpptrace {
namespace detail {
    CPPTRACE_FORCE_NO_INLINE
    stacktrace generate_emscripten_trace(std::size_t skip, std::size_t max_depth) {
        int estimated_size = emscripten_get_callstack(0, nullptr, 0);
        VERIFY(estimated_size >= 0);
        // "Note that this might be fully accurate since subsequent calls will carry different line numbers, so it is
        // best to allocate a few bytes extra to be safe."
        // Thanks, emscripten
        const auto max_size = estimated_size + 1000;
        // actually do the trace now
        std::unique_ptr<char[]> buffer(new char[max_size]);
        emscripten_get_callstack(0, buffer.get(), max_size);
        microfmt::print("{}\n", buffer.get());
        return {};
    }
}
}

#endif

#endif
