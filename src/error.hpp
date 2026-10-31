#pragma once

#include <iostream>
#include <string>
#include <source_location>

#include "location.hpp"


[[noreturn]] inline auto error(const std::string_view msg, const Location &loc,
                  const std::source_location src = std::source_location::current()) -> void {
    std::cerr << loc.line << ":" << loc.col << ": error: " << msg << " [in " << src.function_name() << "]" << std::endl;
    exit(EXIT_FAILURE);
}
