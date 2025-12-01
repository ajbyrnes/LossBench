#include <format>
#include <map>
#include <stdexcept>
#include <string>

#include "interface.hpp"

std::string findOption(
    const std::map<std::string, std::string>& options,
    const std::string& key
)
{
    auto it = options.find(key);
    if (it == options.end()) {
        throw std::runtime_error(std::format("Option '{}' not found.", key));
    }
    return it->second;
}