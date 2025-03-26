#ifndef JIT_OBJECTS_HPP
#define JIT_OBJECTS_HPP

#include "binary/elf.hpp"
#include "cpptrace/forward.hpp"
#include "utils/optional.hpp"

namespace cpptrace {
namespace detail {
    void load_jit_objects();
    struct elf_lookup_result {
        elf& object;
        frame_ptr base;
    };
    optional<elf_lookup_result> lookup_jit_object(frame_ptr pc);
}
}

#endif
