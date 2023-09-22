#ifndef CTRACE_TRACE_HPP
#define CTRACE_TRACE_HPP

#include <iostream>

namespace ctrace {
    template <typename T>
    struct trace_iterator {
        typedef decltype(std::declval<T&>().frames.data()) type;
    };

    template <typename T>
    auto iter_cast(ctrace_iterator iter)
    -> typename trace_iterator<T>::type {
        return reinterpret_cast<typename trace_iterator<T>::type>(iter.curr);
    }

    struct trace_types {
        static const char* get_type(unsigned id) {
            static constexpr const char* names[] = {
                "Raw Trace",
                "Object Trace",
                "Stacktrace",
            };
            return names[id];
        }

    protected:
        enum : unsigned {
            raw_trace,
            obj_trace,
            stacktrace,
        };
    };

    struct trace_handler : trace_types {
        static ctrace_trace wrap_trace(cpptrace::raw_trace&& raw);
        static ctrace_trace wrap_trace(cpptrace::object_trace&& obj);
        static ctrace_trace wrap_trace(cpptrace::stacktrace&& trace);
        static ctrace_trace resolve(ctrace_trace wrapped, bool move = false);
        static void print_trace(ctrace_trace wrapped, ctrace_print_options opts, std::va_list* pargs);
        static ctrace_string stringify(ctrace_trace wrapped);
        static void clear_trace(ctrace_trace wrapped);
        static void free_trace(ctrace_trace wrapped);
    };

    struct trace_iterator_handler : trace_types {
        static ctrace_iterator create_iterator(ctrace_trace trace);
        static void* begin(ctrace_trace trace);
        static void* end(ctrace_trace trace);
        static void next(ctrace_iterator* iter);
    };
}

namespace ctrace {
    ctrace_trace trace_handler::wrap_trace(cpptrace::raw_trace&& raw) {
        auto* handle = new cpptrace::raw_trace { std::move(raw) };
        return { handle, { raw_trace, ctrace_true } };
    }

    ctrace_trace trace_handler::wrap_trace(cpptrace::object_trace&& obj) {
        auto* handle = new cpptrace::object_trace { std::move(obj) };
        return { handle, { obj_trace, ctrace_true } };
    }

    ctrace_trace trace_handler::wrap_trace(cpptrace::stacktrace&& trace) {
        auto* handle = new cpptrace::stacktrace { std::move(trace) };
        return { handle, { stacktrace, ctrace_true } };
    }

    ctrace_trace trace_handler::resolve(ctrace_trace wrapped, bool move) {
        if(wrapped.type == raw_trace) {
            auto* handle = reinterpret_cast<cpptrace::raw_trace*>(wrapped.data);
            auto resolved = wrap_trace(handle->resolve());
            if(move) ctrace_free_trace(wrapped);
            return resolved;
        }
        else if(wrapped.type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_trace*>(wrapped.data);
            auto resolved = wrap_trace(handle->resolve());
            if(move) ctrace_free_trace(wrapped);
            return resolved;
        }
        else if(move) return wrapped;
        else {
            CTRACE_ERR("the current trace has already been resolved\n");
            return { nullptr, { stacktrace, ctrace_false } };
        }
    }

    void trace_handler::print_trace(ctrace_trace wrapped, ctrace_print_options opts, std::va_list* pargs) {
        if(wrapped.type != stacktrace) {
            CTRACE_WARN("the current trace is not printable (%s)\n", get_type(wrapped.type));
            return;
        }

        auto* handle = reinterpret_cast<cpptrace::stacktrace*>(wrapped.data);
        if(opts == 0) {
            handle->print(std::cout);
            return;
        }

        if(opts & ectrace_no_newline)
            CTRACE_INFO("the option `ectrace_no_newline` is currently unavailable\n");

        if(opts & ectrace_cstream) {
            if(!(opts & ectrace_no_color))
                CTRACE_INFO("color is currently unsupported with c streams\n");
            auto string = handle->to_string();

            if(opts & ectrace_out) std::fprintf(stdout, "%s", string.c_str());
            else if(opts & ectrace_err) std::fprintf(stderr, "%s", string.c_str());
            else if(opts & ectrace_custom) {
                auto* stream = va_arg(*pargs, FILE*);
                std::fprintf(stream, "%s", string.c_str());
            }
            else std::fprintf(stdout, "%s", string.c_str());
        }
        else {
            bool use_color = !(opts & ectrace_no_color);
            if(opts & ectrace_out) handle->print(std::cout, use_color);
            else if(opts & ectrace_err) handle->print(std::cerr, use_color);
            else if(opts & ectrace_custom) {
                auto* stream = va_arg(*pargs, std::ostream*);
                if(stream) handle->print(*stream, use_color);
                else {
                    CTRACE_ERR("the std::ostream argument is invalid\n");
                    handle->print(std::cout, use_color);
                }
            }
            else handle->print(std::cerr, use_color);
        }
    }

