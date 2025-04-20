#ifndef SAFE_DL_HPP
#define SAFE_DL_HPP

#include "utils/common.hpp"

namespace cpptrace {
namespace internal {
    void get_safe_object_frame(frame_ptr address, safe_object_frame* out);

    bool has_get_safe_object_frame();
}
}

#endif
