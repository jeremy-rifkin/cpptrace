#ifndef FILE_HPP
#define FILE_HPP

#include "utils/string_view.hpp"
#include "utils/span.hpp"
#include "utils/io/base_file.hpp"
#include "utils/utils.hpp"

namespace cpptrace {
namespace detail {
    class file : public base_file {
        file_wrapper file_obj;
        std::string object_path;

        file(file_wrapper file_obj, string_view path) : file_obj(std::move(file_obj)), object_path(path) {}

    public:
        file(file&&) = default;
        ~file() override = default;

        string_view path() const override {
            return object_path;
        }

        static Result<file, internal_error> open(cstring_view object_path) {
            auto file_obj = raii_wrap(std::fopen(object_path.c_str(), "rb"), file_deleter);
            if(file_obj == nullptr) {
                return internal_error("Unable to read object file {}", object_path);
            }
            return file(std::move(file_obj), object_path);
        }

        virtual Result<monostate, internal_error> read_bytes(bspan buffer, off_t offset) override {
            if(std::fseek(file_obj, offset, SEEK_SET) != 0) {
                return internal_error("fseek error in {} at offset {}", path(), offset);
            }
            if(std::fread(buffer.data(), buffer.size(), 1, file_obj) != 1) {
                return internal_error("fread error in {} at offset {} for {} bytes", path(), offset, buffer.size());
            }
            return monostate{};
        }
    };
}
}

#endif
