#pragma once

#include <ranges> // IWYU pragma: export
#include <version>

// Backfills std::views::concat from range-v3 where the standard library lacks it (e.g. Apple's
// libc++), so TUs can use it as if it were std. Include this header before using it.
//
// Only pieces whose feature-test macro reliably means "absent" can be backfilled this way: a
// using-declaration for a name the library already defines fails to compile. Strictly, adding
// declarations to namespace std is undefined behavior; remove this once toolchains catch up.
#if __cpp_lib_ranges_concat < 202403L
#if __has_include(<range/v3/view/concat.hpp>)
// range-v3 0.12 forward-declares std containers on Apple Clang using a visibility macro that
// newer libc++ no longer defines; opt out of those declarations.
#define META_NO_STD_FORWARD_DECLARATIONS
#include <range/v3/view/concat.hpp>
namespace std::ranges::views {
using ::ranges::views::concat;
} // namespace std::ranges::views
#else
#error "std::views::concat requires C++26 library support (__cpp_lib_ranges_concat) or range-v3"
#endif
#endif

namespace ome::tools::lobster {

// range-v3 owns the global ::ranges, so the short names live in the project namespace
namespace ranges = std::ranges; // NOLINT(misc-unused-alias-decls)
namespace views = std::views;   // NOLINT(misc-unused-alias-decls)

} // namespace ome::tools::lobster
