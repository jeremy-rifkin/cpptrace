#include "cpptrace/forward.hpp"

#include <array>
#include <cctype>
#include <unordered_set>
#include <variant>
#include <vector>

#include "utils/error.hpp"
#include "utils/optional.hpp"
#include "utils/string_view.hpp"
#include "utils/utils.hpp"

// Docs
// https://itanium-cxx-abi.github.io/cxx-abi/abi.html
// https://en.wikiversity.org/wiki/Visual_C%2B%2B_name_mangling

// Demangling
// https://github.com/llvm/llvm-project/blob/main/libcxxabi/src/demangle/ItaniumDemangle.h
// https://github.com/gcc-mirror/gcc/blob/b76f1fb7bf8a7b66b8acd469309257f8b18c0c51/libiberty/cp-demangle.c#L6794
// https://github.com/wine-mirror/wine/blob/3295365ba5654d6ff2da37c1ffa84aed81291fc1/dlls/msvcrt/undname.c#L1476

// Mangling
// https://github.com/llvm/llvm-project/blob/1463da8c4063cf1f1513aa5dbcedb44d2099c87f/clang/include/clang/AST/Mangle.h
// https://github.com/llvm/llvm-project/blob/1463da8c4063cf1f1513aa5dbcedb44d2099c87f/clang/lib/AST/MicrosoftMangle.cpp#L1709-L1721

// Test cases
// https://github.com/llvm/llvm-project/tree/d1b0b4bb4405c144e23be3d5c0459b03f95bd5ac/llvm/test/Demangle
// https://github.com/llvm/llvm-project/blob/d1b0b4bb4405c144e23be3d5c0459b03f95bd5ac/libcxxabi/test/DemangleTestCases.inc
// https://github.com/llvm/llvm-project/blob/d1b0b4bb4405c144e23be3d5c0459b03f95bd5ac/libcxxabi/test/test_demangle.pass.cpp
// https://github.com/wine-mirror/wine/blob/3295365ba5654d6ff2da37c1ffa84aed81291fc1/dlls/msvcrt/tests/cpp.c#L108
// https://github.com/wine-mirror/wine/blob/3295365ba5654d6ff2da37c1ffa84aed81291fc1/dlls/ucrtbase/tests/cpp.c#L57


CPPTRACE_BEGIN_NAMESPACE
namespace detail {
    template<typename T, typename Arg>
    bool is_any(const T& value, const Arg& arg) {
        return value == arg;
    }

    template<typename T, typename Arg, typename... Args>
    bool is_any(const T& value, const Arg& arg, const Args&... args) {
        return (value == arg) || is_any(value, args...);
    }

    // http://eel.is/c++draft/lex.name#nt:identifier
    bool is_identifier_start(char c) {
        return isalpha(c) || c == '$' || c == '_';
    }
    bool is_identifier_continue(char c) {
        return isdigit(c) || is_identifier_start(c);
    }
    bool is_hex_digit(char c) {
        return isdigit(c) || is_any(c, 'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F');
    }
    bool is_octal_digit(char c) {
        return is_any(c, '0', '1', '2', '3', '4', '5', '6', '7');
    }
    bool is_simple_escape_char(char c) {
        return is_any(c, '\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v');
    }

    // http://eel.is/c++draft/lex.operators#nt:operator-or-punctuator
    const std::vector<string_view> punctuators_and_operators = []() {
        std::vector<string_view> vec{
            "{",        "}",        "[",        "]",        "(",        ")",
            "<:",       ":>",       "<%",       "%>",       ";",        ":",        "...",
            "?",        "::",       ".",        ".*",       "->",       "->*",      "~",
            "!",        "+",        "-",        "*",        "/",        "%",        "^",        "&",        "|",
            "=",        "+=",       "-=",       "*=",       "/=",       "%=",       "^=",       "&=",       "|=",
            "==",       "!=",       "<",        ">",        "<=",       ">=",       "<=>",      "&&",       "||",
            "<<",       ">>",       "<<=",      ">>=",      "++",       "--",       ",",
            // "and",      "or",       "xor",      "not",      "bitand",   "bitor",    "compl",
            // "and_eq",   "or_eq",    "xor_eq",   "not_eq",
            "#", // extension for {lambda()#1}
        };
        std::sort(vec.begin(), vec.end(), [](string_view a, string_view b) { return a.size() > b.size(); });
        return vec;
    } ();

