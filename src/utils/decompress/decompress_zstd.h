#ifndef DECOMPRESS_ZSTD_HPP
#define DECOMPRESS_ZSTD_HPP

#include "utils/error.hpp"
#include "utils/result.hpp"
#include "utils/io/base_file.hpp"
#include "utils/span.hpp" // For bspan

namespace cpptrace {
namespace detail {

Result<monostate, internal_error> decompress_zstd(
    bspan decompressed_data,
    base_file& compressed_file,
    off_t offset,
    size_t compressed_size
);

}
}

#endif
