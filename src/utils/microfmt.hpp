#ifndef MICROFMT_HPP
#define MICROFMT_HPP

// Copyright (c) 2024 Jeremy Rifkin; MIT License

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
#include <string_view>
#endif
#ifdef _MSC_VER
#include <intrin.h>
#endif

// {[[align][width]:[fill][base]]}
// width: number or {}

namespace microfmt {
    namespace detail {
        #define STR2(x) #x
        #define STR(x) STR2(x)
        #define MICROFMT_ASSERT(expr) if(!(expr)) { \
            throw std::runtime_error("Microfmt check failed" __FILE__ ":" STR(__LINE__) ": " #expr); \
        }

        inline std::uint64_t clz(std::uint64_t value) {
            #ifdef _MSC_VER
             unsigned long out = 0;
             #ifdef _WIN64
              _BitScanReverse64(&out, value);
             #else
              if(_BitScanReverse(&out, std::uint32_t(value >> 32))) {
                  return 63 - int(out + 32);
              }
              _BitScanReverse(&out, std::uint32_t(value));
             #endif
             return 63 - out;
            #else
             return __builtin_clzll(value);
            #endif
        }

        template<typename U, typename V> U to(V v) {
            return static_cast<U>(v); // A way to cast to U without "warning: useless cast to type"
        }

        enum class alignment { left, right };

        struct format_options {
            alignment align = alignment::left;
            char fill = ' ';
            size_t width = 0;
            char base = 'd';
        };

        template<typename It> void do_write(std::string& out, It begin, It end, const format_options& options) {
            auto size = end - begin;
            MICROFMT_ASSERT(size >= 0);
            if(static_cast<std::size_t>(size) >= options.width) {
                out.append(begin, end);
            } else {
                auto out_size = out.size();
                out.resize(out_size + options.width);
                if(options.align == alignment::left) {
                    std::copy(begin, end, out.begin() + out_size);
                    std::fill(out.begin() + out_size + size, out.end(), options.fill);
                } else {
                    std::fill(out.begin() + out_size, out.begin() + out_size + (options.width - size), options.fill);
                    std::copy(begin, end, out.begin() + out_size + (options.width - size));
                }
            }
        }

        template<int shift, int mask>
        std::string to_string(std::uint64_t value, const char* digits = "0123456789abcdef") {
            if(value == 0) {
                return "0";
            } else {
                // digits = floor(1 + log_base(x))
                // log_base(x) = log_2(x) / log_2(base)
                // log_2(x) == 63 - clz(x)
                // 1 + (63 - clz(value)) / (63 - clz(1 << shift))
                // 63 - clz(1 << shift) is the same as shift
                auto n_digits = to<std::size_t>(1 + (63 - clz(value)) / shift);
                std::string number;
                number.resize(n_digits);
                std::size_t i = n_digits - 1;
                while(value > 0) {
                    number[i--] = digits[value & mask];
                    value >>= shift;
                }
                return number;
            }
        }

        inline std::string to_string(std::uint64_t value, const format_options& options) {
            switch(options.base) {
                case 'd': return std::to_string(value);
                case 'H': return to_string<4, 0xf>(value, "0123456789ABCDEF");
                case 'h': return to_string<4, 0xf>(value);
                case 'o': return to_string<3, 0x7>(value);
                case 'b': return to_string<1, 0x1>(value);
                default:
                    MICROFMT_ASSERT(false);
            }
        }

        class format_value {
            enum class value_type {
                char_value,
                int64_value,
                uint64_value,
                string_value,
                #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
                string_view_value,
                #endif
                c_string_value,
            };
            union {
                char char_value;
                std::int64_t int64_value;
                std::uint64_t uint64_value;
                const std::string* string_value;
                #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
                std::string_view string_view_value;
                #endif
                const char* c_string_value;
            };
            value_type value;

        public:
            format_value(char c) : char_value(c), value(value_type::char_value) {}
            format_value(short int_val) : int64_value(int_val), value(value_type::int64_value) {}
            format_value(int int_val) : int64_value(int_val), value(value_type::int64_value) {}
            format_value(long int_val) : int64_value(int_val), value(value_type::int64_value) {}
            format_value(long long int_val) : int64_value(int_val), value(value_type::int64_value) {}
            format_value(unsigned char int_val) : uint64_value(int_val), value(value_type::uint64_value) {}
            format_value(unsigned short int_val) : uint64_value(int_val), value(value_type::uint64_value) {}
            format_value(unsigned int int_val) : uint64_value(int_val), value(value_type::uint64_value) {}
            format_value(unsigned long int_val) : uint64_value(int_val), value(value_type::uint64_value) {}
            format_value(unsigned long long int_val) : uint64_value(int_val), value(value_type::uint64_value) {}
            format_value(const std::string& string) : string_value(&string), value(value_type::string_value) {}
            #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
            format_value(std::string_view sv) : string_view_value(sv), value(value_type::string_view_value) {}
            #endif
            format_value(const char* c_string) : c_string_value(c_string), value(value_type::c_string_value) {}

            int unwrap_int() const {
                switch(value) {
                    case value_type::int64_value:  return static_cast<int>(int64_value);
                    case value_type::uint64_value: return static_cast<int>(uint64_value);
                    default: MICROFMT_ASSERT(false);
                }
            }