    const std::array<string_view, 2> anonymous_namespace_spellings = {"(anonymous namespace)", "`anonymous namespace'"};

    bool is_opening_punctuation(string_view token) {
        return token == "(" || token == "[" || token == "{" || token == "<";
    }

    bool is_closing_punctuation(string_view token) {
        return token == ")" || token == "]" || token == "}" || token == ">";
    }

    string_view get_corresponding_punctuation(string_view token) {
        if(token == "(") {
            return ")";
        } else if(token == "[") {
            return "]";
        } else if(token == "{") {
            return "}";
        } else if(token == "<") {
            return ">";
        }
        PANIC();
    }

    // There are five kinds of tokens in C++: identifiers, keywords, literals, operators, and other separators
    // We tokenize a mostly-subset of this:
    //  - identifiers/keywords
    //  - literals: char, string, int, float. Msvc `strings' too.
    //  - punctuation
    // Additionally we tokenize a few things that are useful
    //  - anonymous namespace tags

    enum class token_type {
        identifier,
        punctuation,
        literal,
        anonymous_namespace
    };

    struct token {
        token_type type;
        string_view str;

        bool operator==(const token& other) const {
            return type == other.type && str == other.str;
        }
    };

    struct parse_error {
        int x; // this works around a gcc bug with warn_unused_result and empty structs
        string_view what() const {
            return "Parse error";
        }
    };

    class symbol_tokenizer {
    public:

    private:
        string_view source;
        optional<token> next_token;

        bool peek(string_view text, size_t pos = 0) const {
            return text == source.substr(pos, text.size());
        }

        optional<token> peek_anonymous_namespace() const {
            for(const auto& spelling : anonymous_namespace_spellings) {
                if(peek(spelling)) {
                    return token{token_type::anonymous_namespace, {source.begin(), spelling.size()}};
                }
            }
            return nullopt;
        }

        optional<token> peek_number() const {
            // More or less following pp-number https://eel.is/c++draft/lex.ppnumber
            auto cursor = source.begin();
            if(cursor != source.end() && std::isdigit(*cursor)) {
                while(
                    cursor != source.end()
                    && (
                        std::isdigit(*cursor)
                        || is_identifier_continue(*cursor)
                        || is_any(*cursor, '\'', '-', '+', '.')
                    )
                ) {
                    cursor++;
                }
            }
            if(cursor == source.begin()) {
                return nullopt;
            }
            return token{token_type::literal, {source.begin(), cursor}};
        }

        optional<token> peek_msvc_string() const {
            // msvc strings look like `this'
            // they nest, e.g.: ``int main(void)'::`2'::<lambda_1>::operator()(void)const '
            // TODO: Escapes?
            auto cursor = source.begin();
            VERIFY(cursor != source.end() && *cursor == '`');
            int depth = 0;
            do {
                if(*cursor == '`') {
                    depth++;
                } else if(*cursor == '\'') {
                    depth--;
                }
                cursor++;
            } while(cursor != source.end() && depth != 0);
            if(cursor == source.begin()) {
                return nullopt;
            }
            return token{token_type::literal, {source.begin(), cursor}};
        }

        optional<token> peek_char_or_string() const {
            // TODO: Escapes?
            auto cursor = source.begin();
            if(cursor != source.end() && *cursor == '`') {
                return peek_msvc_string();
            }
            if(cursor != source.end() && is_any(*cursor, '\'', '"')) {
                auto closing_quote = *cursor;
                cursor++;
                while(cursor != source.end() && *cursor != closing_quote) {
                    cursor++;
                }
                if(cursor != source.end() && *cursor == closing_quote) {
                    cursor++;
                }
            }
            if(cursor == source.begin()) {
                return nullopt;
            }
            return token{token_type::literal, {source.begin(), cursor}};
        }

