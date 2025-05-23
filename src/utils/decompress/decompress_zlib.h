#ifndef DECOMPRESS_ZLIB_HPP
#define DECOMPRESS_ZLIB_HPP

#include "utils/error.hpp"
#include "utils/io/base_file.hpp"
#include "utils/result.hpp"
#include "utils/span.hpp"

namespace cpptrace {
namespace detail {

Result<monostate, internal_error> decompress_zlib(
    bspan decompressed_data,
    base_file& compressed_file,
    off_t offset,
    size_t compressed_size
);

}
}

#endif
