#include "parser/Serialization.hpp"

#include "parser/Exceptions.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

namespace parser::detail
{

namespace
{

void append_u16(std::vector<std::uint8_t> &output, std::uint16_t value)
{
	output.push_back(static_cast<std::uint8_t>(value));
	output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(std::vector<std::uint8_t> &output, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		output.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint32_t crc32(const std::uint8_t *data, std::size_t size)
{
	std::uint32_t crc = 0xffffffffu;
	for (std::size_t i = 0; i < size; ++i)
	{
		crc ^= data[i];
		for (int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

} // namespace

std::vector<std::uint8_t> serialize_state(const std::vector<Argument> &arguments)
{
	if (arguments.size() > std::numeric_limits<std::uint32_t>::max())
		throw file_write_error("too many arguments to serialize");
	std::vector<std::uint8_t> bytes{'A', 'R', 'G', 'P'};
	append_u16(bytes, 1);
	append_u32(bytes, static_cast<std::uint32_t>(arguments.size()));
	for (const auto &argument : arguments)
	{
		if (argument.shortFlag.size() > std::numeric_limits<std::uint16_t>::max() || argument.longFlag.size() > std::numeric_limits<std::uint16_t>::max())
			throw file_write_error("argument flag is too long to serialize");
		append_u16(bytes, static_cast<std::uint16_t>(argument.shortFlag.size()));
		bytes.insert(bytes.end(), argument.shortFlag.begin(), argument.shortFlag.end());
		append_u16(bytes, static_cast<std::uint16_t>(argument.longFlag.size()));
		bytes.insert(bytes.end(), argument.longFlag.begin(), argument.longFlag.end());
		bytes.push_back(argument.supplied ? 1 : 0);
		if (argument.values.size() > std::numeric_limits<std::uint32_t>::max())
			throw file_write_error("too many values to serialize");
		append_u32(bytes, static_cast<std::uint32_t>(argument.values.size()));
		for (const auto &value : argument.values)
		{
			if (value.size() > std::numeric_limits<std::uint32_t>::max())
				throw file_write_error("argument value is too large to serialize");
			append_u32(bytes, static_cast<std::uint32_t>(value.size()));
			bytes.insert(bytes.end(), value.begin(), value.end());
		}
	}
	append_u32(bytes, crc32(bytes.data(), bytes.size()));
	return bytes;
}

std::map<std::string, PersistedState> deserialize_state(const std::vector<std::uint8_t> &bytes)
{
	constexpr std::size_t maximumBytes = 16 * 1024 * 1024;
	if (bytes.size() < 14 || bytes.size() > maximumBytes)
		throw invalid_binary_format("binary state file has an invalid size");

	const auto storedCrcOffset = bytes.size() - 4;
	const auto storedCrc = static_cast<std::uint32_t>(bytes[storedCrcOffset]) | (static_cast<std::uint32_t>(bytes[storedCrcOffset + 1]) << 8) | (static_cast<std::uint32_t>(bytes[storedCrcOffset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[storedCrcOffset + 3]) << 24);
	if (crc32(bytes.data(), storedCrcOffset) != storedCrc)
		throw invalid_binary_format("binary state checksum mismatch");

	std::size_t offset = 0;
	const auto require = [&](std::size_t count)
	{
		if (count > storedCrcOffset - std::min(offset, storedCrcOffset))
			throw invalid_binary_format("binary state file is truncated");
	};
	const auto read_u16 = [&]()
	{
		require(2);
		const auto value = static_cast<std::uint16_t>(bytes[offset]) | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
		offset += 2;
		return value;
	};
	const auto read_u32 = [&]()
	{
		require(4);
		const auto value = static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
		offset += 4;
		return value;
	};
	const auto read_string = [&](std::size_t length)
	{
		require(length);
		std::string value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
						  bytes.begin() + static_cast<std::ptrdiff_t>(offset + length));
		offset += length;
		return value;
	};

	require(10);
	if (bytes[offset++] != 'A' || bytes[offset++] != 'R' || bytes[offset++] != 'G' || bytes[offset++] != 'P')
		throw invalid_binary_format("binary state magic header is invalid");
	if (read_u16() != 1)
		throw invalid_binary_format("unsupported binary state version");
	const auto count = read_u32();
	if (count > 100000)
		throw invalid_binary_format("binary state has too many records");

	std::map<std::string, PersistedState> states;
	for (std::uint32_t index = 0; index < count; ++index)
	{
		const auto shortLength = read_u16();
		auto shortFlag = read_string(shortLength);
		const auto longLength = read_u16();
		auto longFlag = read_string(longLength);
		require(1);
		const auto suppliedByte = bytes[offset++];
		if (suppliedByte > 1 || (shortFlag.empty() && longFlag.empty()))
			throw invalid_binary_format("binary state contains an invalid argument record");
		PersistedState state;
		state.shortFlag = std::move(shortFlag);
		state.longFlag = std::move(longFlag);
		state.supplied = suppliedByte != 0;
		const auto valueCount = read_u32();
		if (valueCount > 100000)
			throw invalid_binary_format("binary state record has too many values");
		state.values.reserve(valueCount);
		for (std::uint32_t valueIndex = 0; valueIndex < valueCount; ++valueIndex)
			state.values.push_back(read_string(read_u32()));
		const auto key = state.longFlag.empty() ? state.shortFlag : state.longFlag;
		if (!states.emplace(key, std::move(state)).second)
			throw invalid_binary_format("binary state contains duplicate argument records");
	}
	if (offset != storedCrcOffset)
		throw invalid_binary_format("binary state contains trailing data");
	return states;
}

std::vector<std::uint8_t> read_binary_file(const std::string &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input)
		throw file_read_error("cannot open binary state file for reading: " + path);
	const auto length = input.tellg();
	if (length < 0 || static_cast<std::uint64_t>(length) > 16u * 1024u * 1024u)
		throw file_read_error("binary state file has an invalid size: " + path);
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
	input.seekg(0);
	if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw file_read_error("failed to read binary state file: " + path);
	return bytes;
}

void write_binary_file(const std::string &path, const std::vector<std::uint8_t> &bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
		throw file_write_error("cannot open binary state file for writing: " + path);
	if (!bytes.empty())
		output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!output)
		throw file_write_error("failed to write binary state file: " + path);
}

} // namespace parser::detail
