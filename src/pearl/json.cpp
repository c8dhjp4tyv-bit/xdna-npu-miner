#include "pearl/json.hpp"

#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace xdna::pearl::json {
namespace {

class Parser final {
public:
    explicit Parser(std::string_view text)
        : text_(text)
    {
    }

    [[nodiscard]] Value parse_document()
    {
        skip_space();
        Value result = parse_value(0U);
        skip_space();
        if (offset_ != text_.size()) {
            fail("trailing JSON bytes");
        }
        return result;
    }

private:
    [[noreturn]] void fail(const char* message) const
    {
        throw std::runtime_error(std::string("malformed JSON at byte ")
                                 + std::to_string(offset_) + ": " + message);
    }

    void skip_space()
    {
        while (offset_ < text_.size()
               && std::isspace(static_cast<unsigned char>(text_[offset_])) != 0) {
            ++offset_;
        }
    }

    [[nodiscard]] char take()
    {
        if (offset_ >= text_.size()) {
            fail("unexpected end of input");
        }
        return text_[offset_++];
    }

    void expect(char expected)
    {
        if (take() != expected) {
            fail("unexpected character");
        }
    }

    [[nodiscard]] Value parse_value(std::size_t depth)
    {
        if (depth > 64U) {
            fail("JSON nesting limit exceeded");
        }
        skip_space();
        if (offset_ >= text_.size()) {
            fail("missing value");
        }
        switch (text_[offset_]) {
        case 'n':
            consume_literal("null");
            return Value();
        case 't':
            consume_literal("true");
            return Value(true);
        case 'f':
            consume_literal("false");
            return Value(false);
        case '"':
            return Value(parse_string());
        case '[':
            return parse_array(depth);
        case '{':
            return parse_object(depth);
        default:
            if (text_[offset_] == '-' || std::isdigit(static_cast<unsigned char>(text_[offset_])) != 0) {
                return Value(Value::Number{parse_number()}, 0);
            }
            fail("unknown value");
        }
    }

    void consume_literal(std::string_view literal)
    {
        if (text_.substr(offset_, literal.size()) != literal) {
            fail("invalid literal");
        }
        offset_ += literal.size();
    }

    [[nodiscard]] std::string parse_string()
    {
        expect('"');
        std::string result;
        while (offset_ < text_.size()) {
            const unsigned char raw = static_cast<unsigned char>(take());
            if (raw == '"') {
                return result;
            }
            if (raw < 0x20U) {
                fail("control character in string");
            }
            if (raw != '\\') {
                result.push_back(static_cast<char>(raw));
                continue;
            }
            const char escaped = take();
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                std::uint32_t code_point = 0U;
                for (unsigned index = 0U; index < 4U; ++index) {
                    const unsigned char digit = static_cast<unsigned char>(take());
                    code_point <<= 4U;
                    if (digit >= '0' && digit <= '9') {
                        code_point |= digit - '0';
                    } else if (digit >= 'a' && digit <= 'f') {
                        code_point |= digit - 'a' + 10U;
                    } else if (digit >= 'A' && digit <= 'F') {
                        code_point |= digit - 'A' + 10U;
                    } else {
                        fail("invalid unicode escape");
                    }
                }
                if (code_point >= 0xD800U && code_point <= 0xDFFFU) {
                    fail("unicode surrogate escapes are unsupported");
                }
                if (code_point <= 0x7FU) {
                    result.push_back(static_cast<char>(code_point));
                } else if (code_point <= 0x7FFU) {
                    result.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
                    result.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
                } else {
                    result.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
                    result.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
                    result.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
                }
                break;
            }
            default:
                fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }

