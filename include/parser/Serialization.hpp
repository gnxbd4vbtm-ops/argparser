#pragma once

#include "Argument.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace parser::detail
{

struct PersistedState
{
	std::string shortFlag;
	std::string longFlag;
	bool supplied = false;
	std::vector<std::string> values;
};

std::vector<std::uint8_t> serialize_state(const std::vector<Argument> &arguments);
std::map<std::string, PersistedState> deserialize_state(const std::vector<std::uint8_t> &bytes);
std::vector<std::uint8_t> read_binary_file(const std::string &path);
void write_binary_file(const std::string &path, const std::vector<std::uint8_t> &bytes);

} // namespace parser::detail