        optional<token> peek_literal() const {
            if(auto res = peek_number()) {
                return res;
            } else if(auto res = peek_char_or_string()) {
                return res;
            } else {
                return nullopt;
            }
        }

        optional<token> peek_punctuation(bool in_template_argument_list, size_t pos = 0) const {
            for(const auto punctuation : punctuators_and_operators) {
                if(peek(punctuation, pos)) {
                    // https://eel.is/c++draft/temp.names#4 decompose >> to > when we think we're in a template argument
                    // list. We don't have to do this for >>= or >=.
                    if(in_template_argument_list && punctuation == ">>") {
                        return token{token_type::punctuation, {source.begin() + pos, 1}}; // ">"
                    }
                    return token{token_type::punctuation, {source.begin() + pos, punctuation.size()}};
                }
            }
            return nullopt;
        }

        optional<token> peek_identifier(size_t pos = 0) const {
            auto start = source.begin() + std::min(pos, source.size());;
            auto cursor = start;
            if(cursor != source.end() && is_identifier_start(*cursor)) {
                while(cursor != source.end() && is_identifier_continue(*cursor)) {
                    cursor++;
                }
            }
            if(cursor == start) {
                return nullopt;
            }
            return token{token_type::identifier, {start, cursor}};
        }

        token peek_misc() const {
            ASSERT(!source.empty());
            return token{token_type::punctuation, {source.begin(), 1}};
        }

        void maybe_load_next_token(bool in_template_argument_list) {
            if(next_token.has_value()) {
                return;
            }
            while(!source.empty() && std::isspace(source[0])) {
                source.advance(1);
            }
            if(source.empty()) {
                return;
            }
            if(!next_token.has_value()) {
                next_token = peek_anonymous_namespace();
            }
            if(!next_token.has_value()) {
                next_token = peek_literal();
            }
            if(!next_token.has_value()) {
                next_token = peek_punctuation(in_template_argument_list);
            }
            if(!next_token.has_value()) {
                next_token = peek_identifier();
            }
            if(!next_token.has_value()) {
                next_token = peek_misc();
            }
        }

    public:
        symbol_tokenizer(string_view source) : source(source) {}

        optional<token> peek(bool in_template_argument_list = false) {
            maybe_load_next_token(in_template_argument_list);
            return next_token;
        }

        optional<token> advance(bool in_template_argument_list = false) {
            maybe_load_next_token(in_template_argument_list);
            if(!next_token) {
                return nullopt;
            }
            auto token = next_token;
            source.advance(token.unwrap().str.size());
            next_token.reset();
            return token;
        }

        optional<token> accept(token_type type, bool in_template_argument_list = false) {
            auto next = peek(in_template_argument_list);
            if(next && next.unwrap().type == type) {
                advance();
                return next;
            }
            return nullopt;
        }

        optional<token> accept(token token, bool in_template_argument_list = false) {
            auto next = peek(in_template_argument_list);
            if(next && next.unwrap() == token) {
                advance();
                return next;
            }
            return nullopt;
        }
    };

    void consume_balanced(symbol_tokenizer& tokenizer, string_view opening_punctuation);

    // consume a balanced set of some kind of brace, priority means it only considers those and no priority means it
    // considers each independently
    void consume_balanced(symbol_tokenizer& tokenizer, string_view opening_punctuation, bool priority) {
        int depth = 1;
        auto closing_punctuation = get_corresponding_punctuation(opening_punctuation);
        while(depth > 0) {
            const auto token = tokenizer.advance(opening_punctuation == "<");
            if(!token.has_value()) {
                break;
            }
            if(token.unwrap().type == token_type::punctuation) {
                if(token.unwrap().str == opening_punctuation) {
                    depth++;
                } else if(token.unwrap().str == closing_punctuation) {
                    depth--;
                } else if(!priority && is_opening_punctuation(token.unwrap().str)) {
                    consume_balanced(tokenizer, token.unwrap().str);
                }
            }
        }
    }

