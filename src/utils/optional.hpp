#ifndef OPTIONAL_HPP
#define OPTIONAL_HPP

#include <new>
#include <type_traits>
#include <utility>

#include "utils/common.hpp"
#include "utils/error.hpp"

namespace cpptrace {
namespace detail {
    struct nullopt_t {};

    static constexpr nullopt_t nullopt;

    template<
        typename T,
        typename std::enable_if<!std::is_same<typename std::decay<T>::type, void>::value, int>::type = 0
    >
    class optional {
        union {
            char x;
            T uvalue;
        };

        bool holds_value = false;

    public:
        optional() noexcept {}

        optional(nullopt_t) noexcept {}

        ~optional() {
            reset();
        }

        optional(const optional& other) : holds_value(other.holds_value) {
            if(holds_value) {
                new (static_cast<void*>(std::addressof(uvalue))) T(other.uvalue);
            }
        }

        optional(optional&& other)
            noexcept(std::is_nothrow_move_constructible<T>::value)
            : holds_value(other.holds_value)
        {
            if(holds_value) {
                new (static_cast<void*>(std::addressof(uvalue))) T(std::move(other.uvalue));
            }
        }

        optional& operator=(const optional& other) {
            optional copy(other);
            swap(copy);
            return *this;
        }

        optional& operator=(optional&& other)
            noexcept(std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value)
        {
            reset();
            if(other.holds_value) {
                new (static_cast<void*>(std::addressof(uvalue))) T(std::move(other.uvalue));
                holds_value = true;
            }
            return *this;
        }

        template<
            typename U = T,
            typename std::enable_if<!std::is_same<typename std::decay<U>::type, optional<T>>::value, int>::type = 0
        >
        optional(U&& value) : holds_value(true) {
            new (static_cast<void*>(std::addressof(uvalue))) T(std::forward<U>(value));
        }

        template<
            typename U = T,
            typename std::enable_if<!std::is_same<typename std::decay<U>::type, optional<T>>::value, int>::type = 0
        >
        optional& operator=(U&& value) {
            optional o(std::forward<U>(value));
            swap(o);
            return *this;
        }

        optional& operator=(nullopt_t) noexcept {
            reset();
            return *this;
        }

        void swap(optional& other) noexcept {
            if(holds_value && other.holds_value) {
                std::swap(uvalue, other.uvalue);
            } else if(holds_value && !other.holds_value) {
                new (&other.uvalue) T(std::move(uvalue));
                uvalue.~T();
            } else if(!holds_value && other.holds_value) {
                new (static_cast<void*>(std::addressof(uvalue))) T(std::move(other.uvalue));
                other.uvalue.~T();
            }
            std::swap(holds_value, other.holds_value);
        }

        bool has_value() const {
            return holds_value;
        }

        explicit operator bool() const {
            return holds_value;
        }

        void reset() {
            if(holds_value) {
                uvalue.~T();
            }
            holds_value = false;
        }

        NODISCARD T& unwrap() & {
            ASSERT(holds_value, "Optional does not contain a value");
            return uvalue;
        }

        NODISCARD const T& unwrap() const & {
            ASSERT(holds_value, "Optional does not contain a value");
            return uvalue;
        }

        NODISCARD T&& unwrap() && {
            ASSERT(holds_value, "Optional does not contain a value");
            return std::move(uvalue);
        }

        NODISCARD const T&& unwrap() const && {
            ASSERT(holds_value, "Optional does not contain a value");
            return std::move(uvalue);
        }

        template<typename U>
        NODISCARD T value_or(U&& default_value) const & {
            return holds_value ? uvalue : static_cast<T>(std::forward<U>(default_value));
        }

        template<typename U>
        NODISCARD T value_or(U&& default_value) && {
            return holds_value ? std::move(uvalue) : static_cast<T>(std::forward<U>(default_value));
        }
    };
}
}

#endif
