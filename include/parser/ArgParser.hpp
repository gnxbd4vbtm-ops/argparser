#pragma once

#include "Argument.hpp"
#include "Exceptions.hpp"

#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace parser
{

class ArgParser
{
  public:
	ArgParser();

	void addArg(ArgumentConfig config);

	template <typename Callable>
	void arg(std::string shortFlag, std::string longFlag, std::string description, Callable &&callable)
	{
		ArgumentConfig config;
		config.shortFlag = std::move(shortFlag);
		config.longFlag = std::move(longFlag);
		config.description = std::move(description);
		config.target = std::forward<Callable>(callable);
		addArg(std::move(config));
	}

	template <typename Callable>
	void arg(std::string flag, std::string description, Callable &&callable)
	{
		arg(std::move(flag), {}, std::move(description), std::forward<Callable>(callable));
	}

	bool removeArg(std::string_view flag);
	bool deleteArg(std::string_view flag);
	bool hasArg(std::string_view flag) const;
	bool exists(std::string_view flag) const;
	bool wasProvided(std::string_view flag) const;
	const std::vector<std::string> &values(std::string_view flag) const;

	template <typename T>
	T get(std::string_view flag) const
	{
		const auto &stored = values(flag);
		if constexpr (std::is_same_v<T, bool>)
		{
			const auto &argument = arguments_[index_for(flag)];
			if (stored.empty() && argument.typeName == "BOOLEAN")
				return argument.supplied;
		}
		if (stored.empty())
			throw missing_argument_value("argument " + std::string(flag) + " has no value");
		return parser::convert<T>(stored.front());
	}

	template <typename T>
	std::vector<T> getAll(std::string_view flag) const
	{
		const auto &stored = values(flag);
		std::vector<T> result;
		result.reserve(stored.size());
		for (const auto &value : stored)
			result.push_back(parser::convert<T>(value));
		return result;
	}

	const std::vector<Argument> &arguments() const noexcept;
	void reset() noexcept;

	bool parse(int argc, char **argv);
	bool parse(std::span<const std::string_view> tokens);
	bool parse(const std::vector<std::string> &tokens);

	void setApplicationName(std::string name);
	void setApplicationDescription(std::string description);
	void addExample(std::string example);
	void setColorsEnabled(bool enabled) noexcept;
	void setAutomaticUsage(bool enabled) noexcept;
	bool automaticUsage() const noexcept;
	std::string usage() const;
	void printHelp(std::ostream &output = std::cout) const;
	void save(const std::string &path) const;
	void read(const std::string &path);

  private:
	struct Occurrence;

	static std::string canonical_flag(const Argument &argument);
	static void validate_flag(const std::string &flag, bool shortFlag);
	void validate_definition(const ArgumentConfig &config) const;
	std::size_t index_for(std::string_view flag) const;
	void rebuild_lookup();
	static bool looks_like_flag(std::string_view token);
	static bool is_negative_number(std::string_view token);
	static std::string expand_environment(std::string_view input);
	bool is_negative_value_for(const Argument &argument, std::string_view token) const;
	bool parse_tokens(std::span<const std::string_view> tokens);

	std::vector<Argument> arguments_;
	std::unordered_map<std::string, std::size_t> lookup_;
	std::string applicationName_;
	std::string applicationDescription_;
	std::vector<std::string> examples_;
	bool automaticUsage_ = true;
	bool colorsEnabled_ = true;
};

} // namespace parser