    void consume_balanced(symbol_tokenizer& tokenizer, string_view opening_punctuation) {
        consume_balanced(tokenizer, opening_punctuation, opening_punctuation != "<");
    }

    bool is_ignored_identifier(string_view string) {
        return string == "const"
            || string == "volatile"
            || string == "decltype"
            || string == "noexcept";
    }

    std::string name_from_symbol(string_view symbol);

    class symbol_parser {
        symbol_tokenizer& tokenizer;
        std::string name_output;
        bool last_was_identifier = false;
        bool reset_output_flag = false;

        void append_output(token token, bool = false) {
            auto is_identifier = token.type == token_type::identifier;
            if(reset_output_flag) {
                name_output.clear();
                reset_output_flag = false;
                last_was_identifier = false;
            } else if(is_identifier && last_was_identifier) {
                name_output += ' ';
            }
            name_output += token.str;
            last_was_identifier = is_identifier;
        }

        Result<bool, parse_error> accept_anonymous_namespace() {
            if(tokenizer.accept(token_type::anonymous_namespace)) {
                append_output({ token_type::identifier, "(anonymous namespace)" });
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_balanced_punctuation() {
            auto token = tokenizer.peek();
            if(!token) {
                return false;
            }
            if(token.unwrap().type == token_type::punctuation && is_opening_punctuation(token.unwrap().str)) {
                tokenizer.advance();
                consume_balanced(tokenizer, token.unwrap().str);
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_punctuation() {
            if(tokenizer.accept(token_type::punctuation)) {
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_ignored_identifier() {
            auto token = tokenizer.peek();
            if(!token) {
                return false;
            }
            if(token.unwrap().type == token_type::identifier && is_ignored_identifier(token.unwrap().str)) {
                tokenizer.advance();
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_identifier_token() {
            if(auto complement = tokenizer.accept({token_type::punctuation, "~"})) {
                append_output(complement.unwrap());
                auto token = tokenizer.accept(token_type::identifier);
                if(!token) {
                    return parse_error{};
                }
                append_output(token.unwrap());
                return true;
            } else if(auto token = tokenizer.accept(token_type::identifier)) {
                append_output(token.unwrap());
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_new_delete() {
            optional<token> token;
            if(
                (token = tokenizer.accept({token_type::identifier, "new"}))
                || (token = tokenizer.accept({token_type::identifier, "delete"}))
            ) {
                append_output(token.unwrap(), true);
                auto op = tokenizer.accept({token_type::punctuation, "["});
                if(op) {
                    append_output(op.unwrap());
                    auto op2 = tokenizer.accept({token_type::punctuation, "]"});
                    if(!op2) {
                        return parse_error{};
                    }
                    append_output(op2.unwrap());
                }
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_decltype_auto() {
            if(auto token = tokenizer.accept({token_type::identifier, "decltype"})) {
                append_output(token.unwrap(), true);
                auto op = tokenizer.accept({token_type::punctuation, "("});
                if(!op) {
                    return parse_error{};
                }
                append_output(op.unwrap());
                auto ident = tokenizer.accept({token_type::identifier, "auto"});
                if(!ident) {
                    return parse_error{};
                }
                append_output(ident.unwrap());
                auto op2 = tokenizer.accept({token_type::punctuation, ")"});
                if(!op2) {
                    return parse_error{};
                }
                append_output(op2.unwrap());
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_operator() {
            if(auto token = tokenizer.accept({token_type::identifier, "operator"})) {
                append_output(token.unwrap());
                auto res = accept_new_delete();
                if(res.is_error()) {
                    return res.unwrap_error();
                }
                if(res.unwrap_value()) {
                    return true;
                }
                res = accept_decltype_auto();
                if(res.is_error()) {
                    return res.unwrap_error();
                }
                if(res.unwrap_value()) {
                    return true;
                }
                if(auto coawait = tokenizer.accept({token_type::identifier, "co_await"})) {
                    append_output(coawait.unwrap());
                    return true;
                }
                if(auto literal = tokenizer.accept({token_type::literal, "\"\""})) {
                    auto name = tokenizer.accept(token_type::identifier);
                    if(!name) {
                        return parse_error{};
                    }
                    append_output(literal.unwrap());
                    append_output(name.unwrap());
                    return true;
                }
                if(auto op = tokenizer.accept(token_type::punctuation)) {
                    append_output(op.unwrap());
                    if(is_any(op.unwrap().str, "(", "[", "{")) {
                        auto op2 = tokenizer.accept(token_type::punctuation);
                        if(!op2 || op2.unwrap().str != get_corresponding_punctuation(op.unwrap().str)) {
                            return parse_error{};
                        }
                        append_output(op2.unwrap());
                    }
                    return true;
                }
                // otherwise try to parse a name, in the case of a conversion operator
                // there is a bit of a grammer hack here, it doesn't properly "nest," but it works
                auto term_res = parse_symbol_term();
                if(term_res) {
                    return term_res.unwrap();
                }
                return true;
            }
            return false;
        }

        Result<bool, parse_error> accept_lambda() {
            // LLVM does main::'lambda'<...>(...)::operator()<...>(...) -- apparently this can be 'lambda<count>'
            // GCC does main::{lambda<...>(...)#1}::operator()<...>(...)
            // MSVC does `int main(void)'::`2'::<lambda_1>::operator()<...>(...)
            // https://github.com/llvm/llvm-project/blob/90beda2aba3cac34052827c560449fcb184c7313/libcxxabi/src/demangle/ItaniumDemangle.h#L1848-L1850 TODO: What about the count?
            // https://github.com/gcc-mirror/gcc/blob/b76f1fb7bf8a7b66b8acd469309257f8b18c0c51/libiberty/cp-demangle.c#L6210-L6251 TODO: What special characters can appear?
            if(auto opening_brace = tokenizer.accept({token_type::punctuation, "{"})) {
                auto lambda_token = tokenizer.accept({token_type::identifier, "lambda"});
                if(!lambda_token) {
                    return parse_error{};
                }
                if(auto res = consume_punctuation()) {
                    return res.unwrap();
                }
                auto hash_token = tokenizer.accept({token_type::punctuation, "#"});
                if(!hash_token) {
                    return parse_error{};
                }
                auto discriminator_token = tokenizer.accept(token_type::literal);
                if(!discriminator_token) {
                    return parse_error{};
                }
                auto closing_brace = tokenizer.accept({token_type::punctuation, "}"});
                if(!closing_brace) {
                    return parse_error{};
                }
                append_output({token_type::punctuation, "<"});
                append_output(lambda_token.unwrap());
                append_output(hash_token.unwrap());
                append_output(discriminator_token.unwrap());
                append_output({token_type::punctuation, ">"});
                return true;
            }
            auto maybe_literal_token = tokenizer.peek();
            if(
                maybe_literal_token
                && maybe_literal_token.unwrap().type == token_type::literal
                && maybe_literal_token.unwrap().str.starts_with("'lambda")
                && maybe_literal_token.unwrap().str.ends_with("'")
            ) {
                tokenizer.advance();
                append_output({token_type::punctuation, "<"});
                auto str = maybe_literal_token.unwrap().str;
                append_output({token_type::punctuation, str.substr(1, str.size() - 2)});
                append_output({token_type::punctuation, ">"});
                return true;
            }
            if(
                maybe_literal_token
                && maybe_literal_token.unwrap().type == token_type::literal
                && maybe_literal_token.unwrap().str.starts_with("`")
                && maybe_literal_token.unwrap().str.ends_with("'")
            ) {
                tokenizer.advance();
                // append_output(maybe_literal_token.unwrap());
                // This string is going to be another symbol, recursively reduce it
                append_output({token_type::punctuation, "`"});
                auto symbol = maybe_literal_token.unwrap().str;
                ASSERT(symbol.size() >= 2);
                auto name = detail::name_from_symbol(symbol.substr(1, symbol.size() - 2));
                append_output({token_type::literal, name});
                append_output({token_type::punctuation, "'"});
                return true;
            }
            if(auto opening_brace = tokenizer.accept({token_type::punctuation, "<"})) {
                auto lambda_token = tokenizer.accept(token_type::identifier);
                if(!lambda_token || !lambda_token.unwrap().str.starts_with("lambda_")) {
                    return parse_error{};
                }
                auto closing_brace = tokenizer.accept({token_type::punctuation, ">"});
                if(!closing_brace) {
                    return parse_error{};
                }
                append_output(opening_brace.unwrap());
                append_output(lambda_token.unwrap());
                append_output(closing_brace.unwrap());
                return true;
            }
            return false;
        }

        optional<parse_error> parse_symbol_term() {
            {
                Result<bool, parse_error> res = accept_anonymous_namespace();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    return nullopt;
                }
            }
            {
                Result<bool, parse_error> res = accept_operator();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    return nullopt;
                }
            }
            {
                Result<bool, parse_error> res = accept_ignored_identifier();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    return nullopt;
                }
            }
            {
                Result<bool, parse_error> res = accept_identifier_token();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    return nullopt;
                }
            }
            {
                Result<bool, parse_error> res = accept_lambda();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    return nullopt;
                }
            }
            return parse_error{};
        }

        optional<parse_error> consume_punctuation() {
            while(
                tokenizer.peek()
                && tokenizer.peek().unwrap().type == token_type::punctuation
                && tokenizer.peek().unwrap().str != "::"
                && tokenizer.peek().unwrap().str != "#"
            ) {
                Result<bool, parse_error> res = accept_balanced_punctuation();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    continue;
                }
                // otherwise, if not balanced punctuation, just consume and drop
                tokenizer.advance();
            }
            return nullopt;
        }

        optional<parse_error> consume_punctuation_and_trailing_modifiers() {
            while(
                tokenizer.peek()
                && (
                    (
                        tokenizer.peek().unwrap().type == token_type::punctuation
                        && tokenizer.peek().unwrap().str != "::"
                        && tokenizer.peek().unwrap().str != "#"
                    ) || (
                        tokenizer.peek().unwrap().type == token_type::identifier
                        && is_ignored_identifier(tokenizer.peek().unwrap().str)
                    )
                )
            ) {
                Result<bool, parse_error> res = accept_balanced_punctuation();
                if(res.is_error()) {
                    return std::move(res).unwrap_error();
                } else if(res.unwrap_value()) {
                    continue;
                }
                // otherwise, if not balanced punctuation, just consume and drop
                tokenizer.advance();
            }
            return nullopt;
        }

        optional<parse_error> parse_symbol() {
            while(tokenizer.peek()) {
                if(auto res = parse_symbol_term()) {
                    return res.unwrap();
                }
                if(auto res = consume_punctuation_and_trailing_modifiers()) {
                    return res.unwrap();
                }
                if(auto token = tokenizer.accept({token_type::punctuation, "::"})) {
                    append_output(token.unwrap());
                } else {
                    break;
                }
            }
            return nullopt;
        }

    public:
        symbol_parser(symbol_tokenizer& tokenizer) : tokenizer(tokenizer) {}

        optional<parse_error> parse() {
            while(tokenizer.peek()) {
                reset_output_flag = true;
                auto res = parse_symbol();
                if(res) {
                    return res.unwrap();
                }
            }
            return nullopt;
        }

        std::string name() && {
            return std::move(name_output);
        }
    };

    std::string name_from_symbol(string_view symbol) {
        detail::symbol_tokenizer tokenizer(symbol);
        detail::symbol_parser parser(tokenizer);
        auto res = parser.parse();
        if(res) {
            // error
            return std::string(symbol);
        }
        auto name = std::move(parser).name();
        if(name.empty()) {
            return std::string(symbol);
        }
        return name;
    }
}
CPPTRACE_END_NAMESPACE

CPPTRACE_BEGIN_NAMESPACE
    std::string name_from_symbol(const std::string& symbol) {
        try {
            return detail::name_from_symbol(symbol);
        } catch(...) {
            detail::log_and_maybe_propagate_exception(std::current_exception());
            return symbol;
        }
    }
CPPTRACE_END_NAMESPACE