        public:
            void write(std::string& out, const format_options& options) const {
                switch(value) {
                    case value_type::char_value:
                        do_write(out, &char_value, &char_value + 1, options);
                        break;
                    case value_type::int64_value:
                        {
                            std::string str;
                            std::int64_t val = int64_value;
                            if(val < 0) {
                                str += '-';
                                val *= -1;
                            }
                            str += to_string(static_cast<std::uint64_t>(val), options);
                            do_write(out, str.begin(), str.end(), options);
                        }
                        break;
                    case value_type::uint64_value:
                        {
                            std::string str = to_string(uint64_value, options);
                            do_write(out, str.begin(), str.end(), options);
                        }
                        break;
                    case value_type::string_value:
                        do_write(out, string_value->begin(), string_value->end(), options);
                        break;
                    #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
                    case value_type::string_view_value:
                        do_write(out, string_view_value.begin(), string_view_value.end(), options);
                        break;
                    #endif
                    case value_type::c_string_value:
                        do_write(out, c_string_value, c_string_value + std::strlen(c_string_value), options);
                        break;
                    default:
                        MICROFMT_ASSERT(false);
                }
            }
        };

        inline int parse_int(const char* begin, const char* end) {
            int x = 0;
            for(auto it = begin; it != end; it++) {
                MICROFMT_ASSERT(isdigit(*it));
                x *= 10;
                x += *it - '0';
            }
            return x;
        }

        template<std::size_t N>
        std::string format(const char* fmt_begin, const char* fmt_end, std::array<format_value, N> args) {
            std::string str;
            std::size_t arg_i = 0;
            auto it = fmt_begin;
            auto peek = [&] (std::size_t dist = 1) -> char { // 0 on failure
                if(it != fmt_end) {
                    return *(it + dist);
                } else {
                    return 0;
                }
            };
            auto read_number = [&] () -> int { // -1 on failure
                auto scan = it;
                while(scan != fmt_end && isdigit(*scan)) {
                    scan++;
                }
                if(scan != it) {
                    int val = parse_int(it, scan);
                    it = scan;
                    return val;
                } else {
                    return -1;
                }
            };
            while(it != fmt_end) {
                if(*it == '{') {
                    if(peek() == '{') {
                        // try to handle escape
                        str += '{';
                        it++;
                    } else {
                        // parse format string
                        it++;
                        MICROFMT_ASSERT(it != fmt_end);
                        format_options options;
                        // try to parse alignment
                        if(*it == '<' || *it == '>') {
                            options.align = *it == '<' ? alignment::left : alignment::right;
                            it++;
                        }
                        // try to parse width
                        auto width = read_number();
                        if(width != -1) {
                            options.width = width;
                        } else if(*it == '{') { // try to parse variable width
                            MICROFMT_ASSERT(peek() == '}');
                            it += 2;
                            MICROFMT_ASSERT(arg_i < args.size());
                            options.width = args[arg_i++].unwrap_int();
                        }
                        // try to parse fill/base
                        if(*it == ':') {
                            it++;
                            // try to parse fill
                            if(*it != '}' && peek(1) != '}') {
                                // two chars before the }, treat as fill+base
                                options.fill = *it++;
                                options.base = *it++;
                            } else if(*it != '}') {
                                // one char before the }, treat as base if possible
                                if(*it == 'd' || *it == 'h' || *it == 'H' || *it == 'o' || *it == 'b') {
                                    options.base = *it++;
                                } else {
                                    options.fill = *it++;
                                }
                            } else {
                                MICROFMT_ASSERT(false);
                            }
                        }
                        MICROFMT_ASSERT(*it == '}');
                        MICROFMT_ASSERT(arg_i < args.size());
                        args[arg_i++].write(str, options);
                    }
                } else if(*it == '}') {
                    // parse }} escape
                    if(peek() == '}') {
                        str += '}';
                        it++;
                    } else {
                        MICROFMT_ASSERT(false);
                    }
                } else {
                    str += *it;
                }
                it++;
            }
            return str;
        }
    }

    #if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    template<typename... Args>
    std::string format(std::string_view fmt, Args&&... args) {
        return detail::format<sizeof...(args)>(fmt.begin(), fmt.end(), {detail::format_value(args)...});
    }

    inline std::string format(std::string_view fmt) {
        return std::string(fmt);
    }
    #endif

    template<typename... Args>
    std::string format(const char* fmt, Args&&... args) {
        return detail::format<sizeof...(args)>(fmt, fmt + std::strlen(fmt), {detail::format_value(args)...});
    }

    // working around an old msvc bug https://godbolt.org/z/88T8hrzzq mre: https://godbolt.org/z/drd8echbP
    inline std::string format(const char* fmt) {
        return detail::format<1>(fmt, fmt + std::strlen(fmt), {detail::format_value(1)});
    }

    template<typename S, typename... Args>
    void print(const S& fmt, Args&&... args) {
        std::cout<<format(fmt, args...);
    }

    template<typename S, typename... Args>
    void print(std::ostream& ostream, const S& fmt, Args&&... args) {
        ostream<<format(fmt, args...);
    }

    template<typename S, typename... Args>
    void print(std::FILE* stream, const S& fmt, Args&&... args) {
        auto str = format(fmt, args...);
        fwrite(str.data(), 1, str.size(), stream);
    }
}

#endif
