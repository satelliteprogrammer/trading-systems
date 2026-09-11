#pragma once

// Catch2 decomposes `REQUIRE(e == v)` into `ExprLhs<std::expected<T, E> const &> == v`.
// With std::expected as a template argument of ExprLhs, ADL finds expected's hidden-friend
// operator==, whose C++26 (P3379) constraint `*x == v` then recurses on libc++:
// "satisfaction of constraint ... depends on itself".
// Capturing expected through a view that doesn't name std::expected in its template
// arguments keeps those friends out of ADL, and lets failures print the error.

#include <catch2/catch_tostring.hpp>
#include <catch2/internal/catch_decomposer.hpp>

#include <expected>
#include <string>
#include <type_traits>

namespace Catch {

template <typename T, typename E> class ExpectedView {
  public:
    explicit constexpr ExpectedView(std::expected<T, E> const &result) : result_(&result) {}

    constexpr explicit operator bool() const { return result_->has_value(); }
    [[nodiscard]] constexpr auto get() const -> std::expected<T, E> const & { return *result_; }

    // Deliberately unconstrained: a requires-clause here would recurse the same way.
    template <typename U>
    friend constexpr auto operator==(ExpectedView view, U const &rhs) -> bool {
        return *view.result_ == rhs;
    }

  private:
    std::expected<T, E> const *result_; // the expected outlives the REQUIRE full-expression
};

template <typename T, typename E> struct StringMaker<ExpectedView<T, E>> {
    static auto convert(ExpectedView<T, E> const &view) -> std::string {
        auto const &result = view.get();
        if (!result) {
            return "unexpected(" + Detail::stringify(result.error()) + ")";
        }
        if constexpr (std::is_void_v<T>) {
            return "expected";
        } else {
            return Detail::stringify(*result);
        }
    }
};

// One overload per value category, so each beats Decomposer's forwarding `T &&` by partial
// ordering instead of losing to it on reference binding.
template <typename T, typename E>
constexpr auto operator<=(Decomposer /*decomposer*/, std::expected<T, E> const &lhs) {
    return ExprLhs<ExpectedView<T, E>>{ExpectedView<T, E>{lhs}};
}
template <typename T, typename E>
constexpr auto operator<=(Decomposer /*decomposer*/, std::expected<T, E> &lhs) {
    return ExprLhs<ExpectedView<T, E>>{ExpectedView<T, E>{lhs}};
}
// Only views the temporary, which lives until the end of the REQUIRE full-expression.
template <typename T, typename E>
// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
constexpr auto operator<=(Decomposer /*decomposer*/, std::expected<T, E> &&lhs) {
    return ExprLhs<ExpectedView<T, E>>{ExpectedView<T, E>{lhs}};
}

} // namespace Catch
