#include "logging.hpp"

#include <atomic>

namespace cpptrace {
namespace detail {
    std::atomic<log_level> current_log_level(log_level::error); // NOSONAR

    void default_null_logger(log_level, const char*) {}

    void default_stderr_logger(log_level, const char* message) {
        std::cerr<<"cpptrace: "<<message<<std::endl;
    }

    std::function<void(log_level, const char*)>& log_callback() {
        static std::function<void(log_level, const char*)> callback{default_null_logger};
        return callback;
    }

    void do_log(log_level level, const char* message) {
        if(level < current_log_level) {
            return;
        }
        log_callback()(level, message);
    }

    namespace log {
        void error(const char* message) {
            do_log(log_level::error, message);
        }
    }
}
}

namespace cpptrace {
    void set_log_level(log_level level) {
        detail::current_log_level = level;
    }

    void set_log_callback(std::function<void(log_level, const char*)> callback) {
        detail::log_callback() = std::move(callback);
    }

    void use_default_stderr_logger() {
        detail::log_callback() = detail::default_stderr_logger;
    }
}
