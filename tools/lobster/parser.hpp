#pragma once

#include <lobster/reconstructor.hpp>

#include <string>

namespace ome::tools::lobster {

/// Parse a message line from the LOBSTER dataset
auto parse_message(std::string const &line) -> Message;

/// Parse an orderbook line from the LOBSTER dataset
auto parse_orderbook_line(std::string const &line) -> ExpectedOrderbook;

} // namespace ome::tools::lobster
