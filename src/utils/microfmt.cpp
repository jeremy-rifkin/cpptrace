#include "utils/microfmt.hpp"

#include <iostream>

namespace cpptrace {
namespace detail {

    std::ostream& get_cout() {
        return std::cout;
    }

}
}
