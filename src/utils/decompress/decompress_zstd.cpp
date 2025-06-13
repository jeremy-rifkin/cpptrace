#include "utils/decompress/decompress_zstd.h"

#include <zstd.h> // For ZSTD_*, ZSTD_DStream, etc.

#include <vector>
#include <memory>

namespace cpptrace {
namespace detail {

Result<monostate, internal_error> decompress_zstd(
    bspan decompressed_data,
    base_file& compressed_file,
    off_t offset,
    size_t compressed_size
) {
    std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> dstream(ZSTD_createDStream(), ZSTD_freeDStream);
    if(!dstream) {
        return internal_error("ZSTD_createDStream failed");
    }
    size_t init_ret = ZSTD_initDStream(dstream.get());
    if(ZSTD_isError(init_ret)) {
        return internal_error(std::string("ZSTD_initDStream failed: ") + ZSTD_getErrorName(init_ret));
    }

    static const size_t CHUNK_SIZE = ZSTD_DStreamInSize();
    std::vector<char> chunk_buffer(CHUNK_SIZE);

    ZSTD_outBuffer output = {};
    output.dst = decompressed_data.data();
    output.size = decompressed_data.size();
    output.pos = 0;

    size_t total_read = 0;
    while(total_read < compressed_size) {
        size_t to_read = std::min(CHUNK_SIZE, compressed_size - total_read);
        auto read_res = compressed_file.read_span(
            cpptrace::detail::make_span(chunk_buffer.begin(), chunk_buffer.begin() + static_cast<std::ptrdiff_t>(to_read)),
            static_cast<off_t>(offset + total_read)
        );
        if(!read_res) {
            return read_res.unwrap_error();
        }

        ZSTD_inBuffer input = {};
        input.src = chunk_buffer.data();
        input.size = to_read;
        input.pos = 0;

        while(input.pos < input.size) {
            size_t decompress_ret = ZSTD_decompressStream(dstream.get(), &output, &input);
            if(ZSTD_isError(decompress_ret)) {
                return internal_error(std::string("ZSTD_decompressStream failed: ") + ZSTD_getErrorName(decompress_ret));
            }
            if(decompress_ret == 0) {
                break; 
            }
        }
        total_read += to_read;
    }

    if(output.pos != decompressed_data.size()) {
        return internal_error("zstd decompressed size mismatch");
    }

    return monostate{};
}

} // namespace detail
} // namespace cpptrace