    ctrace_string trace_handler::stringify(ctrace_trace wrapped) {
        if(wrapped.type != stacktrace) {
            CTRACE_WARN("the current trace is not convertible to a string (%s)\n", get_type(wrapped.type));
            return string_handler::create_string("");
        }

        auto* handle = reinterpret_cast<cpptrace::stacktrace*>(wrapped.data);
        std::string str = handle->to_string();
        return string_handler::create_string(std::move(str));
    }

    void trace_handler::clear_trace(ctrace_trace wrapped) {
        if(wrapped.type == raw_trace) {
            auto* handle = reinterpret_cast<cpptrace::raw_trace*>(wrapped.data);
            handle->clear();
        }
        else if(wrapped.type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_trace*>(wrapped.data);
            handle->clear();
        }
        else {
            auto* handle = reinterpret_cast<cpptrace::stacktrace*>(wrapped.data);
            handle->clear();
        }
    }

    void trace_handler::free_trace(ctrace_trace wrapped) {
        if(wrapped.type == raw_trace) {
            auto* handle = reinterpret_cast<cpptrace::raw_trace*>(wrapped.data);
            delete handle;
        }
        else if(wrapped.type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_trace*>(wrapped.data);
            delete handle;
        }
        else {
            auto* handle = reinterpret_cast<cpptrace::stacktrace*>(wrapped.data);
            delete handle;
        }
    }


    ctrace_iterator trace_iterator_handler::create_iterator(ctrace_trace trace) {
        if(trace.resolved) return { begin(trace), end(trace), trace.type };
        else return { nullptr, nullptr, trace.type };
    }

    void* trace_iterator_handler::begin(ctrace_trace trace) {
        if(trace.type == raw_trace) {
            auto* handle = reinterpret_cast<cpptrace::raw_trace*>(trace.data);
            return handle->frames.data();
        }
        else if(trace.type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_trace*>(trace.data);
            return handle->frames.data();
        }
        else {
            auto* handle = reinterpret_cast<cpptrace::stacktrace*>(trace.data);
            return handle->frames.data();
        }
    }

    void* trace_iterator_handler::end(ctrace_trace trace) {
        if(trace.type == raw_trace) {
            auto* handle = reinterpret_cast<cpptrace::raw_trace*>(trace.data);
            return handle->frames.data() + handle->frames.size();
        }
        else if(trace.type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_trace*>(trace.data);
            return handle->frames.data() + handle->frames.size();
        }
        else {
            auto* handle = reinterpret_cast<cpptrace::stacktrace*>(trace.data);
            return handle->frames.data() + handle->frames.size();
        }
    }

    void trace_iterator_handler::next(ctrace_iterator* iter) {
        if(iter->type == raw_trace) {
            auto* handle = reinterpret_cast<uintptr_t*>(iter->curr);
            iter->curr = std::next(handle);
        }
        else if(iter->type == obj_trace) {
            auto* handle = reinterpret_cast<cpptrace::object_frame*>(iter->curr);
            iter->curr = std::next(handle);
        }
        else {
            auto* handle = reinterpret_cast<cpptrace::stacktrace_frame*>(iter->curr);
            iter->curr = std::next(handle);
        }
    }
}

#endif