    [[nodiscard]] std::string parse_number()
    {
        const std::size_t begin = offset_;
        if (offset_ < text_.size() && text_[offset_] == '-') {
            ++offset_;
        }
        if (offset_ >= text_.size()) {
            fail("truncated number");
        }
        if (text_[offset_] == '0') {
            ++offset_;
            if (offset_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[offset_])) != 0) {
                fail("leading zero in number");
            }
        } else {
            if (std::isdigit(static_cast<unsigned char>(text_[offset_])) == 0) {
                fail("invalid number");
            }
            while (offset_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[offset_])) != 0) {
                ++offset_;
            }
        }
        if (offset_ < text_.size() && text_[offset_] == '.') {
            ++offset_;
            const std::size_t fraction_begin = offset_;
            while (offset_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[offset_])) != 0) {
                ++offset_;
            }
            if (fraction_begin == offset_) {
                fail("number has no fractional digits");
            }
        }
        if (offset_ < text_.size() && (text_[offset_] == 'e' || text_[offset_] == 'E')) {
            ++offset_;
            if (offset_ < text_.size() && (text_[offset_] == '+' || text_[offset_] == '-')) {
                ++offset_;
            }
            const std::size_t exponent_begin = offset_;
            while (offset_ < text_.size()
                   && std::isdigit(static_cast<unsigned char>(text_[offset_])) != 0) {
                ++offset_;
            }
            if (exponent_begin == offset_) {
                fail("number has no exponent digits");
            }
        }
        return std::string(text_.substr(begin, offset_ - begin));
    }

    [[nodiscard]] Value parse_array(std::size_t depth)
    {
        expect('[');
        Array result;
        skip_space();
        if (offset_ < text_.size() && text_[offset_] == ']') {
            ++offset_;
            return Value(std::move(result));
        }
        for (;;) {
            result.push_back(parse_value(depth + 1U));
            skip_space();
            const char separator = take();
            if (separator == ']') {
                return Value(std::move(result));
            }
            if (separator != ',') {
                fail("array requires comma or closing bracket");
            }
        }
    }

    [[nodiscard]] Value parse_object(std::size_t depth)
    {
        expect('{');
        Object result;
        skip_space();
        if (offset_ < text_.size() && text_[offset_] == '}') {
            ++offset_;
            return Value(std::move(result));
        }
        for (;;) {
            skip_space();
            if (offset_ >= text_.size() || text_[offset_] != '"') {
                fail("object key must be a string");
            }
            const std::string key = parse_string();
            skip_space();
            expect(':');
            const bool inserted = result.emplace(key, parse_value(depth + 1U)).second;
            if (!inserted) {
                fail("duplicate object key");
            }
            skip_space();
            const char separator = take();
            if (separator == '}') {
                return Value(std::move(result));
            }
            if (separator != ',') {
                fail("object requires comma or closing brace");
            }
        }
    }

    std::string_view text_;
    std::size_t offset_ = 0U;
};

} // namespace

Value::Value()
    : storage_(nullptr)
{
}

Value::Value(bool value)
    : storage_(value)
{
}

Value::Value(Number value, int)
    : storage_(std::move(value))
{
}

Value::Value(std::string value)
    : storage_(std::move(value))
{
}

Value::Value(Object value)
    : storage_(std::move(value))
{
}

Value::Value(Array value)
    : storage_(std::move(value))
{
}

bool Value::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::is_bool() const noexcept { return std::holds_alternative<bool>(storage_); }
bool Value::is_number() const noexcept { return std::holds_alternative<Number>(storage_); }
bool Value::is_string() const noexcept { return std::holds_alternative<std::string>(storage_); }
bool Value::is_object() const noexcept { return std::holds_alternative<Object>(storage_); }
bool Value::is_array() const noexcept { return std::holds_alternative<Array>(storage_); }

bool Value::as_bool() const { return std::get<bool>(storage_); }
const Value::Number& Value::as_number() const { return std::get<Number>(storage_); }
const std::string& Value::as_string() const { return std::get<std::string>(storage_); }
const Object& Value::as_object() const { return std::get<Object>(storage_); }
Object& Value::as_object() { return std::get<Object>(storage_); }
const Array& Value::as_array() const { return std::get<Array>(storage_); }

const Value* Value::find(std::string_view key) const
{
    if (!is_object()) {
        return nullptr;
    }
    const auto iterator = as_object().find(key);
    return iterator == as_object().end() ? nullptr : &iterator->second;
}

Value parse(std::string_view text, std::size_t max_bytes)
{
    if (text.size() > max_bytes) {
        throw std::runtime_error("JSON message exceeds configured size limit");
    }
    return Parser(text).parse_document();
}

} // namespace xdna::pearl::json
