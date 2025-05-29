#include "logging.hpp"

#include <cpptrace/forward.hpp>

#include <atomic>

namespace cpptrace {
namespace internal {
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

CPPTRACE_BEGIN_NAMESPACE
    void set_log_level(log_level level) {
        internal::current_log_level = level;
    }

    void set_log_callback(std::function<void(log_level, const char*)> callback) {
        internal::log_callback() = std::move(callback);
    }

    void use_default_stderr_logger() {
        internal::log_callback() = internal::default_stderr_logger;
    }
CPPTRACE_END_NAMESPACE
