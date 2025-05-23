#include "utils/decompress/decompress_zlib.h"

#include <zlib.h>
#include "utils/utils.hpp"

namespace cpptrace {
namespace detail {

Result<monostate, internal_error> decompress_zlib(
    bspan decompressed_data,
    base_file& compressed_file,
    off_t offset,
    size_t compressed_size
) {
    // zlib docs provide an example of 16K and also says:
    // Larger buffer sizes would be more efficient, especially for inflate(). If the memory is
    // available, buffers sizes on the order of 128K or 256K bytes should be used.
    constexpr size_t kChunkSize = 262144; // 256K
    z_stream strm{};
    int ret = inflateInit(&strm);
    if(ret != Z_OK) {
        return internal_error("zlib inflateInit failed");
    }
    std::unique_ptr<z_stream, decltype(&inflateEnd)> strm_raii(&strm, inflateEnd);
    size_t total_read = 0;
    size_t total_written = 0;
    std::vector<char> chunk_buffer(kChunkSize);
    strm.next_out = reinterpret_cast<Bytef*>(decompressed_data.data());
    strm.avail_out = static_cast<uInt>(decompressed_data.size());
    while(total_read < compressed_size) {
        size_t to_read = std::min(kChunkSize, compressed_size - total_read);
        auto read_res = compressed_file.read_span(
            cpptrace::detail::make_span(chunk_buffer.begin(), chunk_buffer.begin() + static_cast<std::ptrdiff_t>(to_read)),
            static_cast<off_t>(offset + total_read)
        );
        if(!read_res) {
            return read_res.unwrap_error();
        }
        strm.next_in = reinterpret_cast<Bytef*>(chunk_buffer.data());
        strm.avail_in = static_cast<uInt>(to_read);
        while(strm.avail_in > 0) {
            ret = inflate(&strm, Z_NO_FLUSH);
            if(ret == Z_STREAM_END) {
                break;
            }
            if(ret != Z_OK) {
                return internal_error("zlib inflate failed");
            }
        }
        total_read += to_read;
        total_written = strm.total_out;
        if(ret == Z_STREAM_END) {
            break;
        }
    }
    if(ret != Z_STREAM_END) {
        return internal_error("zlib did not reach stream end");
    }
    if(total_written != decompressed_data.size()) {
        return internal_error("zlib decompressed size mismatch");
    }
    return monostate{};
}

}  // namespace detail
}
