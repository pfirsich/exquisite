#include "../languages.hpp"

using namespace std::literals;

extern "C" {
TSLanguage* tree_sitter_cpp();
}

namespace {
const auto query = R"scm(
    (string_literal) @literal.string
    (raw_string_literal) @literal.string.raw
    (system_lib_string) @literal.string.systemlib
    (char_literal) @literal.char
    (number_literal) @literal.number
    (true) @literal.boolean.true
    (false) @literal.boolean.false

    (identifier) @identifier
    (namespace_identifier) @identifier.namespace
    (type_identifier) @identifier.type
    (auto) @identifier.type.auto
    (primitive_type) @identifier.type.primitive
    (field_identifier) @identifier.field

    "#include" @include
    (comment) @comment

    "(" @bracket.round.open
    ")" @bracket.round.close
    "[" @bracket.square.open
    "]" @bracket.square.close
    "{" @bracket.curly.open
    "}" @bracket.curly.close
    (template_argument_list "<" @bracket.angle.open)
    (template_argument_list ">" @bracket.angle.close)
    (template_parameter_list "<" @bracket.angle.open)
    (template_parameter_list ">" @bracket.angle.close)

    ;"alignas" @keyword
    ;"alignof" @keyword
    ;"and" @keyword
    ;"and_eq" @keyword
    ;"asm" @keyword
    ;"atomic_cancel" @keyword
    ;"atomic_commit" @keyword
    ;"atomic_noexcept" @keyword
    ;"bitand" @keyword
    ;"bitor" @keyword
    "break" @keyword
    "case" @keyword
    "catch" @keyword
    "class" @keyword
    ;"compl" @keyword
    ;"concept" @keyword
    "const" @keyword
    ;"consteval" @keyword
    "constexpr" @keyword
    ;"constinit" @keyword
    ;"const_cast" @keyword
    "continue" @keyword
    ;"co_await" @keyword
    ;"co_return" @keyword
    ;"co_yield" @keyword
    "decltype" @keyword
    "default" @keyword
    "delete" @keyword
    "do" @keyword
    ;"dynamic_cast @keyword
    "else" @keyword
    "enum" @keyword
    ;"explicit" @keyword
    ;"export" @keyword
    "extern" @keyword
    "for" @keyword
    "friend" @keyword
    "goto" @keyword
    "if" @keyword
    "inline" @keyword
    "mutable" @keyword
    "namespace" @keyword
    "new" @keyword
    "noexcept" @keyword
    ;"not" @keyword
    ;"not_eq" @keyword
    (nullptr) @keyword
    "operator" @keyword
    ;"or" @keyword
    ;"or_eq" @keyword
    "private" @keyword
    "protected" @keyword
    "public" @keyword
    ;"reflexpr" @keyword
    ;"register" @keyword
    ;(reinterpret_cast) @keyword
    "return" @keyword
    "sizeof" @keyword
    "static" @keyword
    ;"static_assert" @keyword
    ;"static_cast" @keyword
    "struct" @keyword
    "switch" @keyword
    ;"synchronized" @keyword
    "template" @keyword
    ;"this" @keyword
    ;"thread_local" @keyword
    "throw" @keyword
    "try" @keyword
    "typedef" @keyword
    ;"typeid" @keyword
    "typename" @keyword
    "union" @keyword
    "using" @keyword
    "virtual" @keyword
    "volatile" @keyword
    "while" @keyword
    ;"xor" @keyword
    ;"xor_eq" @keyword
)scm"sv;
}

namespace languages {
Language cpp {
    "C++"s,
    { "cpp"sv, "cc"sv, "cxx"sv, "c++"sv, "hpp"sv, "hh"sv, "hxx"sv, "h++"sv },
    std::make_unique<Highlighter>(tree_sitter_cpp(), query),
};
}
