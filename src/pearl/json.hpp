#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace xdna::pearl::json {

struct Value;
using Object = std::map<std::string, Value, std::less<>>;
using Array = std::vector<Value>;

struct Value final {
    struct Number {
        std::string value;
    };
    using Storage = std::variant<std::nullptr_t, bool, Number, std::string, Object, Array>;

    Value();
    explicit Value(bool value);
    explicit Value(Number value, int number_tag);
    explicit Value(std::string value);
    explicit Value(Object value);
    explicit Value(Array value);

    [[nodiscard]] bool is_null() const noexcept;
    [[nodiscard]] bool is_bool() const noexcept;
    [[nodiscard]] bool is_number() const noexcept;
    [[nodiscard]] bool is_string() const noexcept;
    [[nodiscard]] bool is_object() const noexcept;
    [[nodiscard]] bool is_array() const noexcept;

    [[nodiscard]] bool as_bool() const;
    [[nodiscard]] const Number& as_number() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] const Object& as_object() const;
    [[nodiscard]] Object& as_object();
    [[nodiscard]] const Array& as_array() const;

    [[nodiscard]] const Value* find(std::string_view key) const;

private:
    Storage storage_;
};

Value parse(std::string_view text, std::size_t max_bytes = 1U << 20U);

} // namespace xdna::pearl::json
