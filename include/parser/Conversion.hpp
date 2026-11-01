#pragma once

#include "Exceptions.hpp"

#include <charconv>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace parser
{

template <typename T, typename Enable = void>
struct Converter
{
	static T convert(std::string_view text)
	{
		std::istringstream input{std::string(text)};
		T value{};
		if (!(input >> value) || (input >> std::ws, !input.eof()))
			throw invalid_value("cannot convert '" + std::string(text) + "' to the requested type");
		return value;
	}

	static constexpr std::string_view type_name() noexcept
	{
		return "VALUE";
	}
};

template <>
struct Converter<std::string>
{
	static std::string convert(std::string_view text);

	static constexpr std::string_view type_name() noexcept
	{
		return "STRING";
	}
};

template <>
struct Converter<std::string_view>
{
	static std::string_view convert(std::string_view text) noexcept;

	static constexpr std::string_view type_name() noexcept
	{
		return "STRING";
	}
};

template <>
struct Converter<bool>
{
	static bool convert(std::string_view text);

	static constexpr std::string_view type_name() noexcept
	{
		return "BOOLEAN";
	}
};

template <typename T>
struct Converter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
{
	static T convert(std::string_view text)
	{
		T value{};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
			throw invalid_value("cannot convert '" + std::string(text) + "' to INTEGER");
		return value;
	}

	static constexpr std::string_view type_name() noexcept
	{
		return "INTEGER";
	}
};

template <typename T>
struct Converter<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
	static T convert(std::string_view text)
	{
		T value{};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
			throw invalid_value("cannot convert '" + std::string(text) + "' to FLOAT");
		return value;
	}

	static constexpr std::string_view type_name() noexcept
	{
		return "FLOAT";
	}
};

template <>
struct Converter<std::filesystem::path>
{
	static std::filesystem::path convert(std::string_view text);

	static constexpr std::string_view type_name() noexcept
	{
		return "PATH";
	}
};

template <typename T>
T convert(std::string_view text)
{
	try
	{
		return Converter<T>::convert(text);
	}
	catch (const invalid_value &)
	{
		throw;
	}
	catch (const std::exception &exception)
	{
		throw invalid_value("cannot convert '" + std::string(text) + "': " + exception.what());
	}
}

} // namespace parser
